// Copyright [2023] NYU

#ifndef SRC_UTILS_TRADING_HPP_
#define SRC_UTILS_TRADING_HPP_

#include <algorithm>
#include <cstdint>
#include <random>
#include <map>
#include <unordered_map>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "./common.hpp"

enum Tradetype {
    BUY,
    SELL,
};

struct Tradedatapoint {
    int64_t type;  // 0 means BUY, 1 means SELL
    int64_t symbol;  // 0 means $APLE, 1 means $MSFT
    int64_t num_shares;
    int64_t price_per_share;

    int64_t mp_id;  // MP id
    int64_t trade_id;
    int64_t timestamp;
    int64_t recv_b4;

    void show() {
        return;
        std::cout <<"T: " << type << " S:" << symbol << " #:" << num_shares << " $" << price_per_share << std::endl;
    }
};

// Used for simulating a toy trading setup
class Tradingmatchingengine {
    int total_trading_symbols;

    std::mt19937 random_generator;
    std::uniform_int_distribution<> dist_num_shares;
    std::uniform_int_distribution<> dist_price_per_share;

    std::unordered_map<int64_t, std::unordered_map<int64_t, int64_t>> available_shares;  // {symbol: price: # of shares}
    std::unordered_map<int64_t, std::unordered_map<int64_t, int64_t>> requested_shares;  // {symbol: price: # of shares}

    std::unordered_map<int64_t, std::vector<int64_t>> user_balance;  // {user_id: {dollars after each trade}}
    std::unordered_map<int64_t, std::unordered_map<int64_t, int64_t>> user_shares;  // {user_id: {symbol: shares}}

    std::map<int64_t, std::vector<Tradedatapoint>> received_orders;  // {tradeId: {trades}}
    bool active = false;
    int sockfd;
    size_t total_mps;
    int64_t batch;  // batch = x means MPs within x microseconds of each other are given the same priority
    int64_t total_processed;
    std::map<int64_t, std::unordered_map<int64_t, int64_t>> balance_timeline;  // {time: {user_id: balance}}
    std::unordered_map<int64_t, std::map<int64_t, int64_t>> winning_audit;  // {user_id: {winning margin: occurences}}

    std::vector<int64_t> mp_ids;

 public:
    explicit Tradingmatchingengine(int proxy_num) {
        active = proxy_num == 0;
        if (proxy_num != 0) return;

        total_trading_symbols = 2;
        srand(time(0));

        std::random_device rd;
        std::mt19937 gen(rd());
        random_generator = gen;

        std::uniform_int_distribution<> dist(10, 30);
        dist_num_shares = dist;

        std::uniform_int_distribution<> dist2(10, 30);
        dist_price_per_share = dist2;

        auto root_addr = get_proxy_management_ip(0);
        auto [ip, port] = get_trading_address(root_addr);
        sockfd = get_bound_udp_socket(inet_addr(ip.c_str()), port);

        total_mps = 4;
        // Set user data
        user_shares[0][0] = 100000000;
        user_shares[1][0] = 100000000;
        user_shares[0][1] = 100000000;
        user_shares[1][1] = 100000000;

        user_shares[99][0] = 100000000;
        user_shares[98][0] = 100000000;
        user_shares[99][1] = 100000000;
        user_shares[98][1] = 100000000;

        user_balance[0] = {1};
        user_balance[1] = {1};
        user_balance[99] = {1};
        user_balance[98] = {1};

        batch = 0;
        total_processed = 0;
        mp_ids = {0, 1, 98, 99};
    }

    Tradedatapoint get_data_point() {
        unsigned int seed = time(NULL);

        Tradedatapoint point;
        point.type = Tradetype::BUY;
        point.symbol = rand_r(reinterpret_cast<unsigned int*>(&seed)) % total_trading_symbols;
        point.num_shares = dist_num_shares(random_generator);
        point.price_per_share = dist_price_per_share(random_generator);

        requested_shares[point.symbol][point.price_per_share] += point.num_shares;

        return point;
    }

