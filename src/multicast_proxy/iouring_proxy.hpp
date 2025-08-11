// Copyright [2023] NYU

#ifndef SRC_MULTICAST_PROXY_IOURING_PROXY_HPP_
#define SRC_MULTICAST_PROXY_IOURING_PROXY_HPP_

#include <vector>
#include <unordered_map>
#include <utility>
#include <set>
#include <string>


#include "./../utils/common.hpp"
#include "./hedging.hpp"
#include "./utils.hpp"


// Iouring automatically round it to larger 2^n
// This number limits the maximum of downstream
constexpr int ENTERIES_LIMIT = 350;

// Proxies
void iouring_proxy(Context *ctx, std::string mode, std::vector<std::string> ips) {
    auto addrs = convert_ips_to_addrs(ips);

    struct io_uring ring;
    int sockfd = get_bound_udp_socket(htonl(INADDR_ANY), UDP_DST_PORT);

    int ret = io_uring_queue_init(ENTERIES_LIMIT, &ring, 0);
    if (ret != 0) {
        fprintf(stderr, "Error initializing io_uring: %s\n", strerror(-ret));
        close(sockfd);
        exit(1);
    }

    auto is_redundant = is_a_redundant_node(ctx->proxy_num, ctx->hedging->total_proxies_without_redundant_nodes);
    std::vector<std::thread> hedging_threads;
    if (is_redundant) {
        hedging_threads.emplace_back(std::thread([ctx, ips, mode] {
            ctx->hedging->periodically_prepare_reordered_children(ctx->proxy_num, mode, ips, {});
        }));
    }

    auto is_leaf_node = is_a_leaf_node(ctx->proxy_num, ctx->total_proxies_without_redundant_nodes, ctx->m);

    int count = 0;
    std::set<int64_t> seen_msg_ids;

    while (true) {
        char buffer[BUF_SIZE];
        int bytes_received = recv_message(sockfd, buffer);
        bool is_parity_0_msg_id = false;  // We will not send messages to hedge nodes if its true

        if (count >= CONFIG::HEDGING::REQUEST_REORDERED_DOWNSTREAMS_AFTER_MESSAGES && is_redundant) {
            addrs = ctx->hedging->get_addrs();
            count = 0;
        }

        MsgDp* msg = reinterpret_cast<MsgDp*>(buffer);

        auto curr_time = get_current_time();

        if (seen_msg_ids.find(msg->msg_id()) != seen_msg_ids.end()) {
            if (2 == msg->is_from_hedge_node()) {
                ctx->hedging->record_latency(curr_time - msg->root_send_time());
            }
            continue;
        }

        count++;
        seen_msg_ids.insert(msg->msg_id());
        if ((msg->msg_id() % CONFIG::HEDGING::PARITY) == 0) {
            is_parity_0_msg_id = true;
        }

        if (ctx->proxy_num == 0) {
            msg->set_deadline(
                curr_time + ctx->holdrelease->get_owd_estimate_for_last_level()
                + CONFIG::HOLDRELEASE::EXTRA_HOLD_ADDED_TO_DEADLINE);
            msg->set_root_send_time(curr_time);
        }

        if (is_redundant) {
            msg->set_is_from_hedge_node(1);
        }

        if (0 < ctx->proxy_num && 2 == msg->is_from_hedge_node()) {
            ctx->hedging->record_latency(curr_time - msg->root_send_time());
        }

        ctx->holdrelease->record_latency(curr_time - msg->root_send_time());

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
                io_uring_sqe *sqe = io_uring_get_sqe(&ring);
                if (sqe == NULL) {
                    perror("io_uring_get_sqe");
                    continue;
                }

                io_uring_prep_sendto(sqe, sockfd, buffer, bytes_received, 0,
                                    (struct sockaddr *)&addrs[i],
                                    sizeof(addrs[i]));
            }

            io_uring_submit(&ring);

            for (int i = 0; i < total_addrs_to_send_messages; i++) {
                struct io_uring_cqe *cqe;
                if (io_uring_wait_cqe(&ring, &cqe) < 0) {
                    perror("io_uring_wait_cqe");
                    goto cleanup;
                }

                if (cqe->res < 0) {
                    goto cleanup;
                }

                io_uring_cqe_seen(&ring, cqe);
            }
        }
    }

    for (auto& thr : hedging_threads) {
        thr.join();
    }

cleanup:
    io_uring_queue_exit(&ring);
    close(sockfd);

    return;
}


#endif  // SRC_MULTICAST_PROXY_IOURING_PROXY_HPP_
