/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: ipv6_nd.c
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
 *  \file ipv6_nd.c
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
#include <kernel/laylaos.h>
#include <kernel/timer.h>
#include <kernel/net.h>
#include <kernel/net/packet.h>
#include <kernel/net/netif.h>
#include <kernel/net/ipv6.h>
#include <kernel/net/icmp6.h>
#include <kernel/net/notify.h>
#include <kernel/net/checksum.h>
#include <mm/kheap.h>

#define NR_ND_SOLICIT               3
#define NR_ND_QUEUED                8


#define ND_STATE_INCOMPLETE         0
#define ND_STATE_REACHABLE          1
#define ND_STATE_STALE              2
#define ND_STATE_DELAY              3
#define ND_STATE_PROBE              4

struct ipv6_neighbor_t
{
    int state;
    struct in6_addr addr;
    struct ether_addr_t ether_addr;
    struct netif_t *ifp;
    int is_router;
    unsigned long long expire;      // in ticks
    int nfailed;
    struct ipv6_neighbor_t *next;
};

struct kernel_mutex_t ipv6_cache_lock = { 0, };
struct ipv6_neighbor_t *ipv6_cache = NULL;

struct kernel_mutex_t postpone_lock = { 0, };
struct packet_t *queued_ipv6_packets[NR_ND_QUEUED] = { 0, };


static inline struct ipv6_neighbor_t *ipv6_nd_neighbor_find(struct in6_addr *addr)
{
    struct ipv6_neighbor_t *neighbor;
    
    for(neighbor = ipv6_cache; neighbor != NULL; neighbor = neighbor->next)
    {
        if(ipv6_cmp(&neighbor->addr, addr) == 0)
        {
            return neighbor;
        }
    }
    
    return NULL;
}


static struct ipv6_neighbor_t *ipv6_nd_add(struct in6_addr *addr, struct netif_t *ifp)
{
    struct ipv6_neighbor_t *neighbor;
    
    if(!(neighbor = kmalloc(sizeof(struct ipv6_neighbor_t))))
    {
        return NULL;
    }
    
    A_memset(neighbor, 0, sizeof(struct ipv6_neighbor_t));
    IPV6_COPY(&neighbor->addr, addr);
    neighbor->ifp = ifp;
    
    kernel_mutex_lock(&ipv6_cache_lock);
    neighbor->next = ipv6_cache;
    ipv6_cache = neighbor;
    kernel_mutex_unlock(&ipv6_cache_lock);
    
    return neighbor;
}


static void ipv6_nd_new_expire_time(struct ipv6_neighbor_t *neighbor)
{
    neighbor->expire = ticks;

    if(neighbor->state == ND_STATE_REACHABLE)
    {
        neighbor->expire += ND_REACHABLE_TIME;
    }
    else if(neighbor->state == ND_STATE_DELAY ||
            neighbor->state == ND_STATE_STALE)
    {
        neighbor->expire += ND_DELAY_FIRST_PROBE_TIME;
    }
    else
    {
        neighbor->expire += neighbor->ifp->hostvars.retrans_time;
    }
}


static void ipv6_nd_queued_trigger(void)
{
    int i;

    kernel_mutex_lock(&postpone_lock);
    
    // add to queue
    for(i = 0; i < NR_ND_QUEUED; i++)
    {
        if(queued_ipv6_packets[i])
        {
            if(IFQ_FULL(&ethernet_outq))
            {
                packet_free(queued_ipv6_packets[i]);
            }
            else
            {
                IFQ_ENQUEUE(&ethernet_outq, queued_ipv6_packets[i]);
            }
            
            queued_ipv6_packets[i] = NULL;
        }
    }

    kernel_mutex_unlock(&postpone_lock);
}


static void ipv6_nd_discover(struct ipv6_neighbor_t *neighbor)
{
    if(!neighbor)
    {
        return;
    }
    
    if(neighbor->expire)
    {
        return;
    }
    
    if(++neighbor->nfailed > NR_ND_SOLICIT)
    {
        return;
    }

    if(neighbor->state == ND_STATE_INCOMPLETE)
    {
        icmp6_neighbor_solicit(neighbor->ifp, &neighbor->addr, 
                               ICMP6_ND_SOLICITED);
    }
    else
    {
        icmp6_neighbor_solicit(neighbor->ifp, &neighbor->addr, 
                               ICMP6_ND_UNICAST);
    }
    
    ipv6_nd_new_expire_time(neighbor);
}


