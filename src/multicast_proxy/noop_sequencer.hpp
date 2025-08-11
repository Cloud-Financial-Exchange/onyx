// Copyright [2024] NYU

#ifndef SRC_MULTICAST_PROXY_NOOP_SEQUENCER_HPP_
#define SRC_MULTICAST_PROXY_NOOP_SEQUENCER_HPP_

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

// No op sequencer, does nothing to the messages
class Noopsequencer {
 public:
    moodycamel::BlockingReaderWriterQueue<
        std::tuple<uint64_t, uint64_t, bool>> result;  // {order id, ts, critical}, ts is just added for easy debugging

    explicit Noopsequencer(uint32_t total_mps, size_t starting_child_index) { }

    void enqueue(uint64_t order_id, uint32_t mp_id, uint64_t order_ts, bool critical, bool dummy);
    void dequeue();
    void logs();
};

void Noopsequencer::enqueue(uint64_t order_id, uint32_t mp_id, uint64_t order_ts, bool critical, bool dummy) {
    if (!dummy) result.enqueue(std::make_tuple(order_id, order_ts, critical));
}

void Noopsequencer::dequeue() {
    auto thread = std::thread([this] { this->logs(); });
    if (thread.joinable()) thread.join();
}

void Noopsequencer::logs() {
    while (1) {
        std::cout << "Total in sequencer: " << result.size_approx() << std::endl;
        sleep(2);
    }
}
#endif  // SRC_MULTICAST_PROXY_NOOP_SEQUENCER_HPP_
