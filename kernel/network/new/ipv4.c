/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: ipv4.c
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
 *  \file ipv4.c
 *
 *  Internet Protocol (IP) v4 implementation.
 *
 *  The code is divided into the following files:
 *  - ipv4.c: main IPv4 handling code
 *  - ipv4_addr.c: functions for working with IPv4 addresses
 *  - ipv4_frag.c: functions for handling IPv4 & IPv6 packet fragments
 */

//#define __DEBUG
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/timer.h>
#include <kernel/task.h>
#include <kernel/net.h>
#include <kernel/net/protocol.h>
#include <kernel/net/packet.h>
#include <kernel/net/netif.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/udp.h>
#include <kernel/net/icmp4.h>
#include <kernel/net/checksum.h>
#include <kernel/net/notify.h>
#include <mm/kheap.h>

struct ipv4_link_t *ipv4_links = NULL;
struct ipv4_route_t *ipv4_routes = NULL;

struct netif_queue_t ipv4_inq = { 0, };
struct netif_queue_t ipv4_outq = { 0, };

static struct ipv4_route_t default_broadcast_route =
{
    .dest = { INADDR_BROADCAST },
    .netmask = { INADDR_BROADCAST },
    .gateway = { 0 },
    .link = NULL,
    .metric = 1000,
};


static uint16_t ipv4_id = 1;
static struct task_t *ipv4_task = NULL;
static struct kernel_mutex_t ipv4_lock = { 0, };


#ifdef __DEBUG

static inline void DUMP_IPV4_HDR(struct ipv4_hdr_t *h)
{
    KDEBUG("+------------------------------------+\n");
    KDEBUG("|  %3d   |  %3d   |      %5d       |\n",
           h->ver_hlen, h->tos, ntohs(h->len));
    KDEBUG("+------------------------------------+\n");
    KDEBUG("|      %5d       |       %5d       |\n",
           ntohs(h->id), ntohs(h->offset));
    KDEBUG("+------------------------------------+\n");
    KDEBUG("|  %3d   |  %3d   |      0x%4x       |\n",
           h->ttl, h->proto, ntohs(h->checksum));
    KDEBUG("+------------------------------------+\n");
    KDEBUG("|        ");
    KDEBUG_IPV4_ADDR(ntohl(h->src.s_addr));
    KDEBUG("        |\n+------------------------------------+\n|        ");
    KDEBUG_IPV4_ADDR(ntohl(h->dest.s_addr));
    KDEBUG("        |\n");
}

#else

#define DUMP_IPV4_HDR(h)        if(0) {}

#endif      /* __DEBUG */


static int ipv4_link_cmp(struct ipv4_link_t *la, struct ipv4_link_t *lb)
{
    struct in_addr *addra = &la->addr;
    struct in_addr *addrb = &lb->addr;
    int res;
    
    if((res = ipv4_cmp(addra, addrb)))
    {
        return res;
    }

    // zero can be assigned multiple times (e.g. for DHCP)
    if(la->ifp && lb->ifp &&
       la->addr.s_addr == INADDR_ANY &&
       lb->addr.s_addr == INADDR_ANY)
    {
        if(la->ifp < lb->ifp)
        {
            return -1;
        }

        if(la->ifp > lb->ifp)
        {
            return 1;
        }
    }
    
    return 0;
}


static int ipv4_route_cmp(struct ipv4_route_t *ra, struct ipv4_route_t *rb)
{
    uint32_t aa, ab;
    int cmp;
    
    aa = ntohl(ra->netmask.s_addr);
    ab = ntohl(rb->netmask.s_addr);
    
    // Routes are sorted by (host side) netmask len, then by addr, 
    // then by metric
    if(aa < ab)
    {
        return -1;
    }

    if(aa > ab)
    {
        return 1;
    }
    
    if((cmp = ipv4_cmp(&ra->dest, &rb->dest)))
    {
        return cmp;
    }
    
    if(ra->metric < rb->metric)
    {
        return -1;
    }

    if(ra->metric > rb->metric)
    {
        return 1;
    }
    
    return 0;
}


struct ipv4_link_t *ipv4_link_find(struct in_addr *addr)
{
    struct ipv4_link_t *link;
    struct ipv4_link_t tmp = { 0 };
    