struct ether_addr_t *ipv6_nd_get(struct in6_addr *addr, struct netif_t *ifp)
{
    struct in6_addr gateway = { 0, };
    struct in6_addr dest = { 0, };
    struct ipv6_neighbor_t *neighbor;

    ipv6_route_gateway_get(&gateway, addr);
    
    if(memcmp(gateway.s6_addr, IPv6_ANY, 16) == 0)
    {
        // no gateway, local destination
        dest = *addr;
    }
    else
    {
        dest = gateway;
    }
    
    neighbor = ipv6_nd_neighbor_find(&dest);
    
    if(!neighbor)
    {
        neighbor = ipv6_nd_add(&dest, ifp);
        ipv6_nd_discover(neighbor);
        return NULL;
    }
    
    if(neighbor->state == ND_STATE_INCOMPLETE)
    {
        return NULL;
    }

    if(neighbor->state == ND_STATE_STALE)
    {
        neighbor->state = ND_STATE_DELAY;
        ipv6_nd_new_expire_time(neighbor);
    }

    if(neighbor->state != ND_STATE_REACHABLE)
    {
        ipv6_nd_discover(neighbor);
    }
    
    return &neighbor->ether_addr;
}


struct ether_addr_t *ipv6_get_neighbor(struct packet_t *p)
{
    struct ipv6_hdr_t *h;
    struct ipv6_link_t *link;
    struct in6_addr src = { 0, };
    struct in6_addr dest = { 0, };

    if(!p)
    {
        return NULL;
    }
    
    h = p->data;
    IPV6_COPY(src.s6_addr, h->src.s6_addr);
    IPV6_COPY(dest.s6_addr, h->dest.s6_addr);
    
    // still probing for duplicate address?
    if(ipv6_link_is_tentative(&src))
    {
        return NULL;
    }
    
    // is it our address?
    if((link = ipv6_link_get(&dest)))
    {
        return &(link->ifp->ethernet_addr);
    }
    
    return ipv6_nd_get(&dest, p->ifp);
}


void ipv6_nd_postpone(struct packet_t *p)
{
    int i;
    static int last = -1;
    
    kernel_mutex_lock(&postpone_lock);
    
    // add to queue
    for(i = 0; i < NR_ND_QUEUED; i++)
    {
        if(queued_ipv6_packets[i] == NULL)
        {
            queued_ipv6_packets[i] = p;
            last = i;
            kernel_mutex_unlock(&postpone_lock);
            return;
        }
    }
    
    // queue is full, overwrite the oldest entry
    if(++last >= NR_ND_QUEUED)
    {
        last = 0;
    }
    
    if(queued_ipv6_packets[last])
    {
        packet_free(queued_ipv6_packets[last]);
    }
    
    queued_ipv6_packets[last] = p;
    kernel_mutex_unlock(&postpone_lock);
}


#if 0

void ipv6_nd_dad(void *arg)
{
    struct in6_addr *addr = (struct in6_addr *)arg;
    struct ipv6_link_t *link;
    
    if(!arg)
    {
        return;
    }
    
    if(!(link = ipv6_link_is_tentative(addr)))
    {
        return;
    }
    
    if(!(link->ifp->flags & IFF_UP))
    {
        KDEBUG("ipv6: rescheduling ND DAD\n");
        timeout(ipv6_nd_dad, &link->addr, 10);
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
            ipv6_link_add(link->ifp, addr, &link->netmask, NULL);
        }
        
        ipv6_link_del(link->ifp, &old_addr);
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
            icmp6_neighbor_solicit(link->ifp, &link->addr, ICMP6_ND_DAD, NULL);
            timeout(ipv6_nd_dad, &link->addr, 100);
        }
    }
}

#endif


static int ipv6_nd_options(uint8_t *options, struct icmp6_opt_lladdr_t *opt,
                           uint8_t expected_opt, int optlen, int len)
{
    uint8_t type = 0;
    int found = 0;
    
    while(optlen > 0)
    {
        type = ((struct icmp6_opt_lladdr_t *)options)->type;
        len = ((struct icmp6_opt_lladdr_t *)options)->len;
        optlen -= len << 3;     // len in units of 8 octets
        
        if(len <= 0)
        {
            // malformed option
            return -EINVAL;
        }
        
        if(type == expected_opt)
        {
            if(found > 0)
            {
                // malformed option
                return -EINVAL;
            }
            
            memcpy(opt, (struct icmp6_opt_lladdr_t *)options,
                   sizeof(struct icmp6_opt_lladdr_t));
            found++;
        }
        
        if(optlen > 0)
        {
            options += len << 3;
        }
        else
        {
            return found;
        }
    }
    
    return found;
}


