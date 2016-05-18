#ifndef _NET_CONFIG_H
#define _NET_CONFIG_H

#include <stdint.h>

#define GATEKEEPERD_MAX_PORTS (4)

struct gatekeeperd_port_conf
{
	uint8_t mac_addr[6];
	uint8_t ip_addr[4];
};

/* 
 * gatekeeper server configuration
 *
 * may incorporate IP-in-IP tunneling information,
 * VLAN header information, etc.
 */
struct gatekeeperd_server_conf
{
	uint8_t num_ports;
	struct gatekeeperd_port_conf ports[GATEKEEPERD_MAX_PORTS];
	uint8_t num_threads;
};

/* 
 * TODO: grantor configuration
 */

/* function */
struct gatekeeperd_server_conf *
gatekeeperd_get_server_conf(const char *filename, const char *server_name);

#endif
