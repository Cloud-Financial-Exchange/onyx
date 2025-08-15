// Copyright [2023] NYU

#ifndef SRC_UTILS_COMMON_HPP_
#define SRC_UTILS_COMMON_HPP_

#include <arpa/inet.h>
#include <inttypes.h>
#include <liburing.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_udp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <algorithm>
#include <cerrno>
#include <future>   // NOLINT(build/c++11)
#include <chrono>   // NOLINT(build/c++11)
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <thread>   // NOLINT(build/c++11)
#include <vector>

#include "./../nlohmann/json.hpp"
#include "./../build/message.pb.h"
#include "./../message.h"
#include "./tree.hpp"
#include "./../config.h"
#include "./../cameron314/readerwriterqueue.h"
#include "./ZMQSocket.hpp"

#define UDP_SRC_PORT 34254
#define UDP_DST_PORT 34254

// DPDK related
#define RX_RING_SIZE 16384
#define TX_RING_SIZE 65535

#define NUM_MBUFS 131071  // 262143
#define MBUF_CACHE_SIZE 32
#define MBUF_SIZE 512
#define HEADERS_MBUF_SIZE (sizeof(rte_udp_hdr) + sizeof(rte_ipv4_hdr) + sizeof(rte_ether_hdr))
#define BURST_SIZE 32

#define IP_DEFTTL 64 /* from RFC 1340. */
#define IP_VERSION 0x40
#define IP_HDRLEN 0x05 /* default IP header length == five 32-bits words. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)

const uint32_t DATA_OFFSET_IN_PKT = sizeof(struct rte_ether_hdr)
                                    + sizeof(struct rte_ipv4_hdr)
                                    + sizeof(struct rte_udp_hdr);

constexpr int BUF_SIZE = 1024;

using json = nlohmann::json;
using RWQ = moodycamel::BlockingReaderWriterQueue<ManagementToDataplane>;


// TODO(haseeb): Make a config loader and do not use hardcoded paths
const char* ips_file = "./../config/ec2-ips.json";
const char* mappings_file = "./../config/mapping.json";
const char* ip_file = "./../config/ip.txt";
const char* ipsocket_file = "./../config/ipsocket.txt";  // Useful for receivers
const char* mac_file = "./../config/mac.txt";
const char* macsocket_file = "./../config/macsocket.txt";  // Useful for receivers
const char* stats_folder = "./../stats/";
const char* aws_config_file = "./../config/aws_config.json";

#define YIELD_AND_CONTINUE { std::this_thread::yield(); continue; }

enum class Role { CLIENT, PROXY, RECEIVER };
enum class Status { BLOCKED, RUNNING };

enum Qtype {
    SIMPLE = 0,
    LOQv2 = 1
};

namespace Logging {
    std::atomic<int> FAILURES = 0;
    std::atomic<int> RECVD = 0;
    std::atomic<int> TRANSMITTED = 0;
    std::atomic<int> DEQUEUED;
    std::atomic<int> REORDERED = 0;
}

// TODO(a): Move the following after separated role's files
extern Status status;
std::unordered_map<int, std::mutex> stat_mutexes;
std::vector<StatsDp> all_stats;
std::atomic<int> total_messages_from_hedge_nodes = 0;

uint32_t string_to_ip(char *s, bool reverse = false) {
    unsigned char a[4];
    int rc = sscanf(s, "%hhd.%hhd.%hhd.%hhd", a + 0, a + 1, a + 2, a + 3);
    if (rc != 4) {
        fprintf(stderr,
                "bad source IP address format. Use like: -s 198.19.111.179\n");
        exit(1);
    }

    if (reverse) {
        return (uint32_t)(a[3]) << 24 | (uint32_t)(a[2]) << 16 |
           (uint32_t)(a[1]) << 8 | (uint32_t)(a[0]);
    }

    return (uint32_t)(a[0]) << 24 | (uint32_t)(a[1]) << 16 |
           (uint32_t)(a[2]) << 8 | (uint32_t)(a[3]);
}

void string_to_mac(char *s, std::vector<uint8_t> &a) {
    int rc = sscanf(s, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &a[0], &a[1], &a[2],
                    &a[3], &a[4], &a[5]);
    if (rc != 6) {
        fprintf(stderr,
                "bad MAC address format. Use like: -m 0a:38:ca:f6:f3:20\n");
        exit(1);
    }
}

std::pair<std::string, int> separate_ip_port(std::string ip_port_str) {
    size_t colon_pos = ip_port_str.find(':');
    std::string ip = ip_port_str.substr(0, colon_pos);
    int port = std::stoi(ip_port_str.substr(colon_pos + 1));
    return std::make_pair(ip, port);
}

int set_fd_limit(rlim_t limit) {
    struct rlimit rlim = {limit, limit};
    return setrlimit(RLIMIT_NOFILE, &rlim);
}


nlohmann::json read_json(std::string filename) {
    nlohmann::json json_data;
    std::ifstream file(filename);
    if (file.is_open()) {
        file >> json_data;
        file.close();
    } else {
        std::cerr << "[read_json] Failed to open the file " << filename
                  << std::endl;
        exit(1);
    }

    return json_data;
}

