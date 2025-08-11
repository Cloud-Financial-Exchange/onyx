// Copyright [2023] NYU

#ifndef SRC_MULTICAST_RECEIVER_DPDK_RECEIVER_HPP_
#define SRC_MULTICAST_RECEIVER_DPDK_RECEIVER_HPP_

#include <string>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "./../utils/common.hpp"
#include "./../utils/holdrelease.hpp"
#include "./../utils/trading.hpp"
#include "./utils.hpp"

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {.max_lro_pkt_size = RTE_ETHER_MAX_LEN},
    .txmode = {
        .offloads = RTE_ETH_TX_OFFLOAD_IPV4_CKSUM,
    },
};

constexpr rte_be16_t DPDK_UDP_DST_PORT = 0xce85;
int PORT_ID = 0;
const int NUM_WORKERS = 1;  // loss exp working requires it to be 1
struct rte_ring *packetQueue;
const int START_OF_UDPHDR_IN_PKT = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr);

/*
 * TODO(haseeb): refactor
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int port_init(uint16_t port, struct rte_mempool *mbuf_pool) {
    struct rte_eth_conf port_conf = port_conf_default;
    if (CONFIG::CLOUD == "gcp")
        port_conf.txmode = {};

    const uint16_t rx_rings = 1, tx_rings = 1;
    uint16_t nb_rxd = RX_RING_SIZE;
    uint16_t nb_txd = TX_RING_SIZE;
    int retval;
    uint16_t q;

    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf txconf;

    if (!rte_eth_dev_is_valid_port(port))
        return -1;

    rte_eth_dev_info_get(port, &dev_info);
    // if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) {
    //     port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
    // }

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

    if (CONFIG::CLOUD == "gcp") {
        // GCP's gvNIC limitations
        txconf.tx_free_thresh = nb_txd - 4;
    } else {
        txconf.tx_free_thresh = TX_RING_SIZE/2;
    }

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

void exit_stats(int sig) {
    printf("Caught signal %d\n", sig);
    exit(0);
}

struct Workerthreadargs {
    struct rte_mempool *clone_pool;
    int worker_id;
    int32_t recipient_id;
    std::string mode;
    Holdrelease *holdrelease;
    StatsDp* stats;
    int first_worker_lcore;
    std::unordered_map<uint16_t, int32_t> port_id_map;
    Tradingmp* tradingmp;
};

Tradedatapoint retrieve_trade_point(MsgDp* msg) {
    Tradedatapoint tp;
    memcpy(&tp, msg->_data, sizeof(Tradedatapoint));
    tp.show();
    return tp;
}

int worker_thread(void* arg) {
    struct Workerthreadargs *args = (struct Workerthreadargs *)arg;
    struct rte_mbuf *bufs[BURST_SIZE];
    args->stats->recipient_id = args->recipient_id;

    while (true) {
        const uint16_t nb_rx = rte_ring_dequeue_burst(
            packetQueue, reinterpret_cast<void **>(bufs), BURST_SIZE, NULL);

        if (nb_rx == 0) continue;

        for (int i = 0; i < nb_rx; i++) {
            if (CONFIG::LOGGING) Logging::DEQUEUED.fetch_add(1);

            struct rte_udp_hdr *udp_hdr = rte_pktmbuf_mtod_offset(bufs[i], rte_udp_hdr *, START_OF_UDPHDR_IN_PKT);
            MsgDp* msg = rte_pktmbuf_mtod_offset(bufs[i], MsgDp*, DATA_OFFSET_IN_PKT);

            handle_message_and_log_it(
                msg, msg->recv_time(), args->mode, args->worker_id,
                args->holdrelease, args->stats, args->port_id_map[udp_hdr->dst_port],
                args->worker_id == args->first_worker_lcore);

            rte_pktmbuf_free(bufs[i]);
        }
    }
}

void sleep_and_process_trade(Tradedatapoint tp, int64_t wake_up, Tradingmp* tradingmp) {
    int64_t received_before = (wake_up-1) > get_current_time() ? 1 : 0;
    if (CONFIG::HOLDRELEASE::HOLDRELEASE_MODE != CONFIG::HOLDRELEASE::NONE) {
        std::chrono::system_clock::time_point target_time{std::chrono::microseconds{wake_up}};
        while (get_current_time() < wake_up) {}  // busy sleep works better
        // std::this_thread::sleep_until(target_time);
    }

    tradingmp->process_data_point(tp, received_before);
}

void receive_and_log_packets(int recipient_id, std::vector<StatsDp>& all_stats, std::string mode,
    int total_proxies_without_redundant_nodes, int redundancy_factor, int m, int port,
    std::unordered_map<uint16_t, int32_t> port_id_map) {

    cpu_set_t cpuset;
    int main_core = rte_get_main_lcore();
    CPU_ZERO(&cpuset);
    CPU_SET(main_core, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    auto holdrelease = new Holdrelease(recipient_id, total_proxies_without_redundant_nodes,
        redundancy_factor, m, Role::RECEIVER, 1);
    auto holdrelease_send_thread = std::thread([holdrelease] {holdrelease->send_estimates();});
    auto tradingmp = new Tradingmp(recipient_id);

    int total_launched = 0;
    int curr_lcore = 0;
    int first_worker_lcore = 0;

    std::vector<std::thread> threads;

    while (total_launched < NUM_WORKERS) {
        if ((unsigned int)curr_lcore == rte_get_main_lcore()) {
            curr_lcore++;
            continue;
        }

        if (first_worker_lcore == 0) first_worker_lcore = curr_lcore;

        Workerthreadargs* args = new Workerthreadargs{
            .clone_pool = NULL,
            .worker_id = curr_lcore,
            .recipient_id = recipient_id,
            .mode = mode,
            .holdrelease = holdrelease,
            .stats = &all_stats[total_launched],
            .first_worker_lcore = first_worker_lcore,
            .port_id_map = port_id_map
        };



        int e = rte_eal_remote_launch(worker_thread, args, args->worker_id);
        if (e != 0) {
            std::cerr << "Remote launch error: " << e << " on: " << args->worker_id << std::endl;
        }

        curr_lcore++;
        total_launched++;
    }

    // threads.emplace_back(std::thread([&all_stats]{
    //     record_lost_messages(all_stats);  // too heavy, puts backpressure
    // }));

    int enqueue_failures = 0;

    if (true == CONFIG::LOGGING) {
        threads.emplace_back(std::thread([&enqueue_failures] {
            while (true) {
                std::clog << "RECVD " << Logging::RECVD << " DEQUEUED " << Logging::DEQUEUED
                << " enqueue_failures: " << enqueue_failures
                << " reordered: " << (100.0 * Logging::REORDERED) / Logging::RECVD << " %"<< std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }));
    }

    // isolate_main_core(main_core);
    std::unordered_map<uint16_t, std::set<int64_t>> seen_msg_ids;
    seen_msg_ids.reserve(100);

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

                struct rte_udp_hdr *udp_hdr = rte_pktmbuf_mtod_offset(bufs[i], rte_udp_hdr *, START_OF_UDPHDR_IN_PKT);
                if (udp_hdr->src_port != DPDK_UDP_DST_PORT) {
                    rte_pktmbuf_free(bufs[i]);
                    continue;
                }

                MsgDp* msg = rte_pktmbuf_mtod_offset(bufs[i], MsgDp*, DATA_OFFSET_IN_PKT);

                if (!CONFIG::LOSS_EXPERIMENT::EXP &&
                    seen_msg_ids[udp_hdr->dst_port].find(msg->msg_id()) != seen_msg_ids[udp_hdr->dst_port].end()) {
                    rte_pktmbuf_free(bufs[i]);
                    continue;
                } else {
                    if (!CONFIG::LOSS_EXPERIMENT::EXP) msg->set_recv_time(get_current_time());

                    if (CONFIG::SIMULATE_TRADING && msg->msg_id() > (CONFIG::MSG_RATE * 10)) {
                        auto tp = retrieve_trade_point(msg);
                        sleep_and_process_trade(tp, msg->deadline(), tradingmp);
                    }

                    if (CONFIG::LOGGING) Logging::RECVD.fetch_add(1);
                    if (0 != rte_ring_enqueue(packetQueue, reinterpret_cast<void *>(bufs[i]))) enqueue_failures += 1;
                    if (!CONFIG::LOSS_EXPERIMENT::EXP)
                        seen_msg_ids[udp_hdr->dst_port].insert(msg->msg_id());

                    if (!CONFIG::LOSS_EXPERIMENT::EXP && msg->_history > 0 &&
                    seen_msg_ids[udp_hdr->dst_port].find(msg->msg_id()-1) == seen_msg_ids[udp_hdr->dst_port].end()) {
                        seen_msg_ids[udp_hdr->dst_port].insert(msg->msg_id() - 1);
                        if (CONFIG::LOGGING) Logging::REORDERED.fetch_add(1);
                        if (CONFIG::LOGGING) Logging::RECVD.fetch_add(1);
                    }
                }
            }
        }
    }

    for (auto& thr : threads) {
        if (thr.joinable()) thr.join();
    }
}

int dpdk_receiver(int32_t recipient_id, int32_t duplication_factor, std::string mode,
    int32_t m, int32_t redundancy_factor, int32_t total_proxies_without_redundant_nodes) {
    // assert(duplication_factor == 1);  // we do not support logical receivers when using DPDK receiver
    std::clog << "------- Starting DPDK receiver -------" << std::endl;

    struct rte_mempool *mbuf_pool;
    unsigned nb_ports;
    uint16_t portid;

    nb_ports = rte_eth_dev_count_avail();
    std::clog << "rte_eth_dev_count_avail()= " << nb_ports << std::endl;

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL", NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    packetQueue = rte_ring_create("PacketQueue", 131072, rte_socket_id(), RING_F_SP_ENQ | RING_F_MC_HTS_DEQ);
    if (packetQueue == NULL)
        rte_panic("Cannot create packet queue\n");

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initialize all ports. */
    RTE_ETH_FOREACH_DEV(portid)
    if (port_init(portid, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "cannot init port");

    signal(SIGINT, exit_stats);

    all_stats.resize(NUM_WORKERS);  // removed +1

    auto [recipient_ip, recipient_ports] = get_logical_receiver_ip_ports_for_physical_receiver(
        recipient_id, duplication_factor);

    std::unordered_map<uint16_t, int32_t> port_id_map;  // port to recipientid (for logical receivers purposes)
    for (size_t i = 0; i < recipient_ports.size(); i++) {
        port_id_map[rte_cpu_to_be_16(recipient_ports[i])] = recipient_id + i;
    }

    receive_and_log_packets(recipient_id, all_stats, mode,
        total_proxies_without_redundant_nodes, redundancy_factor, m, 0, port_id_map);

    return 0;
}


#endif  // SRC_MULTICAST_RECEIVER_DPDK_RECEIVER_HPP_
