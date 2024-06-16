/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: udp.c
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
 *  \file udp.c
 *
 *  User Datagram Protocol (UDP) implementation.
 */

//#define __DEBUG
#include <errno.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/net.h>
#include <kernel/net/protocol.h>
#include <kernel/net/packet.h>
#include <kernel/net/socket.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>
#include <kernel/net/udp.h>
#include <kernel/net/raw.h>
#include <kernel/net/checksum.h>
#include <kernel/net/notify.h>


struct netif_queue_t udp_inq = { 0, };
//struct netif_queue_t udp_outq = { 0, };


int udp_receive(struct packet_t *p)
{
    struct udp_hdr_t *udph = (struct udp_hdr_t *)p->transport_hdr;
    struct sockport_t *sp;
    int ipver = GET_IP_VER((struct ipv4_hdr_t *)p->data);
    
    KDEBUG("udp: received packet with ipv%d\n", ipver);

    if(!udph)
    {
        KDEBUG("udp: discarding packet with invalid UDP header\n");
        goto err;
    }
    
    if(ipver == 4)
    {
        if(udp_checksum_ipv4(p) != 0)
        {
            KDEBUG("udp: discarding IPv4 packet with invalid checksum\n");
            goto err;
        }
    }
    else if(ipver == 6)
    {
        if(udp_checksum_ipv6(p) != 0)
        {
            KDEBUG("udp: discarding IPv6 packet with invalid checksum\n");
            goto err;
        }
    }
    else
    {
        KDEBUG("udp: discarding packet with invalid IP version\n");
        goto err;
    }

    // try raw sockets first
    if(raw_receive(p) == 0)
    {
        // a raw socket consumed the packet
        return 0;
    }

    KDEBUG("udp: looking for port %d\n", ntohs(udph->destp));

    if(!(sp = get_sockport(IPPROTO_UDP, udph->destp)))
    {
        KDEBUG("udp: cannot find port %d\n", ntohs(udph->destp));
        
        if(p->flags & PACKET_FLAG_BROADCAST)
        {
            KDEBUG("udp: sending sock unreachable\n");
            notify_socket_unreachable(p, (ipver == 6));
        }
        
        goto err;
    }

    KDEBUG("udp: passing received packet to socket layer\n");
    
    return socket_udp_receive(sp, p);

err:

    KDEBUG("udp: dropping packet\n");
    packet_free(p);
    netstats.udp.proterr++;
    return -EINVAL;
}


/*
 * Push a packet on the outgoing queue.
 * Called from the socket layer.
 */
int udp_push(struct packet_t *p)
{
    struct udp_hdr_t *h = (struct udp_hdr_t *)p->transport_hdr;
    int res, count;
    
    if(p->transport_hdr != p->data)
    {
        h->srcp = p->sock->local_port;
        
        if(/* p->remote_addr && */ p->remote_port)
        {
            h->destp = p->remote_port;
        }
        else
        {
            h->destp = p->sock->remote_port;
        }
        
        if(packet_add_header(p, ((uintptr_t)p->data - 
                                    (uintptr_t)p->transport_hdr)) < 0)
        {
            KDEBUG("udp_push: insufficient space for udp header\n");
            packet_free(p);
            netstats.udp.err++;
            return -ENOBUFS;
        }
        
        h->len = htons(p->count);
        h->checksum = 0;
    }
    
    
    
    count = p->count;
    
    if((res = ip_push(p)) == 0)
    {
        return count;
    }
    else
    {
        return res;
    }
}

