#ifndef _IP_IN_IP_H_
#define _IP_IN_IP_H_

#include <rte_ether.h>
#include <rte_ip.h>

#define SERVER_MAX_PORTS (4)

/* 
 * fields to support TX offloads
 * from the definition of struct rte_mbuf
 * @dpdk/lib/librte_mbuf/rte_mbuf.h
 */
union tunnel_offload_info {
    uint64_t tx_offload;       /**< combined for easy fetch */
    struct {
        uint64_t l2_len:7; /**< L2 (MAC) Header Length. */
        uint64_t l3_len:9; /**< L3 (IP) Header Length. */
        uint64_t l4_len:8; /**< L4 (TCP/UDP) Header Length. */
        uint64_t tso_segsz:16; /**< TCP TSO segment size */

        /* fields for TX offloading of tunnels */
        uint64_t outer_l3_len:9; /**< Outer L3 (IP) Hdr Length. */
        uint64_t outer_l2_len:7; /**< Outer L2 (MAC) Hdr Length. */

        /* uint64_t unused:8; */
    };
} __rte_cache_aligned;

void
init_hdr_templates(void);

int
decapsulation(struct rte_mbuf *pkt);

void
encapsulation(struct rte_mbuf *m, uint8_t queue_id);

#endif
