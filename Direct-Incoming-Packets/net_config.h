#pragma once

#define GATEKEEPERD_MAX_PORTS (4)

struct gatekeeperd_port_conf
{
	uint8_t mac_addr[6];
	uint8_t ip_addr[4];
};

// gatekeeper server configuration
struct gatekeeperd_server_conf
{
	uint8_t num_ports;
	struct gatekeeperd_port_conf ports[GATEKEEPERD_MAX_PORTS];
	uint8_t num_threads;
};

// TODO: grantor configuration

// function
struct gatekeeperd_server_conf *
gatekeeperd_get_server_conf(const char *filename, const char *server_name);
