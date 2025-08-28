/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
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
#include <kernel/mutex.h>
#include <kernel/net.h>
#include <kernel/net/socket.h>
#include <kernel/net/packet.h>
#include <kernel/net/netif.h>
#include <kernel/net/ether.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/route.h>
#include <kernel/net/icmpv4.h>
#include <kernel/net/checksum.h>

#define LO_MTU          65536

static int loopback_transmit(struct netif_t *ifp, struct packet_t *p);
static void loopback_func(void *unused);

static struct netif_queue_t loopback_outq = { 0, };
static struct netif_t loop_netif = { 0, };
static struct netif_t *loifp = &loop_netif;
static volatile struct task_t *loopback_task = NULL;


void loop_attach(void)
{
    loifp->unit = 0;
    strcpy(loifp->name, "lo0");
    loifp->flags = (IFF_UP | IFF_LOOPBACK);
	loifp->transmit = loopback_transmit;
	loifp->mtu = LO_MTU;
	A_memset(&(loifp->hwaddr), 0, ETHER_ADDR_LEN);

	loopback_outq.max = SOCKET_DEFAULT_QUEUE_SIZE;

	netif_attach(loifp);
    route_add_ipv4(htonl(0x7f000001), 0, htonl(0xff000000), RT_LOOPBACK, 0, loifp);

    (void)start_kernel_task("lo0", loopback_func, NULL, &loopback_task, 0);
}


static void loopback_func(void *unused)
{
    struct packet_t *p;

    UNUSED(unused);

    while(1)
    {
        kernel_mutex_lock(&loopback_outq.lock);
        IFQ_DEQUEUE(&loopback_outq, p);
        kernel_mutex_unlock(&loopback_outq.lock);

        if(p)
        {
            ethernet_receive(loifp, p);
        }
        else
        {
            block_task(&loopback_outq, 1);
        }
    }
}


static int loopback_transmit(struct netif_t *ifp, struct packet_t *p)
{
    struct ipv4_hdr_t *iph;
    int res;

    packet_add_header(p, -ETHER_HLEN);
    iph = (struct ipv4_hdr_t *)p->data;

    // if this is an ICMP echo request, change it to an ICMP reply so that
    // tools like ping work properly

    if(iph->ver == 4 && iph->proto == IPPROTO_ICMP)
    {
        int hlen = iph->hlen * 4;
        struct icmp_echo_header_t *icmph =
                (struct icmp_echo_header_t *)((char *)p->data + hlen);

        if(icmph->type == ICMP_MSG_ECHO)
        {
            uint32_t tmp = iph->dest;

            icmph->type = ICMP_MSG_ECHOREPLY;

            // swap src & dest
            iph->dest = iph->src;
            iph->src = tmp;

            // update checksum
            icmph->checksum = 0;
            icmph->checksum = 
                    inet_chksum((uint16_t *)((char *)p->data + hlen), 
                                        p->count - hlen, 0);
        }
    }
    else if(iph->ver == 6 && iph->proto == IPPROTO_ICMPV6)
    {
        /*
         * FIXME: We only support IPv4 for now.
         */
    }

    packet_add_header(p, ETHER_HLEN);

    ifp->stats.rx_packets++;
    ifp->stats.rx_bytes += p->count;

    kernel_mutex_lock(&loopback_outq.lock);

    if(IFQ_FULL(&loopback_outq))
    {
        kernel_mutex_unlock(&loopback_outq.lock);
        ifp->stats.rx_dropped++;
        netstats.link.drop++;
        free_packet(p);
        res = -ENOBUFS;
    }
    else
    {
        IFQ_ENQUEUE(&loopback_outq, p);
        kernel_mutex_unlock(&loopback_outq.lock);
        netstats.link.recv++;
        res = 0;
    }

    unblock_task(loopback_task);
    return res;
}

