/*
 *  main.c
 *  
 *  This program setups the DPDK with RSS feature, i.e., 
 *  the program will create a queue for each lcore in the NIC, 
 *  and ask the NIC to hash the source and destination IP addresses 
 *  of the packets and place the packets in queues based on the hash.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>

int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");


	/* TODO: setup the aplication lcores */
	
    rte_eal_mp_wait_lcore();
	return 0;
}
