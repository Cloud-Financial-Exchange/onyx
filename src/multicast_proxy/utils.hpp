// Copyright [2023] NYU

#ifndef SRC_MULTICAST_PROXY_UTILS_HPP_
#define SRC_MULTICAST_PROXY_UTILS_HPP_

#include <bpf/bpf.h>
#include <rte_byteorder.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "./hedging.hpp"
#include "./../utils/holdrelease.hpp"
#include "./../utils/trading.hpp"


#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
#define RTE_BE_TO_CPU_16(be_16_v)  (be_16_v)
#define RTE_CPU_TO_BE_16(cpu_16_v) (cpu_16_v)
#else
#define RTE_BE_TO_CPU_16(be_16_v) \
        (uint16_t) ((((be_16_v) & 0xFF) << 8) | ((be_16_v) >> 8))
#define RTE_CPU_TO_BE_16(cpu_16_v) \
        (uint16_t) ((((cpu_16_v) & 0xFF) << 8) | ((cpu_16_v) >> 8))
#endif

class Mcastmessages {
    std::vector<uint64_t> history;

    int64_t starting_msg_id;

 public:
    explicit Mcastmessages(int64_t starting_msg_id) : starting_msg_id(starting_msg_id) {}

    MsgDp get_msg(int i) {
        MsgDp msg;
        auto t = get_current_time();

        msg.set_client_send_time(t);
        msg.set_root_send_time(t);
        msg.set_deadline(t);
        msg.set_is_from_hedge_node(2);
        msg.set_msg_id(i + starting_msg_id);
        msg.set_msg_type(0);
        msg.set_recipient_id(0);
        msg._history = 0;

        if (CONFIG::HEDGING::MSG_HISTORY && history.size()) {
            msg._history = history.back();
            history.pop_back();
        }

        if (CONFIG::HEDGING::MSG_HISTORY) history.push_back(t);

        return msg;
    }
};

struct Context {
    int proxy_num;
    int m;
    int total_proxies_without_redundant_nodes;
    int redundancy_factor;
    Hedging* hedging;
    Holdrelease* holdrelease;
    Tradingmatchingengine* tradingme;

    Levelinfo level_info;
    std::unordered_map<int, std::vector<std::string>> ips_per_sibling;
    std::vector<size_t> children_indices;

    Context(
        int proxy_num, int m, int total_proxies_without_redundant_nodes,
        int redundancy_factor, Hedging* hedging, Holdrelease* holdrelease, Tradingmatchingengine* tradingme,
        Levelinfo level_info, std::unordered_map<int, std::vector<std::string>> ips_per_sibling,
        const std::vector<size_t>& children_indices):
        proxy_num(proxy_num), m(m), total_proxies_without_redundant_nodes(total_proxies_without_redundant_nodes),
        redundancy_factor(redundancy_factor), hedging(hedging), holdrelease(holdrelease), tradingme(tradingme),
        level_info(level_info), ips_per_sibling(ips_per_sibling), children_indices(children_indices) {}
};

/**
 * For debugging purposes. 
*/
void show_pkt_info(struct rte_mbuf *pkt, std::string name) {
    struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
    struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)(ip_hdr + 1);

    printf("Destination MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
        eth_hdr->dst_addr.addr_bytes[0],
        eth_hdr->dst_addr.addr_bytes[1],
        eth_hdr->dst_addr.addr_bytes[2],
        eth_hdr->dst_addr.addr_bytes[3],
        eth_hdr->dst_addr.addr_bytes[4],
        eth_hdr->dst_addr.addr_bytes[5]);

    printf("Source MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
        eth_hdr->src_addr.addr_bytes[0],
        eth_hdr->src_addr.addr_bytes[1],
        eth_hdr->src_addr.addr_bytes[2],
        eth_hdr->src_addr.addr_bytes[3],
        eth_hdr->src_addr.addr_bytes[4],
        eth_hdr->src_addr.addr_bytes[5]);

    std::clog << name << " IP Dst addr: " << rte_cpu_to_be_32(ip_hdr->dst_addr) << std::endl;
    std::clog << name << " IP Src addr: " << rte_cpu_to_be_32(ip_hdr->src_addr) << std::endl;
    std::clog << name << " UDP Dst Port: " << rte_cpu_to_be_16(udp_hdr->dst_port) << std::endl;
    std::clog << name << " UDP Src Port: " << rte_cpu_to_be_16(udp_hdr->src_port) << std::endl;
    std::clog << name << " len: " << pkt->data_len << std::endl;
    if (pkt->next != NULL) {
        std::clog << name << " more len: " << pkt->next->data_len << std::endl;
    }
}

// Refactor the following two function
int store_data_in_bpf_map_ip(void* data, uint32_t size, std::string map_path) {
    int fd = bpf_obj_get(map_path.c_str());
    if (fd < 0) return fd;

    uint32_t* values = reinterpret_cast<uint32_t*>(data);

    for (uint32_t i = 0; i < size; i++) {
        int e = bpf_map_update_elem(fd, &i, reinterpret_cast<const void*>(values + i), BPF_ANY);
        if (e != 0) return e;
    }

    return 0;
}

