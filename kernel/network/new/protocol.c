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

#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/timer.h>
#include <kernel/net/socket.h>
#include <kernel/net/protocol.h>
#include <kernel/net/domain.h>
#include <kernel/net/udp.h>
#include <kernel/net/tcp.h>
#include <kernel/net/route.h>
#include <kernel/net/arp.h>
#include <kernel/net/dhcp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/raw.h>
#include <kernel/net/unix.h>
#include <kernel/net/nettimer.h>

extern void loop_attach(void);


struct proto_t unix_protocols[2] =
{
    { SOCK_STREAM, 0, &unix_domain, &unix_sockops, },
    { SOCK_DGRAM, 0, &unix_domain, &unix_sockops, },
};

struct proto_t internet_protocols[5] =
{
    { 0, 0, &internet_domain, 0, },
    { SOCK_DGRAM, IPPROTO_UDP, &internet_domain, &udp_sockops, },
    { SOCK_STREAM, IPPROTO_TCP, &internet_domain, &tcp_sockops, },
    { SOCK_RAW, IPPROTO_RAW, &internet_domain, &raw_sockops, },
    { SOCK_RAW, IPPROTO_ICMP, &internet_domain, &raw_sockops, },
};


/*
 * Initialize network protocols.
 */
void network_init(void)
{
    printk("Initializing network protocols..\n");
    netif_init();
    route_init();
    nettimer_init();

    ip_init();
    arp_init();
    dhcp_init();

    printk("Attaching pseudo-devices..\n");
    loop_attach();

    printk("Initializing network statistics..\n");
    stats_init();
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

