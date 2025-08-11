// Copyright [2023] NYU

#ifndef SRC_UTILS_TREE_HPP_
#define SRC_UTILS_TREE_HPP_

#include <cassert>
#include <algorithm>
#include <cerrno>
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

bool is_a_leaf_node(int current_node, int total_proxies_without_redundant_nodes, int m) {
    if (1 + m * current_node >= total_proxies_without_redundant_nodes) {
        return true;
    }
    return false;
}

int get_total_leaves(int total_proxies_without_redundant_nodes, int m) {
    int count = 0;

    for (int i = 0; i < total_proxies_without_redundant_nodes; i++) {
        if (is_a_leaf_node(i, total_proxies_without_redundant_nodes, m)) {
            count++;
        }
    }

    return count;
}

bool is_a_redundant_node(int current_node, int total_proxies_without_redundant_nodes) {
    return current_node >= total_proxies_without_redundant_nodes;
}

int get_index_of_redundant_node_among_redundant_nodes(int current_node, int total_proxies_without_redundant_nodes) {
    assert(is_a_redundant_node(current_node, total_proxies_without_redundant_nodes));
    int res = current_node - total_proxies_without_redundant_nodes;
    return res;
}

bool is_a_leaf_redundant_node(
    int current_node, int total_proxies_without_redundant_nodes,
    int total_proxies, int m, int redundancy_factor) {
    assert(true == is_a_redundant_node(current_node, total_proxies_without_redundant_nodes));

    int total_leaves = get_total_leaves(total_proxies_without_redundant_nodes, m);
    int total_parents_of_leaves = total_leaves/m;
    int total_leaf_redundant_nodes = total_parents_of_leaves * redundancy_factor;
    int starting_index_for_total_leaf_redundant_nodes = total_proxies - total_leaf_redundant_nodes;
    return current_node >= starting_index_for_total_leaf_redundant_nodes;
}

int get_parent_of_redundant_node(int current_node, int total_proxies_without_redundant_nodes, int redundancy_factor) {
    assert(true == is_a_redundant_node(current_node, total_proxies_without_redundant_nodes));

    int index_in_redundant_nodes = current_node-total_proxies_without_redundant_nodes;
    int index_of_parent_proxy_node = index_in_redundant_nodes/redundancy_factor;
    return index_of_parent_proxy_node;
}

int get_parent_of_proxy_node(int current_node, int m) {
    assert(current_node > 0);
    return (current_node - 1)/m;
}

int get_parent_of_node(int current_node, int m, int total_proxies_without_redundant_nodes, int redundancy_factor) {
    if (is_a_redundant_node(current_node, total_proxies_without_redundant_nodes)) {
        return get_parent_of_redundant_node(current_node, total_proxies_without_redundant_nodes, redundancy_factor);
    }

    return get_parent_of_proxy_node(current_node, m);
}

std::vector<int> get_indices_of_siblings_that_are_hedge_nodes(
    int proxy_num, int total_proxies_without_redundant_nodes, int redundancy_factor, int m) {
    std::vector<int> indices;

    int parent_node = get_parent_of_proxy_node(proxy_num, m);

    int starting_redundant_node_index =
        total_proxies_without_redundant_nodes + parent_node * redundancy_factor;

    for (int i = starting_redundant_node_index;
        i < starting_redundant_node_index + redundancy_factor; i++) {
        indices.push_back(i);
    }

    return indices;
}

std::vector<int> get_children_indices_for_proxy_node(
    int current_node, int m, int redundancy_factor,
    int total_proxies_without_redundant_nodes, bool return_only_proxy_nodes = false) {

    assert(false == is_a_redundant_node(current_node, total_proxies_without_redundant_nodes));
    std::vector<int> indices;

    if (is_a_leaf_node(current_node, total_proxies_without_redundant_nodes, m)) {
        return indices;
    }

    for (int i = 1; i <= m; i++) {
        indices.push_back(i + m * current_node);
    }

    if (redundancy_factor == 0 || return_only_proxy_nodes == true) {
        return indices;
    }

    int starting_redundant_node_index_for_current_proxy =
        total_proxies_without_redundant_nodes + current_node*redundancy_factor;

    for (int i = starting_redundant_node_index_for_current_proxy;
        i < starting_redundant_node_index_for_current_proxy+redundancy_factor;
        i++) {
        indices.push_back(i);
    }

    return indices;
}

std::vector<int> get_children_indices_for_redundant_node(int current_node, int total_nodes,
    int m, int redundancy_factor, int total_proxies_without_redundant_nodes) {

    assert(true == is_a_redundant_node(current_node, total_proxies_without_redundant_nodes));

    std::vector<int> indices;

    if (is_a_leaf_redundant_node(
        current_node, total_proxies_without_redundant_nodes, total_nodes, m, redundancy_factor)) {
        return indices;
    }

    int parent_node = get_parent_of_redundant_node(
        current_node, total_proxies_without_redundant_nodes, redundancy_factor);
    assert(false == is_a_leaf_node(parent_node, total_proxies_without_redundant_nodes, m));
    assert(false == is_a_redundant_node(parent_node, total_proxies_without_redundant_nodes));

    auto siblings_proxy_nodes_indices = get_children_indices_for_proxy_node(
        parent_node, m, redundancy_factor, total_proxies_without_redundant_nodes, true);

    for (auto sibling : siblings_proxy_nodes_indices) {
        auto nephews = get_children_indices_for_proxy_node(
            sibling, m, redundancy_factor, total_proxies_without_redundant_nodes);
        indices.insert(indices.end(), nephews.begin(), nephews.end());
    }

    return indices;
}

int get_leaf_number(int total_proxies_without_redundant_nodes, int current_node, int m) {
    if (!is_a_leaf_node(current_node, total_proxies_without_redundant_nodes, m)) {
        throw std::runtime_error("get_leaf_number called for a non-leaf index");
    }

    int leaf_number = 0;
    for (int i = 0; i < total_proxies_without_redundant_nodes; i++) {
        if (i == current_node) {
            return leaf_number;
        }

        if (is_a_leaf_node(i, total_proxies_without_redundant_nodes, m)) {
            leaf_number++;
        }
    }

    throw std::runtime_error("Could not determine the leaf number");
}

int get_next_sibling(int current_node, int m, int total_proxies_without_redundant_nodes) {
    if (current_node == 0) return -1;

    int res = current_node + 1;

    int tmp = 0;
    int prev = 0;
    int i = 1;
    while (tmp < total_proxies_without_redundant_nodes) {
        prev = tmp;  // the ending node of the prev level

        tmp += pow(m, i);

        if (tmp == current_node) {
            res = prev + 1;
        }

        i++;
    }

    return res;
}


#endif  // SRC_UTILS_TREE_HPP_