std::vector<std::string> get_proxies_ips(std::string type = "management",
                                         json ips = json::value_t::null) {
    if (ips.is_null()) {
        ips = read_json(ips_file);
    }
    switch (type == "management") {
        case true: {
            auto mgmt_ips =
                ips["proxy_management"].get<std::vector<std::string>>();
            // Remove the port number from the IP
            for (auto &ip : mgmt_ips) {
                size_t colon_pos = ip.find(':');
                ip = ip.substr(0, colon_pos);
            }
            return mgmt_ips;
        }
        default:
            auto proxy_ips = ips["proxy"].get<std::vector<std::string>>();
            for (auto &ip : proxy_ips) {
                size_t colon_pos = ip.find(':');
                ip = ip.substr(0, colon_pos);
            }
            return proxy_ips;
    }
}

std::vector<std::string> get_ips_for_node_indices(
    const std::vector<int> &children_indices,
    json& ips, std::string mode = "dpdk") {

    std::string proxy_type = "proxy";
    if (mode != "dpdk") {
        proxy_type = "proxy_management";
    }

    std::vector<std::string> proxies;
    for (int i : children_indices) {
        proxies.push_back(
            ips[proxy_type].get<std::vector<std::string>>()[i]);
    }

    return proxies;
}

std::string read_file(std::string filename) {
    std::string data;
    std::ifstream file(filename);
    if (file.is_open()) {
        file >> data;
        file.close();
    } else {
        std::cerr << "[read_file] Failed to open the file " << filename
                  << std::endl;
        exit(1);
    }

    while (!data.empty() && data.back() == '\n') {
        data.pop_back();
    }

    return data;
}


std::unordered_map<std::string, std::string>
get_ip_to_mac_mapping(std::vector<std::string> send_to_ips) {
    json mapping = read_json(mappings_file);
    std::unordered_map<std::string, std::string> res;
    for (auto ip : send_to_ips) {
        // Extract ip from ip:port
        size_t colon_pos = ip.find(':');
        ip = ip.substr(0, colon_pos);
        res[ip] = mapping[ip];
    }

    for (auto [k, v] : res) {
        std::cout << k << " => " << v << std::endl;
    }
    return res;
}

std::vector<std::string> get_management_receivers(json ips = json::value_t::null) {
    if (ips.is_null()) {
        ips = read_json(ips_file);
    }
    return ips["recipient_management"].get<std::vector<std::string>>();
}

std::map<std::string, std::vector<int>>
get_management_receivers_ips_ports(json ips = json::value_t::null) {
    auto recvr_info = get_management_receivers(ips);

    std::map<std::string, std::vector<int>> ip_port_map;

    for (const auto &ip_port_str : recvr_info) {
        auto [ip, port] = separate_ip_port(ip_port_str);
        ip_port_map[ip].push_back(port);
    }
    return ip_port_map;
}

std::pair<std::string, int> get_receiver_ip_port(int i, json ips = json::value_t::null) {
    auto recvr_info = get_management_receivers(ips);
    assert(static_cast<int>(recvr_info.size()) > i);
    auto res = recvr_info[i];
    return separate_ip_port(res);
}

std::pair<std::string, std::vector<int>>
get_logical_receiver_ip_ports_for_physical_receiver(int i, int dup_num) {
    auto recvr_info = get_management_receivers();
    std::pair<std::string, std::vector<int>> ip_ports;

    // Extract the IP and corresponding ports from recvr_info
    for (int j = 0; j < dup_num; j++) {
        auto [ip, port] = separate_ip_port(recvr_info[i + j]);
        ip_ports.first = ip;
        ip_ports.second.push_back(port);
    }
    return ip_ports;
}

std::string get_client_ip(int i = 0) {
    auto ips = read_json(ips_file);
    std::string ip = ips["client"].get<std::vector<std::string>>()[i];
    ip = ip.substr(0, ip.length() - 6);
    return ip;
}

std::string get_client_management_ip(int i = 0) {
    auto ips = read_json(ips_file);
    std::string ip = ips["client_management"].get<std::vector<std::string>>()[i];
    ip = ip.substr(0, ip.length() - 6);
    return ip;
}

std::string get_proxy_management_ip(int i) {
    auto ips = read_json(ips_file);
    std::string ip = ips["proxy_management"].get<std::vector<std::string>>()[i];
    ip = ip.substr(0, ip.length() - 6);
    return ip;
}

std::string get_receiver_management_ip(int i) {
    auto ips = read_json(ips_file);
    std::string ip = ips["recipient_management"].get<std::vector<std::string>>()[i];
    ip = ip.substr(0, ip.length() - 6);
    return ip;
}

std::string get_receiver_ip(int i) {
    auto ips = read_json(ips_file);
    std::string ip = ips["recipient"].get<std::vector<std::string>>()[i];
    ip = ip.substr(0, ip.length() - 6);
    return ip;
}

std::string get_proxy_ip(int i) {
    auto ips = read_json(ips_file);
    std::string ip = ips["proxy"].get<std::vector<std::string>>()[i];
    ip = ip.substr(0, ip.length() - 6);
    return ip;
}

// In microseconds
int64_t get_current_time() {
    auto currentTime = std::chrono::system_clock::now();
    auto durationSinceEpoch = currentTime.time_since_epoch();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        durationSinceEpoch);
    int64_t microsecondsCount = microseconds.count();
    return microsecondsCount;
}

