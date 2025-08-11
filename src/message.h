// Copyright [2023] NYU

#ifndef SRC_MESSAGE_H_
#define SRC_MESSAGE_H_

#include <iostream>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>
#include "cameron314/readerwriterqueue.h"

// Msg for Data plane. We use protobuf for non-dataplane purposes
struct MsgDp {
    int64_t _msg_id;
    int64_t _client_send_time;
    int32_t _recipient_id;
    int64_t _root_send_time;
    int32_t _msg_type;  // 0=> for sending msgs, 1=> for requesting stats
    int32_t _is_from_hedge_node;  // 1 => true, 2 => false (see issue #28)
    int64_t _experiment_starting_msg_id;
    int64_t _deadline;
    int64_t _recv_time;
    int64_t _data[43];
    int8_t _is_clone;  // used by ebpf/tc egress hook
    int64_t _history;
    // int64_t _history_data[51];

    // Constructor to initialize fields
    MsgDp() : _msg_id(0), _client_send_time(0), _recipient_id(0),
            _root_send_time(0), _msg_type(0), _is_from_hedge_node(0),
            _experiment_starting_msg_id(0), _deadline(0), _recv_time(0) {}

    void set_msg_id(int64_t value) { _msg_id = value; }
    void set_client_send_time(int64_t value) { _client_send_time = value; }
    void set_recipient_id(int32_t value) { _recipient_id = value; }
    void set_root_send_time(int64_t value) { _root_send_time = value; }
    void set_msg_type(int32_t value) { _msg_type = value; }
    void set_is_from_hedge_node(int32_t value) { _is_from_hedge_node = value; }
    void set_experiment_starting_msg_id(int64_t value) { _experiment_starting_msg_id = value; }
    void set_deadline(int64_t value) { _deadline = value; }
    void set_recv_time(int64_t value) { _recv_time = value; }

    int64_t msg_id() const { return _msg_id; }
    int64_t client_send_time() const { return _client_send_time; }
    int32_t recipient_id() const { return _recipient_id; }
    int64_t root_send_time() const { return _root_send_time; }
    int32_t msg_type() const { return _msg_type; }
    int32_t is_from_hedge_node() const { return _is_from_hedge_node; }
    int64_t experiment_starting_msg_id() const { return _experiment_starting_msg_id; }
    int64_t deadline() const { return _deadline; }
    int64_t recv_time() const { return _recv_time; }
};

struct StatsFieldsDp {
    int64_t latency;
    int64_t release_time;
    int32_t holding_duration;
    int64_t deadline;
    int32_t recipient_id;
    int64_t message_id;
};

struct StatsDp {
    int32_t recipient_id;
    int64_t total_msgs;
    std::map<int64_t, StatsFieldsDp> records;
    std::vector<StatsFieldsDp> records2;  // temporary fix required for supporting logical receivers

    /*Used For loss Experiment*/
    std::vector<std::pair<int64_t, int64_t>> q;  // msg id, ts
    StatsDp() : q() {
        std::clog << "Allocating " << (250'000'000.0*16.0/(1'000'000'000)) << "GBytes" << std::endl;
        q.resize(250'000'000);
        q.resize(0);
    }
};

struct StartMsgsDp {
    int msg_rate;
    int experiment_time_in_seconds;
    int64_t starting_msg_id;
};

struct OrderMsg {
    uint64_t id;
    uint32_t mp_id;  // market participant (receiver) id
    uint64_t timestamp;  // order generation timestamp
    uint64_t last_hop_ts;  // last_hop_ts
    bool is_bid;  // otherwise its `ask`
    double price;
    uint32_t count;  // how many shares to order
    uint32_t symbol;
    bool is_fancy_pq;  // metadata: is used to decide the qtype in Ordersqueue (can be 0, 1)
    int64_t _data[155];  // just emulating more info
    bool is_dummy;
    int last_hop_id;  // id of the proxy/receiver who sent the message
};

#endif  // SRC_MESSAGE_H_
