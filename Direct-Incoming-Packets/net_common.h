#pragma once

#include <rte_mbuf.h>

#include "net_config.h"

#define GATEKEEPERD_MAX_LCORES (16)
#define GATEKEEPERD_MAX_NUMA_NODES (2)
#define GATEKEEPERD_MAX_QUEUES (16)

struct rte_mbuf *
gatekeeperd_packet_alloc();

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

bool
gatekeeperd_init_network(uint64_t cpu_mask, uint64_t port_mask, uint8_t *out_num_ports);

void
gatekeeperd_free_network(uint64_t port_mask);