sockaddr_in get_sock_addr(std::string ip, int port = UDP_DST_PORT) {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    return addr;
}

int get_bound_udp_socket(int64_t ip, int port = UDP_DST_PORT) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = ip;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
        0) {
        perror("bind");
        close(sockfd);
        exit(1);
    }

    return sockfd;
}

// REFACTOR all these into just consts
std::tuple<std::string, int> get_tcp_address(const std::string addr,
                                             int port = UDP_DST_PORT) {
    return std::make_tuple(addr, port + 1);
}

std::tuple<std::string, int> get_hedging_address(const std::string addr,
                                             int port = UDP_DST_PORT) {
    return std::make_tuple(addr, port + 2);
}

std::tuple<std::string, int> get_holdreleaase_send_address(const std::string addr,
                                             int port = UDP_DST_PORT) {
    return std::make_tuple(addr, port + 3);
}

std::tuple<std::string, int> get_holdreleaase_recv_address(const std::string addr,
                                             int port = UDP_DST_PORT) {
    return std::make_tuple(addr, port + 4);
}

std::tuple<std::string, int> get_trading_address(const std::string addr,
                                             int port = UDP_DST_PORT) {
    return std::make_tuple(addr, port + 5);
}

std::tuple<std::string, int> get_mac_collection_address(const std::string addr,
                                             int port = UDP_DST_PORT) {
    return std::make_tuple(addr, port + 6);
}

std::tuple<std::string, int> get_order_submission_address(const std::string addr,
                                             int port = UDP_DST_PORT) {
    return std::make_tuple(addr, port + 7);  // 34261 is used in gcp startup scripts now
}

std::tuple<std::string, int> get_delaymonitor_address(const std::string addr,
                                             int port = UDP_DST_PORT) {
    return std::make_tuple(addr, port + 8);
}

std::tuple<std::string, int> get_second_order_submission_address(const std::string addr,
                                             int port = UDP_DST_PORT) {
    return std::make_tuple(addr, port + 9);   // 34263 is used in gcp startup scripts now
}

inline bool is_order_critical(const int32_t price, const bool is_bid) {
    if (is_bid) return price >= (CONFIG::ORDERS_SUBMISSION::MID_PRICE - CONFIG::ORDERS_SUBMISSION::ACTION_WINDOW);
    return price <= (CONFIG::ORDERS_SUBMISSION::MID_PRICE + CONFIG::ORDERS_SUBMISSION::ACTION_WINDOW);
}

// Used to get the number of MAC to be collected
std::vector<std::string> get_unique_recipients(json ips = json::value_t::null) {
    auto recvrs = get_management_receivers_ips_ports(ips);
    std::set<std::string> recvrs_set;
    for (auto ele : recvrs) {
        recvrs_set.insert(ele.first);
    }
    std::vector<std::string> unique_recvrs(recvrs_set.begin(),
                                           recvrs_set.end());

    return unique_recvrs;
}

std::string get_stats_s3_bucket_name() {
    std::string bucket_name(CONFIG::S3_STATS_BUCKET);
    return bucket_name;
}

void write_json_to_file(json json_data, std::string filename) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << json_data;
        file.close();
        return;
    }

    std::cerr << "[write_json_to_file] Unable to open file " << filename
              << std::endl;
    exit(1);
}

int recv_message(int sockfd, char *buffer) {
    int bytes_read = recvfrom(sockfd, buffer, BUF_SIZE, 0, nullptr, nullptr);
    if (bytes_read < 0) {
        perror("recvfrom()");
        exit(1);
    }

    return bytes_read;
}

int calculate_mean(const std::vector<int>& numbers) {
    if (numbers.empty()) {
        return 0;  // Handle the case of an empty vector
    }

    int sum = 0;
    for (int num : numbers) {
        sum += num;
    }

    return sum / numbers.size();
}

int calculate_standard_deviation(const std::vector<int>& numbers) {
    auto mean = calculate_mean(numbers);
    auto variance = 0;

    for (int num : numbers) {
        auto diff = num - mean;
        variance += (diff * diff);
    }

    variance /= numbers.size();
    return std::sqrt(variance);
}

double calculatePercentile(const std::vector<int>& data, double percentile) {
    if (data.empty()) {
        std::cerr << "Error: Empty dataset." << std::endl;
        return 0.0;
    }

    std::vector<int> sortedData = data;
    std::sort(sortedData.begin(), sortedData.end());

    int index = (percentile / 100.0) * (sortedData.size() - 1);

    if (index == floor(index)) {
        return sortedData[static_cast<size_t>(index)];
    } else {
        size_t floorIndex = static_cast<size_t>(floor(index));
        size_t ceilIndex = static_cast<size_t>(ceil(index));
        int floorValue = sortedData[floorIndex];
        int ceilValue = sortedData[ceilIndex];
        return floorValue + (index - floor(index)) * (ceilValue - floorValue);
    }
}

void store_received_mac_to_json(ManagementMsg &msg) {
    json json_data;
    for (const auto &[ip, mac] : msg.addrs()) {
        std::clog << ip << " => " << mac << std::endl;
        json_data[ip] = mac;
    }

    write_json_to_file(json_data, mappings_file);

    status = Status::RUNNING;
}

