// Copyright [2024] NYU

#ifndef SRC_UTILS_ORDERSQUEUE_HPP_
#define SRC_UTILS_ORDERSQUEUE_HPP_

#include <inttypes.h>
#include <algorithm>
#include <queue>
#include <cstdlib>
#include <tuple>
#include <unordered_map>
#include <set>
#include <vector>
#include <utility>
#include <string>
#include <memory>

#include "./../multicast_proxy/gfifo_sequencer.hpp"
#include "./../multicast_proxy/noop_sequencer.hpp"
#include "./../multicast_proxy/cloudex_sequencer.hpp"
#include "./../multicast_proxy/delaymonitor.hpp"
#include "./../message.h"
#include "./fastmap.hpp"
#include "./../utils/common.hpp"

using sequencer_type = Gfifosequencer;
// using sequencer_type = Noopsequencer;
// using sequencer_type = Cloudexsequencer;

typedef std::tuple<bool, uint64_t, uint64_t> QELE_T;  // critical(0,1), order-time, orderid

// Used by proxies as they relay the received orders from bottom of the tree to the top of the tree
class Ordersqueue {
 private:
    std::string curr_ip;
    size_t starting_children_index = 0;
    std::string parent_ip;
    int32_t curr_id;

    moodycamel::BlockingReaderWriterQueue<QELE_T> simple_q;

    // These are used for LOQv2
    moodycamel::BlockingReaderWriterQueue<QELE_T> critical_loqv2_q;
    moodycamel::BlockingReaderWriterQueue<QELE_T> noncriti_loqv2_q;

    Qtype current_type;

    Fastmap m;  // holds orders by order id

    uint32_t sequencer_delay;  // microseconds

    // returns {is_success, order}
    std::pair<bool, OrderMsg> dequeue();
    std::pair<bool, OrderMsg> dequeue_loqv2();
    std::pair<bool, OrderMsg> dequeue_simple();

    std::vector<std::string> downstreams;
    Delaymonitor* delaymonitor;

    std::unordered_map<uint64_t, uint64_t> logs;  // order count by order generation timestamp
    std::atomic<int64_t> total_received = 0;
    std::atomic<int64_t> total_remaining = 0;
    std::atomic<int64_t> q_size = 0;  // max total_remaining in the inter-log-print window
    std::atomic<int64_t> total_critical_remaining = 0;
    std::atomic<int64_t> total_noncritical_remaining = 0;

    uint64_t last_flush = 0;

    zmq::context_t context;

    sequencer_type* gfifo_sequencer = nullptr;

 public:
    Ordersqueue(std::string curr_ip, size_t starting_children_index, std::string parent_ip,
        uint32_t sequencer_delay, std::vector<std::string> downstreams, int32_t curr_id);

    void enqueue(OrderMsg* order);
    void shortcut_enqueue(OrderMsg* order);  // used by ordergeneration in recievers

    // Receives any orders (children proxies or recievers)
    void receive_orders();

    // Relays any received orders to a parent proxy
    void send_orders();

    void setqueue(Qtype t);

    void get_sequencer_output();
    void flush_logs();
};

Ordersqueue::Ordersqueue(std::string curr_ip, size_t starting_children_index, std::string parent_ip,
    uint32_t sequencer_delay, std::vector<std::string> downstreams, int32_t curr_id) :
    curr_ip(curr_ip), starting_children_index(starting_children_index),
    parent_ip(parent_ip), curr_id(curr_id), sequencer_delay(sequencer_delay), downstreams(downstreams), context(3) {
    if (CONFIG::HEDGING::SIBLING_BASED_HEDGING) {
        auto total_children = this->downstreams.size() / (CONFIG::HEDGING::SIBLING_HEDGING_INTENSITY + 1);
        auto extras = this->downstreams.size() - total_children;  // extras were added due to sibling hedhing
        this->downstreams.erase(this->downstreams.end() - extras, this->downstreams.end());
    }

    delaymonitor = new Delaymonitor(curr_ip, this->downstreams);

    if (this->downstreams.size()) gfifo_sequencer = new sequencer_type(
        this->downstreams.size(), starting_children_index);

    switch (CONFIG::ORDERS_SUBMISSION::QTYPE) {
    case 0: { current_type = Qtype::SIMPLE; break; }
    case 1: { current_type = Qtype::LOQv2; break; }
    default: break;
    }

    setqueue(current_type);
}

