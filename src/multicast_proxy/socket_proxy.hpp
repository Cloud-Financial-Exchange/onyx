// Copyright [2023] NYU

#ifndef SRC_MULTICAST_PROXY_SOCKET_PROXY_HPP_
#define SRC_MULTICAST_PROXY_SOCKET_PROXY_HPP_

#include <vector>
#include <unordered_map>
#include <utility>
#include <set>
#include <string>

#include "./../utils/common.hpp"
#include "./hedging.hpp"
#include "./../utils/ordersqueue.hpp"
#include "./matchingengine.hpp"
#include "./utils.hpp"

void set_ebpf_maps(std::vector<std::string> ips) {
    auto ip_to_mac_mapping = get_ip_to_mac_mapping(ips);
    auto [ip_ports, macs] = format_ips_and_macs_for_dpdk(ips, ip_to_mac_mapping, true);

    std::vector<uint32_t> formatted_ips;
    std::vector<uint64_t> formatted_macs;
    for (size_t i = 0; i < macs.size(); i++) {
        formatted_ips.push_back(ip_ports[i].first);

        uint64_t mac_as_uint64 = 0;
        for (int j = 0; j < 6; j++) {
            mac_as_uint64 <<= 8;
            mac_as_uint64 |= macs[i][j];
        }

        formatted_macs.push_back(mac_as_uint64);
    }

    int e = store_data_in_bpf_map_ip(formatted_ips.data(), formatted_ips.size(), "/sys/fs/bpf/tc/globals/ips_map");
    if (e != 0) {
        std::cerr << "Could not store IPs in eBPF map" << std::endl;
        exit(1);
    }

    uint32_t total_ips = ips.size();
    e = store_data_in_bpf_map_ip(&total_ips, 1, "/sys/fs/bpf/tc/globals/config_map");
    if (e != 0) {
        std::cerr << "Could not store Config in eBPF map" << std::endl;
        exit(1);
    }

    e = store_data_in_bpf_map_mac(formatted_macs.data(), formatted_macs.size(), "/sys/fs/bpf/tc/globals/macs_map");
    if (e != 0) {
        std::cerr << "Could not store MACs in eBPF map" << std::endl;
        exit(1);
    }
}

void process_message(
    Context *ctx, std::set<int64_t>& seen_msg_ids,
    std::vector<sockaddr_in>& addrs, int sockfd,
    MsgDp* msg, bool is_leaf_node, bool is_redundant, int& count) {
    bool is_parity_0_msg_id = false;

    if (false && count >= CONFIG::HEDGING::REQUEST_REORDERED_DOWNSTREAMS_AFTER_MESSAGES && is_redundant) {
        addrs = ctx->hedging->get_addrs();
        count = 0;
    }

    auto curr_time = get_current_time();

    if (ctx->proxy_num !=0 && seen_msg_ids.find(msg->msg_id()) != seen_msg_ids.end()) {
        if (2 == msg->is_from_hedge_node()) {
            ctx->hedging->record_latency(curr_time - msg->root_send_time());
        }
        return;
    }

    ctx->holdrelease->record_latency(curr_time - msg->root_send_time());
    count++;

    if (ctx->proxy_num != 0) seen_msg_ids.insert(msg->msg_id());

    if ((msg->msg_id() % CONFIG::HEDGING::PARITY) == 0) {
        is_parity_0_msg_id = true;
    }

    if (ctx->proxy_num == 0) {
        msg->set_deadline(
            curr_time + ctx->holdrelease->get_owd_estimate_for_last_level()
            + CONFIG::HOLDRELEASE::EXTRA_HOLD_ADDED_TO_DEADLINE);
        msg->set_root_send_time(curr_time);
    }

    if (0 < ctx->proxy_num && 2 == msg->is_from_hedge_node()) {
        ctx->hedging->record_latency(curr_time - msg->root_send_time());
    }

    int total_addrs_to_send_messages = addrs.size();
    if (true == CONFIG::HEDGING::PARITY_BASED_EXPERIMENT && false == is_redundant
        && true == is_parity_0_msg_id && false == is_leaf_node) {
        // For a non-redundant, non-leaf node and parity 0 message id,
        // we will not send messages to hedge nodes
        // We make sure that redundant nodes are placed at the end of fast_ips_ports/addrs
        // , error prone,
        // need to assert that
        total_addrs_to_send_messages = addrs.size() - ctx->redundancy_factor;
    }

    assert(false == (is_redundant && CONFIG::HEDGING::PARITY_BASED_EXPERIMENT && is_parity_0_msg_id));

    for (int dup = 0; dup < CONFIG::REQUEST_DUPLICATION::FACTOR; dup++) {
        for (int i = 0; i < total_addrs_to_send_messages; i++) {
            if (CONFIG::USING_EBPF_AT_SENDER && i > 0) break;
            auto bytes_sent = sendto(sockfd, msg, sizeof(MsgDp), 0,
                                    (struct sockaddr *)&addrs[i], sizeof(addrs[i]));

            if (bytes_sent == -1) {
                std::cerr << "Failed to send data." << std::endl;
                close(sockfd);
                exit(1);
            }
        }
    }
}

