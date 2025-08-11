// Copyright [2023] NYU

#ifndef SRC_MULTICAST_PROXY_DPDK_PROXY_HPP_
#define SRC_MULTICAST_PROXY_DPDK_PROXY_HPP_

#include <vector>
#include <unordered_map>
#include <utility>
#include <set>
#include <unordered_set>
#include <string>
#include <memory>
#include <queue>

#include "./../utils/common.hpp"
#include "./../config.h"
#include "./hedging.hpp"
#include "./utils.hpp"

/* TODO: No hardcoded source port please (0xce85),
 *  0xce85 is 34254 in big endian, the default src port
 */
constexpr rte_be16_t DPDK_UDP_DST_PORT = 0xce85;
Status status = Status::BLOCKED;

int enqueue_failures = 0;

/*  DPDK related */

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {.max_lro_pkt_size = RTE_ETHER_MAX_LEN},
    .txmode = {
        .offloads = RTE_ETH_TX_OFFLOAD_IPV4_CKSUM,
    },
};

int PORT_ID = 0;
struct rte_ring *packetQueue;
struct rte_mempool *mbuf_pool;
int NUM_WORKERS = 6;

struct Workerthreadargs {
    struct rte_mempool *clone_pool;
    struct rte_mempool *header_pool;
    int count;
    Context *ctx;
    std::vector<std::pair<uint32_t, rte_be16_t>> fast_ips_ports;
    std::vector<std::vector<uint8_t>> fast_macs;
    bool is_leaf_node;
    bool is_redundant;
    uint32_t src_ip;
    std::vector<uint8_t> src_mac;
    int tx_queue_id;
    std::vector<int>* logs;
};

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int port_init(uint16_t port, struct rte_mempool *mbuf_pool) {
    struct rte_eth_conf port_conf = port_conf_default;
    std::string cloud {CONFIG::CLOUD};

    if (cloud == "gcp")
        port_conf.txmode = {};

    const uint16_t rx_rings = 1, tx_rings = NUM_WORKERS;
    uint16_t nb_rxd = RX_RING_SIZE;
    uint16_t nb_txd = TX_RING_SIZE;
    int retval;
    uint16_t q;

    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf txconf;

    if (!rte_eth_dev_is_valid_port(port))
        return -1;

    rte_eth_dev_info_get(port, &dev_info);
    printf("Max TX queues: %u\n", dev_info.max_tx_queues);
    // if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) {
    //     port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
    // }
    if (cloud != "gcp")  {
        if (!(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM)) {
            rte_panic(" offload not supported");
        }
    }

    /* Configure the Ethernet device. */
    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
    if (retval != 0)
        return retval;

    /* Allocate and set up 1 RX queue per Ethernet port. */
    for (q = 0; q < rx_rings; q++) {
        retval = rte_eth_rx_queue_setup(
            port, q, nb_rxd, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    // Allocate and set up 1 TX queue
    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    // std::clog << "Default TX Free Thresh: " << txconf.tx_free_thresh << std::endl;

    if (cloud == "gcp") {
        // GCP's gvNIC limitations
        txconf.tx_free_thresh = nb_txd - 4;
    } else {
        txconf.tx_free_thresh = TX_RING_SIZE/2;
    }

    // std::clog << "New TX Free Thresh: " << txconf.tx_free_thresh << std::endl;
    for (q = 0; q < tx_rings; q++) {
        retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                                        rte_eth_dev_socket_id(port), &txconf);
        if (retval < 0)
            return retval;
    }

    /* Start the Ethernet port. */
    retval = rte_eth_dev_start(port);
    if (retval < 0)
        return retval;

    return 0;
}

void add_trade_information(MsgDp* msg, Context* ctx) {
    auto tp = ctx->tradingme->get_data_point();
    tp.show();
    tp.trade_id = msg->msg_id();
    memcpy(msg->_data, &tp, sizeof(Tradedatapoint));
}

void prepare_and_transmit(
    Context *ctx, struct rte_mbuf *pkt,
    std::vector<std::pair<uint32_t, rte_be16_t>>& fast_ips_ports,
    std::vector<std::vector<uint8_t>>& fast_macs, uint32_t &src_ip,
    std::vector<uint8_t>& src_mac, struct rte_mempool *clone_pool,
    struct rte_mempool *header_pool, bool is_redundant,
    bool is_leaf_node, int tx_queue_id, bool free_pkt = true) {
    // assert(pkt != NULL);
    std::vector<std::pair<uint32_t, rte_be16_t>>* ips_ports;
    std::vector<std::vector<uint8_t>>* macs;

    ips_ports = &fast_ips_ports;
    macs = &fast_macs;

    struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
    struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)(ip_hdr + 1);

    auto ip_pkt_len = ip_hdr->total_length;
    auto udp_len = udp_hdr->dgram_len;
    pkt->ol_flags = 0;
    pkt->l2_len = sizeof(struct rte_ether_hdr);
    pkt->l3_len = sizeof(struct rte_ipv4_hdr);

    MsgDp* msg =  reinterpret_cast<MsgDp *>(udp_hdr + 1);

    int total_nodes_in_curr_level = 0;
    int index_shift = 0;  // for supporting dynamic relationships
    if (CONFIG::HEDGING::DYNAMIC_RELATIONSHIPS && ctx->proxy_num != 0) {
        int shift_for_current_node_id = ctx->proxy_num - ctx->level_info.starting_index;
        index_shift += shift_for_current_node_id;

        total_nodes_in_curr_level = ctx->level_info.ending_index - ctx->level_info.starting_index + 1;
        int shift_for_current_msg_id = msg->msg_id() % total_nodes_in_curr_level;

        if (true || msg->msg_id() % 2 == 1) {
            index_shift += shift_for_current_msg_id;
            index_shift = index_shift % total_nodes_in_curr_level;
        }

        index_shift *= ctx->m;  // so that we skip m for each shift
    }

    if (CONFIG::SIMULATE_TRADING && ctx->proxy_num == 0) {
        add_trade_information(msg, ctx);
    }

    int64_t curr_time = 0;
    if (CONFIG::LOSS_EXPERIMENT::EXP == false) curr_time = get_current_time();

    if (CONFIG::HEDGING::INJECT_FAULT
        && ctx->proxy_num >= CONFIG::HEDGING::FAULTY_NODE_LOW
        && ctx->proxy_num <= CONFIG::HEDGING::FAULTY_NODE_HIGH
        && msg->msg_id() > 300000 && msg->msg_id() < 360000) {
        std::this_thread::sleep_for(std::chrono::microseconds(CONFIG::HEDGING::FAULTY_SLEEP_MICROSECONDS));
        curr_time = get_current_time();
    }

    if (0 == ctx->proxy_num &&
        false == CONFIG::SENDER_PROXY_GENERATES_MESSAGES &&
        false == CONFIG::LOSS_EXPERIMENT::EXP) {
        msg->set_deadline(
            curr_time + ctx->holdrelease->get_owd_estimate_for_last_level()
            + CONFIG::HOLDRELEASE::EXTRA_HOLD_ADDED_TO_DEADLINE);
        msg->set_root_send_time(curr_time);

        // assert(false == CONFIG::HEDGING::MSG_HISTORY);
    }

    // bool is_parity_0_msg_id = 0 == (msg->msg_id() % CONFIG::HEDGING::PARITY);

    // Set the source MAC address
    struct rte_ether_addr new_src_addr = {
        .addr_bytes = {src_mac[0], src_mac[1], src_mac[2], src_mac[3],
                       src_mac[4], src_mac[5]}};
    rte_ether_addr_copy(&new_src_addr, &eth_hdr->src_addr);

    // Set the source IP address
    uint32_t new_src_ip = rte_be_to_cpu_32(src_ip);
    ip_hdr->src_addr = new_src_ip;

    udp_hdr->dgram_cksum = 0;

    int total_addrs_to_send_messages = ips_ports->size();
    if (CONFIG::HEDGING::DYNAMIC_RELATIONSHIPS && ctx->proxy_num != 0) {
        total_addrs_to_send_messages = ctx->m;

        if (CONFIG::HEDGING::SIBLING_BASED_HEDGING) {
            assert(CONFIG::HEDGING::SIBLING_HEDGING_INTENSITY <= 2);
            total_addrs_to_send_messages = (CONFIG::HEDGING::SIBLING_HEDGING_INTENSITY+1) * ctx->m;
        }
    }

    // if (false == CONFIG::HEDGING::DYNAMIC_RELATIONSHIPS &&
    //     true == CONFIG::HEDGING::PARITY_BASED_EXPERIMENT && false == is_redundant
    //     && true == is_parity_0_msg_id) {
    //     // For a non-redundant node and parity 0 message id, we will not send messages to hedge nodes
    //     // We make sure that redundant nodes are placed at the end of ips_ports, error prone,
    //     // need to assert that
    //     if (false == is_leaf_node) {
    //         total_addrs_to_send_messages = ips_ports->size() - ctx->redundancy_factor;
    //     }

    //     if (CONFIG::HEDGING::SIBLING_BASED_HEDGING && ctx->proxy_num > 0) {
    //         total_addrs_to_send_messages /= (CONFIG::HEDGING::SIBLING_HEDGING_INTENSITY + 1);
    //     }
    // }

    /**
     * Remove the ethernet and ip header (and udp header for logical recvrs thign)
    */
    auto pkt_w_udp_hdr = rte_pktmbuf_adj(
        pkt, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr));
    // assert(pkt_w_udp_hdr != NULL);

    for (int i = 0; i < total_addrs_to_send_messages; i++) {
        for (int j = 0; j < CONFIG::REQUEST_DUPLICATION::FACTOR; j++) {
            int k = i + index_shift;  // done for supporting dynamic relationships

            if (total_nodes_in_curr_level && ctx->proxy_num != 0) {
                k %= (total_nodes_in_curr_level * ctx->m);
            }

            // assert(k < ips_ports->size());

            /**
             * Clone the original packet
            */
            rte_mbuf* cloned_pkt = rte_pktmbuf_clone(pkt, clone_pool);
            // assert(cloned_pkt != NULL);

            /**
             * Create a new mbuf for ethernet and udp headers
            */
            struct rte_mbuf *hdr = rte_pktmbuf_alloc(header_pool);
            // assert(hdr != NULL);

            /**
             * Prepend hdr to cloned_pkt
            */
            hdr->next = cloned_pkt;

            /**
             * Update metadata of hdr
            */
            hdr->pkt_len = (uint16_t)(hdr->data_len + cloned_pkt->pkt_len);
            hdr->nb_segs = (uint8_t)(cloned_pkt->nb_segs + 1);

            /**
             * Write new headers
            */
            struct rte_ipv4_hdr *iphdr;
            struct rte_ether_hdr *ethdr;
            struct rte_udp_hdr *udphdr;

            // Set the UDP header
            udphdr = (struct rte_udp_hdr *)rte_pktmbuf_prepend(hdr, (uint16_t)sizeof(*udphdr));
            // assert(udphdr != NULL);

            udphdr->dgram_len = udp_len;
            udphdr->dst_port = ips_ports->at(k).second;
            udphdr->src_port = DPDK_UDP_DST_PORT;
            udphdr->dgram_cksum = 0;

            // Set the IP header
            iphdr = (struct rte_ipv4_hdr *)rte_pktmbuf_prepend(hdr, (uint16_t)sizeof(*iphdr));
            // assert(iphdr != NULL);

            iphdr->dst_addr = rte_be_to_cpu_32(ips_ports->at(k).first);
            iphdr->src_addr = new_src_ip;
            iphdr->version_ihl = IP_VHL_DEF;
            iphdr->type_of_service = 0;
            iphdr->total_length = ip_pkt_len;
            iphdr->packet_id = 0;
            iphdr->fragment_offset = 0;
            iphdr->time_to_live = IP_DEFTTL;
            iphdr->next_proto_id = IPPROTO_UDP;
            iphdr->hdr_checksum = 0;
            iphdr->hdr_checksum = rte_ipv4_cksum(iphdr);

            // Set the Ethernet header
            ethdr = (struct rte_ether_hdr *)rte_pktmbuf_prepend(hdr, (uint16_t)sizeof(*ethdr));
            // assert(ethdr != NULL);

            ethdr->ether_type = rte_be_to_cpu_16(RTE_ETHER_TYPE_IPV4);
            struct rte_ether_addr new_dst_addr = {
                .addr_bytes = {(*macs)[k][0], (*macs)[k][1], (*macs)[k][2],
                            (*macs)[k][3], (*macs)[k][4], (*macs)[k][5]}};
            rte_ether_addr_copy(&new_dst_addr, &ethdr->dst_addr);
            rte_ether_addr_copy(&new_src_addr, &ethdr->src_addr);

            // pretty_print_packet(hdr);

            int nb_tx = 0;
            int retries = 50000;
            while (retries--) {
                nb_tx = rte_eth_tx_burst(PORT_ID, tx_queue_id, &hdr, 1);
                if (nb_tx == 1) break;
                rte_eth_tx_done_cleanup(PORT_ID, tx_queue_id, 0);
            }

            if (nb_tx < 1) {
                rte_pktmbuf_free(hdr);
                if (CONFIG::LOGGING) Logging::FAILURES.fetch_add(1);
            } else {
                if (CONFIG::LOGGING) Logging::TRANSMITTED.fetch_add(1);
            }
        }
    }

    rte_pktmbuf_free(pkt);
}

