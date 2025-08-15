// Copyright [2023] NYU

#include "./../../include/CLI11/CLI11.hpp"
#include "./../utils/common.hpp"
#include "./../config.h"

enum class Action { COLLECT_MAC, COLLECT_STATS };
std::atomic<int> mac_collected = 0;
Status status = Status::BLOCKED;

void send_start_message(
    int sockfd, sockaddr_in proxy_addr, int msg_rate, int experiment_time_in_seconds, int64_t starting_msg_id) {

    StartMsgsDp msg;
    msg.experiment_time_in_seconds = experiment_time_in_seconds;
    msg.msg_rate = msg_rate;
    msg.starting_msg_id = starting_msg_id;

    int bytes_sent = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr));

    if (bytes_sent == -1) {
        std::cerr << "Failed to send startmsg." << std::endl;
        exit(1);
    }

    std::clog << "Sent a StartMsg request to root proxy. Check root proxy for progress" << std::endl;
}

void send_bursty_messages(
    int sockfd, sockaddr_in proxy_addr, int rate, int experiment_time_in_seconds, int64_t starting_msg_id) {
    double burst_rate = rate * CONFIG::BURST_FACTOR;
    double burst_duration = 1.0;   // Burst duration in seconds
    double burst_interval = 2.0;  // Time between bursts in seconds
    int total_bursts = 0;

    std::random_device rd;
    // std::mt19937 generator(rd());
    std::mt19937 generator(321);
    std::exponential_distribution<> exp_dist(rate);

    double curr_relative_time = 0;
    auto next_time = std::chrono::system_clock::now();
    uint64_t total_messages = 0;

    auto last_burst_time = std::chrono::system_clock::now();
    bool in_burst_mode = false;
    int i = 0;

    while (curr_relative_time < experiment_time_in_seconds) {
        double interarrival_time = exp_dist(generator);

        auto current_time = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_time = std::chrono::duration_cast<std::chrono::duration<double>>(
            current_time - last_burst_time);

        if (!in_burst_mode && CONFIG::BURSTY && elapsed_time.count() >= burst_interval) {
            in_burst_mode = true;
            last_burst_time = current_time;
            exp_dist = std::exponential_distribution<>(burst_rate);
            total_bursts++;
        } else if (in_burst_mode && elapsed_time.count() >= burst_duration) {
            in_burst_mode = false;
            exp_dist = std::exponential_distribution<>(rate);
        }

        curr_relative_time += interarrival_time;

        next_time += std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::duration<double>(interarrival_time));

        if (curr_relative_time < experiment_time_in_seconds) {
            MsgDp msg;
            msg.set_client_send_time(get_current_time());
            msg.set_root_send_time(get_current_time());
            msg.set_deadline(get_current_time());
            msg.set_is_from_hedge_node(2);
            msg.set_msg_id(i + starting_msg_id);
            msg.set_msg_type(0);
            msg.set_recipient_id(0);

            int bytes_sent = sendto(
                sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr));
            if (bytes_sent == -1) {
                std::cerr << "Failed to send data." << std::endl;
                exit(1);
            }

            i++;
            total_messages++;
            std::this_thread::sleep_until(next_time);
        }
    }

    std::clog << "Msg size: " << sizeof(MsgDp) << std::endl;
    std::clog << "Total bursts: " <<  total_bursts << std::endl;
    std::clog << "Done generating [ " << total_messages << " ] orders!" << std::endl;
    std::clog << "Next Message ID: " << starting_msg_id + i << std::endl;
}

void send_messages(
    int sockfd, sockaddr_in proxy_addr, int msg_rate, int experiment_time_in_seconds, int64_t starting_msg_id) {

    if (CONFIG::BURSTY) {
        send_bursty_messages(sockfd, proxy_addr, msg_rate, experiment_time_in_seconds, starting_msg_id);
        return;
    }

    auto iterations = (int64_t)msg_rate * experiment_time_in_seconds;
    auto time_between_messages = std::chrono::duration<double>(1) / msg_rate;
    std::cout << "Next message id: " << iterations+starting_msg_id << std::endl;
    std::clog << "Started sending the messages at " <<  get_current_time() << std::endl;
    std::clog << "Msg size: " << sizeof(MsgDp) << std::endl;

    auto start_time = std::chrono::system_clock::now();
    for (int64_t i = 0; i < iterations; i++) {
        MsgDp msg;
        msg.set_client_send_time(get_current_time());
        msg.set_root_send_time(get_current_time());
        msg.set_deadline(get_current_time());
        msg.set_is_from_hedge_node(2);
        msg.set_msg_id(i+starting_msg_id);
        msg.set_msg_type(0);
        msg.set_recipient_id(0);

        int bytes_sent = sendto(
            sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr));
        if (bytes_sent == -1) {
            std::cerr << "Failed to send data." << std::endl;
            exit(1);
        }

        std::this_thread::sleep_until(start_time + (time_between_messages * i));
    }

    auto end_time = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    std::clog << "Done sending the messages at " <<  get_current_time() << std::endl;
    std::cout << "Rate achieved: " << (1.0*iterations)/(duration.count()) << std::endl;
}