std::vector<int> get_candidate_recipient_indices(int leaf_number,
                                                 int total_leaves,
                                                 int total_recipients) {
    int chunk_size = (total_recipients / total_leaves) +
                     (total_recipients % total_leaves != 0);
    int starting_index = leaf_number * chunk_size;
    int ending_index =
        std::min(starting_index + chunk_size - 1, total_recipients - 1);

    std::vector<int> res;
    for (int i = starting_index; i <= ending_index; i++) {
        res.push_back(i);
    }

    return res;
}

int get_parent_proxy_of_receiver(
    int current_node, int m, int total_proxies_without_redundant_nodes,
    int total_receivers) {
    int total_leaves = get_total_leaves(total_proxies_without_redundant_nodes, m);

    for (int i = 0; i < total_proxies_without_redundant_nodes; i++) {
        if (false == is_a_leaf_node(i, total_proxies_without_redundant_nodes, m)) {
            continue;
        }

        int leaf_number = get_leaf_number(total_proxies_without_redundant_nodes, i, m);
        std::vector<int> rec_indices = get_candidate_recipient_indices(
            leaf_number, total_leaves, total_receivers);

        if (std::find(rec_indices.begin(), rec_indices.end(), current_node) != rec_indices.end()) {
            return i;
        }
    }

    throw std::runtime_error("Could not determine the parent of a receiver");
}

std::vector<std::string> get_recipients_to_send_messages_for_proxy_node(
    json& ips, int proxy_num, int m, int total_proxies_without_redundant_nodes, std::string mode) {

    int leaf_number = get_leaf_number(total_proxies_without_redundant_nodes, proxy_num, m);
    int total_leaves = get_total_leaves(total_proxies_without_redundant_nodes, m);

    std::string ip_type = "recipient_management";
    if (mode == "dpdk" && CONFIG::ENFORCE_SOCKET_RECEIVER == false) {
        ip_type = "recipient";
    }

    std::vector<int> candidate_recipient_indices = get_candidate_recipient_indices(
        leaf_number, total_leaves, ips[ip_type].get<std::vector<std::string>>().size());

    std::vector<std::string> recipients;
    for (int i : candidate_recipient_indices) {
        recipients.push_back(ips[ip_type].get<std::vector<std::string>>()[i]);
    }

    return recipients;
}

std::vector<std::string> get_recipients_to_send_messages_for_redundant_node(
    json ips, int current_node, int m, int redundancy_factor,
    int total_proxies_without_redundant_nodes, std::string mode) {

    std::vector<std::string> res;

    int total_nodes = get_proxies_ips("management", ips).size();
    assert(true == is_a_leaf_redundant_node(
        current_node,
        total_proxies_without_redundant_nodes,
        total_nodes,
        m, redundancy_factor));

    int parent_node = get_parent_of_redundant_node(
        current_node, total_proxies_without_redundant_nodes, redundancy_factor);

    assert(false == is_a_leaf_node(parent_node, total_proxies_without_redundant_nodes, m));
    assert(false == is_a_redundant_node(parent_node, total_proxies_without_redundant_nodes));

    auto siblings_proxy_nodes_indices = get_children_indices_for_proxy_node(
        parent_node, m, redundancy_factor, total_proxies_without_redundant_nodes, true);

    for (auto sibling : siblings_proxy_nodes_indices) {
        assert(true == is_a_leaf_node(sibling, total_proxies_without_redundant_nodes, m));

        auto nephews = get_recipients_to_send_messages_for_proxy_node(
            ips, sibling, m, total_proxies_without_redundant_nodes, mode);

        res.insert(res.end(), nephews.begin(), nephews.end());
    }

    return res;
}


std::vector<std::string> get_ips_to_send_messages_for_redundant_node(
    int current_index, int branching_factor, std::string mode,
    int redundancy_factor, int total_proxies_without_redundant_nodes) {

    assert(true == is_a_redundant_node(current_index, total_proxies_without_redundant_nodes));

    auto ips = read_json(ips_file);
    auto children_indices = get_children_indices_for_redundant_node(
        current_index, ips["proxy"].get<std::vector<std::string>>().size(),
        branching_factor, redundancy_factor, total_proxies_without_redundant_nodes);

    std::vector<std::string> send_to;
    if (children_indices.empty()) {
        assert(true == is_a_leaf_redundant_node(
                            current_index, total_proxies_without_redundant_nodes,
                            get_proxies_ips("management", ips).size(), branching_factor, redundancy_factor));

        send_to = get_recipients_to_send_messages_for_redundant_node(
            ips, current_index, branching_factor, redundancy_factor,
            total_proxies_without_redundant_nodes, mode);
    } else {
        send_to = get_ips_for_node_indices(
            children_indices, ips, mode);
        std::cout << "Proxy forwarding messages to children proxies" << std::endl;
    }

    return send_to;
}

