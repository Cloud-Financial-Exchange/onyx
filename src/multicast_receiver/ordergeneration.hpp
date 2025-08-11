// Copyright [2023] NYU

#ifndef SRC_MULTICAST_RECEIVER_ORDERGENERATION_HPP_
#define SRC_MULTICAST_RECEIVER_ORDERGENERATION_HPP_

#include <string>
#include <utility>

#include "./../utils/common.hpp"
#include "./../utils/ordersqueue.hpp"
#include "./../message.h"

/**
 * Generate Bids and Asks
 * Alternate between bids and asks generation
 * Have a price range [x,y] for bids and a separate price range [a,b] for asks
 * Uniformly randomly pick price from the range while order generation
 * Only the right end of bids-range, y, and the left end of asks range, a, should overlap
 * */

class Ordergeneration {
    std::string receiver_ip;
    std::string parent_ip;
    uint32_t receiver_id;
    Ordersqueue* ordersqueue;

    uint32_t order_num;
    bool is_bid;

    std::pair<int, int> bid_range;
    std::pair<int, int> ask_range;

    uint16_t common_seed;

    bool use_fancypq;
    void generate_order(OrderMsg* order);

 public:
    Ordergeneration(std::string receiver_ip, std::string parent_ip, uint32_t receiver_id, Ordersqueue* ordersqueue):
    receiver_ip(receiver_ip), parent_ip(parent_ip), receiver_id(receiver_id), ordersqueue(ordersqueue) {
        order_num = 0; is_bid = false;
        std::srand(static_cast<unsigned int>(std::time(nullptr)));

        bid_range = {1, 10};
        ask_range = {10, 19};

        common_seed = 321;  // so that everyone receiver generates the same # of orders
    }

    void start(int experiment_time_in_seconds, int rate, uint64_t orders_start_time,
        int32_t bid_start, int32_t bid_end, int32_t ask_start, int32_t ask_end,
        bool fixed_interarrival, bool use_fancypq);
};

void Ordergeneration::generate_order(OrderMsg* order) {
    uint64_t order_id = receiver_id;
    order_id = (order_id << 32) | ++order_num;
    order->id = order_id;

    order->mp_id = receiver_id;

    order->is_bid = is_bid;
    is_bid = !is_bid;

    order->timestamp = get_current_time();

    order->price = bid_range.first + (std::rand() % (bid_range.second - bid_range.first + 1));
    if (!order->is_bid) order->price = ask_range.first + (std::rand() % (ask_range.second - ask_range.first + 1));

    order->count = 1;
    order->is_fancy_pq = use_fancypq;

    return;
}

void Ordergeneration::start(int experiment_time_in_seconds, int rate, uint64_t orders_start_time,
    int32_t bid_start, int32_t bid_end, int32_t ask_start, int32_t ask_end,
    bool fixed_interarrival, bool use_fancy) {
    this->use_fancypq = use_fancy;

    bid_range = {bid_start, bid_end};
    ask_range = {ask_start, ask_end};

    double burst_rate = rate * CONFIG::ORDERS_SUBMISSION::BURST_FACTOR;
    double burst_duration = 1.0;   // Burst duration in seconds
    // double burst_interval = 5;  // Time between bursts in seconds
    double burst_interval = 3.0;  // Time between bursts in seconds

    std::random_device rd;
    std::mt19937 generator(common_seed);
    std::exponential_distribution<> exp_dist(rate);

    OrderMsg* order = new OrderMsg();

    while (1) {
        if (get_current_time() < static_cast<int64_t>(orders_start_time)) continue;
        break;
    }

    double curr_relative_time = 0;
    auto next_time = std::chrono::system_clock::now();
    uint64_t total_orders = 0;

    auto last_burst_time = std::chrono::system_clock::now();
    bool in_burst_mode = false;

    while (curr_relative_time < experiment_time_in_seconds) {
        double interarrival_time = exp_dist(generator);

        auto current_time = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_time = std::chrono::duration_cast<std::chrono::duration<double>>(
            current_time - last_burst_time);

        if (!in_burst_mode && CONFIG::ORDERS_SUBMISSION::BURST_MODE && elapsed_time.count() >= burst_interval) {
            in_burst_mode = true;
            last_burst_time = current_time;
            exp_dist = std::exponential_distribution<>(burst_rate);
        } else if (in_burst_mode && elapsed_time.count() >= burst_duration) {
            in_burst_mode = false;
            exp_dist = std::exponential_distribution<>(rate);
        }

        if (fixed_interarrival) {
            interarrival_time = 1.0 / rate;
        }
        curr_relative_time += interarrival_time;

        next_time += std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::duration<double>(interarrival_time));

        if (curr_relative_time < experiment_time_in_seconds) {
            generate_order(order);

            order->last_hop_ts = get_current_time();

            ordersqueue->shortcut_enqueue(order);

            total_orders++;

            // busy sleep until next_time
            auto microseconds_next_time = std::chrono::duration_cast<std::chrono::microseconds>(
                next_time.time_since_epoch());
            int64_t microsecondscount_next_time = microseconds_next_time.count();
            while (1) {
                if (get_current_time() < microsecondscount_next_time) continue;
                break;
            }
        }
    }

    std::clog << "Done generating [ " << total_orders << " ] orders!" << std::endl;
    delete order;
}

#endif  // SRC_MULTICAST_RECEIVER_ORDERGENERATION_HPP_