void collect_stats(std::string target_ip, std::string s3_folder) {
    auto [ip, port]     = get_mac_collection_address(target_ip);
    Zmqsocket client(std::string {"tcp"},
                     std::string {target_ip},
                     std::string {std::to_string(port)},
                     zmq::socket_type::peer,
                     "CLIENT_STATS",
                     Zmqsocket::LOG_LEVEL::NONE);
    ManagementMsg msg, reply;

    client.socket.set(zmq::sockopt::heartbeat_ivl, 100);
    client.socket.set(zmq::sockopt::heartbeat_timeout, 1000);

    msg.add_request(s3_folder);

    int rid = client.connect_peer();
    client.send_message_to(msg, static_cast<int>(MessageType::REQ_STATS_COLLECTION), rid);

    auto [n, match] = client.expect_message(reply, MessageType::ACK_STATS_COLLECTION);
    assert(match);

    return;
}

void request_orders(std::string target_ip,
    int32_t duration, int32_t rate, uint64_t id, uint64_t start_time,
    std::vector<int32_t> bid_range, std::vector<int32_t> ask_range,
    bool fixed_interarrival) {
    auto [ip, port]     = get_mac_collection_address(target_ip);
    Zmqsocket client(std::string {"tcp"},
                     std::string {target_ip},
                     std::string {std::to_string(port)},
                     zmq::socket_type::peer,
                     "ORDERS_REQUEST",
                     Zmqsocket::LOG_LEVEL::NONE);
    ManagementMsg msg, reply;

    client.socket.set(zmq::sockopt::heartbeat_ivl, 100);
    client.socket.set(zmq::sockopt::heartbeat_timeout, 1000);

    StartOrdersSubmission req;
    req.set_experiment_duration(duration);
    req.set_rate(rate);
    req.set_request_id(id);
    req.set_start_timestamp(start_time);
    req.set_bid_range_start(bid_range.front());
    req.set_bid_range_end(bid_range.back());
    req.set_ask_range_start(ask_range.front());
    req.set_ask_range_end(ask_range.back());
    req.set_fixed_interarrival(fixed_interarrival);
    req.set_use_fancypq(CONFIG::ORDERS_SUBMISSION::QTYPE > 0);

    msg.mutable_order_sub_req()->CopyFrom(req);

    int rid = client.connect_peer();
    client.send_message_to(msg, static_cast<int>(MessageType::START_ORDERS_SUBMISSION), rid);

    auto [n, match] = client.expect_message(reply, MessageType::ACK_START_ORDERS_SUBMISSION);
    assert(match);

    return;
}

void collect_mac(std::string target_ip) {
    auto [ip, port]     = get_mac_collection_address(target_ip);
    Zmqsocket client(std::string {"tcp"},
                     std::string {ip},
                     std::string {std::to_string(port)},
                     zmq::socket_type::peer,
                     "CLIENT_MAC",
                     Zmqsocket::LOG_LEVEL::NONE);

    ManagementMsg msg, reply;

    client.socket.set(zmq::sockopt::heartbeat_ivl, 100);
    client.socket.set(zmq::sockopt::heartbeat_timeout, 500);
    client.socket.set(zmq::sockopt::heartbeat_ttl, 1000);

    int rid = client.connect_peer();
    client.send_message_to(msg, static_cast<int>(MessageType::REQ_MAC_COLLECTION), rid);
    auto [n, match] = client.expect_message(reply, MessageType::ACK_MAC_COLLECTION);
    assert(match);

    while (true) {
        sleep(2);
        rid = client.connect_peer();
        client.send_message_to(msg, static_cast<int>(MessageType::REQ_MAC_COLLECTION_STATUS), rid);
        client.recv_message(msg);
        if (msg.msg_type() == MessageType::NACK_MAC_COLLECTION_STATUS) {
            continue;
        }

        return;
    }

    return;
}