    tmp.addr.s_addr = addr->s_addr;

    kernel_mutex_lock(&ipv4_lock);

    for(link = ipv4_links; link != NULL; link = link->next)
    {
        if(ipv4_link_cmp(link, &tmp) == 0)
        {
            kernel_mutex_unlock(&ipv4_lock);
            return link;
        }
    }
    
    kernel_mutex_unlock(&ipv4_lock);
    return NULL;
}


struct ipv4_link_t *ipv4_link_find_like(struct ipv4_link_t *target)
{
    struct ipv4_link_t *link;

    kernel_mutex_lock(&ipv4_lock);

    for(link = ipv4_links; link != NULL; link = link->next)
    {
        KDEBUG("ipv4: link %lx, ifp %lx, addr", link, link->ifp);
        KDEBUG_IPV4_ADDR(ntohl(link->addr.s_addr));
        KDEBUG(", netmask ");
        KDEBUG_IPV4_ADDR(ntohl(link->netmask.s_addr));
        KDEBUG("\n");

        if(ipv4_link_cmp(link, target) == 0)
        {
            kernel_mutex_unlock(&ipv4_lock);
            return link;
        }
    }
    
    kernel_mutex_unlock(&ipv4_lock);
    return NULL;
}


struct ipv4_link_t *ipv4_link_get(struct in_addr *addr)
{
    struct ipv4_link_t *link;
    struct ipv4_link_t tmp = { 0 };
    
    tmp.addr.s_addr = addr->s_addr;

    kernel_mutex_lock(&ipv4_lock);

    for(link = ipv4_links; link != NULL; link = link->next)
    {
        if(ipv4_link_cmp(link, &tmp) == 0)
        {
            kernel_mutex_unlock(&ipv4_lock);
            return link;
        }
    }
    
    kernel_mutex_unlock(&ipv4_lock);
    return NULL;
}


struct ipv4_link_t *ipv4_link_by_ifp(struct netif_t *ifp)
{
    struct ipv4_link_t *link;

    for(link = ipv4_links; link != NULL; link = link->next)
    {
        if(link->ifp == ifp)
        {
            return link;
        }
    }
    
    return NULL;
}


struct ipv4_route_t *ipv4_route_find(struct in_addr *addr)
{
    struct ipv4_route_t *route, *default_gateway = NULL;
    
    if(addr->s_addr == INADDR_ANY)
    {
        return NULL;
    }

    if(addr->s_addr == INADDR_BROADCAST)
    {
        return &default_broadcast_route;
    }
    
    kernel_mutex_lock(&ipv4_lock);

    for(route = ipv4_routes; route != NULL; route = route->next)
    {
        KDEBUG("ipv4_route_find: addr ");
        KDEBUG_IPV4_ADDR(ntohl(addr->s_addr));
        KDEBUG(", netmask ");
        KDEBUG_IPV4_ADDR(ntohl(route->netmask.s_addr));
        KDEBUG(", rdest ");
        KDEBUG_IPV4_ADDR(ntohl(route->dest.s_addr));
        KDEBUG("\n");
        
        if(!route->netmask.s_addr && !route->dest.s_addr)
        {
            default_gateway = route;
            continue;
        }

        if((addr->s_addr & route->netmask.s_addr) == 
                    route->dest.s_addr)
        {
            kernel_mutex_unlock(&ipv4_lock);
            return route;
        }
    }
    
    kernel_mutex_unlock(&ipv4_lock);
    //return NULL;
    return default_gateway;
}


struct netif_t *ipv4_source_ifp_find(struct in_addr *addr)
{
    struct ipv4_route_t *route;
    
    if(!addr)
    {
        return NULL;
    }

    if(!(route = ipv4_route_find(addr)))
    {
        return NULL;
    }
    
    return route->link ? route->link->ifp : NULL;
}


/*
 * Get the source IP to send to the given addr. The result is returned in
 * the res argument.
 */
int ipv4_source_find(struct in_addr *res, struct in_addr *addr)
{
    struct ipv4_route_t *route;
    
    KDEBUG("ipv4_source_find: addr ");
    KDEBUG_IPV4_ADDR(ntohl(addr->s_addr));
    KDEBUG("\n");

    if(!(route = ipv4_route_find(addr)) || !route->link)
    {
        res->s_addr = 0;
        return -EHOSTUNREACH;
    }
    
    res->s_addr = route->link->addr.s_addr;
    return 0;
}