// Worker thread function
int worker_thread(void* arg) {
    struct Workerthreadargs *args = (struct Workerthreadargs *)arg;
    struct rte_mbuf *bufs[BURST_SIZE * 4];
    unsigned lcore_id = rte_lcore_id();

    std::string clone_pool_name = "CLONE_POOL" + std::to_string(args->tx_queue_id);
    std::string header_pool_name = "HEADER_POOL" + std::to_string(args->tx_queue_id);
    double total_mem_to_allocate = (1.0 * NUM_MBUFS / NUM_WORKERS) * args->ctx->m;
    if (args->is_redundant) {
        if (CONFIG::HEDGING::MAX_DOWNSTREAMS_FACTOR == -1) CONFIG::HEDGING::MAX_DOWNSTREAMS_FACTOR = args->ctx->m;
        total_mem_to_allocate *= CONFIG::HEDGING::MAX_DOWNSTREAMS_FACTOR;
    } else if (CONFIG::HEDGING::SIBLING_BASED_HEDGING) {
        total_mem_to_allocate *= 2;
    }

    total_mem_to_allocate *= CONFIG::REQUEST_DUPLICATION::FACTOR;

    std::clog << "Attempting allocation of "
    << clone_pool_name << " at " << rte_socket_id() << " worth "
    << total_mem_to_allocate*0 << "B" << std::endl;

    struct rte_mempool *clone_pool = rte_pktmbuf_pool_create(
        clone_pool_name.c_str(), total_mem_to_allocate * 2 /* Number of segments*/ * 2, MBUF_CACHE_SIZE,
        0, 0, rte_socket_id());

    std::clog << "Attempting allocation of "
    << header_pool_name << " at " << rte_socket_id() << " worth "
    << total_mem_to_allocate*HEADERS_MBUF_SIZE << "B" << std::endl;

    struct rte_mempool *header_pool = rte_pktmbuf_pool_create(
        header_pool_name.c_str(), total_mem_to_allocate, MBUF_CACHE_SIZE,
        0, HEADERS_MBUF_SIZE, rte_socket_id());

    if (clone_pool == NULL) {
        std::cerr << rte_errno << " Could not allocate " << clone_pool_name << std::endl;
        exit(1);
    }

    if (header_pool == NULL) {
        std::cerr << rte_errno << " Could not allocate " << header_pool_name << std::endl;
        exit(1);
    }

    args->clone_pool = clone_pool;
    args->header_pool = header_pool;

    std::cout << "Worker on " << lcore_id << " with tx queue " << args->tx_queue_id << std::endl;

    while (true) {
        const uint16_t nb_rx = rte_ring_dequeue_burst(
            packetQueue, reinterpret_cast<void **>(bufs), BURST_SIZE*4, NULL);

        if (nb_rx == 0) continue;

        for (int i = 0; i < nb_rx; i++) {
            if (CONFIG::LOGGING) Logging::DEQUEUED.fetch_add(1);
            prepare_and_transmit(
                args->ctx, bufs[i], args->fast_ips_ports, args->fast_macs, args->src_ip, args->src_mac,
                args->clone_pool, args->header_pool, args->is_redundant, args->is_leaf_node,
                args->tx_queue_id);
        }

        // if (args->is_redundant) args->count += nb_rx;
        // args->logs->at(args->tx_queue_id) += nb_rx;
    }

    return 0;
}