static int neighbor_opts(struct packet_t *p, 
                         struct icmp6_opt_lladdr_t *opt, uint8_t expected_opt)
{
    struct icmp6_hdr_t *icmph = (struct icmp6_hdr_t *)p->transport_hdr;
    uint8_t *option = NULL;
    int optlen, len = 0;
    
    optlen = p->count - ((uintptr_t)icmph - (uintptr_t)p->data) - 24;
    
    if(optlen)
    {
        option = ((uint8_t *)&icmph->msg.info.neighbor_adv) + 20;
    }
    
    return ipv6_nd_options(option, opt, expected_opt, optlen, len);
}


static void ipv6_neighbor_from_unsolicited(struct packet_t *p)
{
    struct ipv6_neighbor_t *neighbor;
    struct ipv6_hdr_t *iph = (struct ipv6_hdr_t *)p->data;
    struct icmp6_opt_lladdr_t opt = { 0, };
    int valid_lladdr = neighbor_opts(p, &opt, ND_OPT_LLADDR_SRC);
    struct in6_addr src = { 0, };
    
    IPV6_COPY(src.s6_addr, iph->src.s6_addr);
    
    if(!ipv6_is_unspecified(src.s6_addr) && valid_lladdr > 0)
    {
        KDEBUG("ipv6_neighbor_from_unsolicited: 1\n");

        if(!(neighbor = ipv6_nd_neighbor_find(&src)))
        {
            KDEBUG("ipv6_neighbor_from_unsolicited: 2\n");

            if((neighbor = ipv6_nd_add(&src, p->ifp)))
            {
                COPY_ETHER_ADDR(neighbor->ether_addr.addr, opt.addr.addr);
                neighbor->state = ND_STATE_STALE;
                ipv6_nd_queued_trigger();
            }
        }
        else if(memcmp(opt.addr.addr, neighbor->ether_addr.addr, ETHER_ADDR_LEN))
        {
            KDEBUG("ipv6_neighbor_from_unsolicited: 3\n");

            COPY_ETHER_ADDR(neighbor->ether_addr.addr, opt.addr.addr);
            neighbor->state = ND_STATE_STALE;
            ipv6_nd_queued_trigger();
            ipv6_nd_new_expire_time(neighbor);
        }
    }

    KDEBUG("ipv6_neighbor_from_unsolicited: 4\n");
}


#define TWO_HOURS       (60 * 60 * 2 * PIT_FREQUENCY)


static inline void ipv6_lifetime_set(struct ipv6_link_t *link,
                                     unsigned long long expire)
{
    if(expire <= ticks)
    {
        return;
    }
    
    if(expire > 0xfffffffffffffffe)
    {
        link->link_expiry = 0;
    }
    else if((expire > (ticks + TWO_HOURS)) || (expire > link->link_expiry))
    {
        link->link_expiry = expire;
    }
    else
    {
        link->link_expiry = ticks + TWO_HOURS;
    }
}


