// Copyright [2024] NYU

#ifndef SRC_MULTICAST_PROXY_CLOUDEX_SEQUENCER_HPP_
#define SRC_MULTICAST_PROXY_CLOUDEX_SEQUENCER_HPP_

#include <inttypes.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <queue>
#include <cstdlib>
#include <map>
#include <unordered_map>
#include <functional>
#include <utility>
#include <vector>
#include <string>
#include <tuple>

#include "./../cameron314/readerwriterqueue.h"
#include "./../utils/common.hpp"

/**
 * Note: critical is only added to the types to hold sufficient information for debugging. It does not affect sorting
 */

// {timestamp, trader id, order id, critical, dummy}
typedef std::tuple<
    uint64_t, uint32_t, uint64_t, bool, bool> SEQ_PQ_ELE;
// {order id, timestamp, critical, dummy}
typedef std::tuple<
    uint64_t, uint64_t, bool, bool> MP_Q_ELE;

class Cloudexsequencer {
 private:
    uint32_t total_children;
    size_t starting_child_index = 0;  // undefined at receivers

    std::vector<
        moodycamel::BlockingReaderWriterQueue<MP_Q_ELE>> per_child_queue;  // index i belongs to mp i

    std::priority_queue<
        SEQ_PQ_ELE, std::vector<SEQ_PQ_ELE>,
        std::greater<SEQ_PQ_ELE>> pq;  // stores earliest non-dequeued order of each mp
    std::vector<bool> present;  // index i represents if an order of mp i is present in the priority queue

    uint64_t sequenced_until = 0;
    std::atomic<uint64_t> unfaired;  // how many orders with t are processed when sequenced_until > t
    std::atomic<uint64_t> total;  // total orders

 public:
    moodycamel::BlockingReaderWriterQueue<
        std::tuple<uint64_t, uint64_t, bool>> result;  // {order id, ts, critical}, ts is just added for easy debugging

    explicit Cloudexsequencer(uint32_t total_children, size_t starting_child_index): total_children(total_children),
        starting_child_index(starting_child_index), per_child_queue(total_children), present(total_children) {
        unfaired.store(0);
        total.store(0);
    }

    void enqueue(uint64_t order_id, uint32_t child_id, uint64_t order_ts, bool critical, bool dummy);
    void dequeue();
    void logs();
};

void Cloudexsequencer::enqueue(uint64_t order_id, uint32_t child_id, uint64_t order_ts, bool critical, bool dummy) {
    bool res = per_child_queue.at(child_id - starting_child_index).enqueue(
        std::tuple(order_id, order_ts, critical, dummy));
    if (!res) {
        std::cerr << "Cloudexsequencer::enqueue failed. Probably memory issue." << std::endl;
        exit(1);
    }
}

void Cloudexsequencer::dequeue() {
    auto thread = std::thread([this] { this->logs(); });

    while (1) {
        uint64_t smallest_ts = UINT64_MAX;
        int index = -1;

        for (uint32_t i = 0; i < total_children; i++) {
            auto top = per_child_queue.at(i).peek();
            if (top == nullptr) {
                // let's clean up some dummys
                for (uint32_t j = 0; j < total_children; j++) {
                    while (1) {
                        top = per_child_queue.at(i).peek();
                        if (top != nullptr && std::get<3>(*top)
                            && per_child_queue.at(j).size_approx() > 2) { per_child_queue.at(j).pop(); }
                        else
                            break;
                    }
                }

                continue;
            }

            const auto& ts = std::get<1>(*top);

            if (ts < smallest_ts
                && (static_cast<uint64_t>(get_current_time() - ts) >= CONFIG::ORDERS_SUBMISSION::SEQUENCER_DELAY)) {
                smallest_ts = ts;
                index = i;
            }
        }

        if (index != -1) {
            MP_Q_ELE res;
            per_child_queue.at(index).wait_dequeue(res);
            const auto& [order_id, order_ts, critical, dummy] = res;
            if (order_ts < sequenced_until) unfaired.fetch_add(1);
            sequenced_until = std::max(sequenced_until, order_ts);
            total.fetch_add(1);
            if (!dummy) result.enqueue(std::make_tuple(order_id, order_ts, critical));
        }
    }

    if (thread.joinable()) thread.join();
}

void Cloudexsequencer::logs() {
    if (CONFIG::ORDERS_SUBMISSION::LOGGING == false) return;
    while (1) {
        for (uint32_t i = 0; i < total_children; i++) {
            std::clog << "Child " << i << " : " << per_child_queue[i].size_approx() << std::endl;
        }

        std::clog << "Unfairness ratio(%): " << (100.0*unfaired.load()) / (1.0* total.load()) << std::endl;
        sleep(2);
    }
}
#endif  // SRC_MULTICAST_PROXY_CLOUDEX_SEQUENCER_HPP_
