/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: protocol.c
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
 *  \file protocol.c
 *
 *  Network protocol tables and different helper functions.
 */

//#define __DEBUG

#include <errno.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/net/protocol.h>
#include <kernel/net/domain.h>
#include <kernel/net/udp.h>
#include <kernel/net/tcp.h>
#include <kernel/net/ether.h>
#include <kernel/net/dhcp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>
#include <kernel/net/raw.h>
#include <kernel/net/unix.h>
#include <kernel/net/socket.h>
#include <kernel/net/icmp4.h>
#include <kernel/net/icmp6.h>
#include <kernel/timer.h>

int dummy_push(struct packet_t *p);


struct proto_t unix_proto[3] =
{
    { SOCK_STREAM, 0, &unix_domain, &unix_sockops, unix_push, },
    { SOCK_DGRAM, 0, &unix_domain, &unix_sockops, unix_push, },
    { SOCK_SEQPACKET, 0, &unix_domain, &unix_sockops, unix_push, },
};

struct proto_t inet_proto[5] =
{
    { 0, 0, &inet_domain, 0, dummy_push, },
    { SOCK_DGRAM, IPPROTO_UDP, &inet_domain, &udp_sockops, udp_push, },
    { SOCK_STREAM, IPPROTO_TCP, &inet_domain, &tcp_sockops, tcp_push, },
    { SOCK_RAW, IPPROTO_RAW, &inet_domain, &raw_sockops, raw_push, },
    { SOCK_RAW, IPPROTO_ICMP, &inet_domain, &raw_sockops, raw_push, },
};

struct proto_t inet6_proto[5] =
{
    { 0, 0, &inet6_domain, 0, 0, },
    { SOCK_DGRAM, IPPROTO_UDP, &inet6_domain, &udp_sockops, udp_push, },
    { SOCK_STREAM, IPPROTO_TCP, &inet6_domain, &tcp_sockops, tcp_push, },
    { SOCK_RAW, IPPROTO_RAW, &inet6_domain, &raw_sockops, raw_push, },
    { SOCK_RAW, IPPROTO_ICMPV6, &inet6_domain, &raw_sockops, raw_push, },
};


/*
 * Dummy push function.
 */
int dummy_push(struct packet_t *p)
{
    UNUSED(p);
    return -EPROTONOSUPPORT;
}


/*
 * Initialize network protocols.
 */
void proto_init(void)
{
    ethernet_inq.max = NETIF_DEFAULT_QUEUE_LEN;
    ipv4_inq.max = NETIF_DEFAULT_QUEUE_LEN;
    ipv6_inq.max = NETIF_DEFAULT_QUEUE_LEN;
    icmp4_inq.max = NETIF_DEFAULT_QUEUE_LEN;
    icmp6_inq.max = NETIF_DEFAULT_QUEUE_LEN;
    tcp_inq.max = NETIF_DEFAULT_QUEUE_LEN;
    udp_inq.max = NETIF_DEFAULT_QUEUE_LEN;
    raw_inq.max = NETIF_DEFAULT_QUEUE_LEN;

    ethernet_outq.max = NETIF_DEFAULT_QUEUE_LEN;
    ipv4_outq.max = NETIF_DEFAULT_QUEUE_LEN;
    ipv6_outq.max = NETIF_DEFAULT_QUEUE_LEN;
    //icmp4_outq.max = NETIF_DEFAULT_QUEUE_LEN;
    //icmp6_outq.max = NETIF_DEFAULT_QUEUE_LEN;
    //tcp_outq.max = NETIF_DEFAULT_QUEUE_LEN;
    //udp_outq.max = NETIF_DEFAULT_QUEUE_LEN;

    //ethernet_init();
    ipv4_init();
    ipv6_init();
    //ipv6_nd_init();
    //icmp4_init();
    //icmp6_init();
    arp_init();
    //udp_init();
    tcp_init();
    //unix_init();
    dhcp_init();
}


/*
 * Find a protocol given its family and protocol type.
 */
struct proto_t *find_proto_by_type(int family, int type)
{
    struct domain_t **dom;
    struct proto_t *proto;
    
    for(dom = domains; *dom != NULL; dom++)
    {
        if((*dom)->family == family)
        {
            break;
        }
    }
    
    if(!*dom)
    {
        return NULL;
    }
    
    for(proto = (*dom)->proto; proto < (*dom)->lproto; proto++)
    {
        if(proto->sock_type && proto->sock_type == type)
        {
            return proto;
        }
    }
    
    return NULL;
}


/*
 * Find a protocol given its family, protocol id and/or type.
 */
struct proto_t *find_proto(int family, int protocol, int type)
{
    struct domain_t **dom;
    struct proto_t *proto, *maybe = NULL;
    
    if(family == 0)
    {
        return NULL;
    }

    for(dom = domains; *dom != NULL; dom++)
    {
        if((*dom)->family == family)
        {
            break;
        }
    }
    
    if(!*dom)
    {
        return NULL;
    }
    
    for(proto = (*dom)->proto; proto < (*dom)->lproto; proto++)
    {
        if(proto->protocol == protocol && proto->sock_type == type)
        {
            return proto;
        }
        
        if(type == SOCK_RAW && proto->sock_type == SOCK_RAW &&
           proto->protocol == 0 && !maybe)
        {
            maybe = proto;
        }
    }
    
    return maybe;
}