/*
 * Get the gateway to the given addr. The result is returned in the gateway
 * argument.
 */
int ipv4_route_gateway_get(struct in_addr *gateway, struct in_addr *addr)
{
    struct ipv4_route_t *route;

    gateway->s_addr = INADDR_ANY;
    
    if(!addr || addr->s_addr == 0)
    {
        KDEBUG("ipv4: destination address error\n");
        return -EINVAL;
    }

    route = ipv4_route_find(addr);
    
    if(!route)
    {
        KDEBUG("ipv4: cannot find route to host\n");
        return -EHOSTUNREACH;
    }
    
    gateway->s_addr = route->gateway.s_addr;
    
    return 0;
}


int ipv4_route_add(struct ipv4_link_t *link, struct in_addr *addr, 
                   struct in_addr *netmask, struct in_addr *gateway, 
                   uint32_t metric)
{
    struct ipv4_route_t *route, *route2, tmp;
    
    tmp.dest.s_addr = addr->s_addr;
    tmp.netmask.s_addr = netmask->s_addr;
    tmp.metric = metric;

    kernel_mutex_lock(&ipv4_lock);

    for(route = ipv4_routes; route != NULL; route = route->next)
    {
        if(ipv4_route_cmp(route, &tmp) == 0)
        {
            break;
        }
    }

    kernel_mutex_unlock(&ipv4_lock);

    // route already in table
    if(route)
    {
        KDEBUG("ipv4_route_add: route already exists\n");
        return -EINVAL;
    }
    
    if(!(route = kmalloc(sizeof(struct ipv4_route_t))))
    {
        return -ENOMEM;
    }
    
    A_memset(route, 0, sizeof(struct ipv4_route_t));
    route->dest.s_addr = addr->s_addr;
    route->netmask.s_addr = netmask->s_addr;
    route->gateway.s_addr = gateway->s_addr;
    route->metric = metric;

    KDEBUG("ipv4_route_add: route dest ");
    KDEBUG_IPV4_ADDR(ntohl(route->dest.s_addr));
    KDEBUG(", netmask ");
    KDEBUG_IPV4_ADDR(ntohl(route->netmask.s_addr));
    KDEBUG(", gateway ");
    KDEBUG_IPV4_ADDR(ntohl(route->gateway.s_addr));
    KDEBUG("\n");
    
    if(gateway->s_addr == 0)
    {
        // No gateway provided, use the link
        route->link = link;
    }
    else
    {
        if(!(route2 = ipv4_route_find(gateway)))
        {
            // Specified Gateway is unreachable
            kfree(route);
            return -EHOSTUNREACH;
        }
        
        if(route2->gateway.s_addr)
        {
            // Specified Gateway is not a neighbor
            kfree(route);
            return -EHOSTUNREACH;
        }
        
        route->link = route2->link;
    }
    
    if(!route->link)
    {
        kfree(route);
        return -EINVAL;
    }
    
    kernel_mutex_lock(&ipv4_lock);
    route->next = ipv4_routes;
    ipv4_routes = route;
    kernel_mutex_unlock(&ipv4_lock);
    
    return 0;
}


void ipv4_route_set_broadcast_link(struct ipv4_link_t *link)
{
    if(link)
    {
        default_broadcast_route.link = link;
    }
}


int ipv4_cleanup_routes(struct ipv4_link_t *link)
{
    struct ipv4_route_t *route, *prev = NULL, *next = NULL;

    kernel_mutex_lock(&ipv4_lock);

    for(route = ipv4_routes; route != NULL; route = next)
    {
        next = route->next;
        
        if(route->link == link)
        {
            if(prev)
            {
                prev->next = route->next;
            }
            else
            {
                ipv4_routes = route->next;
            }
            
            kfree(route);
        }
        else
        {
            prev = route;
        }
        
        if(!next)
        {
            break;
        }
    }

    kernel_mutex_unlock(&ipv4_lock);
    
    return 0;
}