static int neighbor_adv_process(struct packet_t *p)
{
    struct ipv6_neighbor_t *neighbor;
    struct icmp6_hdr_t *icmph = (struct icmp6_hdr_t *)p->transport_hdr;
    struct icmp6_opt_lladdr_t opt = { 0, };
    struct in6_addr dest = { 0, };
    int optres = neighbor_opts(p, &opt, ND_OPT_LLADDR_TGT);

    KDEBUG("neighbor_adv_process: 1\n");
    
    if(optres < 0)
    {
        // Malformed packet: option field cannot be processed
        return -EINVAL;
    }
    
    KDEBUG("neighbor_adv_process: 2\n");

    IPV6_COPY(dest.s6_addr, icmph->msg.info.neighbor_adv.target.s6_addr);
    
    // Check if there's a NCE in the cache
    if(!(neighbor = ipv6_nd_neighbor_find(&dest)))
    {
        return -EINVAL;
    }

    KDEBUG("neighbor_adv_process: 3\n");
    
    if(!optres || IS_OVERRIDE(icmph) ||
       !memcmp(neighbor->ether_addr.addr, opt.addr.addr, ETHER_ADDR_LEN))
    {
        if(!IS_ROUTER(icmph) && neighbor->is_router)
        {
            ipv6_router_down(&neighbor->addr);
        }
        
        neighbor->is_router = IS_ROUTER(icmph) ? 1 : 0;
    }
    
    if(optres > 0 && neighbor->state == ND_STATE_INCOMPLETE)
    {
        KDEBUG("neighbor_adv_process: 4\n");

        if(IS_SOLICITED(icmph))
        {
            neighbor->state = ND_STATE_REACHABLE;
            neighbor->nfailed = 0;
            ipv6_nd_new_expire_time(neighbor);
        }
        else
        {
            neighbor->state = ND_STATE_STALE;
        }
        
        COPY_ETHER_ADDR(neighbor->ether_addr.addr, opt.addr.addr);
        ipv6_nd_queued_trigger();
        
        return 0;
    }
    
    if(optres > 0)
    {
        KDEBUG("neighbor_adv_process: 5\n");

        if(IS_SOLICITED(icmph) && !IS_OVERRIDE(icmph) &&
          !memcmp(neighbor->ether_addr.addr, opt.addr.addr, ETHER_ADDR_LEN))
        {
            neighbor->state = ND_STATE_REACHABLE;
            neighbor->nfailed = 0;
            ipv6_nd_queued_trigger();
            ipv6_nd_new_expire_time(neighbor);
            KDEBUG("neighbor_adv_process: 5a\n");
            return 0;
        }
        
        if(neighbor->state == ND_STATE_REACHABLE &&
           IS_SOLICITED(icmph) && !IS_OVERRIDE(icmph))
        {
            neighbor->state = ND_STATE_STALE;
            KDEBUG("neighbor_adv_process: 5b\n");
            return 0;
        }

        if(IS_SOLICITED(icmph) && IS_OVERRIDE(icmph))
        {
            COPY_ETHER_ADDR(neighbor->ether_addr.addr, opt.addr.addr);
            neighbor->state = ND_STATE_REACHABLE;
            neighbor->nfailed = 0;
            ipv6_nd_queued_trigger();
            ipv6_nd_new_expire_time(neighbor);
            KDEBUG("neighbor_adv_process: 5c\n");
            return 0;
        }

        if(!IS_SOLICITED(icmph) && IS_OVERRIDE(icmph) &&
           memcmp(neighbor->ether_addr.addr, opt.addr.addr, ETHER_ADDR_LEN))
        {
            COPY_ETHER_ADDR(neighbor->ether_addr.addr, opt.addr.addr);
            neighbor->state = ND_STATE_STALE;
            ipv6_nd_queued_trigger();
            ipv6_nd_new_expire_time(neighbor);
            KDEBUG("neighbor_adv_process: 5d\n");
            return 0;
        }

        if(neighbor->state == ND_STATE_REACHABLE &&
           !IS_SOLICITED(icmph) && !IS_OVERRIDE(icmph) &&
           memcmp(neighbor->ether_addr.addr, opt.addr.addr, ETHER_ADDR_LEN))
        {
            // If the Override flag is clear and the supplied link-layer 
            // address differs from that in the cache, then one of two
            // actions takes place:
            //   a. If the state of the entry is REACHABLE, set it to STALE, 
            //      but do not update the entry in any other way.
            //   b. Otherwise, the received advertisement should be ignored 
            //      and MUST NOT update the cache.
            neighbor->state = ND_STATE_STALE;
            ipv6_nd_new_expire_time(neighbor);
            KDEBUG("neighbor_adv_process: 5e\n");
            return 0;
        }
    }
    else
    {
        KDEBUG("neighbor_adv_process: 6\n");

        if(IS_SOLICITED(icmph))
        {
            neighbor->state = ND_STATE_REACHABLE;
            neighbor->nfailed = 0;
            ipv6_nd_queued_trigger();
            ipv6_nd_new_expire_time(neighbor);
            KDEBUG("neighbor_adv_process: 6a\n");
            return 0;
        }
    }

    KDEBUG("neighbor_adv_process: 7\n");

    return -EINVAL;
}


