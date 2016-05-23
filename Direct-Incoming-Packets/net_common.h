#ifndef _NET_COMMON_H
#define _NET_COMMON_H

#include <rte_mbuf.h>

#include "net_config.h"

#define GATEKEEPERD_MAX_LCORES (4)
#define GATEKEEPERD_MAX_NUMA_NODES (1)
#define GATEKEEPERD_MAX_QUEUES (4)
#define RSS_HASH_KEY_LENGTH (40)

struct rte_mbuf *
gatekeeperd_packet_alloc(void);

void
gatekeeperd_packet_free(struct rte_mbuf *mbuf);

struct rte_mbuf *
gatekeeperd_receive_packet(uint8_t port_id);

void
gatekeeperd_receive_packets(uint8_t port_id, struct rte_mbuf **mbufs, size_t *in_out_num_mbufs);

void
gatekeeperd_send_packet(uint8_t port_id, struct rte_mbuf *mbuf);

void
gatekeeperd_send_packet_flush(uint8_t port_id);

struct rte_mbuf *
gatekeeperd_clone_packet(struct rte_mbuf *mbuf_src);

int
gatekeeperd_init_network(uint64_t cpu_mask, uint64_t port_mask, uint8_t *out_num_ports);

void
gatekeeperd_free_network(uint64_t port_mask);

#endif