std::vector<int> get_siblings_indices_for_redundant_node(int current_node, int m, int redundancy_factor,
    int total_proxies_without_redundant_nodes) {
    assert(true == is_a_redundant_node(current_node, total_proxies_without_redundant_nodes));

    std::vector<int> indices;

    int parent_node = get_parent_of_redundant_node(
        current_node, total_proxies_without_redundant_nodes, redundancy_factor);

    auto siblings_proxy_nodes_indices = get_children_indices_for_proxy_node(
        parent_node, m, redundancy_factor, total_proxies_without_redundant_nodes, true);

    return siblings_proxy_nodes_indices;
}

std::vector<std::string> get_ips_to_send_messages_for_proxy_node(
    int current_index, int branching_factor, std::string mode,
    int redundancy_factor, int total_proxies_without_redundant_nodes) {

    assert(false == is_a_redundant_node(current_index, total_proxies_without_redundant_nodes));

    auto ips = read_json(ips_file);
    auto children_indices = get_children_indices_for_proxy_node(
        current_index, branching_factor, redundancy_factor, total_proxies_without_redundant_nodes);

    std::vector<std::string> send_to;

    if (children_indices.empty()) {
        assert(true == is_a_leaf_node(current_index, total_proxies_without_redundant_nodes, branching_factor));

        send_to = get_recipients_to_send_messages_for_proxy_node(
            ips, current_index, branching_factor, total_proxies_without_redundant_nodes, mode);
    } else {
        send_to = get_ips_for_node_indices(
            children_indices, ips, mode);
        std::cout << "Proxy forwarding messages to children proxies" << std::endl;
    }

    return send_to;
}

// works for all nodes (leaf or non leaf)
std::vector<size_t> get_children_indices(int current_node, int branching_factor, int redundancy_factor,
    int total_proxies_without_redundant_nodes, bool return_only_proxy_nodes = false, std::string mode = "socket") {
    assert(redundancy_factor == 0);  // not works for redundancy factor, not really needed as we abandoned it.
    assert(return_only_proxy_nodes == false);

    std::vector<size_t> indices;

    if (is_a_leaf_node(current_node, total_proxies_without_redundant_nodes, branching_factor)) {
        int leaf_number = get_leaf_number(total_proxies_without_redundant_nodes, current_node, branching_factor);
        int total_leaves = get_total_leaves(total_proxies_without_redundant_nodes, branching_factor);

        std::string ip_type = "recipient_management";
        if (mode == "dpdk" && CONFIG::ENFORCE_SOCKET_RECEIVER == false) {
            ip_type = "recipient";
        }

        auto ips = read_json(ips_file);

        std::vector<int> candidate_recipient_indices = get_candidate_recipient_indices(
            leaf_number, total_leaves, ips[ip_type].get<std::vector<std::string>>().size());

        for (auto ind : candidate_recipient_indices) indices.push_back(static_cast<size_t>(ind));
        return indices;
    }

    for (size_t i = 1; i <= static_cast<size_t>(branching_factor); i++) {
        indices.push_back(i + branching_factor * current_node);
    }

    return indices;
}

struct Levelinfo {
    int starting_index;
    int ending_index;
    int level_num;
};

Levelinfo get_level_info(int current_index, int branching_factor) {
    int level = 0;
    int total_nodes = 1;
    int nodes_in_current_level = 1;
    int starting_index = 0;

    while (current_index >= total_nodes) {
        starting_index = total_nodes;
        level += 1;
        nodes_in_current_level *= branching_factor;
        total_nodes += nodes_in_current_level;
    }

    int ending_index = total_nodes - 1;

    return {starting_index, ending_index, level};
}

std::vector<std::string> get_ips_to_send_messages(
    int current_index, int branching_factor, std::string mode,
    bool ip_only, int redundancy_factor, int total_proxies_without_redundant_nodes) {
    std::vector<std::string> send_to;
    if (is_a_redundant_node(current_index, total_proxies_without_redundant_nodes)) {
        send_to = get_ips_to_send_messages_for_redundant_node(
            current_index, branching_factor, mode,
            redundancy_factor, total_proxies_without_redundant_nodes);
    } else {
        send_to = get_ips_to_send_messages_for_proxy_node(
            current_index, branching_factor, mode,
            redundancy_factor, total_proxies_without_redundant_nodes);
    }

    if (false == ip_only) {
        return send_to;
    }

    for (auto &ip : send_to) {
        size_t colon_pos = ip.find(':');
        ip = ip.substr(0, colon_pos);
    }

    std::set<std::string> unique_ips(send_to.begin(), send_to.end());
    send_to.assign(unique_ips.begin(), unique_ips.end());
    return send_to;
}

std::vector<sockaddr_in> convert_node_indices_to_hedge_addrs(
    std::vector<int> indices, json ips_config = json::value_t::null) {
    std::vector<sockaddr_in> addrs;
    auto all_ips = get_proxies_ips("management", ips_config);
    for (auto index : indices) {
        assert(index < static_cast<int>(all_ips.size()));
        auto [ip, port] = get_hedging_address(all_ips[index]);

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);
        addrs.push_back(addr);
    }

    return addrs;
}

std::vector<sockaddr_in> convert_node_indices_to_holdrelease_recv_addrs(
    std::vector<int> indices, json ips_config = json::value_t::null) {
    std::vector<sockaddr_in> addrs;
    auto all_ips = get_proxies_ips("management", ips_config);
    std::clog << "all_ips: " << all_ips.size() << std::endl;
    for (auto index : indices) {
        std::clog << "Index: " << index << std::endl;
        assert(index < static_cast<int>(all_ips.size()));
        auto [ip, port] = get_holdreleaase_recv_address(all_ips[index]);

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);
        addrs.push_back(addr);
    }

    return addrs;
}

