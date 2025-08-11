/* SPDX-License-Identifier: GPL-2.0 */
#include <stddef.h>
#include <linux/bpf.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include <iproute2/bpf_elf.h>
#include <stdint.h>

#ifndef __section
# define __section(NAME)                  \
    __attribute__((section(NAME), used))
#endif

#ifndef __inline
# define __inline                         \
        inline __attribute__((always_inline))
#endif

#define REORDERING_LIMIT_ASSUMPTION 16384

struct bpf_map_def SEC("maps") seen_ids = {
    .type = BPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int32_t),
    .value_size = sizeof(int64_t),
    .max_entries = REORDERING_LIMIT_ASSUMPTION,
};

struct bpf_map_def SEC("maps") zero_seen = {
    .type = BPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int32_t),
    .value_size = sizeof(int64_t),
    .max_entries = 1,
};

// Msg for Data plane. We use protobuf for non-dataplane purposes
struct MsgDp {
    int64_t _msg_id;
    int64_t _client_send_time;
    int32_t _recipient_id;
    int64_t _root_send_time;
    int32_t _msg_type;  // 0=> for sending msgs, 1=> for requesting stats
    int32_t _is_from_hedge_node;  // 1 => true, 2 => false (see issue #28)
    int64_t _experiment_starting_msg_id;
    int64_t _deadline;
    int64_t _recv_time;
    int64_t _data[43];
    int8_t _is_clone;  // used by ebpf/tc egress hook
};

SEC("xdp")
int  xdp_dedup(struct xdp_md *ctx) {
    if (ctx->data + sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct MsgDp) > ctx->data_end) {
        return XDP_PASS;
    }

	void *data = (void *)(long)ctx->data;

    struct ethhdr *_eth = data;
    struct iphdr *_ip = data + sizeof(struct ethhdr);
    struct udphdr *_udp = data + sizeof(struct ethhdr) + sizeof(struct iphdr);
    struct MsgDp *msg = data + sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr);

    if (0 == (_eth->h_proto == bpf_htons(ETH_P_IP)
        && _ip->protocol == IPPROTO_UDP
        && _udp->dest == bpf_htons(34254))) {

        return XDP_PASS;
    }

    if (msg->_msg_id == 0) {
        int64_t* value = bpf_map_lookup_elem(&zero_seen, &(msg->_msg_id));
        if (value == NULL) {
            bpf_printk("NULL found in map\n");
            return XDP_PASS;
        }

        if (*value == 1) return XDP_DROP;
        int64_t one = 1;
        bpf_map_update_elem(&zero_seen, &(msg->_msg_id), &one, BPF_ANY);
        return XDP_PASS;
    }

    int32_t id = msg->_msg_id & (REORDERING_LIMIT_ASSUMPTION - 1);
    int64_t* value = bpf_map_lookup_elem(&seen_ids, &id);
    if (value == NULL) {
        bpf_printk("NULL found in map\n");
        return XDP_PASS;
    }

    if (*value >= msg->_msg_id) {  // seen message id or too old message received
        return XDP_DROP;
    }

    bpf_map_update_elem(&seen_ids, &id, &(msg->_msg_id), BPF_ANY);
    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";