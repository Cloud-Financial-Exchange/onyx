// Copyright [2023] NYU

#ifndef SRC_MULTICAST_PROXY_HEDGING_HPP_
#define SRC_MULTICAST_PROXY_HEDGING_HPP_

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

#include "./../utils/common.hpp"
#include "./../utils/tree.hpp"
#include "./../config.h"

class Hedging {
 private:
    CONFIG::HEDGING::HEDGING_MODE_TYPE type;
    std::vector<HedgingHint> received_hints;
    std::mutex hints_mutex;
    std::mutex assets_mutex;

    std::vector<int> latencies;
    std::vector<HedgingHint> outgoing_hints;
    int total_processed_outgoing_hints;

    int num_of_observations_per_hint_creation;

    std::unordered_map<int, std::vector<std::string>> downstreams_per_node_index;
    bool are_downstreams_memoized;

    std::default_random_engine rng;

 public:
    int proxy_num;
    int total_proxies_without_redundant_nodes;
    int redundancy_factor;
    int m;

    // For iouring and socket based proxies
    std::vector<sockaddr_in> addrs;

    // For DPDK based proxies
    std::pair<std::vector<std::pair<uint32_t, rte_be16_t>>, std::vector<std::vector<uint8_t>>>
    fast_ips_and_ports;

    Hedging(int proxy_num, int total_proxies_without_redundant_nodes,
        int redundancy_factor, int m, CONFIG::HEDGING::HEDGING_MODE_TYPE type):
        type(type),
        proxy_num(proxy_num),
        total_proxies_without_redundant_nodes(total_proxies_without_redundant_nodes),
        redundancy_factor(redundancy_factor),
        m(m) {
        received_hints = {};
        latencies = {};
        outgoing_hints = {};
        num_of_observations_per_hint_creation = 1000;
        total_processed_outgoing_hints = 0;
        are_downstreams_memoized = false;
        rng = std::default_random_engine {(uint64_t)proxy_num};
        addrs = {};
        fast_ips_and_ports = {};
    }

    ~Hedging() {
    }

    void collect_hints() {
        if (type != CONFIG::HEDGING::HEDGING_MODE_TYPE::NONE) {
            return;
        }

        auto [ip, port] = get_hedging_address(get_proxy_management_ip(proxy_num));
        int udp_socket = get_bound_udp_socket(inet_addr(ip.c_str()), port);
        char buffer[BUF_SIZE];

        while (true) {
            int bytes_received = recv_message(udp_socket, buffer);

            HedgingHint hint;
            hint.ParseFromArray(buffer, bytes_received);
            // std::clog << "Received hint: " << hint.proxy_num() << " " << hint.intensity() << std::endl;

            std::lock_guard<std::mutex> lock(hints_mutex);

            bool found = false;
            for (auto& ele : received_hints) {
                if (ele.proxy_num() == hint.proxy_num()) {
                    ele.set_intensity(hint.intensity());
                    found = true;
                }
            }

            if (false == found) {
                received_hints.push_back(hint);
            }

            std::sort(received_hints.begin(), received_hints.end(),
                [](const HedgingHint& a, const HedgingHint& b) {
                    return a.intensity() > b.intensity();});
        }
    }

    void record_latency(int latency) {
        if (type != CONFIG::HEDGING::HEDGING_MODE_TYPE::NONE) {
            return;
        }

        latencies.push_back(latency);
        if (static_cast<int>(latencies.size()) < num_of_observations_per_hint_creation) return;

        auto median = calculatePercentile(latencies, 50);
        auto std_deviation = calculate_standard_deviation(latencies);
        auto intensity = (0.5*median) + (0.5*std_deviation);
        HedgingHint h;
        h.set_intensity(intensity);
        h.set_proxy_num(proxy_num);
        outgoing_hints.push_back(h);
        latencies = {};
    }

    void send_hints() {
        if (type == CONFIG::HEDGING::HEDGING_MODE_TYPE::NONE) {
            return;
        }

        auto [ip, port] = get_hedging_address(get_proxy_management_ip(proxy_num));
        int udp_socket = get_bound_udp_socket(inet_addr(ip.c_str()), port);

        auto node_indices = get_indices_of_siblings_that_are_hedge_nodes(
            proxy_num, total_proxies_without_redundant_nodes, redundancy_factor, m);
        auto hedge_addrs = convert_node_indices_to_hedge_addrs(node_indices);

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(CONFIG::HEDGING::HINTS_FREQUENCY_IN_MILLISECONDS));
            int total_hints = outgoing_hints.size();  // Properly handle it using mutex
            if (total_hints - total_processed_outgoing_hints <= 0) continue;

            // We select the most intense hint out of avaialble outgoing hints
            auto hint = outgoing_hints[total_hints-1];
            for (int i = total_processed_outgoing_hints; i < total_hints; i++) {
                if (outgoing_hints[i].intensity() > hint.intensity()) {
                    hint = outgoing_hints[i];
                }
            }

            std::string data;
            hint.SerializeToString(&data);

