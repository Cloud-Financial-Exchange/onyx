// Copyright [2023] NYU

#ifndef SRC_UTILS_FASTMAP_HPP_
#define SRC_UTILS_FASTMAP_HPP_

#include <unordered_map>
#include <mutex>  // NOLINT(build/c++11)
#include <vector>

#include "./../message.h"

struct Lmap {
    std::unordered_map<uint64_t, OrderMsg> m;
    std::mutex mtx;
};

class Fastmap {
 private:
    std::vector<Lmap> maps;

    int hash(uint64_t key);

 public:
    Fastmap() : maps(64) { }  // dont change 64. It needs to be power of 2.

    void insert(uint64_t key, OrderMsg val);
    OrderMsg get(uint64_t key);
    bool exists(uint64_t key);
    std::size_t erase(uint64_t key);
};

void Fastmap::insert(uint64_t key, OrderMsg val) {
    auto k = hash(key);
    auto& m = maps[k];
    std::lock_guard<std::mutex> lock(m.mtx);
    m.m[key] = val;
}

OrderMsg Fastmap::get(uint64_t key) {
    auto k = hash(key);
    auto& m = maps[k];
    std::lock_guard<std::mutex> lock(m.mtx);
    return m.m[key];
}

bool Fastmap::exists(uint64_t key) {
    auto k = hash(key);
    auto& m = maps[k];
    std::lock_guard<std::mutex> lock(m.mtx);
    return m.m.find(key) != m.m.end();
}

std::size_t Fastmap::erase(uint64_t key) {
    auto k = hash(key);
    auto& m = maps[k];
    std::lock_guard<std::mutex> lock(m.mtx);
    return m.m.erase(key);
}

int Fastmap::hash(uint64_t key) {
    return std::hash<uint64_t>{}(key) & 63;
}


#endif  // SRC_UTILS_FASTMAP_HPP_