int ipv4_link_add(struct netif_t *ifp, 
                  struct in_addr *addr, struct in_addr *netmask)
{
    struct ipv4_link_t *link;
    struct ipv4_link_t tmp = { 0 };
    struct in_addr network, gateway;

    if(!ifp)
    {
        return -EINVAL;
    }
    
    tmp.addr.s_addr = addr->s_addr;
    tmp.netmask.s_addr = netmask->s_addr;
    tmp.ifp = ifp;
    
    if(ipv4_link_find_like(&tmp))
    {
        KDEBUG("ipv4: ifp %lx, addr", ifp);
        KDEBUG_IPV4_ADDR(ntohl(addr->s_addr));
        KDEBUG(", netmask ");
        KDEBUG_IPV4_ADDR(ntohl(netmask->s_addr));
        KDEBUG("\n");

        KDEBUG("ipv4: address in use\n");
        return -EADDRINUSE;
    }
    
    if(!(link = kmalloc(sizeof(struct ipv4_link_t))))
    {
        return -ENOMEM;
    }
    
    A_memset(link, 0, sizeof(struct ipv4_link_t));
    link->addr.s_addr = addr->s_addr;
    link->netmask.s_addr = netmask->s_addr;
    link->ifp = ifp;
    
    kernel_mutex_lock(&ipv4_lock);
    link->next = ipv4_links;
    ipv4_links = link;
    kernel_mutex_unlock(&ipv4_lock);
    
    network.s_addr = (addr->s_addr & netmask->s_addr);
    gateway.s_addr = 0;
    
    ipv4_route_add(link, &network, netmask, &gateway, 1);
    
    if(!default_broadcast_route.link)
    {
        default_broadcast_route.link = link;
    }
    
    return 0;
}


int ipv4_link_del(struct netif_t *ifp, struct in_addr *addr)
{
    struct ipv4_link_t *link, *prev = NULL;
    struct ipv4_link_t tmp = { 0 };
    
    tmp.addr.s_addr = addr->s_addr;
    tmp.ifp = ifp;

    kernel_mutex_lock(&ipv4_lock);

    for(link = ipv4_links; link != NULL; link = link->next)
    {
        if(ipv4_link_cmp(link, &tmp) == 0)
        {
            if(prev)
            {
                prev->next = link->next;
            }
            else
            {
                ipv4_links = link->next;
            }
            
            link->next = NULL;
            break;
        }
        
        prev = link;
    }
    
    kernel_mutex_unlock(&ipv4_lock);

    if(!link)
    {
        return -EINVAL;
    }

    ipv4_cleanup_routes(link);

    kfree(link);
    return 0;
}


int ipv4_cleanup_links(struct netif_t *ifp)
{
    struct ipv4_link_t *link, *prev = NULL, *next;

    kernel_mutex_lock(&ipv4_lock);

    for(link = ipv4_links; link != NULL; )
    {
        next = link->next;
        
        if(link->ifp == ifp)
        {
            if(prev)
            {
                prev->next = next;
            }
            else
            {
                ipv4_links = next;
            }
            
            link->next = NULL;
            kfree(link);
        }
        else
        {
            prev = link;
        }
        
        link = next;
    }
    
    kernel_mutex_unlock(&ipv4_lock);

    return 0;
}


