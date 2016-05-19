#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <rte_eal.h>

#include <rte_lcore.h>
#include <rte_byteorder.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_malloc.h>

#include "net_common.h"

#define GATEKEEPERD_MBUF_ENTRY_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define GATEKEEPERD_MBUF_SIZE (GATEKEEPERD_MAX_PORTS * GATEKEEPERD_MAX_QUEUES * 4096)
#define GATEKEEPERD_MAX_PKT_BURST   (32)

#define GATEKEEPERD_NB_RX_DESC (128)
#define GATEKEEPERD_NB_TX_DESC (128)

static uint16_t nb_rx_desc = GATEKEEPERD_NB_RX_DESC;
static uint16_t nb_tx_desc = GATEKEEPERD_NB_TX_DESC;

static uint8_t hash_key[RSS_HASH_KEY_LENGTH] = 
{ 
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 
    0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 
    0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 
    0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 
    0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 
};

/*
 * used to store the queues' states on each port
 * */
struct gatekeeperd_queue_state 
{
	struct rte_mbuf *rx_mbufs[GATEKEEPERD_MAX_PKT_BURST];
	uint16_t rx_length;
	uint16_t rx_next_to_use;
	uint64_t num_rx_received;

	struct rte_mbuf *tx_mbufs[GATEKEEPERD_MAX_PKT_BURST];
	uint16_t tx_length;
	uint64_t num_tx_sent;
} __rte_cache_aligned;