void send_bursty_messages(
    int rate, int experiment_time_in_seconds, int64_t starting_msg_id,
    Context *ctx, std::set<int64_t>& seen_msg_ids,
    std::vector<sockaddr_in>& addrs, int sockfd,
    bool is_leaf_node, bool is_redundant, int& count) {

    double burst_rate = rate * CONFIG::BURST_FACTOR;
    double burst_duration = 1.0;   // Burst duration in seconds
    double burst_interval = 2.0;  // Time between bursts in seconds
    int total_bursts = 0;

    std::random_device rd;
    // std::mt19937 generator(rd());
    std::mt19937 generator(321);
    std::exponential_distribution<> exp_dist(rate);

    double curr_relative_time = 0;
    auto next_time = std::chrono::system_clock::now();
    uint64_t total_messages = 0;

    auto last_burst_time = std::chrono::system_clock::now();
    bool in_burst_mode = false;
    int i = 0;

    int64_t prev_timestamp = 0;

    while (curr_relative_time < experiment_time_in_seconds) {
        double interarrival_time = exp_dist(generator);

        auto current_time = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_time = std::chrono::duration_cast<std::chrono::duration<double>>(
            current_time - last_burst_time);

        if (!in_burst_mode && CONFIG::BURSTY && elapsed_time.count() >= burst_interval) {
            in_burst_mode = true;
            last_burst_time = current_time;
            exp_dist = std::exponential_distribution<>(burst_rate);
            total_bursts++;
        } else if (in_burst_mode && elapsed_time.count() >= burst_duration) {
            in_burst_mode = false;
            exp_dist = std::exponential_distribution<>(rate);
        }

        curr_relative_time += interarrival_time;

        next_time += std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::duration<double>(interarrival_time));

        if (curr_relative_time < experiment_time_in_seconds) {
            MsgDp msg;
            auto t = get_current_time();
            msg.set_client_send_time(t);
            msg.set_root_send_time(t);
            msg.set_deadline(
                t + ctx->holdrelease->get_owd_estimate_for_last_level()
                + CONFIG::HOLDRELEASE::EXTRA_HOLD_ADDED_TO_DEADLINE);
            msg.set_is_from_hedge_node(2);
            msg.set_msg_id(i + starting_msg_id);
            msg.set_msg_type(0);
            msg.set_recipient_id(0);
            msg._history = prev_timestamp;
            if (CONFIG::HEDGING::MSG_HISTORY) prev_timestamp = t;

            process_message(ctx, seen_msg_ids, addrs, sockfd, &msg, is_leaf_node, is_redundant, count);

            i++;
            total_messages++;
            std::this_thread::sleep_until(next_time);
        }
    }

    std::clog << "Msg size: " << sizeof(MsgDp) << std::endl;
    std::clog << "Total bursts: " <<  total_bursts << std::endl;
    std::clog << "Done generating [ " << total_messages << " ] orders!" << std::endl;
    std::clog << "Next Message ID: " << starting_msg_id + i << std::endl;
}

// Refactor a same function in multicast client
void send_messages(
    int msg_rate, int experiment_time_in_seconds, int64_t starting_msg_id,
    Context *ctx, std::set<int64_t>& seen_msg_ids,
    std::vector<sockaddr_in>& addrs, int sockfd,
    bool is_leaf_node, bool is_redundant, int& count) {

    auto iterations = (int64_t)msg_rate * experiment_time_in_seconds;
    auto time_between_messages = std::chrono::duration<double>(1) / msg_rate;
    std::cout << "Next message id: " << iterations+starting_msg_id << std::endl;
    std::clog << "Started sending the messages at " <<  get_current_time() << std::endl;
    std::clog << "Msg size: " << sizeof(MsgDp) << std::endl;

    auto start_time = std::chrono::system_clock::now();
    for (int64_t i = 0; i < iterations; i++) {
        MsgDp msg;
        msg.set_client_send_time(0);
        msg.set_root_send_time(get_current_time());
        msg.set_deadline(get_current_time());
        msg.set_is_from_hedge_node(2);
        msg.set_msg_id(i+starting_msg_id);
        msg.set_msg_type(0);
        msg.set_recipient_id(0);

        process_message(ctx, seen_msg_ids, addrs, sockfd, &msg, is_leaf_node, is_redundant, count);

        std::this_thread::sleep_until(start_time + (time_between_messages * i));
    }

    auto end_time = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    std::clog << "Done sending the messages at " <<  get_current_time() << std::endl;
    std::cout << "Rate achieved: " << (1.0*iterations)/(duration.count()) << std::endl;
}