void Ordersqueue::setqueue(Qtype t) {
    switch (t) {
    case Qtype::SIMPLE:
        std::cout << "Setting SimplePQ" << std::endl;
        current_type = Qtype::SIMPLE;
        break;

    case Qtype::LOQv2:
        std::cout << "Setting LOQv2" << std::endl;
        current_type = Qtype::LOQv2;
        break;

    default:
        std::cerr << "Unknown qtype requested. " << std::endl;
        exit(1);
    }

    delaymonitor->pace.store(0);
}

void Ordersqueue::enqueue(OrderMsg* order) {
    total_received.fetch_add(1);
    total_remaining.fetch_add(1);
    q_size.exchange(std::max(q_size.load(), total_remaining.load()));

    if (order->is_fancy_pq && current_type != Qtype::LOQv2) {  // does not account for loqv1 now
        setqueue(Qtype::LOQv2);
    }

    if (order->is_fancy_pq == false && current_type != Qtype::SIMPLE) {  // does not account for loqv1 now
        setqueue(Qtype::SIMPLE);
    }

    const bool is_criticl = is_order_critical(order->price, order->is_bid);
    if (is_criticl) total_critical_remaining.fetch_add(1);
    else
        total_noncritical_remaining.fetch_add(1);
    const QELE_T& o = std::make_tuple(is_criticl, order->timestamp, order->id);

    if (current_type == Qtype::LOQv2) {
        if (is_criticl) critical_loqv2_q.enqueue(o);
        else
            noncriti_loqv2_q.enqueue(o);
    } else if (current_type == Qtype::SIMPLE) {
        if (is_criticl) simple_q.enqueue(o);
        else
            simple_q.enqueue(o);
    } else {
        std::cerr << "Unknown q type" << std::endl;
        exit(1);
    }
}

std::pair<bool, OrderMsg> Ordersqueue::dequeue_loqv2() {
    if (critical_loqv2_q.size_approx() != 0) {
        QELE_T top;
        if (critical_loqv2_q.try_dequeue(top)) {
            auto order = m.get(std::get<2>(top));
            m.erase(std::get<2>(top));
            total_critical_remaining.fetch_sub(1);
            return {true, order};
        }
    }

    if (noncriti_loqv2_q.size_approx() != 0) {
        QELE_T top;
        if (noncriti_loqv2_q.try_dequeue(top)) {
            auto order = m.get(std::get<2>(top));
            m.erase(std::get<2>(top));
            total_noncritical_remaining.fetch_sub(1);
            return {true, order};
        }
    }

    return {false, {}};
}

std::pair<bool, OrderMsg> Ordersqueue::dequeue_simple() {
    if (simple_q.size_approx() != 0) {
        QELE_T top;
        if (simple_q.try_dequeue(top)) {
            auto order = m.get(std::get<2>(top));
            m.erase(std::get<2>(top));
            if (std::get<0>(top)) total_critical_remaining.fetch_sub(1);
            else
                total_noncritical_remaining.fetch_sub(1);
            return {true, order};
        }
    }

    return {false, {}};
}

std::pair<bool, OrderMsg> Ordersqueue::dequeue() {
    if (current_type == Qtype::LOQv2) return dequeue_loqv2();
    return dequeue_simple();
}

void generate_dummy_order(uint64_t time, uint32_t mp_id, OrderMsg& order) {
    order.mp_id = mp_id;
    order.timestamp = time;
    order.is_dummy = true;
    unsigned int seed = time;
    order.id = rand_r(&seed);
}

