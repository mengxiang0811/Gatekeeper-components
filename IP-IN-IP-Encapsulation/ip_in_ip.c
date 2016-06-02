#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_hash_crc.h>
#include <rte_byteorder.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>

#include "ip_in_ip.h"

static struct ipv4_hdr ip_hdr_template[SERVER_MAX_PORTS];
static struct ether_hdr l2_hdr_template[SERVER_MAX_PORTS];

/*
 * TODO: Read IP in iP Tunneling configuration information
 * We may need to configurate each port separately
 *
 */
	void
init_hdr_templates(void)
{
	memset(ip_hdr_template, 0, sizeof(ip_hdr_template));
	memset(l2_hdr_template, 0, sizeof(l2_hdr_template));

	ip_hdr_template[0].version_ihl = IP_VHL_DEF;
	ip_hdr_template[0].type_of_service = 0; 
	ip_hdr_template[0].total_length = 0; 
	ip_hdr_template[0].packet_id = 0;
	ip_hdr_template[0].fragment_offset = IP_DN_FRAGMENT_FLAG;
	ip_hdr_template[0].time_to_live = IP_DEFTTL;
	ip_hdr_template[0].next_proto_id = IPPROTO_IPIP;
	ip_hdr_template[0].hdr_checksum = 0;
	ip_hdr_template[0].src_addr = rte_cpu_to_be_32(0x00000000);
	ip_hdr_template[0].dst_addr = rte_cpu_to_be_32(0xFFFFFFFF);

	l2_hdr_template[0].d_addr.addr_bytes[0] = 0x0a;
	l2_hdr_template[0].d_addr.addr_bytes[1] = 0x00;
	l2_hdr_template[0].d_addr.addr_bytes[2] = 0x27;
	l2_hdr_template[0].d_addr.addr_bytes[3] = 0x00;
	l2_hdr_template[0].d_addr.addr_bytes[4] = 0x00;
	l2_hdr_template[0].d_addr.addr_bytes[5] = 0x01;

	l2_hdr_template[0].s_addr.addr_bytes[0] = 0x08;
	l2_hdr_template[0].s_addr.addr_bytes[1] = 0x00;
	l2_hdr_template[0].s_addr.addr_bytes[2] = 0x27;
	l2_hdr_template[0].s_addr.addr_bytes[3] = 0x7d;
	l2_hdr_template[0].s_addr.addr_bytes[4] = 0xc7;
	l2_hdr_template[0].s_addr.addr_bytes[5] = 0x68;

	l2_hdr_template[0].ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

	return;
}

/*
 * Parse an ethernet header to fill the ethertype, outer_l2_len, outer_l3_len and
 * ipproto. This function is able to recognize IPv4/IPv6 with one optional vlan
 * header.
 */
	static void
parse_ethernet(struct ether_hdr *eth_hdr, union tunnel_offload_info *info,
		uint8_t *l4_proto)
{
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;
	uint16_t ethertype;

	info->outer_l2_len = sizeof(struct ether_hdr);
	ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (ethertype == ETHER_TYPE_VLAN) {
		struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);
		info->outer_l2_len  += sizeof(struct vlan_hdr);
		ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
	}

	switch (ethertype) {
		case ETHER_TYPE_IPv4:
			ipv4_hdr = (struct ipv4_hdr *)
				((char *)eth_hdr + info->outer_l2_len);
			info->outer_l3_len = sizeof(struct ipv4_hdr);
			*l4_proto = ipv4_hdr->next_proto_id;
			break;
		case ETHER_TYPE_IPv6:
			ipv6_hdr = (struct ipv6_hdr *)
				((char *)eth_hdr + info->outer_l2_len);
			info->outer_l3_len = sizeof(struct ipv6_hdr);
			*l4_proto = ipv6_hdr->proto;
			break;
		default:
			info->outer_l3_len = 0;
			*l4_proto = 0;
			break;
	}
}

	int
decapsulation(struct rte_mbuf *pkt)
{
	uint8_t l4_proto = 0;
	uint16_t outer_header_len;

	union tunnel_offload_info info = { .data = 0 };

	struct ether_hdr *phdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

	parse_ethernet(phdr, &info, &l4_proto);

	if (l4_proto != IPPROTO_IPIP)
		return -1;

	outer_header_len = info.outer_l2_len + info.outer_l3_len;

	printf("In decapsulation: the outer header len is = %d\n", outer_header_len);

	rte_pktmbuf_adj(pkt, outer_header_len);

	return 0;
}

/*
 * TODO: test the performance of the encapsulation function
 *
 * main overhead: 1 rte_pktmbuf_prepend + 2 rte_memcpy
 */
	void
encapsulation(struct rte_mbuf *m, uint8_t port_id)
{
	uint64_t ol_flags = 0;
	//union tunnel_offload_info info = { .data = 0 };
	//struct ether_hdr *phdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	/* allocate space for new Ethernet + IPv4 header */
	struct ether_hdr *pneth = (struct ether_hdr *) rte_pktmbuf_prepend(m,
			sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr));

	struct ipv4_hdr *ip = (struct ipv4_hdr *) &pneth[1];

	/* 
	 * We may need new MAC addresses after tunneling
	 * Since we can only fill the Ethernet header and new IP header,
	 * the overhead should be slight.
	 *
	 * Fill up the Ethernet + IP headers with our templates
	 */
	pneth = rte_memcpy(pneth, &l2_hdr_template[port_id],
			sizeof(struct ether_hdr));

	ip = rte_memcpy(ip, &ip_hdr_template[port_id],
			sizeof(struct ipv4_hdr));

	ip->total_length = rte_cpu_to_be_16(m->data_len
			- sizeof(struct ether_hdr));

	/*
	 * configure TX offload on a ip-in-ip-encapsulated tcp packet:
	 *      out_eth/out_ip/in_ip/in_tcp/payload
	 *
	 * Now, only calculate outer IP checksum
	 * if necessary, change here to calculate checksum of out_ip, in_ip, in_tcp
	 */
	ip->hdr_checksum = 0;

	//ol_flags |= PKT_TX_OUTER_IP_CKSUM;
	//m->outer_l2_len = sizeof(struct ether_hdr);
	//m->outer_l3_len = sizeof(struct ipv4_hdr);
	//m->ol_flags |= ol_flags;

	ol_flags |= (PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_OUTER_IPV4);
	//m->l2_len = sizeof(struct ether_hdr);
	//m->l3_len = sizeof(struct ipv4_hdr);
	m->outer_l2_len = sizeof(struct ether_hdr);
	m->outer_l3_len = sizeof(struct ipv4_hdr);
	m->ol_flags |= ol_flags;

	return;
}
