/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: ipv4_frag.c
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
 *  \file ipv4_frag.c
 *
 *  Internet Protocol (IP) v4 implementation.
 *
 *  The code is divided into the following files:
 *  - ipv4.c: main IPv4 handling code
 *  - ipv4_addr.c: functions for working with IPv4 addresses
 *  - ipv4_frag.c: functions for handling IPv4 & IPv6 packet fragments
 */

#define __DEBUG
#include <errno.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/timer.h>
#include <kernel/net/packet.h>
#include <kernel/net/protocol.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>
#include <kernel/net/icmp4.h>
#include <kernel/net/icmp6.h>

#define IPV4_FRAGMENT_TIMEOUT       (15000 / MSECS_PER_TICK)
#define IPV6_FRAGMENT_TIMEOUT       (60000 / MSECS_PER_TICK)

#define IPV4_FRAG_OFFSET(frag)      (((uint32_t)frag & IP_OFFMASK) << 3)
#define IPV6_FRAG_OFFSET(frag)      ((frag) & 0xFFF8)

#define IPV6_FRAG_ID(x)             ((uint32_t) \
                                      (((uint32_t)x->ext.frag.id[0] << 24) + \
                                       ((uint32_t)x->ext.frag.id[1] << 16) + \
                                       ((uint32_t)x->ext.frag.id[2] <<  8) + \
                                       ((uint32_t)x->ext.frag.id[3] <<  0)))

static struct packet_t *ipv4_fragments = NULL;
static struct packet_t *ipv6_fragments = NULL;

static struct kernel_mutex_t ipv4_fragments_lock = { 0, };
static struct kernel_mutex_t ipv6_fragments_lock = { 0, };

static uint32_t ipv4_cur_fragment_id = 0;
static uint32_t ipv6_cur_fragment_id = 0;

static int ipv6_fragments_expired = 0;
static int ipv4_fragments_expired = 0;


static void fragments_empty_queue(struct packet_t **first)
{
    struct packet_t *cur, *next = NULL;
    
    if(!first || !*first)
    {
        return;
    }
    
    for(cur = *first; cur != NULL; )
    {
        next = cur->next;
        packet_free(cur);
        cur = next;
    }
    
    *first = NULL;
}


static void fragment_enqueue(struct packet_t **q, struct packet_t *p)
{
    struct packet_t *cur;

    p->next = NULL;
    
    if(*q == NULL)
    {
        *q = p;
    }
    else
    {
        for(cur = *q; cur->next; cur = cur->next)
        {
            ;
        }
        
        cur->next = p;
    }
}


static uint32_t fragments_get_offset(struct packet_t *p, uint8_t net)
{
    if(!p)
    {
        return 0;
    }
    
    if(net == AF_INET)
    {
        return IPV4_FRAG_OFFSET(p->frag);
    }
    else if(net == AF_INET6)
    {
        return IPV6_FRAG_OFFSET(p->frag);
    }
    
    return 0;
}


static int fragments_get_more_flag(struct packet_t *p, uint8_t net)
{
    if(!p)
    {
        return 0;
    }
    
    if(net == AF_INET)
    {
        return !!(p->frag & IP_MF);
    }
    else if(net == AF_INET6)
    {
        return (p->frag & 1);
    }
    
    return 0;
}


/*
static void fragment_expire(void *arg)
{
    struct packet_t *first;

    struct packet_t **q = (struct packet_t **)arg;
    
    if(!arg)
    {
        return;
    }
    
    if(!(first = *q))
    {
        return;
    }
    
    if(q == &ipv6_fragments)
    {
        ipv6_fragments_expired = 1;
    }
    else if(q == &ipv4_fragments)
    {
        ipv4_fragments_expired = 1;
    }
}
*/


