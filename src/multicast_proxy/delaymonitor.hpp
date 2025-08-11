// Copyright [2024] NYU

#ifndef SRC_MULTICAST_PROXY_DELAYMONITOR_HPP_
#define SRC_MULTICAST_PROXY_DELAYMONITOR_HPP_

#include <inttypes.h>
#include <vector>
#include <thread>  // NOLINT(build/c++11)
#include <mutex>  // NOLINT(build/c++11)
#include <string>
#include <utility>
#include <memory>
#include <algorithm>

#include "./../utils/common.hpp"

struct Pacinghint {
    int32_t hint;  // -1 means decrease pacing, 1 means increase, 0 means nothing
};

class Delaymonitor {
 private:
    std::vector<std::thread> threads;

 public:
    std::vector<std::string> downstreams;
    std::mutex mtx;
    std::unique_ptr<std::vector<uint32_t>> delays;
    uint64_t send_estimates_wait;  // micros
    uint64_t send_estimates_size;  // micros
    double estimates_percentile;
    std::string ip;
    std::atomic<int64_t> pace;

    uint32_t prev_estimate;  // micros

    Delaymonitor(std::string ip, std::vector<std::string> downstreams);
    void record(uint32_t owd);
    uint32_t get_percentile(const std::unique_ptr<std::vector<uint32_t>>& data, double percentile);

    ~Delaymonitor();
};

Delaymonitor::Delaymonitor(std::string ip, std::vector<std::string> downstreams) : ip(ip) {
    if (CONFIG::ORDERS_SUBMISSION::ENABLE_DELAYMONITOR == false) return;

    delays = std::make_unique<std::vector<uint32_t>>();
    send_estimates_wait = 40'000;  // 50 millis
    send_estimates_size = 2000;
    estimates_percentile = 50.0;
    prev_estimate = 0;
    int delaymonitor_port;
    pace.store(0);

    std::vector<sockaddr_in> addrs;
    for (auto ele : downstreams) {
        auto [_ip, _] = separate_ip_port(ele);
        auto [ip, port] = get_delaymonitor_address(_ip);
        delaymonitor_port = port;

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);
        addrs.push_back(addr);
    }

    // Periodically makes an estimate out of owds and asks the proxies for pacing if needed
    threads.emplace_back([this, addrs](){
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        std::unique_ptr<std::vector<uint32_t>> old_delays;

        while (true) {
            std::this_thread::sleep_for(std::chrono::microseconds(this->send_estimates_wait));
            {
                std::lock_guard<std::mutex> lock(this->mtx);

                if (this->delays->size() < this->send_estimates_size) continue;

                old_delays = std::move(this->delays);
                this->delays = std::make_unique<std::vector<uint32_t>>();
            }

            auto res = get_percentile(old_delays, this->estimates_percentile);
            old_delays->clear();

            Pacinghint h;

            if (std::abs(static_cast<int>(res) - CONFIG::ORDERS_SUBMISSION::OWD_THRESH) < 10) {
                h.hint = 0;
            } else {
                h.hint = (CONFIG::ORDERS_SUBMISSION::OWD_THRESH < res) ? 1 : -1;
                auto intens = std::abs(static_cast<int>(res) - CONFIG::ORDERS_SUBMISSION::OWD_THRESH);

                if (h.hint > 0) {
                    intens = 20;
                } else {
                    intens = 10;
                }

                h.hint *= intens;
            }

            for (auto ele : addrs) {
                auto bytes_sent = sendto(sockfd, &h, sizeof(Pacinghint), 0,
                                    (struct sockaddr *)&ele, sizeof(ele));

                if (bytes_sent == -1) {
                    std::cerr << "Failed to send data." << std::endl;
                    close(sockfd);
                    exit(1);
                }
            }
        }
    });


    // Check the delaymonitor_port and sets the atomic variable ``pace``
    threads.emplace_back([this, delaymonitor_port]() {
        int sockfd = get_bound_udp_socket(inet_addr(this->ip.c_str()), delaymonitor_port);
        char* buffer[sizeof(Pacinghint)];
        uint64_t prev_sig = 0;

        int max_intensity = 1;
        int intensity = max_intensity;

        while (true) {
            int n = recv(sockfd, buffer, sizeof(Pacinghint), 0);
            if (n < 0) {
                perror("Receive failed");
                break;
            }

            Pacinghint* hint = reinterpret_cast<Pacinghint*>(buffer);
            uint64_t curr_ts = get_current_time();
            if (hint->hint > 0) {
                if (send_estimates_wait > (curr_ts-prev_sig)) {
                    hint->hint = 0;
                    std::cout << ". " << std::flush;
                } else {
                    prev_sig = curr_ts;
                }
            }

            this->pace.fetch_add(intensity * hint->hint);
            if (this->pace.load() < 0) { this->pace.store(0); }  // only one thread writes `pace

            std::ofstream file("pacing.txt", std::ios::app);
            if (!file.is_open()) { std::cerr << "Error opening pacing.txt file: " << std::endl; continue; }
            file << curr_ts << "," << this->pace.load() << std::endl;
            file.close();
        }
    });
}

Delaymonitor::~Delaymonitor() {
    for (auto& ele : threads) {
        if (ele.joinable()) ele.join();
    }
}

void Delaymonitor::record(uint32_t owd) {
    if (CONFIG::ORDERS_SUBMISSION::ENABLE_DELAYMONITOR == false) return;
    std::lock_guard<std::mutex> lock(mtx);
    delays->push_back(owd);
}

uint32_t Delaymonitor::get_percentile(const std::unique_ptr<std::vector<uint32_t>>& data, double percentile) {
    if (data->empty()) {
        std::cerr << "Error: Empty dataset." << std::endl;
        return 0.0;
    }

    std::vector<uint32_t> sorted_data = *data;
    std::sort(sorted_data.begin(), sorted_data.end());

    int index = (percentile / 100.0) * (sorted_data.size() - 1);

    if (index == floor(index)) {
        return sorted_data[static_cast<size_t>(index)];
    } else {
        size_t floor_ind = static_cast<size_t>(floor(index));
        size_t ceil_ind = static_cast<size_t>(ceil(index));
        int floor_val = sorted_data[floor_ind];
        int ceil_val = sorted_data[ceil_ind];
        return floor_val + (index - floor(index)) * (ceil_val - floor_val);
    }
}

#endif  // SRC_MULTICAST_PROXY_DELAYMONITOR_HPP_
