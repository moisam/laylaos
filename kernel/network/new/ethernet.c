/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
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
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/net.h>
#include <kernel/net/packet.h>
#include <kernel/net/netif.h>
#include <kernel/net/ether.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/dhcp.h>
#include <kernel/net/icmp4.h>
#include <kernel/net/checksum.h>

uint8_t ethernet_broadcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
uint8_t ethernet_multicast[6] = { 0x01, 0x00, 0x5e, 0x00, 0x00, 0x00 };
uint8_t ethernet_multicast6[6] = { 0x33, 0x33, 0x00, 0x00, 0x00, 0x00 };

struct netif_queue_t ethernet_inq = { 0, };
struct netif_queue_t ethernet_outq = { 0, };

int ethernet_count = 0;


/*
 * Add an interface.
 */
int ethernet_add(struct netif_t *ifp)
{
    struct netif_t *tmp;

    if(!ifp)
    {
        return -EINVAL;
    }

    // do not reattach the interface if it is already in the list
    for(tmp = netif_list; tmp != NULL; tmp = tmp->next)
    {
        if(tmp == ifp)
        {
            goto next;
        }
    }
    
    ksprintf(ifp->name, sizeof(ifp->name), "eth%d", ethernet_count++);
    netif_add(ifp);

next:
	printk("eth: added interface %s\n", ifp->name);
	
    // Obtain network parameters using DHCP
	if(!ipv4_link_by_ifp(ifp))
    {
    	printk("%s: starting DHCP discovery\n", ifp->name);

        dhcp_initiate_negotiation(ifp, NULL, &ifp->dhcp_xid);
    }
    
    return 0;
}


/*
 * Ethernet receive.
 */
int ethernet_receive(struct packet_t *p)
{
    struct ether_header_t *eh = NULL;
    
    if(!p)
    {
        KDEBUG("eth-recv: null packet\n");
        return -EINVAL;
    }

    if(!p->ifp /* || !!(p->ifp->flags & IFF_UP) */)
    {
        KDEBUG("eth-recv: packet with invalid ifp\n");
        packet_free(p);
        return -ENETDOWN;
    }
    
    eh = (struct ether_header_t *)p->data;
    
    if(p->count < ETHER_HLEN)
    {
        KDEBUG("eth-recv: dropped packet with too short length\n");
        netstats.link.lenerr++;
        netstats.link.drop++;
        packet_free(p);
        return -EINVAL;
    }
    
    if(memcmp(&(eh->dest), &(p->ifp->ethernet_addr), ETHER_ADDR_LEN) &&
       memcmp(&(eh->dest), ethernet_multicast, 3) &&
       memcmp(&(eh->dest), ethernet_multicast6, 2) &&
       memcmp(&(eh->dest), ethernet_broadcast, ETHER_ADDR_LEN))
    {
        KDEBUG("eth-recv: dropped packet with invalid destination\n");
        packet_free(p);
        netstats.link.drop++;
        return -EINVAL;
    }
    
    if(memcmp(&(eh->dest), ethernet_broadcast, ETHER_ADDR_LEN) == 0)
    {
        KDEBUG("eth-recv: broadcast package\n");
        p->flags |= PACKET_FLAG_BROADCAST;
        p->ifp->stats.multicast++;
    }

    KDEBUG("eth-recv: packet with length %u\n", p->count);

    //p->ifp->stats.rx_bytes += p->count;

    switch(ntohs(eh->type))
    {
        case ETHERTYPE_ARP:
            KDEBUG("eth-recv: ARP (0x%x)\n", ntohs(eh->type));
            arp_receive(p);
            return 0;

        case ETHERTYPE_IP:
            KDEBUG("eth-recv: IPv4 (0x%x)\n", ntohs(eh->type));
            packet_add_header(p, -ETHER_HLEN);
            p->incoming_iphdr = p->data;
            
            if(GET_IP_VER((struct ipv4_hdr_t *)p->data) == 4)
            {
                kernel_mutex_lock(&ipv4_inq.lock);

                if(IFQ_FULL(&ipv4_inq))
                {
                    KDEBUG("eth-recv: queue full - dropping IPv4 packet\n");
                    kernel_mutex_unlock(&ipv4_inq.lock);
                    //p->ifp->stats.rx_dropped++;
                    netstats.link.drop++;
                    packet_free(p);
                }
                else
                {
                    KDEBUG("eth-recv: queueing IPv4 packet\n");
                    IFQ_ENQUEUE(&ipv4_inq, p);
                    kernel_mutex_unlock(&ipv4_inq.lock);
                    netstats.link.recv++;
                }

                return 0;
            }

            icmp4_param_problem(p, 0);
            break;

        case ETHERTYPE_IPv6:
            KDEBUG("eth-recv: IPv6 (0x%x)\n", ntohs(eh->type));
            packet_add_header(p, -ETHER_HLEN);
            p->incoming_iphdr = p->data;

            if(GET_IP_VER((struct ipv4_hdr_t *)p->data) == 6)
            {
                kernel_mutex_lock(&ipv6_inq.lock);

                if(IFQ_FULL(&ipv6_inq))
                {
                    KDEBUG("eth-recv: queue full - dropping IPv6 packet\n");
                    kernel_mutex_unlock(&ipv6_inq.lock);
                    //p->ifp->stats.rx_dropped++;
                    netstats.link.drop++;
                    packet_free(p);
                }
                else
                {
                    KDEBUG("eth-recv: queueing IPv6 packet\n");
                    IFQ_ENQUEUE(&ipv6_inq, p);
                    kernel_mutex_unlock(&ipv6_inq.lock);
                    netstats.link.recv++;
                }

                return 0;
            }
            break;

        default:
            KDEBUG("eth-recv: Unknown proto (0x%x)\n", ntohs(eh->type));
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            break;
    }

    KDEBUG("eth-recv: dropping packet\n");
    //p->ifp->stats.rx_dropped++;
    netstats.link.drop++;
    packet_free(p);
    return -EINVAL;
}