int ipv4_push(struct packet_t *p, struct in_addr *dest, uint8_t proto)
{
    struct ipv4_route_t *route;
    struct ipv4_link_t *link = NULL;
    struct ipv4_hdr_t *h;
    int res = 0;
    int need_hdr;
    uint8_t ttl = 64, tos = 0;

    KDEBUG("ipv4_push: packet %lx\n", p);

    if(!dest || dest->s_addr == 0)
    {
        KDEBUG("ipv4: destination address error ");
        if(dest)
        {
            KDEBUG("- dest ");
            KDEBUG_IPV4_ADDR(ntohl(dest->s_addr));
        }
        KDEBUG("\n");
        

        // If the socket has the broadcast flag on, accept ADDR_ANY
        // as the destination address. This will mean we cannot find the
        // source address here, and the Ethernet layer will have to 
        // figure it out
        /*
        if(p->sock && (p->sock->flags & SOCKET_FLAG_BROADCAST))
        {
            p->flags |= PACKET_FLAG_BROADCAST;
        }
        else
        */
        {
            res = -EHOSTUNREACH;
            goto err;
        }
    }

    // If this is a broadcast packet, do not try to find the route
    //if(!(p->flags & PACKET_FLAG_BROADCAST))
    {
        KDEBUG("ipv4_push: dest ");
        KDEBUG_IPV4_ADDR(ntohl(dest->s_addr));
        KDEBUG("\n");

        route = ipv4_route_find(dest);
    
        if(!route)
        {
            KDEBUG("ipv4: cannot find route to host\n");
            res = -EHOSTUNREACH;
            goto err;
        }
    
        link = route->link;

        if(p->sock && p->sock->ifp)
        {
            p->ifp = p->sock->ifp;
        }
        else
        {
            p->ifp = link->ifp;
        
            if(p->sock)
            {
                p->sock->ifp = p->ifp;
            }
        }
    }

    need_hdr = p->sock ? (!(p->sock->flags & SOCKET_FLAG_IPHDR_INCLUDED)) : 1;

    if(need_hdr && packet_add_header(p, IPv4_HLEN) != 0)
    {
        KDEBUG("ipv4: insufficient memory for packet header\n");
        res = -ENOBUFS;
        goto err;
    }
    
    if((p->frag & IP_MF) == 0)
    {
        ipv4_id++;
    }
    
    /*
    if(p->send_ttl > 0)
    {
        ttl = p->send_ttl;
    }
    */

    h = (struct ipv4_hdr_t *)p->data;

    if(need_hdr)
    {
        if(p->sock)
        {
            if(p->sock->ttl >= 0)
            {
                ttl = p->sock->ttl;
            }

            if(p->sock->tos != 0)
            {
                tos = p->sock->tos;
            }
        }

        h->ver_hlen = 5 | (4 << 4);     // IPv4 | hlen of 5 (=20 bytes)
        h->len = htons(p->count);
        h->id = htons(ipv4_id);
        h->proto = proto;
        h->ttl = ttl;
        //h->tos = p->send_tos;
        h->tos = tos /* 0 */;
        h->dest.s_addr = dest ? dest->s_addr : 0;
        h->src.s_addr = link ? link->addr.s_addr : 0;
        h->offset = htons(IP_DF);   // don't fragment

        if(proto == IPPROTO_UDP)
        {
            // set the frag flags/offset as calculated in the socket layer
            h->offset = htons(p->frag);
        }

        if(proto == IPPROTO_ICMP)
        {
            // ditto
            h->offset = htons(p->frag);
        }
    }
    else
    {
        h->len = htons(p->count);

        // fill these fields if needed
        if(h->src.s_addr == 0 && link)
        {
            h->src.s_addr = link->addr.s_addr;
        }

        if(h->id == 0)
        {
            h->id = htons(ipv4_id);
        }
    }

    h->checksum = 0;
    h->checksum = htons(checksum(h, IPv4_HLEN));
    
    //DUMP_IPV4_HDR(h);
    

    /*
     * TODO: process multicast packets
     */

    
    // Check for local destination addresses only if this is not a
    // broadcast packet
    /*
    if(!(p->flags & PACKET_FLAG_BROADCAST) && ipv4_link_get(dest))
    {
        if(IFQ_FULL(&ipv4_inq))
        {
            KDEBUG("ipv4_push: dropping local packet\n");
            netstats.ip.drop++;
            res = -ENOBUFS;
            goto err;
        }
        else
        {
            KDEBUG("ipv4_push: enqueuing local packet\n");
            IFQ_ENQUEUE(&ipv4_inq, p);
            netstats.ip.xmit++;
        }
    }
    else
    */
    {
        if(IFQ_FULL(&ipv4_outq))
        {
            KDEBUG("ipv4_push: dropping outgoing packet\n");
            netstats.ip.drop++;
            res = -ENOBUFS;
            goto err;
        }
        else
        {
            KDEBUG("ipv4_push: enqueuing outgoing packet\n");
            IFQ_ENQUEUE(&ipv4_outq, p);
            netstats.ip.xmit++;
        }
    }

    return 0;

err:

    packet_free(p);
    netstats.ip.err++;
    return res;
    
}