static int neighbor_sol_detect_dad(struct packet_t *p)
{
    struct ipv6_link_t *link;
    struct ipv6_hdr_t *iph = (struct ipv6_hdr_t *)p->data;
    struct icmp6_hdr_t *icmph = (struct icmp6_hdr_t *)p->transport_hdr;
    struct in6_addr dest, src;
    
    IPV6_COPY(src.s6_addr, iph->src.s6_addr);
    IPV6_COPY(dest.s6_addr, icmph->msg.info.neighbor_adv.target.s6_addr);

    if((link = ipv6_link_is_tentative(&dest)))
    {
        if(ipv6_is_unicast(&src))
        {
            // RFC4862 5.4.3 : sender is performing address resolution,
            // our address is not yet valid, discard silently
            KDEBUG("ipv6: Sender performing AR\n");
        }
        else if(ipv6_is_unspecified(src.s6_addr))
        {
            IPV6_COPY(dest.s6_addr, iph->dest.s6_addr);
            
            if(ipv6_is_allhosts_multicast(dest.s6_addr))
            {
                KDEBUG("ipv6: Sender performing DaD\n");

                int is_linklocal = ipv6_is_linklocal(link->addr.s6_addr);
                struct netif_t *ifp = link->ifp;
                
                KDEBUG("ipv6: Duplicate address detected. Removing link\n");
                ipv6_link_del(link->ifp, &link->addr);
                
                if(is_linklocal)
                {
                    netif_ipv6_random_ll(ifp);
                }
            }
        }
        
        return 0;
    }
    
    return -EINVAL;
}


static int neighbor_sol_process(struct packet_t *p)
{
    struct ipv6_link_t *link;
    struct icmp6_hdr_t *icmph = (struct icmp6_hdr_t *)p->transport_hdr;
    struct icmp6_opt_lladdr_t opt = { 0, };
    int valid_lladdr = neighbor_opts(p, &opt, ND_OPT_LLADDR_SRC);
    struct in6_addr dest;

    KDEBUG("neighbor_sol_process: 1\n");

    IPV6_COPY(dest.s6_addr, icmph->msg.info.neighbor_adv.target.s6_addr);
    ipv6_neighbor_from_unsolicited(p);

    KDEBUG("neighbor_sol_process: 2\n");
    
    if(!valid_lladdr && !neighbor_sol_detect_dad(p))
    {
        return 0;
    }

    KDEBUG("neighbor_sol_process: 3\n");
    
    if(valid_lladdr < 0)
    {
        // Malformed packet
        return -EINVAL;
    }

    KDEBUG("neighbor_sol_process: 4\n");
    
    if(!(link = ipv6_link_get(&dest)))
    {
        // Not for us
        return -EINVAL;
    }

    KDEBUG("neighbor_sol_process: 5\n");
    
    icmp6_neighbor_advertise(p, &dest);
    
    return 0;
}