int store_data_in_bpf_map_mac(void* data, uint32_t size, std::string map_path) {
    int fd = bpf_obj_get(map_path.c_str());
    if (fd < 0) return fd;

    uint64_t* values = reinterpret_cast<uint64_t*>(data);

    for (uint32_t i = 0; i < size; i++) {
        int e = bpf_map_update_elem(fd, &i, reinterpret_cast<const void*>(values + i), BPF_ANY);
        if (e != 0) return e;
    }

    return 0;
}

static void setup_pkt_udp_ip_headers(struct rte_ipv4_hdr *ip_hdr,
                         struct rte_udp_hdr *udp_hdr,
                         uint16_t pkt_data_len) {
    uint16_t *ptr16;
    uint32_t ip_cksum;
    uint16_t pkt_len;

    // Initialize UDP header.
    pkt_len = (uint16_t) (pkt_data_len + sizeof(struct rte_udp_hdr));
    udp_hdr->src_port = rte_cpu_to_be_16(UDP_SRC_PORT);
    udp_hdr->dst_port = rte_cpu_to_be_16(UDP_DST_PORT);
    udp_hdr->dgram_len      = RTE_CPU_TO_BE_16(pkt_len);
    udp_hdr->dgram_cksum    = 0;  /* No UDP checksum. */

    uint32_t IP_SRC_ADDR, IP_DST_ADDR;

    // Initialize IP header.
    pkt_len = (uint16_t) (pkt_len + sizeof(struct rte_ipv4_hdr));
    ip_hdr->version_ihl   = IP_VHL_DEF;
    ip_hdr->type_of_service   = 0;
    ip_hdr->fragment_offset = 0;
    ip_hdr->time_to_live   = IP_DEFTTL;
    ip_hdr->next_proto_id = IPPROTO_UDP;
    ip_hdr->packet_id = 0;
    ip_hdr->total_length   = RTE_CPU_TO_BE_16(pkt_len);
    ip_hdr->src_addr = rte_cpu_to_be_32(IP_SRC_ADDR);
    ip_hdr->dst_addr = rte_cpu_to_be_32(IP_DST_ADDR);

    // Compute IP header checksum.
    ptr16 = reinterpret_cast<unaligned_uint16_t*>(ip_hdr);
    ip_cksum = 0;
    ip_cksum += ptr16[0]; ip_cksum += ptr16[1];
    ip_cksum += ptr16[2]; ip_cksum += ptr16[3];
    ip_cksum += ptr16[4];
    ip_cksum += ptr16[6]; ip_cksum += ptr16[7];
    ip_cksum += ptr16[8]; ip_cksum += ptr16[9];

    // Reduce 32 bit checksum to 16 bits and complement it.
    ip_cksum = ((ip_cksum & 0xFFFF0000) >> 16) +
            (ip_cksum & 0x0000FFFF);
    if (ip_cksum > 65535)
            ip_cksum -= 65535;
    ip_cksum = (~ip_cksum) & 0x0000FFFF;
    if (ip_cksum == 0)
            ip_cksum = 0xFFFF;
    ip_hdr->hdr_checksum = (uint16_t) ip_cksum;
}

void pretty_print_packet(struct rte_mbuf *mbuf) {
    // Ethernet header
    struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    printf("Ethernet Header\n");
    printf("  Src MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           eth_hdr->src_addr.addr_bytes[0],
           eth_hdr->src_addr.addr_bytes[1],
           eth_hdr->src_addr.addr_bytes[2],
           eth_hdr->src_addr.addr_bytes[3],
           eth_hdr->src_addr.addr_bytes[4],
           eth_hdr->src_addr.addr_bytes[5]);
    printf("  Dst MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           eth_hdr->dst_addr.addr_bytes[0],
           eth_hdr->dst_addr.addr_bytes[1],
           eth_hdr->dst_addr.addr_bytes[2],
           eth_hdr->dst_addr.addr_bytes[3],
           eth_hdr->dst_addr.addr_bytes[4],
           eth_hdr->dst_addr.addr_bytes[5]);

    // Check for IPv4 packet
    if (eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
        struct rte_ipv4_hdr *ipv4_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
        printf("IPv4 Header\n");
        printf("  Src IP: %u.%u.%u.%u\n",
               (ipv4_hdr->src_addr & 0xFF),
               (ipv4_hdr->src_addr >> 8) & 0xFF,
               (ipv4_hdr->src_addr >> 16) & 0xFF,
               (ipv4_hdr->src_addr >> 24) & 0xFF);
        printf("  Dst IP: %u.%u.%u.%u\n",
               (ipv4_hdr->dst_addr & 0xFF),
               (ipv4_hdr->dst_addr >> 8) & 0xFF,
               (ipv4_hdr->dst_addr >> 16) & 0xFF,
               (ipv4_hdr->dst_addr >> 24) & 0xFF);

        // Check for UDP packet
        if (ipv4_hdr->next_proto_id == IPPROTO_UDP) {
            struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)(ipv4_hdr + 1);
            printf("UDP Header\n");
            printf("  Src Port: %u\n", rte_be_to_cpu_16(udp_hdr->src_port));
            printf("  Dst Port: %u\n", rte_be_to_cpu_16(udp_hdr->dst_port));

            mbuf = mbuf->next;
            MsgDp* msg = rte_pktmbuf_mtod(mbuf, MsgDp*);
            std::cout << msg->msg_id() << " " << msg->root_send_time() << std::endl;
        }
    }
}

#endif  // SRC_MULTICAST_PROXY_UTILS_HPP_