// recieve packets
void receive_and_transmit_packets(
    Context *ctx,
    std::vector<std::string> ips,
    std::unordered_map<std::string, std::string> ip_to_mac, std::string src_ip,
    std::string src_mac, std::string mode) {
    uint16_t port;
    char *cstr_ip = &src_ip[0];
    char *cstr_mac = &src_mac[0];
    uint32_t ip = string_to_ip(cstr_ip);
    std::vector<uint8_t> mac = {0, 0, 0, 0, 0, 0};

    string_to_mac(cstr_mac, mac);

    std::vector<std::pair<uint32_t, rte_be16_t>> fast_ips_ports;
    std::vector<std::vector<uint8_t>> fast_macs;

    if (CONFIG::HEDGING::DYNAMIC_RELATIONSHIPS && ctx->proxy_num != 0) {
        for (int i = ctx->level_info.starting_index; i <= ctx->level_info.ending_index; i++) {
            auto [a, b] = format_ips_and_macs_for_dpdk(ctx->ips_per_sibling[i], ip_to_mac);
            fast_ips_ports.insert(fast_ips_ports.end(), a.begin(), a.end());
            fast_macs.insert(fast_macs.end(), b.begin(), b.end());
        }

        assert(fast_ips_ports.size() == ctx->ips_per_sibling.size() * ctx->m);
        assert(fast_macs.size() == ctx->ips_per_sibling.size() * ctx->m);
    } else {
        auto [a, b] = format_ips_and_macs_for_dpdk(ips, ip_to_mac);
        fast_ips_ports = a;
        fast_macs = b;
    }

    auto is_redundant = is_a_redundant_node(ctx->proxy_num, ctx->total_proxies_without_redundant_nodes);
    std::vector<std::thread> hedging_threads;
    if (is_redundant) {
        hedging_threads.emplace_back(std::thread([ctx, ips, ip_to_mac, mode] {
            ctx->hedging->periodically_prepare_reordered_children(ctx->proxy_num, mode, ips, ip_to_mac);
        }));
    }

    auto is_leaf_node = is_a_leaf_node(ctx->proxy_num, ctx->total_proxies_without_redundant_nodes, ctx->m);

    printf("\nCore %u receiving packets. [Ctrl+C to quit]\n", rte_lcore_id());

    std::set<int64_t> seen_msg_ids;
    int count = 0;

    enqueue_failures = 0;
    std::vector<int> processed_by_worker(NUM_WORKERS, 0);
    if (true == CONFIG::LOGGING) {
        // yeah yeah it should not be put in the hedging threads
        hedging_threads.emplace_back(std::thread([&processed_by_worker] {
            while (true) {
                std::clog << "RECVD " << Logging::RECVD << " TRANSMITTED "
                << Logging::TRANSMITTED << " FAILURES " << Logging::FAILURES <<
                " DEQUEUED " << Logging::DEQUEUED << " enqueue_failures: "
                << enqueue_failures << std::endl;
                for (auto ele : processed_by_worker) {
                    std::clog << ele << " ";
                }
                std::clog << std::endl;

                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }));
    }

    cpu_set_t cpuset;
    int main_core = rte_get_main_lcore();
    CPU_ZERO(&cpuset);
    CPU_SET(main_core, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    std::cout << "Launching " << NUM_WORKERS << " worker threads. " << std::endl;
    // Launch worker thread(s)
    for (int i = 0; i < NUM_WORKERS; ++i) {
        Workerthreadargs* args = new Workerthreadargs{
            .clone_pool = NULL,
            .header_pool = NULL,
            .count = count,
            .ctx = ctx,
            .fast_ips_ports = fast_ips_ports,
            .fast_macs = fast_macs,
            .is_leaf_node = is_leaf_node,
            .is_redundant = is_redundant,
            .src_ip = ip,
            .src_mac = mac,
            .tx_queue_id = i,
            .logs = &processed_by_worker
        };

        rte_eal_remote_launch(worker_thread, args, i + 1);
    }

    // isolate_main_core(main_core);
    int start_of_udphdr_in_pkt = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr);

    for (;;) {
        RTE_ETH_FOREACH_DEV(port) {
            struct rte_mbuf *bufs[BURST_SIZE];
            const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);

            if (unlikely(nb_rx == 0))
                continue;

            for (int i = 0; i < nb_rx; i++) {
                if (rte_pktmbuf_data_len(bufs[i]) < DATA_OFFSET_IN_PKT + sizeof(MsgDp)) {
                    continue;
                }

                struct rte_udp_hdr *udp_hdr = rte_pktmbuf_mtod_offset(bufs[i], rte_udp_hdr *, start_of_udphdr_in_pkt);
                if (udp_hdr->src_port != DPDK_UDP_DST_PORT) {
                    rte_pktmbuf_free(bufs[i]);
                    continue;
                }

                MsgDp *msg =  reinterpret_cast<MsgDp *>(udp_hdr + 1);
                if (seen_msg_ids.find(msg->msg_id()) != seen_msg_ids.end()) {
                    rte_pktmbuf_free(bufs[i]);
                } else {
                    seen_msg_ids.insert(msg->msg_id());
                    if (CONFIG::LOGGING) Logging::RECVD.fetch_add(1);
                    if (0 != rte_ring_enqueue(packetQueue, reinterpret_cast<void *>(bufs[i])))
                        enqueue_failures += 1;
                }
            }
        }
    }

    for (auto& thr : hedging_threads) {
        thr.join();
    }
}

