// Copyright [2023] NYU

#ifndef SRC_UTILS_HOLDRELEASE_HPP_
#define SRC_UTILS_HOLDRELEASE_HPP_

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "./../nlohmann/json.hpp"
#include "./common.hpp"
#include "./../config.h"

class Holdrelease {
    int node_index;
    int total_proxies_without_redundant_nodes;
    int redundancy_factor;
    int m;
    Role role;
    int dup_num;

    int send_udp_socket;
    int recv_udp_socket;
    nlohmann::json ips_config;

    std::atomic<int> outgoing_owd_estimate;  // The OWD estimate for the current node.
    std::mutex mutex_for_received_outgoing_owd_estimates;
    std::vector<int> aggregated_owd_estimates_received_from_children;  // The aggragated received owd estimates.
    std::map<int, std::vector<int>> incoming_owd_estimates_per_child;
    std::size_t window_size;
    std::vector<int> latencies;

    int get_proxy_index_to_send_holdrelease_hints() {
        // this function should not be called for root proxy
        assert(false == is_root_proxy());

        int parent_node_index = 0;

        if (role == Role::PROXY) {
            parent_node_index = get_parent_of_node(
                node_index, m, total_proxies_without_redundant_nodes, redundancy_factor);
        } else if (role == Role::RECEIVER) {
            parent_node_index = get_parent_proxy_of_receiver(
                node_index, m, total_proxies_without_redundant_nodes,
                ips_config["recipient_management"].get<std::vector<std::string>>().size());
        }

        return parent_node_index;
    }

    bool is_root_proxy() {
        return role == Role::PROXY && node_index == 0;
    }

 public:
    Holdrelease(
        int node_index, int total_proxies_without_redundant_nodes,
        int redundancy_factor, int m, Role role, int dup_num):
        node_index(node_index),
        total_proxies_without_redundant_nodes(total_proxies_without_redundant_nodes),
        redundancy_factor(redundancy_factor),
        m(m), role(role), dup_num(dup_num) {
        ips_config = read_json(ips_file);

        outgoing_owd_estimate = CONFIG::HOLDRELEASE::DEFAULT_OWD;
        window_size = static_cast<std::size_t>(
            CONFIG::MSG_RATE * ((1.0 * CONFIG::HOLDRELEASE::WINDOW_SIZE_MILLISECONDS) / 1000.0));

        std::string node_ip;
        int node_port = UDP_DST_PORT;

        if (role == Role::PROXY) {
            node_ip = get_proxy_management_ip(node_index);
        } else if (role == Role::RECEIVER) {
            auto [res_ip, res_port] = get_receiver_ip_port(node_index, ips_config);
            node_ip = res_ip;
            node_port = res_port;
        }

        auto [ip1, port1] = get_holdreleaase_send_address(node_ip, node_port);
        send_udp_socket = get_bound_udp_socket(inet_addr(ip1.c_str()), port1);
        auto [ip2, port2] = get_holdreleaase_recv_address(node_ip, node_port);
        recv_udp_socket = get_bound_udp_socket(inet_addr(ip2.c_str()), port2);
    }

    bool is_holdrelease_turned_off() {
        return CONFIG::HOLDRELEASE::HOLDRELEASE_MODE == CONFIG::HOLDRELEASE::HOLDRELEASE_MODE_TYPE::NONE;
    }

    void record_latency(int latency) {
        if (is_holdrelease_turned_off() || is_root_proxy()) {
            return;
        }

        latencies.push_back(latency);
        if (latencies.size() < window_size) return;

        outgoing_owd_estimate = static_cast<int>(calculatePercentile(
            latencies, CONFIG::HOLDRELEASE::TARGET_PERCENTILE_FOR_OWD_ESTIMATE));
        latencies = {};
    }

    void send_estimates() {
        if (is_holdrelease_turned_off() || is_root_proxy()) {
            return;
        }

        auto parent_node_index = get_proxy_index_to_send_holdrelease_hints();
        auto addrs = convert_node_indices_to_holdrelease_recv_addrs({parent_node_index}, ips_config);

        assert(addrs.size() == 1);
        auto parent_addr = addrs[0];

        while (true) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(CONFIG::HOLDRELEASE::SENDING_ESTIMATE_FREQUENCY_IN_MILLISECONDS));
            std::vector<int> outgoing_owd_estimates_vector = {};

            {
                std::lock_guard<std::mutex> lock(mutex_for_received_outgoing_owd_estimates);
                outgoing_owd_estimates_vector = aggregated_owd_estimates_received_from_children;
                if (aggregated_owd_estimates_received_from_children.size() == 0 && role == Role::PROXY) {
                    // Have not received anything from any child
                    continue;
                }
            }

            outgoing_owd_estimates_vector.push_back(outgoing_owd_estimate.load());

            HoldreleaseHint hint;
            hint.set_index(node_index);
            for (auto ele : outgoing_owd_estimates_vector) {
                hint.add_owd_estimates(ele);
            }

            std::string data;
            hint.SerializeToString(&data);
            int res = sendto(
                send_udp_socket, data.c_str(), data.length(), 0,
                (struct sockaddr *)&parent_addr, sizeof(parent_addr));

            if (res < 0) {
                std::cerr << "Could not send HoldreleaseHint" << std::endl;
                continue;
            }
        }
    }

    /**
     * Receiver sends a vector [owd] to its parent.
     * Every proxy receives the vectors from its children and does an element wise max
     * and stores it. The output will be sent to parent proxy (if exists) by the send_estimates function.
    */
    void recv_estimates() {
        if (is_holdrelease_turned_off()) {
            return;
        }

        char buffer[BUF_SIZE];

        while (true) {
            int bytes_received = recv_message(recv_udp_socket, buffer);

            HoldreleaseHint hint;
            hint.ParseFromArray(buffer, bytes_received);

            const auto& estimate_vec = hint.owd_estimates();
            std::vector<int> rcvd_estimates(estimate_vec.begin(), estimate_vec.end());
            incoming_owd_estimates_per_child[hint.index()] = rcvd_estimates;

            auto current_max = rcvd_estimates;
            for (const auto& [k, v] : incoming_owd_estimates_per_child) {
                assert(v.size() == current_max.size());

                for (size_t i = 0; i < v.size(); i++) {
                    current_max.at(i) = std::max(current_max.at(i), v.at(i));
                }
            }

            {
                std::lock_guard<std::mutex> lock(mutex_for_received_outgoing_owd_estimates);
                aggregated_owd_estimates_received_from_children = current_max;
            }

            // if (node_index != 0) continue;
            // std::clog << "Received HoldreleaseHint: " << hint.index() << " ";
            // for (auto ele : rcvd_estimates) {
            //     std::clog << ele << " ";
            // }
            // std::clog << " --> ";
            // for (auto ele : aggregated_owd_estimates_received_from_children) {
            //     std::clog << ele << " ";
            // }
            // std::clog << std::endl;
        }
    }

    int get_owd_estimate_for_last_level() {
        std::lock_guard<std::mutex> lock(mutex_for_received_outgoing_owd_estimates);
        if (aggregated_owd_estimates_received_from_children.size() == 0) return CONFIG::HOLDRELEASE::DEFAULT_OWD;
        return aggregated_owd_estimates_received_from_children.at(0);
    }
};

#endif  // SRC_UTILS_HOLDRELEASE_HPP_