std::pair<std::vector<std::pair<uint32_t, rte_be16_t>>, std::vector<std::vector<uint8_t>>>
format_ips_and_macs_for_dpdk(std::vector<std::string>& ips,
    std::unordered_map<std::string, std::string>& ip_to_mac, bool reverse_ip = false) {
    std::vector<std::pair<uint32_t, rte_be16_t>> fast_ips_ports;
    std::vector<std::vector<uint8_t>> fast_macs;

    // Initialize fast_macs
    for (int i = 0; i < static_cast<int>(ips.size()); i++) {
        std::vector<uint8_t> tmp = {0, 0, 0, 0, 0, 0};
        fast_macs.push_back(tmp);
    }

    for (int i = 0; i < static_cast<int>(ips.size()); i++) {
        auto ip_port_str = ips[i];
        auto [ip, port] = separate_ip_port(ip_port_str);
        char *cstr_ip = &ip[0];
        uint32_t dest_ip = string_to_ip(cstr_ip, reverse_ip);
        fast_ips_ports.push_back({dest_ip, rte_cpu_to_be_16(port)});
        char *cstr_mac = &ip_to_mac[ip][0];
        string_to_mac(cstr_mac, fast_macs[i]);
    }

    return std::make_pair(fast_ips_ports, fast_macs);
}

std::vector<sockaddr_in> convert_ips_to_addrs(std::vector<std::string> ips) {
    std::vector<sockaddr_in> addrs;
    for (auto ele : ips) {
        auto [ip, port] = separate_ip_port(ele);
        struct sockaddr_in receiver_addr;
        receiver_addr.sin_family = AF_INET;
        receiver_addr.sin_addr.s_addr = inet_addr(ip.c_str());
        receiver_addr.sin_port = htons(port);
        addrs.push_back(receiver_addr);
    }
    return addrs;
}

std::vector<sockaddr_in> get_downstream_addrs(
    int proxy_num, int branching_factor, std::string mode,
    int redundancy_factor, int total_proxies_without_redundant_nodes) {
    auto ips = get_ips_to_send_messages(
        proxy_num, branching_factor, mode, false, redundancy_factor, total_proxies_without_redundant_nodes);

    std::cout << "Proxy " << proxy_num << " serving:" << std::endl;
    std::vector<sockaddr_in> addrs;
    for (auto ele : ips) {
        auto [ip, port] = separate_ip_port(ele);
        std::cout << ip << "        ";
        struct sockaddr_in receiver_addr;
        receiver_addr.sin_family = AF_INET;
        receiver_addr.sin_addr.s_addr = inet_addr(ip.c_str());
        receiver_addr.sin_port = htons(port);
        addrs.push_back(receiver_addr);
    }
    std::cout << std::endl;
    return addrs;
}

std::vector<std::string> extend_ips_for_sibling_hedging(
    std::vector<std::string> ips, int proxy_num, int branching_factor,
    int total_proxies_without_redundant_nodes, int redundancy_factor, std::string mode, bool ip_only) {
    int hedging_intensity = CONFIG::HEDGING::SIBLING_HEDGING_INTENSITY;
    auto curr_index = proxy_num;
    while (CONFIG::HEDGING::SIBLING_BASED_HEDGING && hedging_intensity) {
        int next_siblings_proxy_index = get_next_sibling(
            curr_index, branching_factor, total_proxies_without_redundant_nodes);
        if (next_siblings_proxy_index != -1) {
            auto more_ips = get_ips_to_send_messages(next_siblings_proxy_index, branching_factor, mode, ip_only,
                redundancy_factor, total_proxies_without_redundant_nodes);

            int to_deduct = redundancy_factor;
            if (is_a_leaf_node(curr_index, total_proxies_without_redundant_nodes, branching_factor)) {
                to_deduct = 0;
            }

            ips.erase(ips.end() - to_deduct, ips.end());
            ips.insert(ips.end(), more_ips.begin(), more_ips.end());
            curr_index = next_siblings_proxy_index;
        }

        hedging_intensity--;
    }

    return ips;
}

void create_stats_folder() {
    std::string folder(stats_folder);
    std::string cmd = "mkdir " + folder;
    system(cmd.c_str());
}

std::vector<std::string> get_management_downstreams(int id,
                                                    int branching_factor,
                                                    int redundancy_factor,
                                                    int total_proxies_without_redundant_nodes) {
    if (CONFIG::HEDGING::DYNAMIC_RELATIONSHIPS && id != 0) {
        auto level_info = get_level_info(id, branching_factor);
        std::vector<std::string> downstreams;
        for (int i = level_info.starting_index; i <= level_info.ending_index; i++) {
            auto d = get_ips_to_send_messages(
                i, branching_factor, "socket", true, redundancy_factor, total_proxies_without_redundant_nodes);
            downstreams.insert(downstreams.end(), d.begin(), d.end());
        }
        return downstreams;
    }

    auto downstream = get_ips_to_send_messages(id,
                                               branching_factor,
                                               "socket",
                                               true,
                                               redundancy_factor,
                                               total_proxies_without_redundant_nodes);

    downstream = extend_ips_for_sibling_hedging(downstream,
                                                id,
                                                branching_factor,
                                                total_proxies_without_redundant_nodes,
                                                redundancy_factor,
                                                "socket",
                                                true);
    return downstream;
}