// TODO(haseeb): refactor both wait_command_prepare_*
void wait_command_prepare_and_transmit_packets_helper_constant_rate_busywait(
    int experiment_time_in_seconds, int rate, int64_t starting_msg_id,
    Context *ctx, std::vector<std::pair<uint32_t, rte_be16_t>> &fast_ips_ports,
    std::vector<std::vector<uint8_t>>& fast_macs, uint32_t &src_ip,
    std::vector<uint8_t>& src_mac) {

    std::cout << "Received a startmsg: "
              << experiment_time_in_seconds << " " << rate << " " << starting_msg_id << std::endl;

    std::unique_ptr<Mcastmessages> mcastmessages = std::make_unique<Mcastmessages>(starting_msg_id);

    double interarrival_time = 1.0 / rate;
    double curr_relative_time = 0;
    uint64_t total_messages = 0;
    int i = 0;

    // struct rte_mbuf *bufs[BURST_SIZE];
    enqueue_failures = 0;

    MsgDp msg = mcastmessages->get_msg(i);

    int batch_size = BURST_SIZE;
    auto start_time = std::chrono::steady_clock::now();

    while (curr_relative_time < experiment_time_in_seconds) {
        for (int b = 0; b < batch_size; b++) {
            if (CONFIG::LOSS_EXPERIMENT::SIMULATE_LOSSES) {
                if (i % 10000 == 0) i += 1;
            }

            struct rte_mbuf *pkt = rte_pktmbuf_alloc(mbuf_pool);
            if (pkt == NULL) { printf("trouble at rte_pktmbuf_alloc\n"); return; }
            pkt->data_len = sizeof(MsgDp) + sizeof(rte_ipv4_hdr) + sizeof(rte_udp_hdr) + sizeof(rte_ether_hdr);
            pkt->pkt_len = pkt->data_len;

            struct rte_ipv4_hdr  pkt_ip_hdr;
            struct rte_udp_hdr pkt_udp_hdr;
            struct rte_ether_hdr eth_hdr;
            eth_hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
            setup_pkt_udp_ip_headers(&pkt_ip_hdr, &pkt_udp_hdr, sizeof(MsgDp));

            rte_memcpy(rte_pktmbuf_mtod_offset(pkt, char *, 0),
                    &eth_hdr, sizeof(eth_hdr));
            rte_memcpy(rte_pktmbuf_mtod_offset(pkt, char *, sizeof(struct rte_ether_hdr)),
                    &pkt_ip_hdr, sizeof(pkt_ip_hdr));
            rte_memcpy(rte_pktmbuf_mtod_offset(pkt, char *,
                    sizeof(struct rte_ether_hdr)+sizeof(struct rte_ipv4_hdr)),
                    &pkt_udp_hdr, sizeof(pkt_udp_hdr));
            pkt->data_len = sizeof(MsgDp) + sizeof(rte_ipv4_hdr) + sizeof(rte_udp_hdr) + sizeof(rte_ether_hdr);

            mcastmessages->get_msg(msg, i);

            rte_memcpy(rte_pktmbuf_mtod_offset(pkt, char *,
                       sizeof(struct rte_ether_hdr)+sizeof(struct rte_ipv4_hdr)+sizeof(struct rte_udp_hdr)),
                       &msg, sizeof(MsgDp));

            pkt->nb_segs = 1;
            pkt->ol_flags = 0;

            if (0 != rte_ring_enqueue(packetQueue, reinterpret_cast<void *>(pkt))) {
                enqueue_failures += 1;
                rte_pktmbuf_free(pkt);
            }

            i++;
            total_messages++;
        }

        // Appropriate pacing to achieve desired rate
        auto target_time = start_time + std::chrono::duration<double>(interarrival_time * total_messages);
        while (std::chrono::steady_clock::now() < target_time) { _mm_pause(); }
        curr_relative_time = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_time).count();
        // if (total_messages >= rate * experiment_time_in_seconds) break;
    }

    rte_eth_stats stats;
    rte_eth_stats_get(PORT_ID, &stats);
    printf("TX: Packets: %lu, Errors: %lu, Bytes: %lu\n",
           stats.opackets, stats.oerrors, stats.obytes);

    std::clog << "Msg size: " << sizeof(MsgDp) << std::endl;
    std::clog << "Done generating [ " << total_messages << " ] orders!" << std::endl;
    std::clog << "Next Message ID: " << starting_msg_id + i << std::endl;
}

