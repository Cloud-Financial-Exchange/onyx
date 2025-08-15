// Copyright [2024] NYU

#ifndef SRC_MULTICAST_PROXY_MATCHINGENGINE_HPP_
#define SRC_MULTICAST_PROXY_MATCHINGENGINE_HPP_

#include <inttypes.h>
#include <algorithm>
#include <queue>
#include <cstdlib>
#include <map>
#include <unordered_map>
#include <functional>
#include <utility>
#include <vector>
#include <string>
#include <tuple>

#include "./delaymonitor.hpp"
#include "./gfifo_sequencer.hpp"
#include "./noop_sequencer.hpp"
#include "./../message.h"
#include "./../cameron314/readerwriterqueue.h"


typedef std::pair<uint64_t, uint64_t> ME_ELE_T;  // {recv_timestamp, order id}
typedef std::pair<uint64_t, uint64_t> SEQ_ELE_T;  // {recv_timestamp, order.timestamp}

class Matchingengine {
 private:
    std::string proxy_ip;
    size_t starting_children_index = 0;
    uint32_t sequencer_delay;
    uint64_t logs_flush_threshold;
    uint64_t last_flush;

    std::mutex mtx_orders;
    std::unordered_map<uint64_t, OrderMsg> orders;  // {order id, order}

    std::mutex mtx_qs;
    std::map<
        double,  // price
        std::priority_queue<ME_ELE_T, std::vector<ME_ELE_T>, std::greater<ME_ELE_T>>,  // each scuh queue sees time
        std::greater<double>  // bid prices should be in descending order
        > bids_qs;

    std::map<
        double,
        std::priority_queue<ME_ELE_T, std::vector<ME_ELE_T>, std::greater<ME_ELE_T>>> asks_qs;

    // Shows what is highest timestamped order that has been matched, used for order matching fariness calc.
    std::map<double, uint64_t> sequenced_bids;  // price level -> sequenced until
    std::map<double, uint64_t> sequenced_asks;  // price level -> sequenced until

    std::vector<std::pair<uint64_t, uint64_t>>* orders_latencies;  // {match-timestamp, latency}
    std::mutex logs_mtx;
    std::atomic<int> counter{0};  // total received orders
    std::atomic<int> counter_matched{0};  // total matched orders
    std::atomic<int> active_orders_counter{0};  // total orders in LOB

    std::vector<std::string> downstreams;
    Delaymonitor* delaymonitor;

    void enqueue(uint64_t order_id);
    void flush_logs();
    void get_sequencer_output();

    std::map<
        double,  // price
        std::priority_queue<SEQ_ELE_T, std::vector<SEQ_ELE_T>, std::greater<SEQ_ELE_T>>> sequencer;
    std::map<double, uint64_t> sequenced_until;
    std::atomic<int> out_of_orders;
    std::mutex mtx_seq;

    moodycamel::BlockingReaderWriterQueue<std::pair<uint64_t, uint64_t>> q;  // all orders are logged here
    moodycamel::BlockingReaderWriterQueue<int> active_orders_q;  // LOB size is recorded here
    moodycamel::BlockingReaderWriterQueue<
        std::pair<uint64_t, uint64_t>> critical_orders_owd;  // just for logging during stress test
    moodycamel::BlockingReaderWriterQueue<
        std::pair<uint64_t, uint64_t>> noncritical_orders_owd;  // just for logging during stress test

    zmq::context_t context;

    sequencer_type* gfifo_sequencer;

 public:
    Matchingengine(std::string proxy_ip, size_t starting_children_index,
    uint32_t sequencer_delay, uint64_t logs_flush_threshold, std::vector<std::string> downstreams);

    void receive_orders();
    void match_orders();
};

Matchingengine::Matchingengine(std::string proxy_ip, size_t starting_children_index, uint32_t sequencer_delay,
    uint64_t logs_flush_threshold, std::vector<std::string> downstreams):
    proxy_ip(proxy_ip), starting_children_index(starting_children_index), sequencer_delay(sequencer_delay),
    logs_flush_threshold(logs_flush_threshold), last_flush(0), counter(0), counter_matched(0),
    downstreams(downstreams), out_of_orders(0), context(3) {
    orders_latencies = new std::vector<std::pair<uint64_t, uint64_t>>();

    // matching engine always runs at the root of the tree
    // there is no hedging at the root, so we do not shrink downstreams
    // like we do in ordersqueue
    delaymonitor = new Delaymonitor(proxy_ip, downstreams);
    int e = system("touch alogs.csv mlogs.csv qlogs.csv slogs.csv && rm alogs.csv mlogs.csv qlogs.csv slogs.csv");
    if (e != 0) {
        std::cerr << "Could not rm alogs.csv mlogs.csv qlogs.csv slogs.csv" << e << std::endl;
        exit(1);
    }

    gfifo_sequencer = new sequencer_type(downstreams.size(), starting_children_index);
}

