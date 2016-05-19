/*
 * TODO: add License Header
 *
 *  The program references to the following materials:
 *  (1) the DPDK API documents: http://dpdk.org/doc/api/
 *  (2) the DPDK examples: http://dpdk.org/doc/api/examples.html
 *  (3) MICA implementation: https://github.com/efficient/mica
 *  (4) the Symmetric Receive-side Scaling key is from: 
 *      http://www.ndsl.kaist.edu/~kyoungsoo/papers/TR-symRSS.pdf
 *
 *  The source code uses the DPDK Coding Style:
 *  http://dpdk.org/doc/guides/contributing/coding_style.html
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
/* Use queue(3) macros rather than rolling your own lists, whenever possible */
#include <sys/queue.h> 
#include <assert.h>
#include <signal.h>

#include <rte_eal.h>

#include <rte_launch.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_debug.h>
#include <rte_errno.h>

#include "net_common.h"
#include "net_config.h"

static volatile int exiting = 0;

static void
signal_handler(int signum)
{
    if (signum == SIGINT)
        fprintf(stderr, "caught SIGINT\n");
    else if (signum == SIGTERM)
        fprintf(stderr, "caught SIGTERM\n");
    else
        fprintf(stderr, "caught unknown signal\n");
    exiting = 1;
}

/* 
 * TODO: add gatekeeperd server function 
 * 
 * The capabilities maintainance may happen here,
 * including capabilities lookup, insertion, deletion, etc.
 *
 * The server will recieve packets, queue packets, check packets' capabilities,
 * schedule packets, drop packets, transmit packets, etc.
 */
static int
gatekeekperd_server_proc(void *arg) 
{
	uint32_t lcore = rte_lcore_id();
	printf("Gatekeeperd deamon is running at lcore = %u\n", lcore);
    while (!exiting) 
    {
    }
    return 0;
}

/*
 *  This program setups the DPDK with RSS feature, i.e., 
 *  the program will create a queue for each lcore in the NIC, 
 *  and ask the NIC to hash the source and destination IP addresses 
 *  of the packets and place the packets in queues based on the hash.
 */
int
main(int argc, char **argv)
{
    int ret;

    /* 
     * TODO: define the format of the command, and parse the command; 
     *      create the configuration file for the gatekeeperd server!
     */
    struct gatekeeperd_server_conf *gatekeeperd_conf = gatekeeperd_get_server_conf(argv[1], argv[2]);

    printf("initializing DPDK\n");
 
    /* enable the log type */
    rte_set_log_type(RTE_LOG_NOTICE, 1);
    /* log in notice level */
    rte_set_log_level(RTE_LOG_NOTICE);
    
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_panic("Cannot init EAL\n");

#ifdef GATEKEEPER_NETWORK
    /* 
     * TODO: setup the aplication lcores 
     */

	uint8_t num_ports_max = 0;
    /* should be in the configuration file*/
    uint64_t cpu_mask = ((uint64_t)1 << 4) - 1;
	uint64_t port_mask = ((size_t)1 << 4) - 1;

	if (!gatekeeperd_init_network(cpu_mask, port_mask, &num_ports_max))
	{
		fprintf(stderr, "failed to initialize network\n");
		return -1;
	}
	assert(4 <= num_ports_max);

	printf("setting MAC address\n");
    uint8_t port_id;
    for (port_id = 0; port_id < gatekeeperd_conf->num_ports; port_id++)
    {
    	struct ether_addr mac_addr;
    	memcpy(&mac_addr, gatekeeperd_conf->ports[port_id].mac_addr, sizeof(struct ether_addr));
    	if (rte_eth_dev_mac_addr_add(port_id, &mac_addr, 0) != 0)
    	{
			fprintf(stderr, "failed to add a MAC address\n");
			return -1;
    	}
    }

#endif

    printf("running gatekeeper servers\n");
    struct sigaction new_action;
    new_action.sa_handler = signal_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGINT, &new_action, NULL);
    sigaction(SIGTERM, &new_action, NULL);

    uint8_t thread_id;
    for (thread_id = 1; thread_id < 4; thread_id++)
        rte_eal_remote_launch(gatekeekperd_server_proc, NULL, (unsigned int)thread_id);

    gatekeekperd_server_proc(NULL);

    rte_eal_mp_wait_lcore();
    
    /* 
     * TODO: release all the resources
     */

    printf("finished\n");
    
    return 0;
}
