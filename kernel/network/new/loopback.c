/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: loopback.c
 *    This file is part of LaylaOS.
 *
 *    LaylaOS is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    LaylaOS is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with LaylaOS.  If not, see <http://www.gnu.org/licenses/>.
 */    

/**
 *  \file loopback.c
 *
 *  Loopback device implementation.
 */

//#define __DEBUG
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/mutex.h>
#include <kernel/net.h>
#include <kernel/net/packet.h>
#include <kernel/net/netif.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>
#include <kernel/net/icmp4.h>
#include <kernel/net/icmp6.h>
#include <kernel/net/raw.h>
#include <kernel/net/checksum.h>

#define LO_MTU          65536
#define LO_QUEUE_MAX    128

static struct netif_t loop_netif = { 0, };
static struct netif_t *ifp = &loop_netif;

struct netif_queue_t loopback_outq = { 0, };

int loopback_transmit(struct netif_t *ifp, struct packet_t *p);
void loopback_process_input(struct netif_t *ifp);


int loop_attach(void)
{
    struct in_addr ipv4, netmask4;
    struct in6_addr ipv6 = { 0, }, netmask6 = { 0, };

    ifp->unit = 0;
    strcpy(ifp->name, "lo0");
    ifp->flags = (IFF_UP | IFF_LOOPBACK);
	ifp->transmit = loopback_transmit;
	ifp->process_input = loopback_process_input;
	ifp->process_output = NULL;
	ifp->mtu = LO_MTU;
	ifp->inq = NULL;
	ifp->outq = NULL;
	A_memset(&(ifp->ethernet_addr.addr), 0, ETHER_ADDR_LEN);
	
	loopback_outq.max = LO_QUEUE_MAX;
	
	netif_add(ifp);

    string_to_ipv4("127.0.0.1", &ipv4.s_addr);
    string_to_ipv4("255.0.0.0", &netmask4.s_addr);
    ipv4_link_add(ifp, &ipv4, &netmask4);

    string_to_ipv6("::1", ipv6.s6_addr);
    string_to_ipv6("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", netmask6.s6_addr);
    ipv6_link_add(ifp, &ipv6, &netmask6, NULL);
    
    ipv6_ifp_routing_enable(ifp);
    
    return 0;
}


int loopback_transmit(struct netif_t *ifp, struct packet_t *p)
{
    struct ipv4_hdr_t *iph;

    //UNUSED(ifp);

    packet_add_header(p, -ETHER_HLEN);
    iph = (struct ipv4_hdr_t *)p->data;
    
    KDEBUG("loopback_transmit: ifp %lx, p %lx\n", ifp, p);
    KDEBUG("loopback_transmit: ipv%d, proto %d, icmp %d\n", GET_IP_VER(iph), iph->proto, IPPROTO_ICMP);

    // if this is an ICMP echo request, change it to an ICMP reply so that
    // tools like ping work properly
    
    if(GET_IP_VER(iph) == 4 && iph->proto == IPPROTO_ICMP)
    {
        struct icmp4_hdr_t *icmph = (struct icmp4_hdr_t *)p->transport_hdr;

        if(icmph->type == ICMP_MSG_ECHO)
        {
            uint32_t tmp = iph->dest.s_addr;

            icmph->type = ICMP_MSG_ECHOREPLY;

            // swap src & dest
            iph->dest.s_addr = iph->src.s_addr;
            iph->src.s_addr = tmp;

            // update checksum
            icmp4_checksum(p);
        }
    }
    else if(GET_IP_VER(iph) == 6 && iph->proto == IPPROTO_ICMPV6)
    {
        struct ipv6_hdr_t *iph6 = (struct ipv6_hdr_t *)p->data;
        struct icmp6_hdr_t *icmph = (struct icmp6_hdr_t *)p->transport_hdr;
        struct in6_addr tmp = { 0, };

        if(icmph->type == ICMP6_MSG_ECHO_REQUEST)
        {
            icmph->type = ICMP6_MSG_ECHO_REPLY;
            icmph->code = 0;

            // swap src & dest
            IPV6_COPY(tmp.s6_addr, iph6->dest.s6_addr);
            IPV6_COPY(iph6->dest.s6_addr, iph6->src.s6_addr);
            IPV6_COPY(iph6->src.s6_addr, tmp.s6_addr);

            // update checksum
            icmph->checksum = 0;
            icmph->checksum = htons(icmp6_checksum(p));
        }
    }

    packet_add_header(p, ETHER_HLEN);

    if(IFQ_FULL(&loopback_outq))
    {
        KDEBUG("loopback_transmit: dropping packet - queue full\n");
        loop_netif.stats.rx_dropped++;
        netstats.link.drop++;
        packet_free(p);
        return -ENOBUFS;
    }
    else
    {
        KDEBUG("loopback_transmit: queuing packet\n");
        IFQ_ENQUEUE(&loopback_outq, p);
        loop_netif.stats.rx_packets++;
        loop_netif.stats.rx_bytes += p->count;
        return 0;
    }
}


void loopback_process_input(struct netif_t *ifp)
{
    UNUSED(ifp);

    //KDEBUG("loopback_process_input: ifp %lx\n", ifp);

    struct packet_t *p;

    while(1)
    {
        IFQ_DEQUEUE(&loopback_outq, p);
        
        if(!p)
        {
            break;
        }
        
        IFQ_ENQUEUE(&ethernet_inq, p);
    }
}

