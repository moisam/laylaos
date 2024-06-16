/* 
 *    Copyright 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: notify.c
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
 *  \file notify.c
 *
 *  Helper functions used by the Internet Control Message Protocol (ICMP) 
 *  layer.
 */

#include <kernel/net/packet.h>
#include <kernel/net/icmp4.h>
#include <kernel/net/icmp6.h>

void notify_socket_unreachable(struct packet_t *p, int is_ipv6)
{
    if(is_ipv6)
    {
        icmp6_port_unreachable(p);
    }
    else
    {
        icmp4_port_unreachable(p);
    }
}


void notify_proto_unreachable(struct packet_t *p, int is_ipv6)
{
    if(is_ipv6)
    {
        icmp6_proto_unreachable(p);
    }
    else
    {
        icmp4_proto_unreachable(p);
    }
}


void notify_dest_unreachable(struct packet_t *p, int is_ipv6)
{
    if(is_ipv6)
    {
        icmp6_dest_unreachable(p);
    }
    else
    {
        icmp4_dest_unreachable(p);
    }
}


void notify_ttl_expired(struct packet_t *p, int is_ipv6)
{
    if(is_ipv6)
    {
        icmp6_ttl_expired(p);
    }
    else
    {
        icmp4_ttl_expired(p);
    }
}


void notify_packet_too_big(struct packet_t *p, int is_ipv6)
{
    if(is_ipv6)
    {
        icmp6_packet_too_big(p);
    }
    else
    {
        icmp4_packet_too_big(p);
    }
}

