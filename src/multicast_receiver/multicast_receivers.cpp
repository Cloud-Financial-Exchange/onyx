// Copyright [2023] NYU

#include "./../utils/common.hpp"
#include "./../config.h"
#include "./utils.hpp"
#include "./../utils/holdrelease.hpp"
#include "./../cameron314/readerwriterqueue.h"
#include "./socket_receiver.hpp"
#include "./dpdk_receiver.hpp"

// Currently status is actually not used in receiver
Status status = Status::BLOCKED;

extern std::unordered_map<int, std::mutex> stat_mutexes;
extern std::vector<StatsDp> all_stats;
extern std::atomic<int> total_messages_from_hedge_nodes;

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Arguments needed: recipient_id, duplication_factor, mode"
            << ", total_proxies_without_redundant_nodes, redundancy_factor, m"
            << std::endl;
        exit(1);
    }

    int32_t recipient_id = std::stoi(argv[1]);
    int32_t duplication_factor = std::stoi(argv[2]);
    std::string mode = argv[3];
    int32_t m = std::stoi(argv[4]);
    int32_t redundancy_factor = std::stoi(argv[5]);
    int32_t total_proxies_without_redundant_nodes = std::stoi(argv[6]);

    RWQ management_to_dataplane_q(128);
    std::thread controller_thread = std::thread(
        receiver_management_controller, recipient_id, mode, std::ref(management_to_dataplane_q));

    if (mode == "dpdk" && CONFIG::ENFORCE_SOCKET_RECEIVER == false) {
        /* Initialize the Environment Abstraction Layer (EAL). */
        int ret = 0;
        ret = rte_eal_init(argc, argv);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

        dpdk_receiver(recipient_id, duplication_factor, mode, m, redundancy_factor,
            total_proxies_without_redundant_nodes);
    } else {
        socket_receiver(recipient_id, duplication_factor, mode, m, redundancy_factor,
            total_proxies_without_redundant_nodes, management_to_dataplane_q);
    }

    controller_thread.join();

    return 0;
}