static int ipv4_forward(struct packet_t *p)
{
    static struct in_addr last_src = { 0 };
    static struct in_addr last_dest = { 0 };
    static uint16_t last_id = 0, last_proto = 0;
    struct ipv4_route_t *route;
    struct ipv4_hdr_t *h = (struct ipv4_hdr_t *)p->data;
    struct in_addr dest = { .s_addr = h->dest.s_addr };
    struct in_addr src = { .s_addr = h->src.s_addr };
    int res = 0;

    route = ipv4_route_find(&dest);
    
    if(!route || !route->link)
    {
        KDEBUG("ipv4: cannot find route to host\n");
        notify_dest_unreachable(p, 0);
        res = -EHOSTUNREACH;
        goto err;
    }
    
    p->ifp = route->link->ifp;
    
    // decrease hop (time to live) count
    h->ttl = (uint8_t)(h->ttl - 1);
    
    if(h->ttl < 1)
    {
        KDEBUG("ipv4: dropping packet with expired ttl\n");
        notify_ttl_expired(p, 0);
        res = -ETIMEDOUT;       // TODO: find the right errno here
        goto err;
    }
    
    // increase checksum to compensate for decreased ttl
    h->checksum++;
    
    // local source, discard as packet is bouncing (locally forwarded)
    if(ipv4_link_get(&src))
    {
        KDEBUG("ipv4: dropping bouncing packet - src ");
        KDEBUG_IPV4_ADDR(ntohl(src.s_addr));
        KDEBUG(", dest ");
        KDEBUG_IPV4_ADDR(ntohl(dest.s_addr));
        KDEBUG("\n");

        res = -EHOSTUNREACH;
        goto err;
    }
    
    // silently discard if this is the same last forwarded packet
    if(last_src.s_addr == h->src.s_addr && last_id == h->id &&
       last_dest.s_addr == h->dest.s_addr && last_proto == h->proto)
    {
        packet_free(p);
        return 0;
    }
    else
    {
        last_src.s_addr = h->src.s_addr;
        last_dest.s_addr = h->dest.s_addr;
        last_id = h->id;
        last_proto = h->proto;
    }
    
    /*
     * TODO: implement NAT
     */
    
    /*
    ipv4_nat_outbound(p, &route->link->addr);
    */
    
    // check packet size
    if((p->count + ETHER_HLEN) > (size_t)p->ifp->mtu)
    {
        KDEBUG("ipv4: dropping packet as too big\n");
        notify_packet_too_big(p, 0);
        res = -E2BIG;
        goto err;
    }
    
    // enqueue for the ethernet layer to process next
    kernel_mutex_lock(&ethernet_outq.lock);
    if(!IFQ_FULL(&ethernet_outq))
    {
        IFQ_ENQUEUE(&ethernet_outq, p);
        kernel_mutex_unlock(&ethernet_outq.lock);
        netstats.ip.xmit++;
        return 0;
    }

    kernel_mutex_unlock(&ethernet_outq.lock);
    netstats.ip.drop++;
    res = -ENOBUFS;

err:

    packet_free(p);
    netstats.ip.err++;
    return res;
}


static int ipv4_process_received_broadcast(struct packet_t *p)
{
    struct ipv4_hdr_t *h = (struct ipv4_hdr_t *)p->data;
    
    if(ipv4_is_broadcast(h->dest.s_addr))
    {
        KDEBUG("ipv4: received broadcast packet\n");

        p->flags |= PACKET_FLAG_BROADCAST;

        if(h->proto == IPPROTO_UDP)
        {
            // broadcast UDP packet
            if(!IFQ_FULL(&udp_inq))
            {
                IFQ_ENQUEUE(&udp_inq, p);
            }

            return 0;
        }

        if(h->proto == IPPROTO_ICMP)
        {
            // broadcast ICMPv4 packet
            if(!IFQ_FULL(&icmp4_inq))
            {
                IFQ_ENQUEUE(&icmp4_inq, p);
            }

            return 0;
        }
    }
    
    return -EINVAL;
}


static int ipv4_process_received_multicast(struct packet_t *p)
{
    struct ipv4_hdr_t *h = (struct ipv4_hdr_t *)p->data;
    
    if(ipv4_is_multicast(h->dest.s_addr))
    {
        KDEBUG("ipv4: dropped multicast packet\n");

        /*
         * TODO: process the packet
         */

        packet_free(p);
        return 0;
    }
    
    return -EINVAL;
}