void ip_fragment_check_expired(void)
{
    struct packet_t *cur;

    kernel_mutex_lock(&ipv6_fragments_lock);
    ipv6_fragments_expired = 0;

    if(ipv6_fragments)
    {
        for(cur = ipv6_fragments; cur != NULL; cur = cur->next)
        {
            if(cur->timestamp > (ticks + IPV6_FRAGMENT_TIMEOUT))
            {
                ipv6_fragments_expired = 1;
                break;
            }
        }

        struct ipv6_hdr_t *h = (struct ipv6_hdr_t *)ipv6_fragments->data;

        KDEBUG("ipv6: Packet expired! ID: %u\n", ipv6_cur_fragment_id);
        
        if(((fragments_get_offset(ipv6_fragments, AF_INET) == 0) &&
           !ipv6_is_multicast(h->dest.s6_addr) &&
           !ipv6_is_unspecified(h->dest.s6_addr)))
        {
            icmp6_frag_expired(ipv6_fragments);
        }
        
        fragments_empty_queue(&ipv6_fragments);
    }

    ipv6_fragments_expired = 0;
    kernel_mutex_unlock(&ipv6_fragments_lock);

    
    kernel_mutex_lock(&ipv4_fragments_lock);
    ipv4_fragments_expired = 0;

    if(ipv4_fragments)
    {
        for(cur = ipv4_fragments; cur != NULL; cur = cur->next)
        {
            if(cur->timestamp > (ticks + IPV4_FRAGMENT_TIMEOUT))
            {
                ipv4_fragments_expired = 1;
                break;
            }
        }

        struct ipv4_hdr_t *h = (struct ipv4_hdr_t *)ipv4_fragments->data;

        KDEBUG("ipv4: Packet expired! ID: %u\n", ipv4_cur_fragment_id);
        
        if(!ipv4_is_multicast(h->dest.s_addr) &&
           !ipv4_is_broadcast(h->dest.s_addr))
        {
            icmp4_frag_expired(ipv4_fragments);
        }

        fragments_empty_queue(&ipv4_fragments);
    }
    
    ipv4_fragments_expired = 0;
    kernel_mutex_unlock(&ipv4_fragments_lock);
}


static int fragments_reassemble(struct packet_t **q, unsigned int len, 
                                uint8_t proto, uint8_t net)
{
    struct packet_t *first, *cur, *next, *p;
    uint16_t hlen = 0;
    unsigned int bookmark = 0, tlen;
    
    if(!q || !*q)
    {
        return -EINVAL;
    }
    
    if(!(first = *q))
    {
        return -EINVAL;
    }
    
    if(net == AF_INET)
    {
        hlen = IPv4_HLEN;
    }
    else if(net == AF_INET6)
    {
        hlen = IPv6_HLEN;
    }
    
    if(!(p = packet_alloc(hlen + len, PACKET_LINK)))
    {
        return -ENOMEM;
    }
    
    A_memcpy(p->data, first->data, hlen);
    p->transport_hdr = (void *)((uintptr_t)(p->data) + hlen);
    p->ifp = first->ifp;
    
    for(cur = *q; cur != NULL; )
    {
        next = cur->next;
        tlen = cur->count - ((uintptr_t)cur->transport_hdr - 
                             (uintptr_t)cur->data);
        A_memcpy((void *)((uintptr_t)p->transport_hdr + bookmark), 
                 cur->transport_hdr, tlen);
        bookmark += tlen;
        packet_free(cur);
        cur = next;
    }
    
    *q = NULL;
    
    KDEBUG("ipv%d: finished reassembly\n", (net == AF_INET6) ? 6 : 4);
    
    transport_enqueue_in(p, proto, (net == AF_INET6));
    
    return 0;
}


static void fragments_complete(unsigned int bookmark, uint8_t proto, uint8_t net)
{
    if(net == AF_INET)
    {
        if(fragments_reassemble(&ipv4_fragments, bookmark, proto, AF_INET) == 0)
        {
            //untimeout(fragment_expire, &ipv4_fragments);
        }
    }
    else if(net == AF_INET6)
    {
        if(fragments_reassemble(&ipv6_fragments, bookmark, proto, AF_INET6) == 0)
        {
            //untimeout(fragment_expire, &ipv6_fragments);
        }
    }
}


static void fragments_check_complete(struct packet_t *q, uint8_t proto, uint8_t net)
{
    struct packet_t *cur;
    unsigned int bookmark = 0;
    
    if(!q)
    {
        return;
    }
    
    for(cur = q; cur != NULL; cur = cur->next)
    {
        if(fragments_get_offset(cur, net) != bookmark)
        {
            return;
        }
        
        bookmark += cur->count - ((uintptr_t)cur->transport_hdr - (uintptr_t)cur->data);
        
        if(!fragments_get_more_flag(cur, net))
        {
            fragments_complete(bookmark, proto, net);
            return;
        }
    }
}


static int ipv4_fragment_match(struct packet_t *p1, struct packet_t *p2)
{
    struct ipv4_hdr_t *h1, *h2;
    
    if(!p1 || !p2)
    {
        return -1;
    }
    
    h1 = (struct ipv4_hdr_t *)p1->data;
    h2 = (struct ipv4_hdr_t *)p2->data;
    
    if((h1->src.s_addr != h2->src.s_addr) ||
       (h1->dest.s_addr != h2->dest.s_addr))
    {
        return 1;
    }
    
    return 0;
}