static inline int dest_is_multicast(void *iph)
{
    return (GET_IP_VER((struct ipv4_hdr_t *)iph) == 6) ?
            ipv6_is_multicast(((struct ipv6_hdr_t *)iph)->dest.s6_addr) :
            ipv4_is_multicast(((struct ipv4_hdr_t *)iph)->dest.s_addr);
}


static inline int dest_is_broadcast(void *iph)
{
    return (GET_IP_VER((struct ipv4_hdr_t *)iph) == 6) ? 0 :
            ipv4_is_broadcast(((struct ipv4_hdr_t *)iph)->dest.s_addr);
}


static int ethernet_ipv6_dest_eth(struct packet_t *p, 
                                  struct ether_addr_t *dest_ethernet)
{
    int res = -EINVAL;
    struct ipv6_hdr_t *iph = p->data;
    
    if(dest_is_multicast(p->data))
    {
        /* first 2 octets are 0x33, last four are the last four of the dest */
        dest_ethernet->addr[0] = 0x33;
        dest_ethernet->addr[1] = 0x33;
        dest_ethernet->addr[2] = iph->dest.s6_addr[12];
        dest_ethernet->addr[3] = iph->dest.s6_addr[13];
        dest_ethernet->addr[4] = iph->dest.s6_addr[14];
        dest_ethernet->addr[5] = iph->dest.s6_addr[15];
        res = 0;
    }
    else
    {
        struct ether_addr_t *neighbor;
        
        if((neighbor = ipv6_get_neighbor(p)))
        {
            COPY_ETHER_ADDR(dest_ethernet->addr, neighbor->addr);
            res = 0;
        }
    }
    
    return res;
}


static void broadcast_set_src_dest(struct packet_t *p, struct ipv4_hdr_t *iph)
{
    struct ipv4_link_t *link;

    //printk("broadcast_set_src: 1 - ip%d, ipf 0x%lx\n", GET_IP_VER(iph), p->ifp);

    if(GET_IP_VER(iph) != 4 || !p->ifp)
    {
        return;
    }

    //printk("broadcast_set_src: looking for src\n");

    if((link = ipv4_link_by_ifp(p->ifp)))
    {
        /*
        printk("broadcast_set_src: src is %3d.%3d.%3d.%3d",
                (link->addr.s_addr >> 24) & 0xff,
                (link->addr.s_addr >> 16) & 0xff,
                (link->addr.s_addr >> 8 ) & 0xff,
                (link->addr.s_addr >> 0 ) & 0xff);
        */

        iph->src.s_addr = link->addr.s_addr;

        if(iph->dest.s_addr == 0)
        {
            // calculate the directed broadcast address
            iph->dest.s_addr = (link->addr.s_addr | (~link->netmask.s_addr));
        }

        // unfortunately, we have to recalculate the checksum
        iph->checksum = 0;
        iph->checksum = htons(checksum(iph, IPv4_HLEN));
    }
}


/*
 * Ethernet send.
 */