// prepare packets
void wait_command_prepare_and_transmit_packets(
    Context *ctx,
    std::vector<std::string> ips,
    std::unordered_map<std::string, std::string> ip_to_mac, std::string src_ip,
    std::string src_mac) {
    uint16_t port;
    char *cstr_ip = &src_ip[0];
    char *cstr_mac = &src_mac[0];
    uint32_t ip = string_to_ip(cstr_ip);
    std::vector<uint8_t> mac = {0, 0, 0, 0, 0, 0};

    string_to_mac(cstr_mac, mac);

    auto [fast_ips_ports, fast_macs] = format_ips_and_macs_for_dpdk(ips, ip_to_mac);

    printf("\nCore %u receiving packets. [Ctrl+C to quit]\n", rte_lcore_id());

    int start_of_udphdr_in_pkt = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr);

    std::vector<std::thread> threads;
    if (true == CONFIG::LOGGING) {
        threads.emplace_back(std::thread([] {
            while (true) {
                std::clog << " enqueue_failures: " << enqueue_failures << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }));
    }

    cpu_set_t cpuset;
    int main_core = rte_get_main_lcore();
    CPU_ZERO(&cpuset);
    CPU_SET(main_core, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    // isolate_main_core(main_core);

    std::cout << "Launching " << NUM_WORKERS << " worker threads. " << std::endl;
    // Launch worker thread(s)
    for (int i = 0; i < NUM_WORKERS; ++i) {
        Workerthreadargs* args = new Workerthreadargs{
            .clone_pool = NULL,
            .header_pool = NULL,
            .count = 0,
            .ctx = ctx,
            .fast_ips_ports = fast_ips_ports,
            .fast_macs = fast_macs,
            .is_leaf_node = false,
            .is_redundant = false,
            .src_ip = ip,
            .src_mac = mac,
            .tx_queue_id = i,
            .logs = NULL
        };

        rte_eal_remote_launch(worker_thread, args, i + 1);
    }

    for (;;) {
        RTE_ETH_FOREACH_DEV(port) {
            struct rte_mbuf *bufs[BURST_SIZE];
            const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);

            if (unlikely(nb_rx == 0))
                continue;

            for (int i = 0; i < nb_rx; i++) {
                if (rte_pktmbuf_data_len(bufs[i]) < DATA_OFFSET_IN_PKT + sizeof(StartMsgsDp)) {
                    continue;
                }

                struct rte_udp_hdr *udp_hdr = rte_pktmbuf_mtod_offset(bufs[i], rte_udp_hdr *, start_of_udphdr_in_pkt);
                if (udp_hdr->src_port != DPDK_UDP_DST_PORT) {
                    rte_pktmbuf_free(bufs[i]);
                    continue;
                }

                StartMsgsDp *msg =  reinterpret_cast<StartMsgsDp *>(udp_hdr + 1);
                wait_command_prepare_and_transmit_packets_helper_constant_rate_busywait(
                    msg->experiment_time_in_seconds, msg->msg_rate, msg->starting_msg_id,
                    ctx, fast_ips_ports, fast_macs, ip, mac);
                rte_pktmbuf_free(bufs[i]);
            }
        }
    }

    for (auto& thr : threads) {
        if (thr.joinable()) thr.join();
    }
}