void Matchingengine::enqueue(uint64_t order_id) {
    OrderMsg order;
    {
        std::lock_guard<std::mutex> lock(mtx_orders);
        order = orders[order_id];
    }

    auto curr_time = static_cast<uint64_t>(get_current_time());
    q.enqueue({curr_time, curr_time - order.last_hop_ts});

    std::lock_guard<std::mutex> lock(mtx_qs);
    if (order.is_bid) {
        if (order.timestamp < sequenced_bids[order.price]) out_of_orders.fetch_add(1);
        auto& pq = bids_qs[order.price];
        pq.push({curr_time, order.id});
    } else {
        if (order.timestamp < sequenced_asks[order.price]) out_of_orders.fetch_add(1);
        auto& pq = asks_qs[order.price];
        pq.push({curr_time, order.id});
    }

    active_orders_counter.fetch_add(1);
}

void Matchingengine::receive_orders() {
    auto thread = std::thread([this] { this->get_sequencer_output(); });

    zmq::socket_t pullsock(context, ZMQ_PULL);
    auto [ip1, port1] = get_order_submission_address(proxy_ip);
    auto pull_addr = "tcp://" + ip1 + ":" + std::to_string(port1);
    std::clog << "Puller on " << pull_addr << std::endl;
    pullsock.bind(pull_addr);

    zmq::message_t message;
    while (1) {
        auto res = pullsock.recv(message, zmq::recv_flags::none);
        if (!res || *res < sizeof(OrderMsg)) {
            std::cerr << "Did not receive successfully!" << std::endl;
            exit(1);
        }

        OrderMsg* order = reinterpret_cast<OrderMsg*>(message.data());
        if (false == CONFIG::ORDERS_SUBMISSION::STRESS_TEST) {
            std::lock_guard<std::mutex> lock(mtx_orders);  // make mtx_orders concurrent to remove contention please
            orders[order->id] = *order;
        }

        gfifo_sequencer->enqueue(
            order->id, order->last_hop_id, order->timestamp,
            is_order_critical(order->price, order->is_bid), order->is_dummy);

        if (order->is_dummy == false) counter.fetch_add(1);
    }

    if (thread.joinable()) thread.join();
}

void Matchingengine::get_sequencer_output() {
    auto thread = std::thread([this] { this->gfifo_sequencer->dequeue(); });
    // uint64_t curr = 0;
    while (1) {
        std::tuple<uint64_t, uint64_t, bool> res;
        gfifo_sequencer->result.wait_dequeue(res);
        const auto& [order_id, ts, critical] = res;

        if (false == CONFIG::ORDERS_SUBMISSION::STRESS_TEST) {
            enqueue(order_id);
        }

        // assert(curr <= ts);  // only true if not using LOQ
        // curr = ts;
        if (CONFIG::ORDERS_SUBMISSION::STRESS_TEST) {
            auto curr_time = static_cast<uint64_t>(get_current_time());
            if (true || critical) {
                if (false == critical_orders_owd.enqueue({curr_time, curr_time-ts})) {
                    std::cerr << "Could not enqueue to critical_orders_owd" << std::endl;
                }
            } else {
                if (false == noncritical_orders_owd.enqueue({curr_time, curr_time-ts})) {
                    std::cerr << "Could not enqueue to noncritical_orders_owd" << std::endl;
                }
            }
        }
    }
    if (thread.joinable()) thread.join();
}

void Matchingengine::match_orders() {
    auto thread = std::thread([this] { this->flush_logs(); });

    while (1) {
        uint64_t ts_bid, ts_ask;
        auto curr_time = static_cast<uint64_t>(get_current_time());

        std::lock_guard<std::mutex> lock_bids(mtx_qs);

        // Get the bid
        if (bids_qs.empty()) { YIELD_AND_CONTINUE; }
        auto elem_bids = bids_qs.begin();
        auto price_bid = elem_bids->first;
        auto& pq_bid = elem_bids->second;

        assert(pq_bid.empty() == false);
        const auto [recv_timestamp_bid, id_bid] = pq_bid.top();
        // if (curr_time - recv_timestamp_bid < sequencer_delay) { YIELD_AND_CONTINUE; }

        // Get the ask
        if (asks_qs.empty()) { YIELD_AND_CONTINUE; }
        auto elem_asks = asks_qs.begin();
        auto price_ask = elem_asks->first;
        auto& pq_ask = elem_asks->second;

        if (price_bid < price_ask) { YIELD_AND_CONTINUE; }  // early termination

        assert(pq_ask.empty() == false);
        const auto [recv_timestamp_ask, id_ask] = pq_ask.top();
        // if (curr_time - recv_timestamp_ask < sequencer_delay) { YIELD_AND_CONTINUE; }

        // Match the id_bid and id_ask.
        // 1. Remove the corresponding orders from the priority queues and maps
        // 2. Update portfolios (todo)
        // 3. Generate announcement (todo)

        {
            std::lock_guard<std::mutex> lock(mtx_orders);
            ts_bid = orders[id_bid].timestamp;  // timestamp of order generation
            ts_ask = orders[id_ask].timestamp;

            sequenced_bids[price_bid] = std::max(sequenced_bids[price_bid], ts_bid);
            sequenced_asks[price_ask] = std::max(sequenced_asks[price_ask], ts_ask);
            counter_matched.fetch_add(2);

            pq_bid.pop();
            orders.erase(id_bid);
            if (pq_bid.empty()) { bids_qs.erase(price_bid); }

            pq_ask.pop();
            orders.erase(id_ask);
            if (pq_ask.empty()) { asks_qs.erase(price_ask); }
        }

        // Logging
        {
            active_orders_counter.fetch_sub(2);

            std::lock_guard lock(logs_mtx);
            orders_latencies->push_back({curr_time, curr_time-ts_bid});
            orders_latencies->push_back({curr_time, curr_time-ts_ask});
        }

        YIELD_AND_CONTINUE;
    }

    if (thread.joinable()) thread.join();
}

