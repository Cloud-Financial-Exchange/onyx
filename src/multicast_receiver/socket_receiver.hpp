// Copyright [2023] NYU

#ifndef SRC_MULTICAST_RECEIVER_SOCKET_RECEIVER_HPP_
#define SRC_MULTICAST_RECEIVER_SOCKET_RECEIVER_HPP_

#include <set>
#include <string>
#include <vector>

#include <condition_variable>   // NOLINT(build/c++11)
#include <mutex>   // NOLINT(build/c++11)
#include <queue>
#include <unordered_set>

#include "./../cameron314/readerwriterqueue.h"

#include "./../utils/common.hpp"
#include "./../utils/ordersqueue.hpp"
#include "./../config.h"
#include "./utils.hpp"
#include "./ordergeneration.hpp"
#include "./../utils/holdrelease.hpp"

void process_messages(
    moodycamel::BlockingReaderWriterQueue<ReceivedMessage>& message_queue,
    std::string mode, int recipient_id, StatsDp* stats,
    int total_proxies_without_redundant_nodes, int redundancy_factor, int m, int dup_num) {
    // Create and spin Holdrelease thread
    auto holdrelease = new Holdrelease(
        recipient_id, total_proxies_without_redundant_nodes, redundancy_factor, m, Role::RECEIVER, dup_num);
    auto holdrelease_send_thread = std::thread([holdrelease] {holdrelease->send_estimates();});

    while (true) {
        ReceivedMessage received_message;
        message_queue.wait_dequeue(received_message);
        MsgDp* msg = received_message.msg;
        int64_t reception_time = received_message.reception_time;
        handle_message_and_log_it(msg, reception_time, mode, recipient_id, holdrelease, stats, recipient_id);
        delete msg;
    }
}

void receive_and_process_messages(int recipient_id, std::string receiver_ip,
    int receiver_port, StatsDp* stats, std::string mode, int dup_num,
    int total_proxies_without_redundant_nodes, int redundancy_factor, int m) {
    int udp_socket = -1;

    // Setup UDP socket. `awstg` is a special case because a multicast socket needs to be setup
    if (mode != "awstg") {
        udp_socket = get_bound_udp_socket(inet_addr(receiver_ip.c_str()), receiver_port);
    } else {
        udp_socket = multicast_socket_setup("224.0.0.20");  // Only support one aws transit gateway mc group rn
    }

    moodycamel::BlockingReaderWriterQueue<ReceivedMessage> message_queue(2048);

    // Message processing thread
    std::thread processing_thread(
        process_messages, std::ref(message_queue), mode, recipient_id, stats,
        total_proxies_without_redundant_nodes, redundancy_factor, m, dup_num);

    stats->recipient_id = recipient_id;
    std::set<int> seen_experiment_ids;
    std::set<int64_t> seen_msg_ids;

    // Receive messages, construct ReceivedMessage and pass to the message processing thread
    while (true) {
        char* buffer = new char[sizeof(MsgDp)];
        int bytes_received = recv_message(udp_socket, buffer);
        if (bytes_received == -1) {
            Logging::FAILURES++;
            continue;
        }

        if (bytes_received < static_cast<int>(sizeof(MsgDp))) {
            delete[] buffer;
            continue;
        }

        MsgDp* msg = reinterpret_cast<MsgDp*>(buffer);

        if (CONFIG::XDP_BASED_DEDUP == false && seen_msg_ids.find(msg->msg_id()) != seen_msg_ids.end()) {
            delete[] buffer;
            continue;
        }

        seen_msg_ids.insert(msg->msg_id());
        int64_t current_time = get_current_time();
        message_queue.enqueue(ReceivedMessage(msg, current_time));
    }

    processing_thread.join();
}