static int radv_process(struct packet_t *p)
{
    struct ipv6_link_t *link;
    struct ipv6_hdr_t *iph = (struct ipv6_hdr_t *)p->data;
    struct icmp6_hdr_t *icmph = (struct icmp6_hdr_t *)p->transport_hdr;
    struct in6_addr any = { 0, };
    struct in6_addr src = { 0, };
    int optlen = p->count - ((uintptr_t)icmph - (uintptr_t)iph) - 16;
    uint8_t *optstart = ((uint8_t *)&icmph->msg.info.router_adv) + 12;
    uint8_t *nextopt = optstart;
    uint32_t pref_lifetime;
    
    while(optlen > 0)
    {
        uint8_t *type = (uint8_t *)nextopt;
        
        switch(*type)
        {
            case ND_OPT_PREFIX:
            {
                KDEBUG("radv_process: option PREFIX\n");

                struct icmp6_opt_prefix_t *prefix = 
                            (struct icmp6_opt_prefix_t *)nextopt;

                pref_lifetime = ntohl(prefix->pref_lifetime);
                KDEBUG("radv_process: pref_lifetime %u\n", pref_lifetime);

                // Silently ignore the Prefix Information option if:
                // a) If the Autonomous flag is not set
                // b) If the prefix is the link-local prefix
                // c) If the preferred lifetime is greater than the valid 
                //    lifetime
                if(prefix->aac == 0 || 
                   ipv6_is_linklocal(prefix->prefix.s6_addr) ||
                   pref_lifetime > ntohl(prefix->val_lifetime) ||
                   prefix->val_lifetime == 0)
                {
                    KDEBUG("radv_process: ignoring PREFIX\n");
                    optlen -= (prefix->len << 3);
                    nextopt += (prefix->len << 3);
                    break;
                }
                
                if(prefix->prefix_len != 64)
                {
                    KDEBUG("radv_process: invalid PREFIX len (%u)\n", prefix->prefix_len);
                    return -EINVAL;
                }
                
                IPV6_COPY(src.s6_addr, prefix->prefix.s6_addr);

                // Refresh lifetime of a prefix
                if((link = ipv6_prefix_configured(&src)))
                {
                    KDEBUG("radv_process: PREFIX configured\n");
                    ipv6_lifetime_set(link, ticks + 
                            (ntohl(prefix->val_lifetime) * PIT_FREQUENCY));
                    optlen -= (prefix->len << 3);
                    nextopt += (prefix->len << 3);
                    break;
                }
                
                // Configure a non linklocal IPv6 address
                if(ipv6_link_add_local(p->ifp, &src, &link) == 0)
                {
                    KDEBUG("radv_process: added local PREFIX\n");
                    ipv6_lifetime_set(link, ticks + 
                            (ntohl(prefix->val_lifetime) * PIT_FREQUENCY));

                    // Add a default gateway to the default routers list 
                    // with source of RADV
                    IPV6_COPY(src.s6_addr, iph->src.s6_addr);
                    ipv6_route_add(link, &any, &any, &src, 10);
                }

                KDEBUG("radv_process: finished with PREFIX\n");
                optlen -= (prefix->len << 3);
                nextopt += (prefix->len << 3);
                break;
            }

            case ND_OPT_LLADDR_SRC:
            {
                KDEBUG("radv_process: option LLADDR_SRC\n");

                struct icmp6_opt_lladdr_t *lladdr_src =
                        (struct icmp6_opt_lladdr_t *)nextopt;

                optlen -= (lladdr_src->len << 3);
                nextopt += (lladdr_src->len << 3);
                break;
            }

            case ND_OPT_MTU:
            {
                KDEBUG("radv_process: option MTU\n");

                struct icmp6_opt_mtu_t *mtu =
                        (struct icmp6_opt_mtu_t *)nextopt;

                optlen -= (mtu->len << 3);
                nextopt += (mtu->len << 3);
                break;
            }

            case ND_OPT_REDIRECT:
            {
                KDEBUG("radv_process: option REDIRECT\n");

                struct icmp6_opt_redirect_t *redirect =
                        (struct icmp6_opt_redirect_t *)nextopt;

                optlen -= (redirect->len << 3);
                nextopt += (redirect->len << 3);
                break;
            }

            case ND_OPT_RDNSS:
            {
                KDEBUG("radv_process: option RDNSS\n");

                struct icmp6_opt_rdnss_t *rdnss =
                        (struct icmp6_opt_rdnss_t *)nextopt;

                optlen -= (rdnss->len << 3);
                nextopt += (rdnss->len << 3);
                break;
            }
            
            default:
                KDEBUG("radv_process: option UNKNOWN\n");

                icmp6_param_problem(p, ICMP6_PARAMPROBLEM_IPV6OPT,
                                    IPv6_HLEN + 16 + (nextopt - optstart));
                return -EINVAL;
        }
    }
    
    if(icmph->msg.info.router_adv.retransmit_time != 0)
    {
        p->ifp->hostvars.retrans_time = 
                ntohl(icmph->msg.info.router_adv.retransmit_time);
    }
    
    return 0;
}


static int neighbor_adv_optlen_check(struct packet_t *p)
{
    struct icmp6_hdr_t *icmph = (struct icmp6_hdr_t *)p->transport_hdr;
    uint8_t *opt;
    int optlen = p->count - ((uintptr_t)icmph - (uintptr_t)p->data) - 24;
    
    // All included options have a length that is greater than zero
    opt = ((uint8_t *)&icmph->msg.info.neighbor_adv) + 20;
    
    while(optlen > 0)
    {
        int optsz = (opt[1] << 3);
        
        if(optsz == 0)
        {
            return -EINVAL;
        }
        
        opt += optsz;
        optlen -= optsz;
    }
    
    return 0;
}