static int ipv6_fragment_match(struct packet_t *p1, struct packet_t *p2)
{
    struct ipv6_hdr_t *h1, *h2;
    
    if(!p1 || !p2)
    {
        return -1;
    }
    
    h1 = (struct ipv6_hdr_t *)p1->data;
    h2 = (struct ipv6_hdr_t *)p2->data;
    
    if(memcmp(h1->src.s6_addr, h2->src.s6_addr, 16) ||
       memcmp(h1->dest.s6_addr, h2->dest.s6_addr, 16))
    {
        return 1;
    }
    
    return 0;
}


void ipv4_process_fragment(struct packet_t *p, 
                           struct ipv4_hdr_t *h, uint8_t proto)
{
    struct packet_t *first, *tmp;
    
    if(!p || !h)
    {
        return;
    }
    
    kernel_mutex_lock(&ipv4_fragments_lock);
    first = ipv4_fragments;
    
    if(first)
    {
        // fragments from old packets still in tree, and new first fragment ?
        if(h->id != ipv4_cur_fragment_id &&
           IPV4_FRAG_OFFSET(p->frag) == 0)
        {
            fragments_empty_queue(&ipv4_fragments);
            first = NULL;
            ipv4_cur_fragment_id = 0;
        }
        
        if(ipv4_fragment_match(p, first) == 0 &&
           h->id == ipv4_cur_fragment_id)
        {
            if(!(tmp = packet_duplicate(p)))
            {
                KDEBUG("ipv4: insufficient memory to reassemble IPv4 fragmented"
                       "packet (id %u)\n", ipv4_cur_fragment_id);
                kernel_mutex_unlock(&ipv4_fragments_lock);
                return;
            }
            
            tmp->timestamp = ticks;
            fragment_enqueue(&ipv4_fragments, tmp);
        }
    }
    else
    {
        if(ipv4_cur_fragment_id && h->id == ipv4_cur_fragment_id)
        {
            // Discard late arrivals, without firing the timer
            kernel_mutex_unlock(&ipv4_fragments_lock);
            return;
        }
        
        if(!(tmp = packet_duplicate(p)))
        {
            KDEBUG("ipv4: insufficient memory to start reassembling IPv4 "
                       "fragmented packet\n");
            kernel_mutex_unlock(&ipv4_fragments_lock);
            return;
        }
        
        tmp->timestamp = ticks;
        //untimeout(fragment_expire, &ipv4_fragments);
        //timeout(fragment_expire, &ipv4_fragments, IPV4_FRAGMENT_TIMEOUT);
        ipv4_cur_fragment_id = h->id;

        KDEBUG("Started new reassembly, ID: %u\n", ipv4_cur_fragment_id);

        fragment_enqueue(&ipv4_fragments, tmp);
    }
    
    fragments_check_complete(ipv4_fragments, proto, AF_INET);
    kernel_mutex_unlock(&ipv4_fragments_lock);
}


void ipv6_process_fragment(struct packet_t *p, 
                           struct ipv6_exthdr_t *h, uint8_t proto)
{
    struct packet_t *first, *tmp;
    
    if(!p || !h)
    {
        return;
    }
    
    kernel_mutex_lock(&ipv6_fragments_lock);
    first = ipv6_fragments;
    
    if(first)
    {
        if(ipv6_fragment_match(p, first) == 0 &&
           IPV6_FRAG_ID(h) == ipv6_cur_fragment_id)
        {
            if(!(tmp = packet_duplicate(p)))
            {
                KDEBUG("ipv6: insufficient memory to reassemble IPv6 fragmented"
                       "packet (id %u)\n", ipv6_cur_fragment_id);
                kernel_mutex_unlock(&ipv6_fragments_lock);
                return;
            }
            
            tmp->timestamp = ticks;
            fragment_enqueue(&ipv6_fragments, tmp);
        }
    }
    else
    {
        if(ipv6_cur_fragment_id && IPV6_FRAG_ID(h) == ipv6_cur_fragment_id)
        {
            // Discard late arrivals, without firing the timer
            kernel_mutex_unlock(&ipv6_fragments_lock);
            return;
        }
        
        if(!(tmp = packet_duplicate(p)))
        {
            KDEBUG("ipv6: insufficient memory to start reassembling IPv6 "
                       "fragmented packet\n");
            kernel_mutex_unlock(&ipv6_fragments_lock);
            return;
        }
        
        tmp->timestamp = ticks;
        //untimeout(fragment_expire, &ipv6_fragments);
        //timeout(fragment_expire, &ipv6_fragments, IPV6_FRAGMENT_TIMEOUT);
        ipv6_cur_fragment_id = IPV6_FRAG_ID(h);

        KDEBUG("Started new reassembly, ID: %u\n", ipv6_cur_fragment_id);

        fragment_enqueue(&ipv6_fragments, tmp);
    }
    
    fragments_check_complete(ipv6_fragments, proto, AF_INET6);
    kernel_mutex_unlock(&ipv6_fragments_lock);
}