static std::string stats_to_csv(StatsDp& stats) {
    std::ostringstream csv_ss;

    std::clog << "Total records: " << stats.records2.size() << std::endl;
    csv_ss << "HEADER:MsgId,Latency,Recipientid,Holdingduration,Releasetime,Deadline\n";
    for (const auto& v : stats.records2) {
        csv_ss << v.message_id << "," << v.latency << "," << v.recipient_id
        << "," << v.holding_duration << "," << v.release_time
        << "," << v.deadline << "\n";
    }

    std::clog << "csv formed" << std::endl;
    return csv_ss.str();
}

// receiver_id acts as worker id in the case of dpdk receiver
void append_stat_to_csv_file_with_id(int receiver_id, StatsDp &stats) {
    if (stats.records2.empty()) {
        return;
    }

    std::ofstream outfile;
    std::string folder(stats_folder);
    std::string filename = folder + "/" + std::to_string(receiver_id) + ".csv";
    outfile.open(filename, std::ios::out | std::ios_base::app);

    const std::lock_guard<std::mutex> lock(stat_mutexes[receiver_id]);
    outfile << stats_to_csv(stats) << std::endl;
    outfile.close();
}

void upload_stat_to_s3(int receiver_id, std::string folder) {
    std::string filename = std::to_string(receiver_id);
    std::string s3_bucket = get_stats_s3_bucket_name();
    std::clog << "Uploading..." << std::endl;
    std::string script_cmd = "../aws-deploy/stats_to_s3.sh --upload " +
                             s3_bucket + " " + folder + " " + filename
                             + " " + CONFIG::CLOUD + " " + std::to_string(receiver_id);
    std::clog << "Command: " << script_cmd << std::endl;
    system(script_cmd.c_str());
    std::clog << "Uploaded!" << std::endl;
}

void flush_stats_to_local_and_s3(int receiver_id, std::string s3_folder) {
    auto original_receiver_id = receiver_id;
    // One file per VM in S3
    for (auto &stat : all_stats) {
        append_stat_to_csv_file_with_id(receiver_id++, stat);
        const std::lock_guard<std::mutex> lock(stat_mutexes[receiver_id]);
        stat.records.clear();
        stat.records2.clear();
    }

    upload_stat_to_s3(original_receiver_id, s3_folder);
    std::cout << total_messages_from_hedge_nodes << std::endl;
    total_messages_from_hedge_nodes = 0;
}

