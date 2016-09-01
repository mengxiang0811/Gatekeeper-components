#ifndef _IP_IN_IP_H_
#define _IP_IN_IP_H_

#include <rte_ether.h>
#include <rte_ip.h>

#define SERVER_MAX_PORTS (1)

#define IP_VERSION 0x40                                                         
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */   
#define IP_DEFTTL  64   /* from RFC 1340. */                                    
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)                                     
#define IP_DN_FRAGMENT_FLAG 0x0040

#define IPV6_ADDR_LEN		  16
#define IPv6_DEFAULT_VTC_FLOW     0x60000000
#define IPv6_DEFAULT_HOP_LIMITS   0xFF

/* 
 * fields to support TX offloads
 * from the definition of struct rte_mbuf
 * @dpdk/lib/librte_mbuf/rte_mbuf.h
 */
union tunnel_offload_info {
	uint64_t data;       /**< combined for easy fetch */
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
init_hdr4_templates(void);

void
init_hdr6_templates(void);

int
decapsulation(struct rte_mbuf *pkt);

void
encapsulation_v4(struct rte_mbuf *m, uint8_t port_id);

void
encapsulation_v6(struct rte_mbuf *m, uint8_t port_id);

#endif