static const struct rte_eth_conf gatekeeperd_port_conf = {
	.rxmode = {
        .max_rx_pkt_len = ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.header_split   = 0, /* Header Split disabled */
		.hw_ip_checksum = 0, /* IP checksum offload disabled */
//		.hw_vlan_filter = 0, /* VLAN filtering disabled */
		.jumbo_frame    = 0, /* Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /* CRC stripped by hardware */
		.mq_mode = ETH_MQ_RX_RSS,
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
	.rx_adv_conf = {
        .rss_conf = {
            //.rss_key = hash_key, /* symmetric RSS key, but we use default ones for now */
            //.rss_key_len = 40,
            .rss_hf = ETH_RSS_IPV4, /* IPv4 Source and Destination addresses */
        },
	},
};

static const struct rte_eth_rxconf gatekeeperd_rx_conf = {
	.rx_thresh = {
		.pthresh = 8,
		.hthresh = 8,
		.wthresh = 4,
	},
	.rx_free_thresh = 32,
	.rx_drop_en = 0,
};

static const struct rte_eth_txconf gatekeeperd_tx_conf = {
	.tx_thresh = {
		.pthresh = 36,
		.hthresh = 0,
		.wthresh = 0,
	},
	.tx_free_thresh = 0, /* Use PMD default values */
	.tx_rs_thresh = 0, /* Use PMD default values */
    .txq_flags = (ETH_TXQ_FLAGS_NOMULTSEGS | ETH_TXQ_FLAGS_NOREFCOUNT | ETH_TXQ_FLAGS_NOOFFLOADS),
};

static struct rte_mempool *gatekeeperd_pktmbuf_pool[GATEKEEPERD_MAX_NUMA_NODES];
static struct gatekeeperd_queue_state *gatekeeperd_queue_states[GATEKEEPERD_MAX_QUEUES * GATEKEEPERD_MAX_PORTS];

struct rte_mbuf *
gatekeeperd_packet_alloc(void)
{
	return rte_pktmbuf_alloc(gatekeeperd_pktmbuf_pool[rte_socket_id()]);
}

void
gatekeeperd_packet_free(struct rte_mbuf *mbuf)
{
	rte_pktmbuf_free(mbuf);
}

struct rte_mbuf *
gatekeeperd_receive_packet(uint8_t port_id)
{
	uint32_t lcore = rte_lcore_id();
	uint16_t queue = (uint16_t)lcore;
	struct gatekeeperd_queue_state *state = gatekeeperd_queue_states[queue * GATEKEEPERD_MAX_PORTS + port_id];

	if (state->rx_next_to_use == state->rx_length)
	{
		state->rx_length = rte_eth_rx_burst(port_id, queue, state->rx_mbufs, GATEKEEPERD_MAX_PKT_BURST);
		state->num_rx_received += state->rx_length;
		state->rx_next_to_use = 0;
	}

	if (state->rx_next_to_use < state->rx_length)
    {
        printf("gatekeepered_receive_packet: lcore=%u, port=%hhu, queue=%u\n", lcore, port_id, queue);
		return state->rx_mbufs[state->rx_next_to_use++];
    }
	else
		return NULL;
}

void
gatekeeperd_receive_packets(uint8_t port_id, struct rte_mbuf **mbufs, size_t *in_out_num_mbufs)
{
	uint32_t lcore = rte_lcore_id();
	uint16_t queue = (uint16_t)lcore;
	struct gatekeeperd_queue_state *state = gatekeeperd_queue_states[queue * GATEKEEPERD_MAX_PORTS + port_id];

	*in_out_num_mbufs = (size_t)rte_eth_rx_burst(port_id, queue, mbufs, (uint16_t)*in_out_num_mbufs);
	state->num_rx_received += *in_out_num_mbufs;
}

/* the VLAN insertion & IP packets encapsulation may happen here */
void
gatekeeperd_send_packet(uint8_t port_id, struct rte_mbuf *mbuf)
{
	uint32_t lcore = rte_lcore_id();
	uint16_t queue = (uint16_t)lcore;
	struct gatekeeperd_queue_state *state = gatekeeperd_queue_states[queue * GATEKEEPERD_MAX_PORTS + port_id];

    printf("gatekeeperd_send_packet: lcore=%u, port=%hhu, queue=%u\n", lcore, port_id, queue);

	state->tx_mbufs[state->tx_length++] = mbuf;
	if (state->tx_length == GATEKEEPERD_MAX_PKT_BURST)
	{
		uint16_t count = rte_eth_tx_burst(port_id, queue, state->tx_mbufs, GATEKEEPERD_MAX_PKT_BURST);
		state->num_tx_sent += count;
		
        /* 
         * TODO: dropped packets stat or some other useful stats
         */

        for (; count < GATEKEEPERD_MAX_PKT_BURST; count++)
			rte_pktmbuf_free(state->tx_mbufs[count]);
		
        state->tx_length = 0;
	}
}

/* the VLAN insertion & IP packets encapsulation may happen here */
void
gatekeeperd_send_packet_flush(uint8_t port_id)
{
	uint32_t lcore = rte_lcore_id();
	uint16_t queue = (uint16_t)lcore;
	struct gatekeeperd_queue_state *state = gatekeeperd_queue_states[queue * GATEKEEPERD_MAX_PORTS + port_id];

	if (state->tx_length > 0)
	{
		uint16_t count = rte_eth_tx_burst(port_id, queue, state->tx_mbufs, state->tx_length);
		state->num_tx_sent += count;
		
        /*
         * TODO: dropped packets stat or some other useful stats
         */

        for (; count < state->tx_length; count++)
			rte_pktmbuf_free(state->tx_mbufs[count]);
	
        state->tx_length = 0;
	}
}

struct rte_mbuf *
gatekeeperd_clone_packet(struct rte_mbuf *mbuf_src)
{
	return rte_pktmbuf_clone(mbuf_src, gatekeeperd_pktmbuf_pool[rte_socket_id()]);
}

int
gatekeeperd_init_network(uint64_t cpu_mask, uint64_t port_mask, uint8_t *out_num_ports)
{
	int ret;
	size_t i;

	size_t num_numa_nodes = 0;
	uint16_t num_queues = 0;

	assert(rte_lcore_count() <= GATEKEEPERD_MAX_LCORES);

	/* count required queues */
	for (i = 0; i < rte_lcore_count(); i++)
	{
		if ((cpu_mask & ((uint64_t)1 << i)) != 0)
			num_queues++;
	}
	assert(num_queues <= GATEKEEPERD_MAX_QUEUES);

	/* count numa nodes */
	for (i = 0; i < rte_lcore_count(); i++)
	{
		uint32_t socket_id = (uint32_t)rte_lcore_to_socket_id((unsigned int)i);
		if (num_numa_nodes <= socket_id)
			num_numa_nodes = socket_id + 1;
	}
	assert(num_numa_nodes <= GATEKEEPERD_MAX_NUMA_NODES);

	/* initialize pktmbuf */
	for (i = 0; i < num_numa_nodes; i++)
	{
		printf("allocating pktmbuf on node %zu... \n", i);
		char pool_name[64];
		snprintf(pool_name, sizeof(pool_name), "pktmbuf_pool%zu", i);
		
        /* the maximum cache size can be adjusted in DPDK's .config file: CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE */
		const unsigned int cache_size = GATEKEEPERD_MAX_PORTS * 1024;
		gatekeeperd_pktmbuf_pool[i] = rte_mempool_create(pool_name, GATEKEEPERD_MBUF_SIZE, GATEKEEPERD_MBUF_ENTRY_SIZE, cache_size, sizeof(struct rte_pktmbuf_pool_private), rte_pktmbuf_pool_init, NULL, rte_pktmbuf_init, NULL, (int)i, 0);

		if (gatekeeperd_pktmbuf_pool[i] == NULL)
		{
			fprintf(stderr, "failed to allocate mbuf for numa node %zu\n", i);
			return 0;
		}
	}

	/* TODO: initialize PMD driver */

	printf("probing PCI\n");
	if (rte_eal_pci_probe() < 0)
	{
		fprintf(stderr, "failed to probe PCI\n");
		return 0;
	}

	/* check port and queue limits */
	uint8_t num_ports = rte_eth_dev_count();
	assert(num_ports <= GATEKEEPERD_MAX_PORTS);
	*out_num_ports = num_ports;

	printf("checking queue limits\n");
	uint8_t port_id;
	for (port_id = 0; port_id < num_ports; port_id++)
	{
		if ((port_mask & ((uint64_t)1 << port_id)) == 0)
			continue;

		struct rte_eth_dev_info dev_info;
		rte_eth_dev_info_get((uint8_t)port_id, &dev_info);

		if (num_queues > dev_info.max_tx_queues || num_queues > dev_info.max_rx_queues)
		{
			fprintf(stderr, "device supports too few queues\n");
			return 0;
		}
	}

    /* initialize ports */
	for (port_id = 0; port_id < num_ports; port_id++)
	{
		if ((port_mask & ((uint64_t)1 << port_id)) == 0)
			continue;

		printf("initializing port %hhu...\n", port_id);

		ret = rte_eth_dev_configure(port_id, num_queues, num_queues, &gatekeeperd_port_conf);
		if (ret < 0)
		{
			fprintf(stderr, "failed to configure port %hhu (err=%d)\n", port_id, ret);
			return 0;
		}

		uint32_t lcore;
		for (lcore = 0; lcore < rte_lcore_count(); lcore++)
		{
			uint16_t queue = (uint16_t)lcore;

			size_t numa_node = rte_lcore_to_socket_id((unsigned int)lcore);

			ret = rte_eth_rx_queue_setup(port_id, queue, (unsigned int)nb_rx_desc, (unsigned int)numa_node, &gatekeeperd_rx_conf, gatekeeperd_pktmbuf_pool[numa_node]);
			if (ret < 0)
			{
				fprintf(stderr, "failed to configure port %hhu rx_queue %hu (err=%d)\n", port_id, queue, ret);
				return 0;
			}

			ret = rte_eth_tx_queue_setup(port_id, queue, (unsigned int)nb_tx_desc, (unsigned int)numa_node, &gatekeeperd_tx_conf);
			if (ret < 0)
			{
				fprintf(stderr, "failed to configure port %hhu tx_queue %hu (err=%d)\n", port_id, queue, ret);
				return 0;
			}
		}

		/* start device */
		ret = rte_eth_dev_start(port_id);
		if (ret < 0)
		{
			fprintf(stderr, "failed to start port %hhu (err=%d)\n", port_id, ret);
			return 0;
		}
	}

	for (port_id = 0; port_id < num_ports; port_id++)
	{
		if ((port_mask & ((uint64_t)1 << port_id)) == 0)
			continue;

		printf("querying port %hhu... ", port_id);
		fflush(stdout);

		struct rte_eth_link link;
		rte_eth_link_get(port_id, &link);
		if (!link.link_status)
		{
			printf("link down\n");
			return 0;
		}

		printf("%hu Gbps (%s)\n", link.link_speed / 1000, (link.link_duplex == ETH_LINK_FULL_DUPLEX) ? ("full-duplex") : ("half-duplex"));
	}

	uint32_t lcore;
	memset(gatekeeperd_queue_states, 0, sizeof(gatekeeperd_queue_states));
	for (port_id = 0; port_id < num_ports; port_id++) 
    {
		for (lcore = 0; lcore < rte_lcore_count(); lcore++)
		{
			uint16_t queue = (uint16_t)lcore;

            /* 
             * is allocated by corresponding lcore more efficient???
             */
			//gatekeeperd_queue_states[queue * GATEKEEPERD_MAX_PORTS + port_id] = gatekeeperd_eal_malloc_lcore(sizeof(struct gatekeeperd_queue_state), lcore);
			gatekeeperd_queue_states[queue * GATEKEEPERD_MAX_PORTS + port_id] = rte_zmalloc(NULL, sizeof(struct gatekeeperd_queue_state), 0);
		}
    }

	return 1;
}

void
gatekeeperd_free_network(uint64_t port_mask)
{
	uint8_t port_id;
	uint8_t num_ports = rte_eth_dev_count();
	
	for (port_id = 0; port_id < num_ports; port_id++)
	{
		if ((port_mask & ((uint64_t)1 << port_id)) == 0)
			continue;

		printf("stopping port %hhu...\n", port_id);
		rte_eth_dev_stop(port_id);
	}

	for (port_id = 0; port_id < num_ports; port_id++)
	{
		if ((port_mask & ((uint64_t)1 << port_id)) == 0)
			continue;

		printf("closing port %hhu...\n", port_id);
		rte_eth_dev_close(port_id);
	}
}