void proxy_management_controller(int entity_id, int branching_factor,
                                 std::string mode, int redundancy_factor,
                                 int total_proxies_without_redundant_nodes) {
    auto [ip, port]     = get_mac_collection_address(get_proxy_ip(entity_id));
    std::string src_ip  = get_proxy_management_ip(entity_id);
    std::string src_mac = read_file((mode != "dpdk" ? macsocket_file : mac_file));

    auto downstream_addrs = get_management_downstreams(entity_id,
                                                       branching_factor,
                                                       redundancy_factor,
                                                       total_proxies_without_redundant_nodes);
    auto downstream_left{downstream_addrs};

    ManagementMsg msg;

    json macs_dict;
    int  macs_max = downstream_addrs.size();

    Zmqsocket proxy(std::string {"tcp"},
                    std::string {src_ip},
                    std::string {std::to_string(port)},
                    zmq::socket_type::peer,
                    "PROXY",
                    Zmqsocket::LOG_LEVEL::ERR);

    // Currently we are using ZMQ's DRAFT API Socket: ZMQ_PEER.
    // We have found sporadic issues at startup time when attempting to bind
    // to one's own IP address. We have found that a try/while loop has solved this.
    int64_t att = 0;
    while (true) {
        try {
            proxy.bind();
        } catch(const zmq::error_t& e) {
            std::cerr << "FAILED TO BIND: " << src_ip << ":" << std::to_string(port) << std::endl;
            std::cerr << "ERROR[" << att++ << "]: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        break;
    }

    bool done = false;

    while (true) {
        if (static_cast<int>(macs_dict.size()) == macs_max && done == false) {
            write_json_to_file(macs_dict, mappings_file);
            done = true;
            status = Status::RUNNING;
        }

        int id;
        ManagementMsg msg, reply;
        proxy.recv_message_from(msg, id);

        switch (msg.msg_type()) {
            case static_cast<int>(MessageType::REQ_MAC_COLLECTION): {
                done = false;
                macs_dict = json{};

                // std::clog << "RECEIVED MAC COLLECTION REQUEST FROM: " << id << std::endl;
                for (auto addr : downstream_addrs) {
                    int rid = proxy.connect_peer(std::string {"tcp"},
                                                 std::string {addr},
                                                 std::string {std::to_string(port)});
                    proxy.send_message_to(ManagementMsg(), static_cast<int>(MessageType::REQ_MAC_ADDRESS), rid);
                }

                downstream_left = std::vector<std::string>{downstream_addrs};
                // for (auto addr : downstream_left) {
                //     std::cout << "IP LEFT: " << addr << std::endl;
                // }

                reply.set_msg_type(MessageType::ACK_MAC_COLLECTION);
                proxy.send_message_to(reply, id);
                break;
            }

            case static_cast<int>(MessageType::REQ_MAC_COLLECTION_STATUS): {
                // std::clog << "RECEIVED MAC COLLECTION STATUS REQUEST FROM: " << id << std::endl;

                if (done)
                    reply.set_msg_type(MessageType::ACK_MAC_COLLECTION_STATUS);
                else
                    reply.set_msg_type(MessageType::NACK_MAC_COLLECTION_STATUS);

                reply.set_data(ip);
                proxy.send_message_to(reply, id);

                break;
            }

            case static_cast<int>(MessageType::REQ_MAC_ADDRESS): {
                // std::clog << "MAC REQUESTED BY " << id << std::endl;
                reply.set_msg_type(MessageType::ACK_MAC_ADDRESS);
                reply.set_data(ip);
                reply.set_ip_address(src_ip);
                reply.set_mac_address(src_mac);
                proxy.send_message_to(reply, id);
                break;
            }

            case static_cast<int>(MessageType::ACK_MAC_ADDRESS): {
                // std::clog << "RECEIVED MAC"
                //           << "[" << macs_dict.size() + 1 << "/" << downstream_addrs.size() << "]: "
                //           << msg.mac_address()
                //           << " <= M: " << msg.ip_address() << " | NON-M:" << msg.data() <<  std::endl;

                macs_dict[msg.data()] = msg.mac_address();

                auto it = std::find(downstream_left.begin(), downstream_left.end(), msg.ip_address());
                if (it != downstream_left.end()) downstream_left.erase(it);

                // for (auto addr : downstream_left) {
                //     std::cout << "IP LEFT: " << addr << std::endl;
                // }

                break;
            }

            default: {
                std::cerr << "ERR: INCORRECT MESSAGE TYPE RECEIVED: "
                          << std::string {MessageType_Name(MessageType(msg.msg_type()))} << std::endl;
                break;
            }
        }
    }
}

void receiver_management_controller(int entity_id, std::string mode, RWQ& management_to_dataplane_q) {
    create_stats_folder();
    status = Status::RUNNING;

    auto [ip, port]     = get_mac_collection_address(get_receiver_ip(entity_id));
    std::string src_ip  = get_receiver_management_ip(entity_id);
    std::string src_mac = read_file(((mode != "dpdk") ||
                                    (CONFIG::ENFORCE_SOCKET_RECEIVER) ? macsocket_file : mac_file));

    Zmqsocket receiver(std::string {"tcp"},
                       std::string {src_ip},
                       std::string {std::to_string(port)},
                       zmq::socket_type::peer,
                       "RECEIVER",
                       Zmqsocket::LOG_LEVEL::ERR);

    int64_t att = 0;
    while (true) {
        try {
            receiver.bind();
        } catch(const zmq::error_t& e) {
            std::cerr << "FAILED TO BIND: " << src_ip << ":" << std::to_string(port) << std::endl;
            std::cerr << "ERROR[" << att++ << "]: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        break;
    }

    while (true) {
        int id;
        ManagementMsg msg, reply;
        receiver.recv_message_from(msg, id);

        switch (msg.msg_type()) {
            case static_cast<int>(MessageType::REQ_MAC_ADDRESS): {
                std::clog << "REQUESTED MAC FROM " << id << std::endl;
                reply.set_msg_type(MessageType::ACK_MAC_ADDRESS);
                reply.set_data(ip);
                if (CONFIG::ENFORCE_SOCKET_RECEIVER) reply.set_data(src_ip);
                reply.set_ip_address(src_ip);
                reply.set_mac_address(src_mac);
                receiver.send_message_to(reply, id);
                break;
            }

            case static_cast<int>(MessageType::REQ_STATS_COLLECTION): {
                std::clog << "REQUESTED STATS FROM " << id << std::endl;
                flush_stats_to_local_and_s3(entity_id, msg.request()[0]);
                reply.set_msg_type(MessageType::ACK_STATS_COLLECTION);
                receiver.send_message_to(reply, id);
                break;
            }

            case static_cast<int>(MessageType::START_ORDERS_SUBMISSION): {
                std::clog << "REQUESTED ORDERS SUBMISSION " << id << std::endl;

                ManagementToDataplane action;
                action.mutable_order_sub_req()->CopyFrom(msg.order_sub_req());
                management_to_dataplane_q.enqueue(action);

                reply.set_msg_type(MessageType::ACK_START_ORDERS_SUBMISSION);
                receiver.send_message_to(reply, id);
                break;
            }

            default: {
                std::cerr << "ERR: INCORRECT MESSAGE TYPE RECEIVED: "
                          << std::string {MessageType_Name(MessageType(msg.msg_type()))}
                          << std::endl;
                reply.set_msg_type(MessageType::ACK_UNKNOWN);
                receiver.send_message_to(reply, id);
                break;
            }
        }
    }
}

#endif  // SRC_UTILS_COMMON_HPP_