int ethernet_send(struct packet_t *p)
{
    //KDEBUG("%s: 1\n", __func__);
    struct ether_addr_t dest_ethernet;
    struct ether_header_t *h;
    struct ipv4_hdr_t *iph;
    int i;
    uint16_t proto = ETHERTYPE_IP;
    
    if(!p)
    {
        return -EINVAL;
    }

    KDEBUG("eth-send: p->count %d, p 0x%lx, p->data 0x%lx, hlen %d\n", p->count, p, p->data, ETHER_HLEN);

    iph = (struct ipv4_hdr_t *)p->data;
    
    KDEBUG("eth-send: ipv%d\n", GET_IP_VER(iph));

    if(GET_IP_VER(iph) == 6)
    {
        if(ethernet_ipv6_dest_eth(p, &dest_ethernet) != 0)
        {
            KDEBUG("eth-send: postponing ipv6\n");

            // Enqueue frame in IPv6 network discovery module to retry later
            ipv6_nd_postpone(p);
            return 0;
        }
        
        proto = ETHERTYPE_IPv6;
    }
    else
    {
        // broadcast (IPv4 only)
        if((p->flags & PACKET_FLAG_BROADCAST) || dest_is_broadcast(p->data))
        {
            KDEBUG("eth-send: broadcast\n");

            COPY_ETHER_ADDR(dest_ethernet.addr, ethernet_broadcast);
            p->flags |= PACKET_FLAG_BROADCAST;
        }
        // multicast
        else if(dest_is_multicast(p->data))
        {
            KDEBUG("eth-send: multicast\n");

            uint32_t dest_ip4 = ntohl(iph->dest.s_addr);

            // put lower 23 bits of IP in lower 23 bits of MAC
            dest_ethernet.addr[0] = 0x01;
            dest_ethernet.addr[1] = 0x00;
            dest_ethernet.addr[2] = 0x5e;
            dest_ethernet.addr[3] = (uint8_t)((dest_ip4 & 0x007F0000) >> 16);
            dest_ethernet.addr[4] = (uint8_t)((dest_ip4 & 0x0000FF00) >> 8);
            dest_ethernet.addr[5] = (dest_ip4 & 0x000000FF);
        }
        else
        {
            KDEBUG("eth-send: unicast\n");

            struct ether_addr_t *from_arp;
            
            if((from_arp = arp_get(p)))
            {
                COPY_ETHER_ADDR(dest_ethernet.addr, from_arp->addr);
            }
            else
            {
                // Enqueue packet in ARP module to retry later
                KDEBUG("eth-send: postponing ipv4\n");
                arp_postpone(p);
                return 0;
            }
        }
    }
    
    KDEBUG("eth-send: p->count %d before hdr\n", p->count);

    if(packet_add_header(p, ETHER_HLEN) != 0)
    {
        KDEBUG("eth-send: insufficient memory for packet header\n");
        netstats.link.err++;
        packet_free(p);
        return -ENOBUFS;
    }

    KDEBUG("eth-send: p->count %d\n", p->count);

    h = (struct ether_header_t *)p->data;
    h->type = htons(proto);
    COPY_ETHER_ADDR(h->dest.addr, dest_ethernet.addr);
    COPY_ETHER_ADDR(h->src.addr, p->ifp->ethernet_addr.addr);
    
    // check for a locally sent package (e.g. loopback dev)
    /*
    if(!memcmp(h->dest.addr, h->src.addr, ETHER_ADDR_LEN))
    {
        KDEBUG("eth-send: local eth addr - ");
        KDEBUG_ETHER_ADDR(h->dest.addr);
        KDEBUG("\n");

        ethernet_receive(p);
        return 0;
    }
    */
    
    // check for broadcast packages (IPv4 only)
    if(p->flags & PACKET_FLAG_BROADCAST)
    {
        struct netif_t *ifp;

        for(i = 0, ifp = netif_list; ifp != NULL; ifp = ifp->next)
        {
            struct packet_t *copy;

            KDEBUG("eth-send: duplicating packet\n");
            
            if((copy = packet_duplicate(p)))
            {
                KDEBUG("eth-send: sending duplicate to ifp %lx (%s)\n", ifp, ifp->name);

                if(ifp != p->ifp)
                {
                    copy->ifp = ifp;
                }

                broadcast_set_src_dest(copy, iph);

                if(copy->ifp->transmit(copy->ifp, copy) == 0)
                {
                    i++;
                }
            }
        }

        KDEBUG("eth-send: finished broadcast\n");
        
        packet_free(p);

        KDEBUG("eth-send: broadcasted %d packets\n", i);
        
        // if at least one device succeeded in transmitting the package,
        // the broadcast is a success
        if(i)
        {
            return 0;
        }
    }
    
    // unicast package, check if the device has an output queue
    if(p->ifp->outq == NULL)
    {
        KDEBUG("eth-send: sending directly\n");
	    //__asm__ __volatile__("xchg %%bx, %%bx"::);
        // nope, send directly
        if(p->ifp->transmit(p->ifp, p) != 0)
        {
            packet_free(p);
            return -EHOSTUNREACH;
        }
        
        return 0;
    }
    else
    {
        KDEBUG("eth-send: adding to ifp queue\n");
	    //__asm__ __volatile__("xchg %%bx, %%bx"::);
        //kernel_mutex_lock(&p->ifp->outq->lock);
        IFQ_ENQUEUE(p->ifp->outq, p);
        //kernel_mutex_unlock(&p->ifp->outq->lock);
        return 0;
    }
}

