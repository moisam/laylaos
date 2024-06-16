/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: ipv6.c
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
 *  \file ipv6.c
 *
 *  Internet Protocol (IP) v6 implementation.
 *
 *  The code is divided into the following files:
 *  - ipv6.c: main IPv6 handling code
 *  - ipv6_addr.c: functions for working with IPv6 addresses
 *  - ipv4_nd.c: functions for handling IPv6 neighbour discovery
 */

//#define __DEBUG
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/timer.h>
#include <kernel/task.h>
//#include <kernel/timeout.h>
#include <kernel/net.h>
#include <kernel/net/protocol.h>
#include <kernel/net/packet.h>
#include <kernel/net/netif.h>
#include <kernel/net/ipv6.h>
#include <kernel/net/udp.h>
#include <kernel/net/icmp6.h>
#include <kernel/net/notify.h>
#include <kernel/net/checksum.h>
#include <mm/kheap.h>

#define IPv6_EXTHDR_OPT_PAD1            0
#define IPv6_EXTHDR_OPT_PADN            1
#define IPv6_EXTHDR_OPT_SRCADDR         201

#define IPv6_EXTHDR_OPT_ROUTER_ALERT            5
#define IPv6_EXTHDR_OPT_ROUTER_ALERT_DATALEN    2

/* highest-order two bits */
#define IPv6_EXTHDR_OPT_ACTION_MASK             0xC0
/* skip and continue processing */
#define IPv6_EXTHDR_OPT_ACTION_SKIP             0x00
/* discard packet */
#define IPv6_EXTHDR_OPT_ACTION_DISCARD          0x40
/* discard and send ICMP parameter problem */
#define IPv6_EXTHDR_OPT_ACTION_DISCARD_SI       0x80
/* discard and send ICMP parameter problem if not multicast */
#define IPv6_EXTHDR_OPT_ACTION_DISCARD_SINM     0xC0

struct ipv6_link_t *ipv6_links = NULL;
struct ipv6_route_t *ipv6_routes = NULL;

struct netif_queue_t ipv6_inq = { 0, };
struct netif_queue_t ipv6_outq = { 0, };

static struct task_t *ipv6_slow_task = NULL;
//static struct task_t *ipv6_fast_task = NULL;
static struct kernel_mutex_t ipv6_lock = { 0, };


