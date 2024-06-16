/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: icmp4.c
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
 *  \file icmp4.c
 *
 *  Internet Control Message Protocol (ICMP) v4 implementation.
 */

#define __DEBUG
#include <errno.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/net.h>
#include <kernel/net/socket.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/icmp4.h>
#include <kernel/net/raw.h>
#include <kernel/net/ether.h>
#include <kernel/net/packet.h>
#include <kernel/net/checksum.h>


struct netif_queue_t icmp4_inq = { 0, };
//struct netif_queue_t icmp4_outq = { 0, };


int icmp4_receive(struct packet_t *p)
{
    static int first_packet = 1;
    static uint16_t lastid = 0, lastseq = 0;
    struct icmp4_hdr_t *icmph = (struct icmp4_hdr_t *)p->transport_hdr;
    struct ipv4_hdr_t *iph = (struct ipv4_hdr_t *)p->data;

    KDEBUG("icmp4_receive: type %u\n", icmph->type);

    // try raw sockets first
    if(raw_receive(p) == 0)
    {
        // a raw socket consumed the packet
        return 0;
    }
    
    if(icmph->type == ICMP_MSG_ECHO)
    {
        struct in_addr dest = { 0 };

        icmph->type = ICMP_MSG_ECHOREPLY;
        
        if(!first_packet && 
           icmph->hun.idseq.id == lastid && 
           icmph->hun.idseq.seq == lastseq)
        {
            // duplicate echo, discard
            packet_free(p);
            return 0;
        }
        
        first_packet = 0;
        lastid = icmph->hun.idseq.id;
        lastseq = icmph->hun.idseq.seq;

        netstats.icmp.xmit++;

        icmp4_checksum(p);
        
        /*
         * TODO: fragment large packets
         */

        dest.s_addr = iph->src.s_addr;
        packet_add_header(p, -((uintptr_t)icmph - (uintptr_t)iph));

        return ipv4_push(p, &dest, iph->proto);
    }
    else if(icmph->type == ICMP_MSG_DESTUNREACH)
    {
        return socket_error(p, iph->proto);
    }
    else if(icmph->type == ICMP_MSG_ECHOREPLY)
    {
        // update our ARP table with the sender's address
        arp_update_entry(p->ifp, iph->src.s_addr,
                         &(((struct ether_header_t *)
                            ((uintptr_t)p->data - ETHER_HLEN))->src));
        packet_free(p);
        return 0;
    }
    else
    {
        packet_free(p);
    }
    
    return 0;
}


static int icmp4_notify(struct packet_t *p, uint8_t type, uint8_t code)
{
    struct packet_t *p2;
    struct ipv4_hdr_t *h;
    struct icmp4_hdr_t *icmph;
    struct in_addr src = { 0 };
    uint16_t len;

    if(!p)
    {
        return -EINVAL;
    }
    
    h = (struct ipv4_hdr_t *)p->data;
    len = ntohs(h->len);

    // do not exceed min IPv4 MTU
    if(len > (IPv4_HLEN + 8))
    {
        len = IPv4_HLEN + 8;
    }

    if(!(p2 = packet_alloc(len + 8, PACKET_IP)))
    {
        return -ENOMEM;
    }

    p2->transport_hdr = p2->data;
    icmph = (struct icmp4_hdr_t *)p2->data;
    icmph->type = type;
    icmph->code = code;
    icmph->hun.pmtu.nmtu = htons(1500);
    icmph->hun.pmtu.null = 0;

    A_memcpy((char *)p2->data + sizeof(struct icmp4_hdr_t), p->data, len);

    netstats.icmp.xmit++;

    icmp4_checksum(p2);
    src.s_addr = h->src.s_addr;
    ipv4_push(p2, &src, IPPROTO_ICMP);
    return 0;
}


int icmp4_port_unreachable(struct packet_t *p)
{
    return icmp4_notify(p, ICMP_MSG_DESTUNREACH, ICMP_DESTUNREACH_PORT);
}


int icmp4_proto_unreachable(struct packet_t *p)
{
    return icmp4_notify(p, ICMP_MSG_DESTUNREACH, ICMP_DESTUNREACH_PROTO);
}


int icmp4_dest_unreachable(struct packet_t *p)
{
    return icmp4_notify(p, ICMP_MSG_DESTUNREACH, ICMP_DESTUNREACH_HOST);
}


int icmp4_packet_too_big(struct packet_t *p)
{
    return icmp4_notify(p, ICMP_MSG_DESTUNREACH, ICMP_DESTUNREACH_FRAG);
}


int icmp4_ttl_expired(struct packet_t *p)
{
    return icmp4_notify(p, ICMP_MSG_TIMEEXCEEDED, ICMP_TIMEEXCEEDED_INTRANS);
}


int icmp4_frag_expired(struct packet_t *p)
{
    return icmp4_notify(p, ICMP_MSG_TIMEEXCEEDED, ICMP_TIMEEXCEEDED_REASSEMBLY);
}


int icmp4_param_problem(struct packet_t *p, uint8_t problem)
{
    return icmp4_notify(p, ICMP_MSG_PARAMPROBLEM, problem);
}


/*
int icmp4_send_echo(struct in_addr *dest)
{
    struct packet_t *p;
    struct icmp4_hdr_t *icmph;
    static uint16_t id = 0x1000;

    if(!dest)
    {
        return -EINVAL;
    }
    
    if(!(p = packet_alloc(IPv4_HLEN + 8, PACKET_IP)))
    {
        return -ENOMEM;
    }

    p->transport_hdr = p->data;
    icmph = (struct icmp4_hdr_t *)p->data;
    icmph->type = ICMP_MSG_ECHO;
    icmph->code = 0;
    icmph->hun.idseq.id = htons(id);
    icmph->hun.idseq.seq = 1;
    id++;

    netstats.icmp.xmit++;

    icmp4_checksum(p);
    ipv4_push(p, dest, IPPROTO_ICMP);
    return 0;
}
*/

