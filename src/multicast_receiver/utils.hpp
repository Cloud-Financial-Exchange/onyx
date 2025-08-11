// Copyright [2023] NYU

#ifndef SRC_MULTICAST_RECEIVER_UTILS_HPP_
#define SRC_MULTICAST_RECEIVER_UTILS_HPP_

#include <sched.h>
#include <unistd.h>
#include <dirent.h>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <utility>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <set>

#include "./../utils/common.hpp"
#include "./../utils/holdrelease.hpp"

struct ReceivedMessage {
    MsgDp* msg;
    int64_t reception_time;

    ReceivedMessage() {}
    ReceivedMessage(MsgDp* msg, int64_t reception_time): msg(msg), reception_time(reception_time) {}
};

void handle_message_and_log_it(
    MsgDp* msg, int64_t reception_time, std::string& mode,
    int worker_id, Holdrelease *holdrelease, StatsDp* stats, int recipient_id, bool record_latency = true) {
    if (CONFIG::LOSS_EXPERIMENT::EXP) {
        stats->q.push_back(std::make_pair(msg->msg_id(), msg->root_send_time()));
        return;
    }

    int msg_holding_duration = 0;  // microseconds
    int64_t msg_release_time = reception_time;

    // if (msg->is_from_hedge_node() == 1) {
    //     total_messages_from_hedge_nodes.fetch_add(1);
    //     assert(!CONFIG::HEDGING::PARITY_BASED_EXPERIMENT || msg->msg_id()%2 == 1);
    // }

    int64_t send_time = msg->root_send_time();
    if (mode == "awstg") {
        send_time = msg->client_send_time();
    }

    auto elapsed = reception_time - send_time;

    if (holdrelease && record_latency)
        holdrelease->record_latency(elapsed);

    // Emulate hold by using deadline as current time
    // if the deadline is in the future
    if (holdrelease && false == holdrelease->is_holdrelease_turned_off()
        && msg->deadline() > reception_time) {
        elapsed = msg->deadline() - send_time;
        msg_holding_duration = msg->deadline() - reception_time;
        msg_release_time = msg->deadline();
    }

    StatsFieldsDp fields;
    fields.latency = elapsed;  // latency includes the effects of holdrelease
    fields.holding_duration = msg_holding_duration;
    fields.release_time = msg_release_time;
    fields.deadline = msg->deadline();
    fields.recipient_id = recipient_id;
    fields.message_id = msg->msg_id();

    const std::lock_guard<std::mutex> lock(stat_mutexes[worker_id]);
    stats->records2.push_back(fields);
    // stats->records[msg->msg_id()] = fields;
}

int multicast_socket_setup(std::string receiver_ip) {
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        perror("socket");
        exit(1);
    }

    // Set the socket to reuse the address
    int reuse = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        perror("setsockopt(SO_REUSEADDR)");
        close(udp_socket);
        exit(1);
    }

    struct sockaddr_in mc_addr;
    /* construct a multicast address structure */
    memset(&mc_addr, 0, sizeof(mc_addr));
    mc_addr.sin_family = AF_INET;
    mc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    mc_addr.sin_port = htons(34254);

    /* bind to multicast address to socket */
    if ((bind(udp_socket, (struct sockaddr *) &mc_addr,
        sizeof(mc_addr))) < 0) {
        perror("bind() failed");
        exit(1);
    }

    struct ip_mreq mc_req;        /* multicast request structure */

    const char * madr = receiver_ip.c_str();

    /* construct an IGMP join request structure */
    mc_req.imr_multiaddr.s_addr = inet_addr(madr);
    mc_req.imr_interface.s_addr = htonl(INADDR_ANY);

    /* send an ADD MEMBERSHIP message via setsockopt */
    if ((setsockopt(udp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
        reinterpret_cast<char*>(&mc_req), sizeof(mc_req))) < 0) {
        perror("setsockopt() failed");
        exit(1);
    }

    return udp_socket;
}


#endif  // SRC_MULTICAST_RECEIVER_UTILS_HPP_
