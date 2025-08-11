// Copyright 2023 Haseeb
#include <linux/types.h>
#include <linux/bpf.h>
#include <linux/pkt_cls.h>
#include <stdint.h>
#include <iproute2/bpf_elf.h>
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h>

#ifndef __section
# define __section(NAME)                  \
    __attribute__((section(NAME), used))
#endif

#ifndef __inline
# define __inline                         \
        inline __attribute__((always_inline))
#endif

#ifndef BPF_FUNC
# define BPF_FUNC(NAME, ...)              \
        (*NAME)(__VA_ARGS__) = (void *)BPF_FUNC_##NAME
#endif

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

#define IPv4(a, b, c, d) ((uint32_t)(((uint8_t)(d) << 24) | \
                                     ((uint8_t)(c) << 16) | \
                                     ((uint8_t)(b) << 8) | \
                                     (uint8_t)(a)))


struct bpf_elf_map ips_map __section("maps") = {
    .type           = BPF_MAP_TYPE_ARRAY,
    .size_key       = sizeof(uint32_t),
    .size_value     = sizeof(uint32_t),
    .pinning        = PIN_GLOBAL_NS,
    .max_elem       = 128,
};

struct bpf_elf_map macs_map __section("maps") = {
    .type           = BPF_MAP_TYPE_ARRAY,
    .size_key       = sizeof(uint32_t),
    .size_value     = sizeof(uint64_t),
    .pinning        = PIN_GLOBAL_NS,
    .max_elem       = 128,
};

// Used for configuration
// index 0 will be set by userspace to have total downstreams
struct bpf_elf_map config_map __section("maps") = {
    .type           = BPF_MAP_TYPE_ARRAY,
    .size_key       = sizeof(uint32_t),
    .size_value     = sizeof(uint32_t),
    .pinning        = PIN_GLOBAL_NS,
    .max_elem       = 10,
};

static __always_inline __u16
csum_fold_helper(__u64 csum)
{
    int i;
#pragma unroll
    for (i = 0; i < 4; i++)
    {
        if (csum >> 16)
            csum = (csum & 0xffff) + (csum >> 16);
    }
    return ~csum;
}

static __always_inline __u16
iph_csum(struct iphdr *iph)
{
    iph->check = 0;
    unsigned long long csum = bpf_csum_diff(0, 0, (unsigned int *)iph, sizeof(struct iphdr), 0);
    return csum_fold_helper(csum);
}

const uint32_t LOOP_LIMIT = 20;

__section("egress")
int tc_egress(struct __sk_buff *skb)
{
    void *data = (void *)(long)skb->data;
    if (skb->data + sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct MsgDp) > skb->data_end) {
        return TC_ACT_OK;
    }

    struct ethhdr *_eth = data;
    struct iphdr *_ip = data + sizeof(struct ethhdr);
    struct udphdr *_udp = data + sizeof(struct ethhdr) + sizeof(struct iphdr);
    struct MsgDp *msg = data + sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr);

    if (0 == (_eth->h_proto == bpf_htons(ETH_P_IP)
        && _ip->protocol == IPPROTO_UDP
        && _udp->dest == bpf_htons(34254))) {

        return TC_ACT_OK;
    }

    if (msg->_is_clone == 1) {
        msg->_is_clone = 0;
        return TC_ACT_OK;
    }

    msg->_is_clone = 1;

    const uint32_t total_downstreams_index = 0;
    uint32_t* val_p = bpf_map_lookup_elem(&config_map, &total_downstreams_index);
    if (val_p == NULL) {
        return TC_ACT_SHOT;
    }

    const uint32_t downstreams = *val_p;
    if (downstreams > LOOP_LIMIT || downstreams < 0) {
        bpf_printk("Number of downstreams is more than what we can handle!\n");
        return TC_ACT_SHOT;
    }

    uint32_t count = 0;

    for (uint32_t key = 0; key < downstreams; key++) {
        const uint32_t k = key;

        // set up new IP
        uint32_t* ip = bpf_map_lookup_elem(&ips_map, &k);
        if (ip == NULL || *ip == 0) {
            bpf_printk("IP Lookup error\n");
            break;
        }

        uint32_t new_ip = *ip;
        _ip->daddr = new_ip;
        _ip->check = iph_csum(_ip);

        // set up new Mac
        uint64_t* mac_p = bpf_map_lookup_elem(&macs_map, &k);
        if (mac_p == NULL || *mac_p == 0) {
            bpf_printk("Mac Lookup error\n");
            return TC_ACT_SHOT;
        }

        uint64_t mac = *mac_p;
        for (int i = 5; i >= 0; i--) {
            _eth->h_dest[i] = mac & 0xFF;
            mac >>= 8;
        }

        if (key + 1 >= downstreams) break;  // so that we reuse the existing packet

        int e = bpf_clone_redirect(skb, skb->ifindex, 0);
        if (e != 0) {
            bpf_printk("Clone Error. %d\n", e);
            return TC_ACT_SHOT;
        }

        count++;
        if (skb->data + sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct MsgDp) > skb->data_end) {
            bpf_printk("Packet malformed\n");
            return TC_ACT_SHOT;
        }

        data = (void *)(long)skb->data;
        _eth = data;
        _ip = data + sizeof(struct ethhdr);
        _udp = data + sizeof(struct ethhdr) + sizeof(struct iphdr);
        msg = data + sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr);
    }

    if (count + 1 != downstreams) {
        bpf_printk("Woah\n");  // never gets printed even when some receivers do not receive the packets
        return TC_ACT_SHOT;
    }

    return TC_ACT_OK;
}

char __license[] __section("license") = "GPL";