    void process_order(Tradedatapoint order) {
        assert(order.type == Tradetype::SELL);

        // std::clog << "Received order from : " << order.mp_id << std::endl;
        order.show();

        received_orders[order.trade_id].push_back(order);

        if (received_orders[order.trade_id].size() != total_mps) return;

        auto trades = received_orders[order.trade_id];
        received_orders.erase(order.trade_id);

        std::sort(trades.begin(), trades.end(), [](const Tradedatapoint& a, const Tradedatapoint& b) {
            return a.timestamp < b.timestamp;
        });

        // for (auto tr : trades) {
        //     if (tr.recv_b4 == 0) return;
        //     std::clog << tr.trade_id << ": " << tr.recv_b4 << ", " << tr.mp_id << ", " << tr.timestamp << std::endl;
        // }
        // std::cout << std::endl;

        total_processed++;
        if (trades[1].timestamp - trades[0].timestamp) {
            winning_audit[trades[0].mp_id][trades[1].timestamp - trades[0].timestamp] += 1;
        }

        for (int i = trades.size()-1; i >= 0; i--) {
            if (abs(trades[i].timestamp - trades[0].timestamp) > batch) {
                trades.pop_back();
            } else {
                break;
            }
        }

        std::shuffle(trades.begin(), trades.end(), random_generator);
        order = trades[0];

        if (trades.size() > 1) {
            winning_audit[trades[0].mp_id][0] += 1;
        }

        auto processable = std::min(order.num_shares, user_shares[order.mp_id][order.symbol]);
        processable = std::min(processable, requested_shares[order.symbol][order.price_per_share]);

        if (processable == 0) {
            return;
        }

        available_shares[order.symbol][order.price_per_share] += processable;
        requested_shares[order.symbol][order.price_per_share] -= processable;

        user_shares[order.mp_id][order.symbol] -= processable;
        user_balance[order.mp_id].push_back(user_balance[order.mp_id].back() + (processable * order.price_per_share));
    }

    void listen_for_orders() {
        if (active == false) return;
        char buffer[BUF_SIZE];
        while (true) {
            int bytes_received = recv_message(sockfd, buffer);
            if (static_cast<int>(sizeof(Tradedatapoint)) > bytes_received) continue;
            Tradedatapoint* tp = reinterpret_cast<Tradedatapoint*>(buffer);
            process_order(*tp);
        }
    }

    std::string balance_to_csv(std::map<int64_t, std::unordered_map<int64_t, int64_t>> balance_t) {
        std::ostringstream csv_ss;

        csv_ss << "Time,Recipientid,Balance\n";
        for (auto [t, balance] : balance_t) {
            for (auto [user, b] : balance) {
                csv_ss << t << "," << user << "," << b << "\n";
            }
        }

        std::clog << "csv formed" << std::endl;
        return csv_ss.str();
    }

    void periodic_snapshot() {
        if (CONFIG::SIMULATE_TRADING == false) return;
        while (1) {
            auto t = get_current_time();
            for (auto id : mp_ids) {
                balance_timeline[t][id] = user_balance[id].back();
            }
            sleep(1);
        }
    }

    void show_balance() {
        if (active == false) return;
        if (CONFIG::SIMULATE_TRADING == false) return;
        while (true) {
            std::cout << "["<< total_processed <<"]>> ";
            for (auto [k, v] : user_balance) {
                std::cout << k << ": " << v.back() << " __ " << std::flush;
            }
            std::cout << "<<" << std::endl;

            for (auto [k, v] : winning_audit) {
                std::cout << k << ": ";
                for (auto [margin, occurences] : v) {
                    std::cout << "(" << margin << " = " << occurences << "), ";
                }
                std::cout << std::endl;
            }
            sleep(10);
            if (total_processed >= 5000) {
                std::clog << "Writing results." << std::endl;
                auto data = balance_to_csv(balance_timeline);
                std::ofstream outfile;
                outfile.open("./trading_results.csv", std::ios::out | std::ios_base::app);
                outfile << data << std::endl;
                outfile.close();
                total_processed = 0;
            }
        }
    }
};

class Tradingmp {
    int64_t id;
    int64_t balance;
    std::unordered_map<int64_t, int64_t> shares;  // {symbol: shares}
    int sockfd;
    sockaddr_in matching_engine_addr;

    bool active;

    int64_t selling_thresh;

 public:
    explicit Tradingmp(int64_t id): id(id) {
        balance = 3000000;
        shares[0] = 1000;
        shares[1] = 1000;
        selling_thresh = 20;

        active = false;
        if (id == 0 || id == 1 || id == 99 || id == 98) active = true;

        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        auto root_addr = get_proxy_management_ip(0);
        auto [ip, port] = get_trading_address(root_addr);
        matching_engine_addr = get_sock_addr(ip, port);
    }

    void process_data_point(Tradedatapoint point, int64_t received_before) {
        if (active == false) return;

        if (point.price_per_share > selling_thresh) {
            Tradedatapoint order {
                .type = Tradetype::SELL,
                .symbol = point.symbol,
                .num_shares = point.num_shares,
                .price_per_share = point.price_per_share,
                .mp_id = id,
                .trade_id = point.trade_id,
                .timestamp = get_current_time(),
                .recv_b4 = received_before
            };

            sendto(sockfd, &order, sizeof(order), 0,
                (struct sockaddr *)&matching_engine_addr, sizeof(matching_engine_addr));

            // if (bytes_sent < 0) {
            //     std::cerr << "Could not send order" << std::endl;
            // }
        }
    }
};

#endif  // SRC_UTILS_TRADING_HPP_