int socket_receiver(int32_t recipient_id, int32_t duplication_factor, std::string mode,
    int32_t m, int32_t redundancy_factor, int32_t total_proxies_without_redundant_nodes,
    RWQ& management_to_dataplane_q) {
    std::clog << "------- Starting Socket receiver -------" << std::endl;

    if (CONFIG::XDP_BASED_DEDUP) {
        int e = system("sudo ip link set dev ens5 mtu 3498");
        if (e != 0) {
            std::cerr << "Could not enhance MTU" << e << std::endl;
            exit(1);
        }
        e = system("sudo ethtool -L ens5 combined 1");
        if (e != 0) {
            std::cerr << "Could not Free NIC Channels" << e << std::endl;
            exit(1);
        }
        e = system("sudo ip link set ens5 xdpdrv obj xdp-deduplication.o sec xdp");
        if (e != 0) {
            std::cerr << "Could not attach xdp hook " << e << std::endl;
            exit(1);
        }
    }

    auto [recipient_ip, recipient_ports] = get_logical_receiver_ip_ports_for_physical_receiver(
        recipient_id, duplication_factor);

    std::vector<std::thread> receiver_threads(recipient_ports.size());

    all_stats.resize(recipient_ports.size());

    for (int i = 0; i < static_cast<int>(recipient_ports.size()); i++) {
        receiver_threads[i] = std::thread(receive_and_process_messages, recipient_id + i, recipient_ip,
            recipient_ports[i], &(all_stats[i]), mode, duplication_factor,
            total_proxies_without_redundant_nodes, redundancy_factor, m);
    }

    {  // Orders queue and Orders submission
        std::string parent_ip = get_proxy_management_ip(get_parent_proxy_of_receiver(
            recipient_id, m, total_proxies_without_redundant_nodes, get_management_receivers().size()));

        /**
         * Note:
         * Empty downstreams for receivers. Logic may identify a receiver based on downsteams being empty
         */

        auto ordersqueue = new Ordersqueue(
            recipient_ip, 0, parent_ip, CONFIG::ORDERS_SUBMISSION::SEQUENCER_DELAY, {}, recipient_id);
        receiver_threads.emplace_back(std::thread([ordersqueue] { ordersqueue->send_orders(); }));
        receiver_threads.emplace_back(std::thread([ordersqueue] { ordersqueue->flush_logs(); }));

        Ordergeneration* ordergeneration = new Ordergeneration(recipient_ip, parent_ip, recipient_id, ordersqueue);
        receiver_threads.emplace_back(std::thread([&management_to_dataplane_q, ordergeneration] {
            std::unordered_set<uint64_t> prev_requests;
            while (1) {
                ManagementToDataplane action;
                management_to_dataplane_q.wait_dequeue(action);

                if (prev_requests.find(action.order_sub_req().request_id()) != prev_requests.end()) continue;
                prev_requests.insert(action.order_sub_req().request_id());

                std::clog << "Received action(" << action.order_sub_req().request_id() << "): rate=" <<
                    action.order_sub_req().rate() << ", duration=" <<
                    action.order_sub_req().experiment_duration() << ", bid_range={ " <<
                    action.order_sub_req().bid_range_start() << ", " <<
                    action.order_sub_req().bid_range_end() << " }, ask_range={ " <<
                    action.order_sub_req().ask_range_start() << " , " <<
                    action.order_sub_req().ask_range_end() << " }" <<
                    action.order_sub_req().fixed_interarrival() << " }" << std::endl;

                ordergeneration->start(
                    action.order_sub_req().experiment_duration(),
                    action.order_sub_req().rate(), action.order_sub_req().start_timestamp(),
                    action.order_sub_req().bid_range_start(), action.order_sub_req().bid_range_end(),
                    action.order_sub_req().ask_range_start(), action.order_sub_req().ask_range_end(),
                    action.order_sub_req().fixed_interarrival(), action.order_sub_req().use_fancypq());
            }
        }));
    }

    for (int k = 0; k < static_cast<int>(receiver_threads.size()); ++k) {
        receiver_threads[k].join();
    }

    return 0;
}

#endif  // SRC_MULTICAST_RECEIVER_SOCKET_RECEIVER_HPP_