static int ipv6_link_cmp(struct ipv6_link_t *la, struct ipv6_link_t *lb)
{
    struct in6_addr *addra = &la->addr;
    struct in6_addr *addrb = &lb->addr;
    int res;
    
    if((res = ipv6_cmp(addra, addrb)))
    {
        return res;
    }

    // zero can be assigned multiple times (e.g. for DHCP)
    if(la->ifp && lb->ifp &&
       memcmp(la->addr.s6_addr, IPv6_ANY, 16) == 0 &&
       memcmp(lb->addr.s6_addr, IPv6_ANY, 16) == 0)
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


struct ipv6_link_t *ipv6_link_is_tentative(struct in6_addr *addr)
{
    struct ipv6_link_t *link;
    struct ipv6_link_t tmp = { 0 };
    
    IPV6_COPY(tmp.addr.s6_addr, addr->s6_addr);
    //tmp.addr = *addr;

    for(link = ipv6_links; link != NULL; link = link->next)
    {
        if(ipv6_link_cmp(link, &tmp) == 0)
        {
            return link->is_tentative ? link : NULL;
        }
    }
    
    return NULL;
}


struct ipv6_link_t *ipv6_prefix_configured(struct in6_addr *prefix)
{
    struct ipv6_link_t *link;

    for(link = ipv6_links; link != NULL; link = link->next)
    {
        if(memcmp(link->addr.s6_addr, prefix->s6_addr, 8) == 0)
        {
            return link;
        }
    }
    
    return NULL;
}


struct ipv6_link_t *ipv6_link_get(struct in6_addr *addr)
{
    struct ipv6_link_t *link;
    struct ipv6_link_t tmp = { 0 };
    
    IPV6_COPY(tmp.addr.s6_addr, addr->s6_addr);
    //tmp.addr = *addr;

    for(link = ipv6_links; link != NULL; link = link->next)
    {
        if(ipv6_link_cmp(link, &tmp) == 0)
        {
            return link->is_tentative ? NULL : link;
        }
    }
    
    return NULL;
}


struct ipv6_link_t *ipv6_link_by_ifp(struct netif_t *ifp)
{
    struct ipv6_link_t *link;

    for(link = ipv6_links; link != NULL; link = link->next)
    {
        if(link->ifp == ifp)
        {
            return link;
        }
    }
    
    return NULL;
}


struct ipv6_link_t *ipv6_link_by_ifp_next(struct netif_t *ifp, 
                                          struct ipv6_link_t *last)
{
    struct ipv6_link_t *link;
    int found = 0;

    for(link = ipv6_links; link != NULL; link = link->next)
    {
        if(link->ifp == ifp)
        {
            if(link == last)
            {
                found = 1;
            }
            else if(found)
            {
                return link;
            }
        }
    }
    
    return NULL;
}


struct ipv6_route_t *ipv6_route_find(struct in6_addr *addr)
{
    struct ipv6_route_t *route, *default_gateway = NULL;
    
    if(!ipv6_is_localhost(addr->s6_addr) &&
       (ipv6_is_linklocal(addr->s6_addr) || ipv6_is_sitelocal(addr->s6_addr)))
    {
        return NULL;
    }

    for(route = ipv6_routes; route != NULL; route = route->next)
    {
        int i;

        KDEBUG("ipv6_route_find: addr ");
        KDEBUG_IPV6_ADDR(addr->s6_addr);
        KDEBUG(", rdest ");
        KDEBUG_IPV6_ADDR(route->dest.s6_addr);
        KDEBUG(", rmask ");
        KDEBUG_IPV6_ADDR(route->netmask.s6_addr);
        KDEBUG("\n");

        if(ipv6_is_unspecified(route->netmask.s6_addr) &&
           ipv6_is_unspecified(route->dest.s6_addr))
        {
            default_gateway = route;
            continue;
        }
        
        for(i = 0; i < 16; i++)
        {
            if((addr->s6_addr[i] & route->netmask.s6_addr[i]) !=
               (route->dest.s6_addr[i] & route->netmask.s6_addr[i]))
            {
                break;
            }
        }
        
        if(i == 16)
        {
            return route;
        }
    }
    
    //return NULL;
    return default_gateway;
}


struct netif_t *ipv6_source_ifp_find(struct in6_addr *addr)
{
    struct ipv6_route_t *route;
    
    if(!addr)
    {
        return NULL;
    }

    if(!(route = ipv6_route_find(addr)))
    {
        return NULL;
    }
    
    return route->link ? route->link->ifp : NULL;
}


/*
 * Get the source IP to send to the given addr. The result is returned in
 * the res argument.
 */
int ipv6_source_find(struct in6_addr *res, struct in6_addr *addr)
{
    struct ipv6_route_t *route;

    if(!(route = ipv6_route_find(addr)) || !route->link)
    {
        IPV6_COPY(res->s6_addr, IPv6_ANY);
        return -EHOSTUNREACH;
    }
    
    IPV6_COPY(res->s6_addr, route->link->addr.s6_addr);
    return 0;
}


/*
 * Get the gateway to the given addr. The result is returned in the gateway
 * argument.
 */
int ipv6_route_gateway_get(struct in6_addr *gateway, struct in6_addr *addr)
{
    struct ipv6_route_t *route = NULL;
    
    if(!gateway)
    {
        return -EINVAL;
    }

    if(!addr)
    {
        A_memset(gateway, 0, sizeof(struct in6_addr));
        return -EINVAL;
    }
    
    if(!(route = ipv6_route_find(addr)))
    {
        A_memset(gateway, 0, sizeof(struct in6_addr));
        return -EHOSTUNREACH;
    }
    
    memcpy(gateway, &route->gateway, sizeof(struct in6_addr));
    return 0;
}


struct ipv6_link_t *ipv6_sitelocal_get(struct netif_t *ifp)
{
    struct ipv6_link_t *link = ipv6_link_by_ifp(ifp);
    
    while(link && !ipv6_is_sitelocal(link->addr.s6_addr))
    {
        link = ipv6_link_by_ifp_next(ifp, link);
    }
    
    return link;
}


struct ipv6_link_t *ipv6_linklocal_get(struct netif_t *ifp)
{
    struct ipv6_link_t *link = ipv6_link_by_ifp(ifp);
    
    while(link && !ipv6_is_linklocal(link->addr.s6_addr))
    {
        link = ipv6_link_by_ifp_next(ifp, link);
    }
    
    return link;
}


struct ipv6_link_t *ipv6_global_get(struct netif_t *ifp)
{
    struct ipv6_link_t *link = ipv6_link_by_ifp(ifp);
    
    while(link && !ipv6_is_global(link->addr.s6_addr))
    {
        link = ipv6_link_by_ifp_next(ifp, link);
    }
    
    return link;
}


int ipv6_route_add_locked(struct ipv6_link_t *link, 
                          struct in6_addr *addr, struct in6_addr *netmask,
                          struct in6_addr *gateway, uint32_t metric, int locked)
{
    struct ipv6_route_t *route, *route2;
    
    if(!(route = kmalloc(sizeof(struct ipv6_route_t))))
    {
        return -ENOMEM;
    }
    
    A_memset(route, 0, sizeof(struct ipv6_route_t));
    IPV6_COPY(route->dest.s6_addr, addr->s6_addr);
    IPV6_COPY(route->netmask.s6_addr, netmask->s6_addr);
    IPV6_COPY(route->gateway.s6_addr, gateway->s6_addr);
    route->metric = metric;
    
    if(memcmp(gateway, IPv6_ANY, 16) == 0)
    {
        // No gateway provided, use the link
        route->link = link;
    }
    else
    {
        if(!(route2 = ipv6_route_find(gateway)))
        {
            if(link)
            {
                route->link = link;
            }
            else
            {
                // Specified Gateway is unreachable
                kfree(route);
                return -EHOSTUNREACH;
            }
        }
        else if(memcmp(route2->gateway.s6_addr, IPv6_ANY, 16) != 0)
        {
            if(link)
            {
                route->link = link;
            }
            else
            {
                // Specified Gateway is not a neighbor
                kfree(route);
                return -EHOSTUNREACH;
            }
        }
        else
        {
            route->link = route2->link;
        }
    }
    
    if(route->link && ipv6_is_global(addr->s6_addr) && 
                      !ipv6_is_global(route->link->addr.s6_addr))
    {
        route->link = ipv6_global_get(route->link->ifp);
    }
    
    if(!route->link)
    {
        kfree(route);
        return -EINVAL;
    }

    if(!locked)
    {
        kernel_mutex_lock(&ipv6_lock);
    }
    
    route->next = ipv6_routes;
    ipv6_routes = route;

    if(!locked)
    {
        kernel_mutex_unlock(&ipv6_lock);
    }
    
    return 0;
}


int ipv6_route_add(struct ipv6_link_t *link, 
                   struct in6_addr *addr, struct in6_addr *netmask,
                   struct in6_addr *gateway, uint32_t metric)
{
    return ipv6_route_add_locked(link, addr, netmask, gateway, metric, 0);
}


#if 0
/*
 * NOTE: SHOULD BE called with ipv6_lock locked!
 */
int ipv6_route_del(struct ipv6_link_t *link, struct in6_addr *addr,
                   struct in6_addr *netmask, struct in6_addr *gateway,
                   uint32_t metric)
{
    struct ipv6_route_t *prev = NULL, *cur, tmp = { 0 };

    IPV6_COPY(tmp.dest.s6_addr, addr->s6_addr);
    IPV6_COPY(tmp.netmask.s6_addr, netmask->s6_addr);
    tmp.metric = metric;

    for(cur = ipv6_links; cur != NULL; cur = cur->next)
    {
        if(ipv6_link_cmp(cur, &tmp) == 0)
        {
            KDEBUG("ipv6: address in use\n");
            return -EADDRINUSE;
        }
    }
}
#endif


static int ipv6_cleanup_routes_locked(struct ipv6_link_t *link, int locked)
{
    struct ipv6_route_t *route, *prev = NULL, *next = NULL;

    if(!locked)
    {
        kernel_mutex_lock(&ipv6_lock);
    }

    for(route = ipv6_routes; route != NULL; route = next)
    {
        next = route->next;
        
        if(route->link == link)
        {
            //ipv6_route_del(route->link, route->dest, route->netmask, 
            //               route->gateway, route->metric);
            
            if(prev)
            {
                prev->next = route->next;
            }
            else
            {
                ipv6_routes = route->next;
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

    if(!locked)
    {
        kernel_mutex_unlock(&ipv6_lock);
    }
    
    return 0;
}


static int ipv6_cleanup_routes(struct ipv6_link_t *link)
{
    return ipv6_cleanup_routes_locked(link, 0);
}


void ipv6_router_down(struct in6_addr *addr)
{
    struct ipv6_route_t *route, *prev = NULL, *next = NULL;

    kernel_mutex_lock(&ipv6_lock);

    for(route = ipv6_routes; route != NULL; route = next)
    {
        next = route->next;
        
        if(ipv6_cmp(addr, &route->gateway) == 0)
        {
            if(prev)
            {
                prev->next = route->next;
            }
            else
            {
                ipv6_routes = route->next;
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

    kernel_mutex_unlock(&ipv6_lock);
}


int ipv6_do_link_add(struct netif_t *ifp, 
                    struct in6_addr *addr, struct in6_addr *netmask,
                    struct ipv6_link_t **res, int locked)
{
    struct ipv6_link_t *link;
    struct ipv6_link_t tmp = { 0 };
    struct in6_addr network = { 0, }, gateway = { 0, };
    int i;

    *res = NULL;

    if(!ifp)
    {
        return -EINVAL;
    }

    IPV6_COPY(tmp.addr.s6_addr, addr->s6_addr);
    tmp.ifp = ifp;

    for(link = ipv6_links; link != NULL; link = link->next)
    {
        if(ipv6_link_cmp(link, &tmp) == 0)
        {
            KDEBUG("ipv6: address in use\n");
            return -EADDRINUSE;
        }
    }

    if(!(link = kmalloc(sizeof(struct ipv6_link_t))))
    {
        return -ENOMEM;
    }
    
    A_memset(link, 0, sizeof(struct ipv6_link_t));
    IPV6_COPY(link->addr.s6_addr, addr->s6_addr);
    IPV6_COPY(link->netmask.s6_addr, netmask->s6_addr);
    link->ifp = ifp;
    link->is_tentative = 1;
    
    if(!locked)
    {
        kernel_mutex_lock(&ipv6_lock);
    }
    
    link->next = ipv6_links;
    ipv6_links = link;

    if(!locked)
    {
        kernel_mutex_unlock(&ipv6_lock);
    }
    
    for(i = 0; i < 16; i++)
    {
        network.s6_addr[i] = (addr->s6_addr[i] & netmask->s6_addr[i]);
    }

    KDEBUG("ipv6_do_link_add: network ");
    KDEBUG_IPV6_ADDR(network.s6_addr);
    KDEBUG(", netmask ");
    KDEBUG_IPV6_ADDR(netmask->s6_addr);
    KDEBUG("\n");
    
    //ipv6_route_add(link, &network, netmask, &gateway, 1);
    ipv6_route_add_locked(link, &network, netmask, &gateway, 1, locked);
    
    *res = link;
    return 0;
}


int ipv6_link_add_locked(struct netif_t *ifp, 
                         struct in6_addr *addr, struct in6_addr *netmask,
                         struct ipv6_link_t **res, int locked)
{
    struct ipv6_link_t *link;
    int i;
    
    if(res)
    {
        *res = NULL;
    }

    // Try to add the basic link
    if((i = ipv6_do_link_add(ifp, addr, netmask, &link, locked)) < 0)
    {
        return i;
    }
    
    // Apply DAD (Duplicate Address Detection)
    link->dup_detect_retrans = 1;
    //timeout(ipv6_nd_dad, &link->addr, 10);
    link->dad_expiry = ticks + 10;

    if(res)
    {
        *res = link;
    }

    return 0;
}


int ipv6_link_add(struct netif_t *ifp, 
                  struct in6_addr *addr, struct in6_addr *netmask,
                  struct ipv6_link_t **res)
{
    return ipv6_link_add_locked(ifp, addr, netmask, res, 0);
}


int ipv6_link_add_local(struct netif_t *ifp, 
                        struct in6_addr *prefix, struct ipv6_link_t **res)
{
    int i;
    struct in6_addr newaddr;
    struct in6_addr netmask =
    {
        .s6_addr =
        {
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        }
    };
    
    *res = NULL;
    
    IPV6_COPY(newaddr.s6_addr, prefix->s6_addr);
    
    // modified EUI-64 + invert universal/local bit
    newaddr.s6_addr[8] = ifp->ethernet_addr.addr[0] ^ 0x02;
    newaddr.s6_addr[9] = ifp->ethernet_addr.addr[1];
    newaddr.s6_addr[10] = ifp->ethernet_addr.addr[2];
    newaddr.s6_addr[11] = 0xff;
    newaddr.s6_addr[12] = 0xfe;
    newaddr.s6_addr[13] = ifp->ethernet_addr.addr[3];
    newaddr.s6_addr[14] = ifp->ethernet_addr.addr[4];
    newaddr.s6_addr[15] = ifp->ethernet_addr.addr[5];

    KDEBUG("ipv6_link_add_local: addr ");
    KDEBUG_IPV6_ADDR(newaddr.s6_addr);
    KDEBUG(", netmask ");
    KDEBUG_IPV6_ADDR(netmask.s6_addr);
    KDEBUG("\n");
    
    if((i = ipv6_link_add(ifp, &newaddr, &netmask, res)) == 0)
    {
        ifp->hostvars.base_time = ND_REACHABLE_TIME;
        
        // RFC 4861 $6.3.2 value between 0.5 and 1.5 times basetime
        ifp->hostvars.reachable_time = 
            ((5 + genrand_int32() % 10) * ND_REACHABLE_TIME) / 10;
        ifp->hostvars.retrans_time = ND_RETRANS_TIMER;
        icmp6_router_solicit(ifp, &newaddr, NULL);
        ifp->hostvars.hop_limit = 255 /* 64 */;
    }
    
    return i;
}


int ipv6_link_del_locked(struct netif_t *ifp, struct in6_addr *addr, int locked)
{
    struct ipv6_link_t *link, *prev = NULL;
    struct ipv6_link_t tmp = { 0 };

    IPV6_COPY(tmp.addr.s6_addr, addr->s6_addr);
    tmp.ifp = ifp;

    if(!locked)
    {
        kernel_mutex_lock(&ipv6_lock);
    }

    for(link = ipv6_links; link != NULL; link = link->next)
    {
        if(ipv6_link_cmp(link, &tmp) == 0)
        {
            if(prev)
            {
                prev->next = link->next;
            }
            else
            {
                ipv6_links = link->next;
            }
            
            link->next = NULL;
            break;
        }
        
        prev = link;
    }

    if(!locked)
    {
        kernel_mutex_unlock(&ipv6_lock);
    }
    
    if(!link)
    {
        return -EINVAL;
    }
    
    ipv6_cleanup_routes(link);
    //untimeout(ipv6_nd_dad, &link->addr);

    kfree(link);
    return 0;
}


int ipv6_link_del(struct netif_t *ifp, struct in6_addr *addr)
{
    return ipv6_link_del_locked(ifp, addr, 0);
}


void ipv6_ifp_routing_enable(struct netif_t *ifp)
{
    ifp->hostvars.routing = 1;
}


void ipv6_ifp_routing_disable(struct netif_t *ifp)
{
    ifp->hostvars.routing = 0;
}


int ipv6_push(struct packet_t *p, 
              struct in6_addr *dest, struct in6_addr *src,
              uint8_t proto, int is_dad)
{
    struct ipv6_route_t *route;
    struct ipv6_link_t *link;
    struct ipv6_hdr_t *h;
    int res = 0;
    int need_hdr;
    
    if(!dest)
    {
        KDEBUG("ipv6: destination address error\n");
        res = -EHOSTUNREACH;
        goto err;
    }

    if(ipv6_is_linklocal(dest->s6_addr) ||
       ipv6_is_multicast(dest->s6_addr) ||
       ipv6_is_sitelocal(dest->s6_addr))
    {
        if(!p->ifp)
        {
            KDEBUG("ipv6: destination address error\n");
            res = -EHOSTUNREACH;
            goto err;
        }
        
        link = ipv6_is_sitelocal(dest->s6_addr) ? 
                    ipv6_sitelocal_get(p->ifp) : 
                    ipv6_linklocal_get(p->ifp);

        if(link)
        {
            goto fin;
        }
    }
    
    if(ipv6_is_localhost(dest->s6_addr))
    {
        p->ifp = netif_by_name("lo0");
    }
    
    if(memcmp(dest->s6_addr, IPv6_ANY, 16) == 0)
    {
        KDEBUG("ipv6: destination address error\n");
        res = -EHOSTUNREACH;
        goto err;
    }
    
    route = ipv6_route_find(dest);
    
    if(!route /* && !p->ifp */)
    {
        KDEBUG("ipv6: cannot find route to host\n");
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

fin:

    need_hdr = p->sock ? (!(p->sock->flags & SOCKET_FLAG_IPHDR_INCLUDED)) : 1;

    p->transport_hdr = p->data;

    if(need_hdr && packet_add_header(p, IPv6_HLEN) != 0)
    {
        KDEBUG("ipv6: insufficient memory for packet header\n");
        res = -ENOBUFS;
        goto err;
    }

    h = (struct ipv6_hdr_t *)p->data;

    if(need_hdr)
    {
        h->vtf = htonl(0x60000000); /* version 6, traffic class 0, flow label 0 */
        h->len = htons((uint16_t)(p->count - IPv6_HLEN));
        h->proto = proto;
        h->ttl = p->ifp->hostvars.hop_limit;
        IPV6_COPY(h->dest.s6_addr, dest->s6_addr);
    
        if(!src || !ipv6_is_unicast(src))
        {
            IPV6_COPY(h->src.s6_addr, link->addr.s6_addr);
        }
        else
        {
            IPV6_COPY(h->src.s6_addr, src->s6_addr);
        }

        if(p->sock)
        {
            if(p->sock->ttl >= 0)
            {
                h->ttl = p->sock->ttl;
            }

            if(p->sock->tos)
            {
                h->vtf |= ((uint32_t)p->sock->tos << 20);
            }
        }
    }
    else
    {
        h->len = htons((uint16_t)(p->count - IPv6_HLEN));

        // fill these fields if needed
        if(memcmp(h->src.s6_addr, IPv6_ANY, 16) == 0)
        {
            if(!src || !ipv6_is_unicast(src))
            {
                IPV6_COPY(h->src.s6_addr, link->addr.s6_addr);
            }
            else
            {
                IPV6_COPY(h->src.s6_addr, src->s6_addr);
            }
        }
    }
    
    switch(proto)
    {
        case IPPROTO_ICMPV6:
        {
            struct icmp6_hdr_t *icmph;
            
            //icmph = (struct icmp6_hdr_t *)((uintptr_t)p->data + IPv6_HLEN);
            icmph = (struct icmp6_hdr_t *)p->transport_hdr;
            
            if(icmph->type == ICMP6_MSG_NEIGHBOR_SOLICIT ||
               icmph->type == ICMP6_MSG_NEIGHBOR_ADV     ||
               icmph->type == ICMP6_MSG_ROUTER_SOLICIT   ||
               icmph->type == ICMP6_MSG_ROUTER_ADV)
            {
                h->ttl = 255;
            }
            
            if((is_dad || link->is_tentative) && 
               icmph->type == ICMP6_MSG_NEIGHBOR_SOLICIT)
            {
                IPV6_COPY(h->src.s6_addr, IPv6_ANY);
            }
            
            icmph->checksum = 0;
            icmph->checksum = htons(icmp6_checksum(p));
            break;
        }
        
        case IPPROTO_UDP:
        {
            /*
            struct udp_hdr_t *udph = (struct udp_hdr_t *)
                                        ((uintptr_t)p->data + IPv6_HLEN);

            udph->checksum = htons(udp_checksum_ipv6(p, iph, udph, 
                                    ((uint32_t)(p->count - IPv6_HLEN))));
            */

            //p->transport_hdr = ((uintptr_t)p->data + IPv6_HLEN);

            struct udp_hdr_t *udph = (struct udp_hdr_t *)p->transport_hdr;

            udph->checksum = 0;
            udph->checksum = htons(udp_checksum_ipv6(p));
        }
        
        default:
            break;
    }
    
    if(ipv6_link_get(dest))
    {
        if(IFQ_FULL(&ipv6_inq))
        {
            netstats.ip.drop++;
            res = -ENOBUFS;
            goto err;
        }
        else
        {
            IFQ_ENQUEUE(&ipv6_inq, p);
            netstats.ip.xmit++;
        }
    }
    else
    {
        if(IFQ_FULL(&ipv6_outq))
        {
            netstats.ip.drop++;
            res = -ENOBUFS;
            goto err;
        }
        else
        {
            IFQ_ENQUEUE(&ipv6_outq, p);
            netstats.ip.xmit++;
        }
    }

    return 0;

err:

    packet_free(p);
    netstats.ip.err++;
    return res;
}


static int ipv6_process_hopbyhop(struct packet_t *p, 
                                 struct ipv6_exthdr_t *exth)
{
    uint8_t *opt = ((uint8_t *)&exth->ext.hopbyhop) + 1;
    uint8_t len = (((exth->ext.hopbyhop.len + 1) << 3) - 2);
    uint8_t optlen;
    uint8_t alignment = 1;
    uint32_t i = IPv6_HLEN;
    uint8_t *hraw = (uint8_t *)exth;
    
    while(len)
    {
        switch(*opt)
        {
            case IPv6_EXTHDR_OPT_PAD1:
                ++opt;
                --len;
                break;

            case IPv6_EXTHDR_OPT_PADN:
                optlen = (uint8_t)((*(opt + 1)) + 2);
                opt += optlen;
                len = (uint8_t)(len - optlen);
                break;
            
            case IPv6_EXTHDR_OPT_ROUTER_ALERT:
                optlen = (uint8_t)((*(opt + 1)) + 2);
                
                if(*(opt + 1) == 2)
                {
                    alignment = 0;
                }

                opt += optlen;
                len = (uint8_t)(len - optlen);
                break;
            
            default:
                // unknown options
                optlen = (uint8_t)((*(opt + 1)) + 2);
                
                switch((*opt) & IPv6_EXTHDR_OPT_ACTION_MASK)
                {
                    case IPv6_EXTHDR_OPT_ACTION_SKIP:
                        break;

                    case IPv6_EXTHDR_OPT_ACTION_DISCARD:
                        return -EINVAL;

                    case IPv6_EXTHDR_OPT_ACTION_DISCARD_SI:
                        icmp6_param_problem(p, ICMP6_PARAMPROBLEM_IPV6OPT,
                                            i + (uint32_t)(opt - hraw));
                        return -EINVAL;

                    case IPv6_EXTHDR_OPT_ACTION_DISCARD_SINM:
                        if(!ipv6_is_multicast(((struct ipv6_hdr_t *)
                                                    p->data)->dest.s6_addr))
                        {
                            icmp6_param_problem(p, ICMP6_PARAMPROBLEM_IPV6OPT,
                                                i + (uint32_t)(opt - hraw));
                        }

                        return -EINVAL;
                }

                opt += optlen;
                len = (uint8_t)(len - optlen);
                break;
        }
    }
    
    return alignment;
}


static int ipv6_process_routing(struct packet_t *p, 
                                struct ipv6_exthdr_t *exth, uint32_t i)
{
    if(exth->ext.routing.segleft == 0)
    {
        return 0;
    }
    
    switch(exth->ext.routing.routtype)
    {
        case 0:
            // deprecated
            icmp6_param_problem(p, ICMP6_PARAMPROBLEM_HDRFIELD, i + 2);
            return -EINVAL;

        case 2:
            // routing type for MIPv6: not supported yet
            break;

        default:
            icmp6_param_problem(p, ICMP6_PARAMPROBLEM_HDRFIELD, i + 2);
            return -EINVAL;
    }
    
    return 0;
}


static int ipv6_check_headers_seq(struct packet_t *p)
{
    struct ipv6_hdr_t *h = (struct ipv6_hdr_t *)p->data;
    int i = IPv6_HLEN;
    int cur_proto = 6;  // offset of proto in the header
    uint8_t proto = h->proto;
    uint8_t *hraw = (uint8_t *)p->data;
    
    for(;;)
    {
        uint8_t optlen = *(hraw + i + 1);
        
        switch(proto)
        {
            case IPPROTO_DSTOPTS:
            case IPPROTO_ROUTING:
            case IPPROTO_HOPOPTS:
            case IPPROTO_ESP:
            case IPPROTO_AH:
                optlen = (uint8_t)IPv6_OPTLEN(optlen);
                break;
            
            case IPPROTO_FRAGMENT:
                optlen = 8;
                break;
            
            case IPPROTO_NONE:
                return 0;
            
            case IPPROTO_TCP:
            case IPPROTO_UDP:
            case IPPROTO_ICMPV6:
                return 0;
            
            default:
                // invalid next header (proto)
                icmp6_param_problem(p, ICMP6_PARAMPROBLEM_NXTHDR, (uint32_t)cur_proto);
                return -EINVAL;
        }
        
        cur_proto = i;
        proto = *(hraw + i);
        i += optlen;
    }
}


static int ipv6_process_destopt(struct packet_t *p, 
                                struct ipv6_exthdr_t *exth, uint32_t i)
{
    i += 2;
    uint8_t *opt, len, optlen;
    
    opt = ((uint8_t *)&exth->ext.destopt) + 1;
    len = (uint8_t)(((exth->ext.destopt.len + 1) << 3) - 2);
    
    while(len)
    {
        optlen = (uint8_t)(*(opt + 1) + 2);
        
        switch(*opt)
        {
            case IPv6_EXTHDR_OPT_PAD1:
                break;

            case IPv6_EXTHDR_OPT_PADN:
                break;

            case IPv6_EXTHDR_OPT_SRCADDR:
                break;
            
            default:
                switch((*opt) & IPv6_EXTHDR_OPT_ACTION_MASK)
                {
                    case IPv6_EXTHDR_OPT_ACTION_SKIP:
                        break;

                    case IPv6_EXTHDR_OPT_ACTION_DISCARD:
                        return -EINVAL;

                    case IPv6_EXTHDR_OPT_ACTION_DISCARD_SI:
                        icmp6_param_problem(p, ICMP6_PARAMPROBLEM_IPV6OPT, i);
                        return -EINVAL;

                    case IPv6_EXTHDR_OPT_ACTION_DISCARD_SINM:
                        if(!ipv6_is_multicast(((struct ipv6_hdr_t *)
                                                    p->data)->dest.s6_addr))
                        {
                            icmp6_param_problem(p, ICMP6_PARAMPROBLEM_IPV6OPT, i);
                        }

                        return -EINVAL;
                }

                break;
        }

        i += optlen;
        opt += optlen;
        len = (uint8_t)(len - optlen);
    }
    
    return 0;
}


static int ipv6_ext_headers(struct packet_t *p)
{
    struct ipv6_hdr_t *h = (struct ipv6_hdr_t *)p->data;
    struct ipv6_exthdr_t *exth, *fragh = NULL;
    uint32_t ptr = IPv6_HLEN;
    uint8_t next_hdr = h->proto;
    uint16_t optlen;
    int alignment = 0;
    uint8_t *hraw = (uint8_t *)p->data;
    uint32_t hlen = IPv6_HLEN;
    uint32_t cur_next_hdr = 6;  // offset of proto in the header
    
    if(ipv6_check_headers_seq(p) < 0)
    {
        return -EINVAL;
    }
    
    for(;;)
    {
        exth = (struct ipv6_exthdr_t *)(hraw + hlen);
        optlen = 0;
        
        switch(next_hdr)
        {
            case IPPROTO_HOPOPTS:
                if(cur_next_hdr != 6)
                {
                    // this header must immediately follow the IP header
                    icmp6_param_problem(p, ICMP6_PARAMPROBLEM_NXTHDR, 
                                           (uint32_t)cur_next_hdr);
                    return -EINVAL;
                }
                
                optlen = IPv6_OPTLEN(exth->ext.hopbyhop.len);
                hlen = (uint16_t)(hlen + optlen);
                
                if((alignment = ipv6_process_hopbyhop(p, exth)) < 0)
                {
                    return -EINVAL;
                }
                
                break;

            case IPPROTO_ROUTING:
                optlen = IPv6_OPTLEN(exth->ext.routing.len);
                hlen = (uint16_t)(hlen + optlen);

                if(ipv6_process_routing(p, exth, ptr) < 0)
                {
                    return -EINVAL;
                }
                
                break;

            case IPPROTO_FRAGMENT:
                optlen = 8;
                hlen = (uint16_t)(hlen + optlen);
                fragh = exth;
                p->frag = (uint16_t)((fragh->ext.frag.om[0] << 8) + fragh->ext.frag.om[1]);

                // If M-Flag is set, and packet is not 8B aligned, discard and alert 
                if((p->frag & 0x0001) && (ntohs(h->len) % 8) != 0)
                {
                    icmp6_param_problem(p, ICMP6_PARAMPROBLEM_HDRFIELD, 4);
                    return -EINVAL;
                }

                break;

            case IPPROTO_DSTOPTS:
                optlen = IPv6_OPTLEN(exth->ext.destopt.len);
                hlen = (uint16_t)(hlen + optlen);
                alignment = 1;

                if(ipv6_process_destopt(p, exth, ptr) < 0)
                {
                    return -EINVAL;
                }
                
                break;

            case IPPROTO_ESP:
                // not supported
                return 0;

            case IPPROTO_AH:
                // not supported
                return 0;

            case IPPROTO_NONE:
                if(alignment && (ntohs(h->len) % 8) != 0)
                {
                    icmp6_param_problem(p, ICMP6_PARAMPROBLEM_HDRFIELD, 4);
                    return -EINVAL;
                }

                return 0;

            case IPPROTO_TCP:
            case IPPROTO_UDP:
            case IPPROTO_ICMPV6:
                if(alignment && (ntohs(h->len) % 8) != 0)
                {
                    icmp6_param_problem(p, ICMP6_PARAMPROBLEM_HDRFIELD, 4);
                    return -EINVAL;
                }
                
                p->transport_hdr = hraw + hlen;
                
                if(fragh)
                {
                    ipv6_process_fragment(p, fragh, next_hdr);
                    return -EINVAL;
                }
                else
                {
                    return next_hdr;
                }

            default:
                // invalid next header (proto)
                icmp6_param_problem(p, ICMP6_PARAMPROBLEM_NXTHDR, cur_next_hdr);
                return -EINVAL;
        }
        
        next_hdr = exth->next_hdr;
        cur_next_hdr = ptr;
        ptr += optlen;
    }
}


static int ipv6_forward(struct packet_t *p)
{
    struct ipv6_route_t *route;
    struct ipv6_hdr_t *h = (struct ipv6_hdr_t *)p->data;
    struct in6_addr dest = { 0, };
    struct in6_addr src = { 0, };
    int res = 0;
    
    IPV6_COPY(dest.s6_addr, h->dest.s6_addr);
    IPV6_COPY(src.s6_addr, h->src.s6_addr);

    route = ipv6_route_find(&dest);
    
    if(!route || !route->link)
    {
        KDEBUG("ipv6: cannot find route to host\n");
        notify_dest_unreachable(p, 1);
        res = -EHOSTUNREACH;
        goto err;
    }
    
    p->ifp = route->link->ifp;
    
    // decrease hop (time to live) count
    h->ttl = (uint8_t)(h->ttl - 1);
    
    if(h->ttl < 1)
    {
        KDEBUG("ipv6: dropping packet with expired ttl\n");
        notify_ttl_expired(p, 1);
        res = -ETIMEDOUT;       // TODO: find the right errno here
        goto err;
    }
    
    // local source, discard as packet is bouncing (locally forwarded)
    if(ipv6_link_get(&src))
    {
        KDEBUG("ipv6: dropping bouncing packet\n");
        res = -EHOSTUNREACH;
        goto err;
    }
    
    // check packet size
    if((p->count + ETHER_HLEN) > (size_t)p->ifp->mtu)
    {
        KDEBUG("ipv6: dropping packet as too big\n");
        notify_packet_too_big(p, 1);
        res = -E2BIG;
        goto err;
    }
    
    // enqueue for the ethernet layer to process next
    if(!IFQ_FULL(&ethernet_outq))
    {
        IFQ_ENQUEUE(&ethernet_outq, p);
        netstats.ip.xmit++;
        return 0;
    }

    netstats.ip.drop++;
    res = -ENOBUFS;

err:

    packet_free(p);
    netstats.ip.err++;
    return res;
}


static int ipv6_process_received_multicast(struct packet_t *p)
{
    /*
     * TODO
     */

    /*
    struct ipv6_hdr_t *h = (struct ipv6_hdr_t *)p->data;
    struct ipv6_exthdr_t *exth = NULL;

    if(ipv6_is_multicast(h->dest.s_addr))
    {
        if(h->proto == 0)
        {
            exth = (struct ipv6_exthdr_t *)
        }
    }
    */
    
    packet_free(p);
    return 0;
}


int ipv6_receive(struct packet_t *p)
{
    struct ipv6_hdr_t *h = (struct ipv6_hdr_t *)p->data;
    struct in6_addr dest = { 0, };
    int proto;

    IPV6_COPY(dest.s6_addr, h->dest.s6_addr);
    
    if(ipv6_is_unicast(&dest) && !ipv6_link_get(&dest))
    {
        // not a local packet, try to forward
        if(h->proto == 0)
        {
            struct ipv6_exthdr_t *exth = (struct ipv6_exthdr_t *)&h[1];
            
            if(exth->ext.routing.routtype == 0)
            {
                return ipv6_forward(p);
            }
        }
        else
        {
            return ipv6_forward(p);
        }
    }
    
    if((proto = ipv6_ext_headers(p)) <= 0)
    {
        packet_free(p);
        netstats.ip.err++;
        return 0;
    }
    
    if(ipv6_is_unicast(&dest))
    {
        transport_enqueue_in(p, proto, 1);
        netstats.ip.recv++;
        return 0;
    }

    if(ipv6_is_multicast(dest.s6_addr))
    {
        if(ipv6_process_received_multicast(p) == 0)
        {
            netstats.ip.recv++;
            return 0;
        }

        transport_enqueue_in(p, proto, 1);
        netstats.ip.recv++;
    }

    return 0;
}


/*
 * Send a packet to the Ethernet layer.
 * Called from the network dispatcher when processing IPv6 output queue.
 */
int ipv6_process_out(struct packet_t *p)
{
    // enqueue for the ethernet layer to process next
    if(IFQ_FULL(&ethernet_outq))
    {
        netstats.ip.drop++;
        netstats.ip.err++;
        packet_free(p);
        return -ENOBUFS;
    }

    IFQ_ENQUEUE(&ethernet_outq, p);
    netstats.ip.xmit++;
    return 0;
}


void ipv6_nd_dad(struct in6_addr *addr)
{
    struct ipv6_link_t *link;
    
    if(!(link = ipv6_link_is_tentative(addr)))
    {
        return;
    }
    
    if(!(link->ifp->flags & IFF_UP))
    {
        KDEBUG("ipv6: rescheduling ND DAD\n");
        //timeout(ipv6_nd_dad, &link->addr, 10);
        link->dad_expiry = ticks + 10;
        return;
    }
    
    if(link->is_duplicate)
    {
        struct in6_addr old_addr;
        KDEBUG("ipv6: duplicate address\n");
        IPV6_COPY(&old_addr, addr);
        
        if(ipv6_is_linklocal(addr->s6_addr))
        {
            uint32_t r1 = genrand_int32();
            uint32_t r2 = genrand_int32();

            addr->s6_addr[8] = (uint8_t)((uint8_t)(r1 & 0xff) & (uint8_t)(~3));
            addr->s6_addr[9] = (r1 >> 8) & 0xff;
            addr->s6_addr[10] = (r1 >> 16) & 0xff;
            addr->s6_addr[11] = (r1 >> 24) & 0xff;
            addr->s6_addr[12] = (r2) & 0xff;
            addr->s6_addr[13] = (r2 >> 8) & 0xff;
            addr->s6_addr[14] = (r2 >> 16) & 0xff;
            addr->s6_addr[15] = (r2 >> 24) & 0xff;
            ipv6_link_add_locked(link->ifp, addr, &link->netmask, NULL, 1);
        }
        
        ipv6_link_del_locked(link->ifp, &old_addr, 1);
    }
    else
    {
        if(link->dup_detect_retrans-- == 0)
        {
            KDEBUG("ipv6: DAD verified valid address\n");
            link->is_tentative = 0;
        }
        else
        {
            icmp6_neighbor_solicit(link->ifp, &link->addr, ICMP6_ND_DAD);
            //timeout(ipv6_nd_dad, &link->addr, 100);
            link->dad_expiry = ticks + 100;
        }
    }
}


/*
 * IPv6 slow task function.
 */
void ipv6_slow_task_func(void *arg)
{
    UNUSED(arg);

    for(;;)
    {
        struct ipv6_link_t *link, *next;

        //KDEBUG("ipv6_slow_task_func:\n");

        kernel_mutex_lock(&ipv6_lock);

        for(next = NULL, link = ipv6_links; link != NULL; link = next)
        {
            next = link->next;

            if(link->dad_expiry && link->dad_expiry < ticks)
            {
                ipv6_nd_dad(&link->addr);
            }
        }

        // check for expired IPv6 addresses
        for(next = NULL, link = ipv6_links; link != NULL; link = next)
        {
            next = link->next;

            if(link->link_expiry > 0 && link->link_expiry < ticks)
            {
                KDEBUG("ipv6: address has expired\n");
                ipv6_link_del_locked(link->ifp, &link->addr, 1);
            }
        }

        kernel_mutex_unlock(&ipv6_lock);


        ipv6_nd_check_expired();


        //KDEBUG("ipv6_slow_task_func: sleeping\n");
        
        // schedule every 500 ms
        //block_task2(&ipv6_slow_task, PIT_FREQUENCY / 2);
        block_task2(&ipv6_slow_task, PIT_FREQUENCY);
    }
}


/*
void ipv6_fast_task_func(void *arg)
{
    UNUSED(arg);

    for(;;)
    {
        //KDEBUG("ipv6_fast_task_func:\n");

        ipv6_nd_check_expired();

        //KDEBUG("ipv6_fast_task_func: sleeping\n");

        // schedule every 200 ms
        block_task2(&ipv6_fast_task, PIT_FREQUENCY / 5);
    }
}
*/


void ipv6_init(void)
{
    // fork the slow timeout task
    (void)start_kernel_task("ip6-sl", ipv6_slow_task_func, NULL, 
                            &ipv6_slow_task, 0);

    // fork the fast timeout task
    /*
    (void)start_kernel_task("ip6-fa", ipv6_fast_task_func, NULL, 
                            &ipv6_fast_task, 0);
    */
}

