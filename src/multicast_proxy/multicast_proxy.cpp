// Copyright [2023] NYU

#include "./../utils/common.hpp"
#include "./dpdk_proxy.hpp"
#include "./iouring_proxy.hpp"
#include "./socket_proxy.hpp"
#include "./hedging.hpp"
#include "./../utils/holdrelease.hpp"
#include "./utils.hpp"
#include "./../utils/trading.hpp"

void test_memory() {
    /* Creates a new mempool in memory to hold the mbufs. */
    int total_mem_to_allocate = NUM_MBUFS * 12 * 12;
    auto mbufs = rte_pktmbuf_pool_create(
        "MBUF_POOL", total_mem_to_allocate, MBUF_CACHE_SIZE, 0,
        MBUF_SIZE, rte_socket_id());

    if (mbufs == NULL) {
        std::cerr << rte_errno << " Could not allocate "
        << total_mem_to_allocate*MBUF_SIZE << "B" << std::endl;
        exit(1);
    }
    std::cout << "Allocated." << std::endl;
}

std::vector<std::string> randomize_ips(std::vector<std::string> ips, int proxy_num,
    int branching_factor, int redundancy_factor, int total_proxies_without_redundant_nodes) {
    std::random_device rd;
    std::mt19937 g(rd());
    if (false == is_a_leaf_node(proxy_num, total_proxies_without_redundant_nodes, branching_factor)) {
        // ips contain ips of proxies. Any redundant nodes, if present, must be at the end of
        // the list
        std::shuffle(ips.begin(), ips.begin() + (ips.size()-redundancy_factor), g);
    } else {
        std::shuffle(ips.begin(), ips.end(), g);
    }

    return ips;
}

CONFIG::HEDGING::HEDGING_MODE_TYPE get_hedging_mode_for_current_node(
    int proxy_num, int total_proxies_without_redundant_nodes, bool is_redundant) {
    CONFIG::HEDGING::HEDGING_MODE_TYPE hedging_mode = CONFIG::HEDGING::HEDGING_MODE;
    if (is_redundant && CONFIG::HEDGING::MULTIPLE_HEDGING_MODES) {
        int redundant_node_index = get_index_of_redundant_node_among_redundant_nodes(
            proxy_num, total_proxies_without_redundant_nodes);

        if (redundant_node_index%2 == 0) {
            hedging_mode = CONFIG::HEDGING::HEDGING_MODE_1;
        } else {
            hedging_mode = CONFIG::HEDGING::HEDGING_MODE_2;
        }

        std::clog << "I am HEDGING_MODE_TYPE " << hedging_mode << std::endl;
    }
    return hedging_mode;
}

void proxy(
    int proxy_num, int branching_factor,
    std::string mode, int redundancy_factor, int total_proxies_without_redundant_nodes) {
    while (status != Status::RUNNING) {
        sleep(1);
    }

    auto level_info = get_level_info(proxy_num, branching_factor);
    std::unordered_map<int, std::vector<std::string>> ips_per_sibling;
    for (int i = level_info.starting_index; i <= level_info.ending_index; i++) {
        ips_per_sibling[i] = get_ips_to_send_messages(i, branching_factor, mode, false,
            redundancy_factor, total_proxies_without_redundant_nodes);
    }

    auto ips = get_ips_to_send_messages(proxy_num, branching_factor, mode, false,
        redundancy_factor, total_proxies_without_redundant_nodes);

    if (proxy_num != 0) {
        ips = extend_ips_for_sibling_hedging(
            ips, proxy_num, branching_factor, total_proxies_without_redundant_nodes, redundancy_factor, mode, false);
    }

    bool is_redundant = is_a_redundant_node(proxy_num, total_proxies_without_redundant_nodes);

    auto hedging_mode = get_hedging_mode_for_current_node(
        proxy_num, total_proxies_without_redundant_nodes, is_redundant);

    auto hedging = new Hedging(
        proxy_num, total_proxies_without_redundant_nodes, redundancy_factor,
        branching_factor, hedging_mode);

    std::thread hedging_thread;
    if (is_redundant) {
        hedging_thread = std::thread([hedging] {hedging->collect_hints();});
    } else if (proxy_num > 0) {
        hedging_thread = std::thread([hedging] {hedging->send_hints();});
    }

    auto holdrelease = new Holdrelease(
        proxy_num, total_proxies_without_redundant_nodes, redundancy_factor, branching_factor, Role::PROXY, 1);
    auto holdrelease_recv_thread = std::thread([holdrelease] {holdrelease->recv_estimates();});
    auto holdrelease_send_thread = std::thread([holdrelease] {holdrelease->send_estimates();});

    auto trading = new Tradingmatchingengine(proxy_num);
    auto trading_info_thread = std::thread([trading] {trading->show_balance();});
    auto trading_orders_thread = std::thread([trading] {trading->listen_for_orders();});
    auto trading_snapshot_thread = std::thread([trading] {trading->periodic_snapshot();});

    auto children_indices = get_children_indices(
        proxy_num, branching_factor, redundancy_factor, total_proxies_without_redundant_nodes, false, mode);

    auto ctx = new Context(
        proxy_num, branching_factor,
        total_proxies_without_redundant_nodes,
        redundancy_factor, hedging, holdrelease, trading, level_info, ips_per_sibling, children_indices);

    if (mode == "socket" || (proxy_num == 0 && CONFIG::ENFORCE_SOCKET_SENDER)) {
        std::clog << "Going into Socket Mode" << std::endl;
        socket_proxy(ctx, ips);
    } else if (mode == "iouring") {
        std::clog << "Going into io-uring Mode" << std::endl;
        iouring_proxy(ctx, mode, ips);
    } else if (mode == "dpdk") {
        std::clog << "Going into DPDK Mode" << std::endl;
        dpdk_proxy(ctx, mode, ips);
    }

    // Ahhh should just make a vec now, todo(haseeb)
    if (hedging_thread.joinable()) hedging_thread.join();
    if (holdrelease_recv_thread.joinable()) holdrelease_recv_thread.join();
    if (holdrelease_send_thread.joinable()) holdrelease_send_thread.join();
    if (trading_info_thread.joinable()) trading_info_thread.join();
    if (trading_orders_thread.joinable()) trading_orders_thread.join();
    if (trading_snapshot_thread.joinable()) trading_snapshot_thread.join();
}


int main(int argc, char *argv[]) {
    if (argc < 6) {
        std::cerr << "Arguments needed: proxy_num, branching_factor, mode, redundancy factor"
                  << std::endl;
        exit(1);
    }

    int proxy_num = std::stoi(argv[1]);
    int branching_factor = std::stoi(argv[2]);
    std::string mode = argv[3];
    int redundancy_factor = std::stoi(argv[4]);
    int total_proxies_without_redundant_nodes = std::stoi(argv[5]);

    if (mode == "dpdk") {
        /* Initialize the Environment Abstraction Layer (EAL). */
        int ret = 0;
        ret = rte_eal_init(argc, argv);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    }

    std::thread controller_thread = std::thread(proxy_management_controller, proxy_num,
        branching_factor, mode, redundancy_factor, total_proxies_without_redundant_nodes);

    std::thread proxy_thread = std::thread(proxy, proxy_num, branching_factor,
        mode, redundancy_factor, total_proxies_without_redundant_nodes);

    if (controller_thread.joinable()) controller_thread.join();
    if (proxy_thread.joinable()) proxy_thread.join();

    return 0;
}