static int neighbor_sol_validate_unspec(struct packet_t *p)
{
    // RFC4861, 7.1.1:
    // - If the IP source address is the unspecified address, the IP
    //   destination address is a solicited-node multicast address.
    //
    // - If the IP source address is the unspecified address, there is no
    //   source link-layer address option in the message.

    struct ipv6_hdr_t *iph = (struct ipv6_hdr_t *)p->data;
    struct icmp6_opt_lladdr_t opt = { 0, };
    int valid_lladdr = neighbor_opts(p, &opt, ND_OPT_LLADDR_SRC);
    struct in6_addr dest;
    
    IPV6_COPY(dest.s6_addr, iph->dest.s6_addr);
    
    if(!ipv6_is_solnode_multicast(p->ifp, dest.s6_addr))
    {
        return -EINVAL;
    }
    
    return valid_lladdr ? -EINVAL : 0;
}


int ipv6_nd_recv(struct packet_t *p)
{
    struct ipv6_link_t *link;
    struct ipv6_hdr_t *iph = (struct ipv6_hdr_t *)p->data;
    struct icmp6_hdr_t *icmph = (struct icmp6_hdr_t *)p->transport_hdr;
    struct in6_addr dest = { 0, };
    struct in6_addr tmp = { 0, };
    struct in6_addr src = { 0, };
    int res = -EINVAL;
    
    switch(icmph->type)
    {
        case ICMP6_MSG_REDIRECT:
        case ICMP6_MSG_ROUTER_SOLICIT:
            ipv6_neighbor_from_unsolicited(p);
            res = 0;
            break;
        
        case ICMP6_MSG_ROUTER_ADV:
            KDEBUG("ipv6_nd_recv: received router advertisement\n");
            if(iph->ttl != 255 || icmp6_checksum(p) || icmph->code)
            {
                break;
            }
            
            if(p->count - ((uintptr_t)icmph - (uintptr_t)iph) < 16)
            {
                break;
            }

            ipv6_neighbor_from_unsolicited(p);
            res = radv_process(p);
            break;

        case ICMP6_MSG_NEIGHBOR_SOLICIT:
            KDEBUG("ipv6_nd_recv: received neighbor solicitation\n");
            if(iph->ttl != 255 || icmp6_checksum(p) || icmph->code)
            {
                break;
            }
            
            if(p->count - ((uintptr_t)icmph - (uintptr_t)iph) < 24)
            {
                break;
            }
            
            IPV6_COPY(src.s6_addr, iph->src.s6_addr);
            IPV6_COPY(tmp.s6_addr, icmph->msg.info.neighbor_solicit.target.s6_addr);
            
            if(ipv6_is_unspecified(src.s6_addr) &&
               neighbor_sol_validate_unspec(p) < 0)
            {
                break;
            }
            
            if(ipv6_is_multicast(dest.s6_addr))
            {
                if(!ipv6_is_solnode_multicast(p->ifp, tmp.s6_addr))
                {
                    break;
                }
            }
            else
            {
                link = ipv6_link_by_ifp(p->ifp);
                res = -EINVAL;
                
                while(link)
                {
                    // RFC4861, 7.2.3:
                    //  - The Target Address is a "valid" unicast or anycast 
                    //    address assigned to the receiving interface 
                    //    [ADDRCONF],
                    //  - The Target Address is a unicast or anycast address
                    //    for which the node is offering proxy service, or
                    //  - The Target Address is a "tentative" address on which
                    //    Duplicate Address Detection is being performed
                    
                    if(!ipv6_cmp(&link->addr, &tmp))
                    {
                        res = 0;
                        break;
                    }
                    
                    link = ipv6_link_by_ifp_next(p->ifp, link);
                }
                
                if(res < 0)
                {
                    break;
                }
            }

            res = neighbor_sol_process(p);
            break;

        case ICMP6_MSG_NEIGHBOR_ADV:
            KDEBUG("ipv6_nd_recv: received neighbor advertisement\n");
            if(iph->ttl != 255 || icmp6_checksum(p) || icmph->code)
            {
                break;
            }
            
            if(p->count - ((uintptr_t)icmph - (uintptr_t)iph) < 24)
            {
                break;
            }
            
            IPV6_COPY(dest.s6_addr, iph->dest.s6_addr);
            
            // If the IP Destination Address is a multicast address the 
            // Solicited flag is zero.
            if(ipv6_is_multicast(dest.s6_addr) && IS_SOLICITED(icmph))
            {
                break;
            }
            
            if(neighbor_adv_optlen_check(p) < 0)
            {
                break;
            }
            
            IPV6_COPY(tmp.s6_addr, icmph->msg.info.neighbor_adv.target.s6_addr);

            // Target address belongs to a tentative link on this 
            // device, DaD detected a dup
            if((link = ipv6_link_is_tentative(&tmp)))
            {
                int is_linklocal = ipv6_is_linklocal(link->addr.s6_addr);
                struct netif_t *ifp = link->ifp;
                
                KDEBUG("ipv6: Duplicate address detected. Removing link\n");
                ipv6_link_del(link->ifp, &link->addr);
                
                if(is_linklocal)
                {
                    netif_ipv6_random_ll(ifp);
                }
            }
            
            res = neighbor_adv_process(p);
            break;
    }
    
    packet_free(p);
    return res;
}