void mac_controller() {
    int total = 0;
    auto proxies_ips = get_proxies_ips("management", read_json(ips_file));
    int total_proxies = proxies_ips.size();

    std::cout << "PROXIES TO REQUEST MAC COLLECTION: " << total_proxies << std::endl;
    std::vector<std::pair<std::future<void>, int>> futures;

    for (int i=0; i < total_proxies; i++) {
        futures.emplace_back(
                std::async(std::launch::async,
                           collect_mac,
                           proxies_ips[i]),
                i);
    }

    auto it = futures.begin();
    while ( !(futures.empty()) ) {
        if (it == futures.end())
            it = futures.begin();

        std::string state = "BLOCKED";

        auto& future = it->first;
        auto  index  = it->second;

        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ++it;
        } else {
            state = "COMPLETED";
            future.get();
            it = futures.erase(it);
            ++total;
        }

        std::cout << " "      << std::setw(4)   << total
                  << "/"      << std::setw(4)   << total_proxies <<  " | "
                  << "PROXY[" << index << "]: " << proxies_ips[index] << " => " << state
                  << std::endl;
    }
}

// TODO(haseeb): refactor all these controllers
void stats_controller(std::string folder_prefix) {
    int total = 0;
    auto unique_recipients_ips = get_unique_recipients(read_json(ips_file));
    int total_unique_recipients = unique_recipients_ips.size();
    std::string folder_name = folder_prefix + "_" + std::to_string(get_current_time());

    std::cout << "CLIENTS TO REQUEST STATS: " << total_unique_recipients;
    std::cout << " | FOLDER: " << folder_name << std::endl;
    std::vector<std::pair<std::future<void>, int>> futures;

    for (int i=0; i < total_unique_recipients; i++) {
        futures.emplace_back(
                std::async(std::launch::async,
                           collect_stats,
                           unique_recipients_ips[i],
                           folder_name),
                i);
    }

    auto it = futures.begin();
    while ( !(futures.empty()) ) {
        if (it == futures.end())
            it = futures.begin();

        std::string state = "BLOCKED";

        auto& future = it->first;
        auto  index  = it->second;

        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ++it;
        } else {
            state = "COMPLETED";
            future.get();
            it = futures.erase(it);
            ++total;
        }

        std::cout << " "      << std::setw(4)   << total
                  << "/"      << std::setw(4)   << total_unique_recipients <<  " | "
                  << "RECEIVER[" << index << "]: " << unique_recipients_ips[index] << " => " << state
                  << std::endl;
    }
}

void orders_controller(int32_t duration, uint64_t id, std::vector<int32_t> bid_range,
    std::vector<int32_t> ask_range, bool fixed_interarrival) {
    int total = 0;
    auto unique_recipients_ips = get_unique_recipients(read_json(ips_file));
    int total_unique_recipients = unique_recipients_ips.size();

    std::vector<std::pair<std::future<void>, int>> futures;

    int32_t rate = CONFIG::ORDERS_SUBMISSION::COLLECTIVE_RATE / total_unique_recipients;
    std::clog << "Order Request: duration=" << duration << ", rate=" << rate
    << "bid-range={" << bid_range.front() << "," << bid_range.back()
    << "},ask-range={" << ask_range.front() << "," << ask_range.back() << "}" << std::endl;

    uint64_t start_time = static_cast<uint64_t>(get_current_time()) + 2'000'000;  // current time + 2 seconds
    for (int i=0; i < total_unique_recipients; i++) {
        futures.emplace_back(
                std::async(std::launch::async,
                           request_orders,
                           unique_recipients_ips[i],
                           duration, rate, id, start_time, bid_range, ask_range, fixed_interarrival),
                i);
    }

    auto it = futures.begin();
    while ( !(futures.empty()) ) {
        if (it == futures.end())
            it = futures.begin();

        std::string state = "BLOCKED";

        auto& future = it->first;
        auto  index  = it->second;

        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ++it;
        } else {
            state = "COMPLETED";
            future.get();
            it = futures.erase(it);
            ++total;
        }

        std::cout << " "      << std::setw(4)   << total
                  << "/"      << std::setw(4)   << total_unique_recipients <<  " | "
                  << "RECEIVER[" << index << "]: " << unique_recipients_ips[index] << " => " << state
                  << std::endl;
    }
}