void exit_stats(int sig) {
    printf("Caught signal %d\n", sig);
    exit(0);
}

void dpdk_proxy(Context *ctx, std::string mode, std::vector<std::string> ips) {
    std::cout << "PID: " << getpid() << std::endl;
    std::cout << "If crash occurs, core will be at /tmp/core.multicast_proxy." << getpid() << std::endl;
    unsigned nb_ports;
    uint16_t portid;
    std::string cloud {CONFIG::CLOUD};

    if (cloud == "gcp") {
        // currently c2d-highcpu-8 only allows 2 TX_QUEUES
        NUM_WORKERS = 2;
    }

    nb_ports = rte_eth_dev_count_avail();
    std::clog << "rte_eth_dev_count_avail()= " << nb_ports << std::endl;

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL", NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());  // gvnic requires RTE_MBUF_DEFAULT_BUF_SIZE

    packetQueue = rte_ring_create("PacketQueue", 131072, rte_socket_id(), RING_F_SP_ENQ | RING_F_MC_HTS_DEQ);
    if (packetQueue == NULL)
        rte_panic("Cannot create packet queue\n");

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initialize all ports. */
    RTE_ETH_FOREACH_DEV(portid)
    if (port_init(portid, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "cannot init port");

    rte_log_set_global_level(RTE_LOG_DEBUG);
    signal(SIGINT, exit_stats);

    auto ip_to_mac_mapping = get_ip_to_mac_mapping(ips);

    if (CONFIG::HEDGING::DYNAMIC_RELATIONSHIPS && ctx->proxy_num != 0) {
        std::vector<std::string> all_level_ips;
        for (int i = ctx->level_info.starting_index; i <= ctx->level_info.ending_index; i++) {
            auto& tmp = ctx->ips_per_sibling[i];
            all_level_ips.insert(all_level_ips.end(), tmp.begin(), tmp.end());
        }
        ip_to_mac_mapping = get_ip_to_mac_mapping(all_level_ips);
    }

    auto src_ip = read_file(ip_file);
    auto src_mac = read_file(mac_file);

    std::cout << "Here are the downstreams" << std::endl;
    for (auto ele : ips) {
        std::cout << ele << ", ";
    }
    std::cout << std::endl;

    if (ctx->proxy_num == 0 && CONFIG::SENDER_PROXY_GENERATES_MESSAGES) {
        wait_command_prepare_and_transmit_packets(ctx, ips, ip_to_mac_mapping, src_ip, src_mac);
    } else {
        receive_and_transmit_packets(ctx, ips, ip_to_mac_mapping, src_ip, src_mac, mode);
    }
}


#endif  // SRC_MULTICAST_PROXY_DPDK_PROXY_HPP_