void Ordersqueue::send_orders() {
    zmq::socket_t pushsock(context, ZMQ_PUSH);
    auto [ip, port] = get_order_submission_address(parent_ip);
    auto parent_addr = "tcp://" + ip + ":" + std::to_string(port);
    std::clog << "Pushing to " << parent_addr << std::endl;
    pushsock.connect(parent_addr);

    uint64_t last_sent = 0;
    while (1) {
        auto [status, order] = dequeue();
        auto curr_ts = static_cast<uint64_t>(get_current_time());
        if (status) {
            order.last_hop_ts = curr_ts;
            last_sent = curr_ts;
            order.last_hop_id = curr_id;
            zmq::message_t message(&order, sizeof(OrderMsg));

            zmq::send_result_t result = pushsock.send(message, zmq::send_flags::none);

            if (!result || *result != sizeof(OrderMsg)) {
                std::cerr << "Message could not be queued for sending!" << std::endl;
                exit(1);
            }

            total_remaining.fetch_sub(1);
        } else if (CONFIG::ORDERS_SUBMISSION::DUMMY_GENERATION) {
            if (curr_ts >= last_sent
            && curr_ts - last_sent >= CONFIG::ORDERS_SUBMISSION::THRESH_FOR_DUMMY_GENERATION) {
                generate_dummy_order(curr_ts, curr_id, order);
                last_sent = curr_ts;
                order.last_hop_id = curr_id;

                zmq::message_t message(&order, sizeof(OrderMsg));
                zmq::send_result_t result = pushsock.send(message, zmq::send_flags::none);
                if (!result || *result != sizeof(OrderMsg)) {
                    std::cerr << "Message could not be queued for sending!" << std::endl;
                    exit(1);
                }
            }

            std::this_thread::yield();
        }
    }
}

void Ordersqueue::receive_orders() {
    if (downstreams.size() == 0) return;

    auto thread = std::thread([this] { this->get_sequencer_output(); });
    zmq::socket_t pullsock(context, ZMQ_PULL);
    auto [ip1, port1] = get_order_submission_address(this->curr_ip);
    auto pull_addr = "tcp://" + ip1 + ":" + std::to_string(port1);
    std::clog << "Puller on " << pull_addr << std::endl;
    pullsock.bind(pull_addr);

    while (1) {
        try {
            zmq::message_t message;
            auto res = pullsock.recv(message, zmq::recv_flags::none);
            if (!res || *res < sizeof(OrderMsg)) {
                std::cerr << "Did not receive successfully!" << std::endl;
                exit(1);
            }
            OrderMsg* order = reinterpret_cast<OrderMsg*>(message.data());
            auto curr_ts = static_cast<uint64_t>(get_current_time());
            delaymonitor->record(curr_ts - order->last_hop_ts);

            m.insert(order->id, *order);

            bool is_critical = is_order_critical(order->price, order->is_bid);
            if (downstreams.size()) {  // is not a receiver
                gfifo_sequencer->enqueue(order->id, order->last_hop_id, order->timestamp, is_critical, order->is_dummy);
            } else {
                enqueue(order);
            }
        } catch (const zmq::error_t& e) {
            std::cerr << "ZeroMQ error: " << e.what() << std::endl;
            exit(1);
        }
    }

    if (thread.joinable()) thread.join();
}

void Ordersqueue::get_sequencer_output() {
    if (downstreams.size() == 0) return;  // no sequencer for receivers

    auto thread = std::thread([this] { this->gfifo_sequencer->dequeue(); });
    OrderMsg order;

    while (1) {
        std::tuple<uint64_t, uint64_t, bool> res;
        gfifo_sequencer->result.wait_dequeue(res);
        const auto& [order_id, ts, critical] = res;

        order = m.get(order_id);
        enqueue(&order);
    }

    if (thread.joinable()) thread.join();
}

void Ordersqueue::shortcut_enqueue(OrderMsg* order) {
    m.insert(order->id, *order);
    enqueue(order);
}

void Ordersqueue::flush_logs() {
    if (CONFIG::ORDERS_SUBMISSION::LOGGING == false) return;
    while (1) {
        // std::clog << "Total orders in: " << total_received.load()
        // << ", total left: " << total_remaining.load() << std::endl;

        std::clog << "| " << total_remaining.load() << " . " << q_size.load()
        << " |            "<< std::flush;
        q_size.exchange(0);

        std::clog << " ("+ std::to_string(total_critical_remaining.load())
        << " // " << std::to_string(total_noncritical_remaining.load()) << ") " << std::flush;
        sleep(1);
    }
}

#endif  // SRC_UTILS_ORDERSQUEUE_HPP_