            for (auto addr : hedge_addrs) {
                int res = sendto(
                    udp_socket, data.c_str(), data.length(), 0, (struct sockaddr *)&addr, sizeof(addr));

                if (res < 0) {
                    std::cerr << "Could not send hint to hedge node" << std::endl;
                    continue;
                }
            }

            total_processed_outgoing_hints = total_hints;
        }
    }

    void prepare_dyn_random_based_children(std::vector<std::string> ips, std::string mode,
        std::unordered_map<std::string, std::string> ip_to_mac) {
        std::lock_guard<std::mutex> lock(assets_mutex);

        std::shuffle(ips.begin(), ips.end(), rng);
        this->addrs = convert_ips_to_addrs(ips);
        if (mode == "dpdk") {
            this->fast_ips_and_ports = format_ips_and_macs_for_dpdk(ips, ip_to_mac);
        }
    }

    void prepare_variance_based_children(std::vector<int> sibling_indices, json ips_config,
        std::string mode, std::unordered_map<std::string, std::string> ip_to_mac) {
        // Get siblings indices based on the received hints
        std::vector<int> indices_sorted_on_hedging;
        {
            std::lock_guard<std::mutex> lock(hints_mutex);
            for (auto ele : received_hints) {
                indices_sorted_on_hedging.push_back(ele.proxy_num());
            }
        }

        // Get siblings that might not be present in the received hints list
        for (auto ele : sibling_indices) {
            if (std::find(indices_sorted_on_hedging.begin(), indices_sorted_on_hedging.end(), ele)
                == indices_sorted_on_hedging.end()) {
                indices_sorted_on_hedging.push_back(ele);
            }
        }

        // Get children of the siblings
        std::vector<std::string> children;
        for (auto ele : indices_sorted_on_hedging) {
            std::vector<std::string> res;
            if (is_a_leaf_node(ele, total_proxies_without_redundant_nodes, m)) {
                res = get_recipients_to_send_messages_for_proxy_node(
                    ips_config, ele, m, total_proxies_without_redundant_nodes, mode);
            } else {
                auto indices = get_children_indices_for_proxy_node(
                    ele, m, redundancy_factor, total_proxies_without_redundant_nodes, true);
                res = get_ips_for_node_indices(indices, ips_config, mode);
            }
            std::reverse(res.begin(), res.end());
            std::reverse(res.begin(), res.end());
            downstreams_per_node_index[ele] = res;
            children.insert(children.end(), res.begin(), res.end());
        }

        std::lock_guard<std::mutex> lock(assets_mutex);
        this->addrs = convert_ips_to_addrs(children);
        if (mode == "dpdk") {
            this->fast_ips_and_ports = format_ips_and_macs_for_dpdk(children, ip_to_mac);
        }
    }

    void periodically_prepare_reordered_children(
        int current_index, std::string mode, std::vector<std::string> ips,
        std::unordered_map<std::string, std::string> ip_to_mac) {
        if (type == CONFIG::HEDGING::HEDGING_MODE_TYPE::NONE) {
            return;
        }

        assert(true == is_a_redundant_node(current_index, total_proxies_without_redundant_nodes));

        if (type == CONFIG::HEDGING::HEDGING_MODE_TYPE::FIXED_RANDOM) {
            std::lock_guard<std::mutex> lock(assets_mutex);

            std::shuffle(ips.begin(), ips.end(), rng);
            this->addrs = convert_ips_to_addrs(ips);
            if (mode == "dpdk")
                this->fast_ips_and_ports = format_ips_and_macs_for_dpdk(ips, ip_to_mac);
            return;
        }

        if (type == CONFIG::HEDGING::HEDGING_MODE_TYPE::RANDOM) {
            while (true) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(CONFIG::HEDGING::REORDER_DOWNSTREAMS_AFTER_MILLISECONDS));
                prepare_dyn_random_based_children(ips, mode, ip_to_mac);
            }
        }

        if (type == CONFIG::HEDGING::HEDGING_MODE_TYPE::VARIANCE) {
            std::vector<int> sibling_indices = {};
            json ips_config;
            ips_config = read_json(ips_file);
            sibling_indices = get_siblings_indices_for_redundant_node(
                current_index, m, redundancy_factor, total_proxies_without_redundant_nodes);

            while (true) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(CONFIG::HEDGING::REORDER_DOWNSTREAMS_AFTER_MILLISECONDS));
                prepare_variance_based_children(sibling_indices, ips_config, mode, ip_to_mac);
            }
        }
    }

    std::pair<std::vector<std::pair<uint32_t, rte_be16_t>>, std::vector<std::vector<uint8_t>>>
    get_fast_ips_and_ports() {
        std::lock_guard<std::mutex> lock(assets_mutex);
        return this->fast_ips_and_ports;
    }

    std::vector<sockaddr_in> get_addrs() {
        std::lock_guard<std::mutex> lock(assets_mutex);
        return this->addrs;
    }
};


#endif  // SRC_MULTICAST_PROXY_HEDGING_HPP_
