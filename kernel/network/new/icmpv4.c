/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: icmpv4.c
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
 *  \file icmpv4.c
 *
 *  Internet Control Message Protocol (ICMP) version 4 implementation.
 */

#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <endian.h>
#include <sys/socket.h>
#include <kernel/laylaos.h>
#include <kernel/net.h>
#include <kernel/net/packet.h>
#include <kernel/net/netif.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/stats.h>
#include <kernel/net/icmpv4.h>
#include <kernel/net/route.h>
#include <kernel/net/checksum.h>


int icmpv4_input(struct packet_t *p)
{
    struct ipv4_hdr_t *iph;
    uint8_t type;
    uint32_t src_ip, dest_ip;
    int hlen;

    netstats.icmp.recv++;

    iph = IPv4_HDR(p);
    hlen = ETHER_HLEN + (iph->hlen * 4);

    if(packet_add_header(p, -hlen) || p->count < 4)
    {
        printk("icmp: discarding short packet\n");
        netstats.icmp.lenerr++;
        netstats.icmp.drop++;
        free_packet(p);
        return -EPROTO;
    }
    
    type = *((uint8_t *)p->data);
    dest_ip = iph->dest;
    src_ip = iph->src;

    switch(type)
    {
        case ICMP_MSG_ECHO:
            if(!route_for_ipv4(dest_ip))
            {
                netstats.icmp.err++;
                netstats.udp.drop++;
                free_packet(p);
                return -EPROTO;
            }

            if(p->count < sizeof(struct icmp_echo_header_t))
            {
                printk("icmp: dropping short package\n");
                netstats.icmp.lenerr++;
                netstats.udp.drop++;
                free_packet(p);
                return -EPROTO;
            }
            
            struct icmp_echo_header_t *eh = 
                                (struct icmp_echo_header_t *)p->data;

            iph->src = dest_ip;
            iph->dest = src_ip;
            eh->type = ICMP_MSG_ECHOREPLY;

            // adjust checksum
            if(eh->checksum >= htons(0xFFFF - (ICMP_MSG_ECHO << 8)))
            {
                eh->checksum += htons(ICMP_MSG_ECHO << 8) + 1;
            }
            else
            {
                eh->checksum += htons(ICMP_MSG_ECHO << 8);
            }

            netstats.icmp.xmit++;
            packet_add_header(p, hlen - ETHER_HLEN);
            p->flags |= PACKET_FLAG_HDRINCLUDED;

            return ipv4_send(p, dest_ip, src_ip, IPPROTO_ICMP, iph->ttl);

        default:
            printk("icmp: dropping package with unsupported protocol\n");
            netstats.icmp.proterr++;
            netstats.udp.drop++;
            free_packet(p);
            return -EPROTO;
    }
}


void icmpv4_send(struct packet_t *p, uint8_t type, uint8_t code)
{
    struct ipv4_hdr_t *iph;
    struct icmp_te_header_t *icmph;
    struct packet_t *p2;
    uint32_t src, dest;

    if(!(p2 = alloc_packet(ETHER_HLEN + IPv4_HLEN + ICMP_HLEN + IPv4_HLEN + 8)))
    {
        free_packet(p);
        return;
    }

    packet_add_header(p2, -(ETHER_HLEN + IPv4_HLEN));
    iph = IPv4_HDR(p);
    icmph = (struct icmp_te_header_t *)p2->data;

    // get the src IP address in host format before we restore it to
    // network format
    src = iph->src;
    dest = iph->dest;

    // return multibyte fields to network representation for
    // checksum calculation
    A_memset(icmph, 0, ICMP_HLEN);
    iph->tlen = htons(iph->tlen);
    iph->id = htons(iph->id);
    iph->offset = htons(iph->offset);
    iph->src = htonl(iph->src);
    iph->dest = htonl(iph->dest);

    icmph->type = type;
    icmph->code = code;
    A_memcpy((char *)p2->data + 8, (char *)iph, IPv4_HLEN + 8);
    free_packet(p);
    
    // calculate checksum
    icmph->checksum = 0;
    icmph->checksum = htons(inet_chksum((uint16_t *)p2->data, p2->count, 0));

    netstats.icmp.xmit++;

    // this call will free the packet buffer for us
    ipv4_send(p2, dest, src, IPPROTO_ICMP, 255 /* ttl */);
}