static int ipv4_process_received_local_unicast(struct packet_t *p)
{
    struct ipv4_hdr_t *h = (struct ipv4_hdr_t *)p->data;
    struct in_addr dest = { .s_addr = h->dest.s_addr };
    struct in_addr tmp = { 0, };

    if(ipv4_link_find(&dest))
    {
        KDEBUG("ipv4: received unicast packet\n");

        /*
         * TODO: implement NAT
         */

        /*
        if(ipv4_nat_inbound(p, &dest) == 0)
        {
            // destination changed, reprocess
            IFQ_ENQUEUE(&ipv4_inq, p);
        }
        else
        */
        {
            transport_enqueue_in(p, h->proto, 0);
        }
        
        return 0;
    }
    else
    {
        if(ipv4_link_find(&tmp))
        {
            KDEBUG("ipv4: received unicast packet with addr any\n");

            // A network interface with IPv4_ADDR_ANY as its address.
            // This could be a DHCP packet coming in.
            IFQ_ENQUEUE(&udp_inq, p);
            return 0;
        }
    }
    
    return -EINVAL;
}


static inline int ipv4_is_invalid_loopback(struct netif_t *ifp, uint32_t addr)
{
    uint8_t *bytes = (uint8_t *)&addr;

    return (bytes[0] == 0x7f && (!ifp || strcmp(ifp->name, "lo0") != 0));
}


int ipv4_is_invalid_src(struct netif_t *ifp, uint32_t addr)
{
    return ipv4_is_broadcast(addr) || 
           ipv4_is_multicast(addr) ||
           ipv4_is_invalid_loopback(ifp, addr);
}


int ipv4_receive(struct packet_t *p)
{
    struct ipv4_hdr_t *h = (struct ipv4_hdr_t *)p->data;
    //uint8_t optlen;
    uint32_t hlen;
    /*
    uint16_t maxbytes = ((int)p->count - 
                         ((uintptr_t)p->data - ((uintptr_t)p + sizeof(struct packet_t))) - 
                         IPv4_HLEN);

    // NAT transport needs hdr info
    if((h->ver_hlen & 0x0f) > 5)
    {
        optlen = (uint8_t)(4 * ((h->ver_hlen & 0x0f) - 5));
    }
    
    // check for sensible packet length
    if((ntohs(h->len) - IPv4_HLEN - optlen) > maxbytes)
    {
        packet_free(p);
        return 0;
    }
    */
    
    KDEBUG("ipv4_receive:\n");
    DUMP_IPV4_HDR(h);

    // check header length is valid, should be at least 5, which equates
    // to 20 bytes, and no more than 60 bytes (hlen is expressed in terms
    // of dwords, see RFC 791)
    hlen = GET_IP_HLEN(h) * 4;
    
    if(hlen < 20 || hlen > 60 || hlen > p->count)
    {
        KDEBUG("ipv4: dropped packet with invalid length: %d\n", hlen);
        icmp4_param_problem(p, 0);
        netstats.ip.lenerr++;
        goto err;
    }

    KDEBUG("ipv4: received packet with length: %d (h->len %d)\n", p->count, ntohs(h->len));
    
    // if the packet contains padding, adjust the length so that upper
    // layer protocols (e.g. TCP) can accurately calculate checksums and
    // get the right data length of the packet
    if(ntohs(h->len) < p->count)
    {
        p->count = ntohs(h->len);
    }

    p->transport_hdr = (void *)((uintptr_t)(p->data) + hlen);
    p->frag = ntohs(h->offset);
    
    /*
     * TODO
     */

    /*
    if(ipfilter(p))
    {
        return 0;
    }
    */

    // validate checksum
    if(ntohs(checksum(h, hlen)))
    {
        KDEBUG("ipv4: dropped packet with invalid header checksum (0x%x)\n", ntohs(checksum(h, hlen)));
        netstats.ip.chkerr++;
        goto err;
    }

    // validate source address
    if(ipv4_is_invalid_src(p->ifp, h->src.s_addr))
    {
        KDEBUG("ipv4: dropped packet with invalid source address - ");
        KDEBUG_IPV4_ADDR(ntohl(h->src.s_addr));
        KDEBUG("\n");

        netstats.ip.err++;
        goto err;
    }
    
    if(p->frag & 0x8000U)
    {
        icmp4_param_problem(p, 0);
        netstats.ip.err++;
        goto err;
    }

    netstats.ip.recv++;

#if 0
    // update our ARP table with the sender's address
    // (could be handy if the sender does not reply to ARP requests, e.g.
    //  some gateways/routers)
    arp_update_entry(p->ifp, h->src.s_addr,
                        &(((struct ether_header_t *)
                            ((uintptr_t)p->data - ETHER_HLEN))->src));
#endif

    // reassemble fragmented packets
    if((p->frag & (IP_OFFMASK | IP_MF)) != 0)
    {
        KDEBUG("ipv4: fragmented packet\n");
        DUMP_IPV4_HDR(h);
        //screen_refresh(NULL);
        __asm__ __volatile__("xchg %%bx, %%bx":::);
        
        ipv4_process_fragment(p, h, h->proto);
        packet_free(p);
        return 0;
    }
    
    if(ipv4_process_received_broadcast(p) == 0)
    {
        return 0;
    }

    if(ipv4_process_received_multicast(p) == 0)
    {
        return 0;
    }
    
    if(ipv4_process_received_local_unicast(p) == 0)
    {
        return 0;
    }
    
    // forward
    if((ipv4_is_broadcast(h->dest.s_addr)) || 
       (p->flags & PACKET_FLAG_BROADCAST))
    {
        // discard broadcast packets
        KDEBUG("ipv4: dropped broadcast packet\n");
        goto err;
    }
    
    if(ipv4_forward(p) != 0)
    {
        KDEBUG("ipv4: failed to forward packet\n");
        netstats.ip.drop++;
        return -EINVAL;
    }
    
    return 0;

err:

    netstats.ip.drop++;
    packet_free(p);
    return -EINVAL;
}


