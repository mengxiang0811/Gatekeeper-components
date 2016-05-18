/*
 *  main.c
 *  
 *  This program setups the DPDK with RSS feature, i.e., 
 *  the program will create a queue for each lcore in the NIC, 
 *  and ask the NIC to hash the source and destination IP addresses 
 *  of the packets and place the packets in queues based on the hash.
 *
 *  The program references to the following materials:
 *  (1) the DPDK API documents: http://dpdk.org/doc/api/
 *  (2) the DPDK examples: http://dpdk.org/doc/api/examples.html
 *  (3) MICA implementation: https://github.com/efficient/mica
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_log.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_debug.h>

#include "net_common.h"
#include "net_config.h"

static volatile bool exiting = false;

static void signal_handler(int signum)
{
    if (signum == SIGINT)
        fprintf(stderr, "caught SIGINT\n");
    else if (signum == SIGTERM)
        fprintf(stderr, "caught SIGTERM\n");
    else
        fprintf(stderr, "caught unknown signal\n");
    exiting = true;
}

/* TODO: add gatekeeperd server function */
static int gatekeekperd_server_proc(void *arg) 
{
    while (!exiting) 
    {
    }
    return 0;
}

    int
main(int argc, char **argv)
{
    int ret;
    unsigned lcore_id;

    /* TODO: define the format of the command, and parse the command; 
     *      create the configuration file for the gatekeeperd server!
     * */
    struct gatekeeperd_server_conf *gatekeeperd_conf = gatekeeperd_get_server_conf(argv[1], argv[2]);

    printf("initializing DPDK\n");
    
    rte_set_log_level(RTE_LOG_NOTICE);
    
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_panic("Cannot init EAL\n");

    /* TODO: setup the aplication lcores */

    /* should be in the configuration file*/
    uint64_t cpu_mask = ((uint64_t)1 << 4) - 1;
	uint64_t port_mask = ((size_t)1 << 4) - 1;

	if (!gatekeeperd_init_network(cpu_mask, port_mask, &num_ports_max))
	{
		fprintf(stderr, "failed to initialize network\n");
		return;
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
			return;
    	}
    }

    printf("running gatekeeper servers\n");
    struct sigaction new_action;
    new_action.sa_handler = signal_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGINT, &new_action, NULL);
    sigaction(SIGTERM, &new_action, NULL);

    uint8_t thread_id;
    for (thread_id = 1; thread_id < workload_conf->num_threads; thread_id++)
        rte_eal_remote_launch(gatekeekperd_server_proc, NULL, (unsigned int)thread_id);

    gatekeekperd_server_proc(NULL);

    rte_eal_mp_wait_lcore();
    
    /* TODO: release all the resources */

    printf("finished\n");
    
    return 0;
}