int main(int argc, char* argv[]) {
    CLI::App app{"Multicast client"};

    // Options
    std::string action = "multicast";
    std::string folder_prefix = "tmp";
    std::string mode = "dpdk";
    rlim_t fd_num = 8192;
    u_int experiment_time_in_seconds = 0;
    u_int starting_msg_id = 0;
    std::vector<int32_t> bid_range = {1, 5};
    std::vector<int32_t> ask_range = {5, 9};  // will be modified later on to accomodate action window
    bool fixed_interarrival = false;

    app.add_option("-a,--action", action, "Action to perform")
            ->check(CLI::IsMember({"mac", "messages", "request_orders", "request_stats", "rs"}));
    app.add_option("-t,--time", experiment_time_in_seconds,
                   "Experiment time in seconds")
            ->check(CLI::PositiveNumber);
    app.add_option("-i,--starting-msg-id", starting_msg_id,
                   "Starting message id");
    app.add_option("--fd_num", fd_num, "File descriptors limit to set")
            ->check(CLI::PositiveNumber);
    app.add_option("-s,--s3-folder", folder_prefix,
                   "S3 folder name (with timestamp) to store stats");
    app.add_option("-m,--mode", mode,
                   "Mode (dpdk, iouring or socket)");
    app.add_option("-b,--bid-range", bid_range,
                "Bid range like {13, 15}");
    app.add_option("-k,--ask-range", ask_range,
                "Ask range like {15, 17}");
    app.add_option("--fixed-interarrival", fixed_interarrival,
                "Fixed Interarrival Times? (1/0)");

    CLI11_PARSE(app, argc, argv);
    std::string cloud{CONFIG::CLOUD};
    std::cout << "Config says we are using " << cloud << " cloud!" << std::endl;

    if (set_fd_limit(fd_num) == -1) {
        std::cerr << "Failed to set file descriptor limit to " << fd_num
                  << std::endl;
        exit(1);
    } else {
        std::cout << "File descriptor limit is set to " << fd_num << std::endl;
    }

    if (action == "mac") {
        mac_controller();
        std::clog << "Everyone served with mac addresses" << std::endl;
        return 0;
    }

    if (action == "request_orders") {
        assert(bid_range.size() == 2);
        assert(ask_range.size() == 2);

        bid_range[1] += CONFIG::ORDERS_SUBMISSION::ACTION_WINDOW;
        ask_range[0] -= CONFIG::ORDERS_SUBMISSION::ACTION_WINDOW;

        assert(bid_range[1] == CONFIG::ORDERS_SUBMISSION::MID_PRICE + CONFIG::ORDERS_SUBMISSION::ACTION_WINDOW);
        assert(ask_range[0] == CONFIG::ORDERS_SUBMISSION::MID_PRICE - CONFIG::ORDERS_SUBMISSION::ACTION_WINDOW);
        assert(bid_range[0] <= CONFIG::ORDERS_SUBMISSION::MID_PRICE - CONFIG::ORDERS_SUBMISSION::ACTION_WINDOW);
        assert(ask_range[1] >= CONFIG::ORDERS_SUBMISSION::MID_PRICE + CONFIG::ORDERS_SUBMISSION::ACTION_WINDOW);

        orders_controller(experiment_time_in_seconds, starting_msg_id, bid_range, ask_range, fixed_interarrival);
        std::cout << "Next order_request id should be >= " << starting_msg_id+1 << std::endl;
        return 0;
    }

    if (action == "messages" && experiment_time_in_seconds > 0) {
        // Send messages
        std::string client_ip;

        // socket mode needs management ips to send/receive data
        // dpdk conversely uses a separate channel.
        // However on AWS, for either mode there is no need to indicate a separate IP.
        if (mode == "dpdk" || cloud == "aws") {
            client_ip = get_client_ip(0);
        } else {
            client_ip = get_client_management_ip(0);
        }
        auto sockfd = get_bound_udp_socket(inet_addr(client_ip.c_str()));

        auto proxy_addr = get_sock_addr(
            mode == "dpdk" && CONFIG::ENFORCE_SOCKET_SENDER == false ? get_proxy_ip(0) : get_proxy_management_ip(0));

        if (CONFIG::SENDER_PROXY_GENERATES_MESSAGES) {
            send_start_message(sockfd, proxy_addr, CONFIG::MSG_RATE, experiment_time_in_seconds, starting_msg_id);
        } else {
            send_messages(sockfd, proxy_addr, CONFIG::MSG_RATE, experiment_time_in_seconds, starting_msg_id);
            experiment_time_in_seconds = 0;
        }

        close(sockfd);

        sleep(5 + (2*experiment_time_in_seconds));
        std::cout << "Run request_stats" << std::endl;
        stats_controller(folder_prefix);
    }

    if (action == "request_stats" || action == "rs") {
        stats_controller(folder_prefix);
    }

    return 0;
}