static void ipv6_nd_unreachable(struct in6_addr *addr)
{
    int i;
    struct ipv6_hdr_t *iph;
    struct in6_addr gateway, dest, src;

    kernel_mutex_lock(&postpone_lock);
    
    for(i = 0; i < NR_ND_QUEUED; i++)
    {
        if(queued_ipv6_packets[i])
        {
            iph = (struct ipv6_hdr_t *)queued_ipv6_packets[i]->data;
            IPV6_COPY(dest.s6_addr, iph->dest.s6_addr);
            ipv6_route_gateway_get(&gateway, &dest);
            
            if(ipv6_is_unspecified(gateway.s6_addr))
            {
                IPV6_COPY(gateway.s6_addr, iph->dest.s6_addr);
            }
            
            if(memcmp(gateway.s6_addr, addr->s6_addr, 16) == 0)
            {
                IPV6_COPY(src.s6_addr, iph->src.s6_addr);

                if(!ipv6_is_unspecified(src.s6_addr) &&
                   !ipv6_link_get(&src))
                {
                    // source is not local
                    notify_dest_unreachable(queued_ipv6_packets[i], 1);
                }
                
                packet_free(queued_ipv6_packets[i]);
                queued_ipv6_packets[i] = NULL;
            }
        }
    }

    kernel_mutex_unlock(&postpone_lock);
}


void ipv6_nd_check_expired(void)
{
    struct ipv6_neighbor_t *neighbor, *prev = NULL, *next;

    kernel_mutex_lock(&ipv6_cache_lock);
    
    for(neighbor = ipv6_cache; neighbor != NULL; )
    {
        if(ticks <= neighbor->expire)
        {
            prev = neighbor;
            neighbor = neighbor->next;
            continue;
        }
        
        switch(neighbor->state)
        {
            case ND_STATE_INCOMPLETE:
            case ND_STATE_PROBE:
                KDEBUG("ipv6_nd_check_expired: PROBE\n");
                if(neighbor->nfailed > NR_ND_SOLICIT)
                {
                    ipv6_nd_unreachable(&neighbor->addr);
                    next = neighbor->next;
                    
                    if(prev)
                    {
                        prev->next = next;
                    }
                    else
                    {
                        ipv6_cache = next;
                    }
                    
                    neighbor->next = NULL;
                    kfree(neighbor);
                    
                    neighbor = next;
                    continue;
                }
                
                neighbor->expire = 0;
                ipv6_nd_discover(neighbor);
                ipv6_nd_new_expire_time(neighbor);
                break;
            
            case ND_STATE_REACHABLE:
                KDEBUG("ipv6_nd_check_expired: REACHABLE\n");
                neighbor->state = ND_STATE_STALE;
                break;
            
            case ND_STATE_STALE:
                KDEBUG("ipv6_nd_check_expired: STALE\n");
                ipv6_nd_new_expire_time(neighbor);
                break;
            
            case ND_STATE_DELAY:
                KDEBUG("ipv6_nd_check_expired: DELAY\n");
                neighbor->expire = 0;
                neighbor->state = ND_STATE_PROBE;
                ipv6_nd_new_expire_time(neighbor);
                break;

            default:
                KDEBUG("ipv6: neighbor in invalid state (%u)\n", neighbor->state);
                ipv6_nd_new_expire_time(neighbor);
                break;
        }
    }

    kernel_mutex_unlock(&ipv6_cache_lock);
}