void Matchingengine::flush_logs() {
    std::vector<std::pair<uint64_t, uint64_t>>* data;

    if (CONFIG::ORDERS_SUBMISSION::STRESS_TEST) {
        while (1) {
            std::ofstream file(CONFIG::ORDERS_SUBMISSION::LATENCY_LOGS, std::ios::app);
            if (!file.is_open()) { std::cerr << "Error opening slogs file: " << std::endl; return; }

            size_t qsize = critical_orders_owd.size_approx();
            std::pair<uint64_t, uint64_t> item;
            while (qsize--) {
                auto status = critical_orders_owd.try_dequeue(item);
                if (!status) continue;

                file << item.first << "," << item.second << "\n";
            }

            file.close();

            // also empty control groupish
            qsize = noncritical_orders_owd.size_approx();
            while (qsize--) {
                auto status = noncritical_orders_owd.try_dequeue(item);
                if (!status) continue;
            }

            if (CONFIG::ORDERS_SUBMISSION::LOGGING)
                std::clog << "Orders log written! Total order received by now: " << counter.load() << std::endl;
            sleep(5);
        }
    }

    while (1) {
        // std::clog << "Unfairess (acc. to order matching fairness definition): "
        // <<  (1.0 * out_of_orders.load() * 100.0) / (1.0 * counter_matched.load()) << std::endl;
        // // std::clog << "Out of order (abs): " <<  out_of_orders.load() << std::endl;

        auto curr_time = static_cast<uint64_t>(get_current_time());
        if (curr_time - last_flush >= logs_flush_threshold) {
            std::ofstream file(CONFIG::ORDERS_SUBMISSION::LOGS_FILE, std::ios::app);
            if (!file.is_open()) { std::cerr << "Error opening logs file: " << std::endl; return; }

            {
                std::lock_guard lock(logs_mtx);
                data = orders_latencies;
                orders_latencies = new std::vector<std::pair<uint64_t, uint64_t>>();
            }

            if (data->size()) {
                auto duration = (1.0 * (data->back().first - data->front().first)) / 1'000'000;  // in seconds
                std::clog << "Goodput: " << (1.0 * data->size()) / duration << std::endl;
            }

            for (const auto& [ts, lat] : *data) { file << ts << "," << lat << "\n"; }
            data->clear();

            file.close();
            last_flush = curr_time;
            std::clog << "Orders log written! Total order received by now: " << counter.load() << std::endl;
        }

        // flushing all orders
        {
            std::ofstream file(CONFIG::ORDERS_SUBMISSION::ALOGS_FILE, std::ios::app);
            if (!file.is_open()) { std::cerr << "Error opening alogs file: " << std::endl; return; }

            size_t qsize = q.size_approx();
            std::pair<uint64_t, uint64_t> item;
            while (qsize--) {
                auto status = q.try_dequeue(item);
                if (!status) continue;

                file << item.first << "," << item.second << "\n";
            }

            file.close();
        }

        // flushing LOB size q
        {
            std::clog << "Total orders in LOB: " << active_orders_counter.load() << std::endl;

            // std::ofstream file(CONFIG::ORDERS_SUBMISSION::QLOGS_FILE, std::ios::app);
            // if (!file.is_open()) { std::cerr << "Error opening qlogs file: " << std::endl; return; }

            // size_t qsize = active_orders_q.size_approx();
            // int item = 0;
            // while (qsize--) {
            //     auto status = active_orders_q.try_dequeue(item);
            //     if (!status) continue;

            //     file << item << "\n";
            // }

            // file.close();
        }

        sleep(5);
    }
}

#endif  // SRC_MULTICAST_PROXY_MATCHINGENGINE_HPP_