/*
 * Send a packet to the Ethernet layer.
 * Called from the network dispatcher when processing IPv4 output queue.
 */
int ipv4_process_out(struct packet_t *p)
{
    KDEBUG("ipv4_process_out:\n");

    // enqueue for the ethernet layer to process next
    kernel_mutex_lock(&ethernet_outq.lock);
    if(IFQ_FULL(&ethernet_outq))
    {
        kernel_mutex_unlock(&ethernet_outq.lock);
        netstats.ip.drop++;
        netstats.ip.err++;
        packet_free(p);
        return -ENOBUFS;
    }

    IFQ_ENQUEUE(&ethernet_outq, p);
    kernel_mutex_unlock(&ethernet_outq.lock);
    netstats.ip.xmit++;
    return 0;
}


/*
 * IPv4 task function.
 */
void ipv4_task_func(void *arg)
{
    UNUSED(arg);

    for(;;)
    {
        //KDEBUG("ipv4_task_func:\n");

        ip_fragment_check_expired();
        
        // schedule every 500 ms
        //block_task2(&ipv4_task, PIT_FREQUENCY / 2);
        block_task2(&ipv4_task, PIT_FREQUENCY);
    }
}


void ipv4_init(void)
{
    // fork the slow timeout task
    (void)start_kernel_task("ipv4", ipv4_task_func, NULL, &ipv4_task, 0);
}


int ip_push(struct packet_t *p)
{
    KDEBUG("ip_push: sock domain %d\n", p->sock->domain);
    
    if(p->sock->domain == AF_INET6)
    {
        struct in6_addr *dest;

        if(!ipv6_is_unspecified(p->remote_addr.ipv6.s6_addr) /* && p->remote_port */)
        {
            dest = &p->remote_addr.ipv6;
        }
        else
        {
            dest = &p->sock->remote_addr.ipv6;
        }
        
        return ipv6_push(p, dest, NULL, p->sock->proto->protocol, 0);
    }
    else
    {
        struct in_addr *dest;

        if(p->remote_addr.ipv4.s_addr /* && p->remote_port */)
        {
            dest = &p->remote_addr.ipv4;
        }
        else
        {
            dest = &p->sock->remote_addr.ipv4;
        }
        
        return ipv4_push(p, dest, p->sock->proto->protocol);
    }
}

