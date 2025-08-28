/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: ethernet.c
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
 *  \file ethernet.c
 *
 *  Ethernet layer implementation.
 */

//#define __DEBUG

#define __USE_MISC              // to get // IFF_* macro defs

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <endian.h>
#include <netinet/in.h>         // IFF_* macros
#include <kernel/laylaos.h>
#include <kernel/net.h>
#include <kernel/mutex.h>
#include <kernel/asm.h>
#include <kernel/net.h>
#include <kernel/net/ether.h>
#include <kernel/net/netif.h>
#include <kernel/net/route.h>
#include <kernel/net/arp.h>
#include <kernel/net/dhcp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv4_addr.h>
#include <mm/kheap.h>

uint8_t ethernet_broadcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

int ethernet_count = 0;


/*
 * Interface attach.
 */
int ethernet_attach(struct netif_t *ifp)
{
    if(!ifp)
    {
        return -EINVAL;
    }

    if(netif_attach(ifp) == 0)
    {
        ksprintf(ifp->name, sizeof(ifp->name), "eth%d", ethernet_count++);

    	printk("eth: attached interface %s\n", ifp->name);
    	//__asm__ __volatile__("xchg %%bx, %%bx"::);
    
        // Obtain network parameters using DHCP
        if(!route_for_ifp(ifp))
        {
        	printk("%s: starting DHCP discovery\n", ifp->name);
        	//__asm__ __volatile__("xchg %%bx, %%bx"::);

            dhcp_start(ifp);
        }
    }
    
    return 0;
}


/*
 * Ethernet receive.
 */
void ethernet_receive(struct netif_t *ifp, struct packet_t *p)
{
    struct ether_header_t *eh;
    
    if(!p)
    {
        return;
    }
    
    if(!ifp || !(ifp->flags & IFF_UP))
    {
        free_packet(p);
        return;
    }

    eh = (struct ether_header_t *)p->data;
    p->ifp = ifp;
    netstats.link.recv++;
    
    if(p->count < ETHER_HLEN)
    {
        printk("eth: dropped packet with too short length\n");
        netstats.link.lenerr++;
        netstats.link.drop++;
        free_packet(p);
        return;
    }
    
    if(memcmp(ethernet_broadcast, eh->dest, ETHER_ADDR_LEN) == 0)
    {
        p->flags |= PACKET_FLAG_BROADCAST;
    }
    else if(eh->dest[0] & 1)
    {
        p->flags |= PACKET_FLAG_BROADCAST;
    }

    if(p->flags & PACKET_FLAG_BROADCAST)
    {
        ifp->stats.multicast++;
    }

    switch(ntohs(eh->type))
    {
        case ETHERTYPE_IP:
            //packet_add_header(p, -ETHER_HLEN);
            ipv4_recv(p);
            break;

        case ETHERTYPE_ARP:
            arp_recv(p);
            break;
        
        case ETHERTYPE_IPv6:
        default:
            //KDEBUG("ethernet_receive: 4 UNKNOWN (0x%x)\n", ntohs(eh->type));
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            ifp->stats.rx_dropped++;
            netstats.link.drop++;
            free_packet(p);
            break;
    }
}


/*
 * Ethernet send.
 */
int ethernet_send(struct netif_t *ifp, struct packet_t *p, 
                  uint8_t *hwdest)
{
    struct ether_header_t *h;
    int i;
    
    if(!p || !hwdest)
    {
        return -EINVAL;
    }
    
    if(!ifp || (ifp->flags & IFF_UP) != IFF_UP)
    {
        printk("eth: interface down -- dropping packet\n");
        free_packet(p);
        return -ENETDOWN;
    }

    KDEBUG("ethernet_send: p->count 0x%x, p 0x%x, p->data 0x%x, hlen 0x%x\n", p->count, p, p->data, ETHER_HLEN);
    
    if(packet_add_header(p, ETHER_HLEN) != 0)
    {
        printk("eth: insufficient memory for packet header\n");
        netstats.link.err++;
        free_packet(p);
        return -ENOBUFS;
    }

    KDEBUG("ethernet_send: p->count 0x%x\n", p->count);

    h = (struct ether_header_t *)p->data;
    h->type = htons(ETHERTYPE_IP);

    for(i = 0; i < ETHER_ADDR_LEN; i++)
    {
        h->dest[i] = hwdest[i];
        h->src[i] = ifp->hwaddr[i];
    }

    /*
     * NOTE: it is the transmitting function's duty to free the packet!
     *       we leave this to the transmitting function, as it may queue the
     *       packet instead of sending it right away.
     */
    
    if((i = ifp->transmit(ifp, p)) < 0)
    {
        printk("eth: failed to send packet (err %d)\n", i);
        return i;
    }

    return 0;
}