void socket_proxy(Context *ctx, std::vector<std::string> ips) {
    auto addrs = convert_ips_to_addrs(ips);
    int sockfd = get_bound_udp_socket(
        inet_addr(get_proxy_management_ip(ctx->proxy_num).c_str()), UDP_DST_PORT);

    std::vector<std::thread> threads;
    assert(ctx->children_indices.empty() == false);

    if (ctx->proxy_num != 0) {
        auto proxy_ip = get_proxy_management_ip(ctx->proxy_num);
        auto ordersqueue = new Ordersqueue(
            proxy_ip, ctx->children_indices.front(),
            get_proxy_management_ip(get_parent_of_proxy_node(ctx->proxy_num, ctx->m)),
            CONFIG::ORDERS_SUBMISSION::SEQUENCER_DELAY, ips, ctx->proxy_num);
            // 0, ips);  // (made it 0 just for testing the raw tree's impact on packet loss rate)

        threads.emplace_back(std::thread([ordersqueue] { ordersqueue->receive_orders(); }));
        threads.emplace_back(std::thread([ordersqueue, proxy_ip] { ordersqueue->send_orders(); }));
        threads.emplace_back(std::thread([ordersqueue] { ordersqueue->flush_logs(); }));
    } else {
        auto matchingengine = new Matchingengine(
            get_proxy_management_ip(ctx->proxy_num), ctx->children_indices.front(),
            CONFIG::ORDERS_SUBMISSION::SEQUENCER_DELAY,
            CONFIG::ORDERS_SUBMISSION::LOGS_FLUSH_THRESHOLD, ips);

        threads.emplace_back(std::thread([matchingengine] { matchingengine->receive_orders(); }));
        threads.emplace_back(std::thread([matchingengine] { matchingengine->match_orders(); }));
    }

    if (ctx->proxy_num == 0 && CONFIG::USING_EBPF_AT_SENDER) {
        int e = system("sudo tc filter add dev ens5 egress bpf da obj tc-replication.o sec egress");
        if (e != 0) {
            std::cerr << "Could not attach tc hook " << e << std::endl;
            exit(1);
        }

        set_ebpf_maps(ips);
    }

    int count = 0;
    std::set<int64_t> seen_msg_ids;
    auto is_leaf_node = is_a_leaf_node(ctx->proxy_num, ctx->total_proxies_without_redundant_nodes, ctx->m);

    char buffer[BUF_SIZE];
    if (ctx->proxy_num == 0 && CONFIG::SENDER_PROXY_GENERATES_MESSAGES) {
        while (true) {
            int bytes_received = recv_message(sockfd, buffer);
            if (bytes_received != sizeof(StartMsgsDp)) continue;
            StartMsgsDp* msg = reinterpret_cast<StartMsgsDp*>(buffer);

            if (CONFIG::BURSTY) {
                send_bursty_messages(msg->msg_rate, msg->experiment_time_in_seconds, msg->starting_msg_id,
                    ctx, seen_msg_ids, addrs, sockfd, is_leaf_node, false, count);
            } else {
                send_messages(msg->msg_rate, msg->experiment_time_in_seconds, msg->starting_msg_id,
                    ctx, seen_msg_ids, addrs, sockfd, is_leaf_node, false, count);
            }
        }
    } else {
        while (true) {
            int bytes_received = recv_message(sockfd, buffer);
            if (bytes_received != sizeof(MsgDp)) continue;

            MsgDp* msg = reinterpret_cast<MsgDp*>(buffer);
            process_message(ctx, seen_msg_ids, addrs, sockfd, msg, is_leaf_node, false, count);
        }
    }

    std::clog << "Returning" << std::endl;
    for (int i = 0; i < static_cast<int>(threads.size()); i++) {
        if (threads.at(i).joinable())
            threads.at(i).join();
    }
}


#endif  // SRC_MULTICAST_PROXY_SOCKET_PROXY_HPP_
