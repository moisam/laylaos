/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: checksum.c
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
 *  \file checksum.c
 *
 *  Functions to calculate checksums for the network layer.
 */

//#define __DEBUG
#include <endian.h>
#include <kernel/laylaos.h>
#include <kernel/net.h>
#include <kernel/net/packet.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>
#include <kernel/net/icmp4.h>
#include <kernel/net/icmp6.h>

static inline uint32_t checksum_adder(uint32_t checksum, void *data, uint32_t len)
{
    uint16_t *buf = (uint16_t *)data;
    uint16_t *stop;
    
    if(len & 1)
    {
        --len;

#if BYTE_ORDER == LITTLE_ENDIAN
        checksum += ((uint8_t *)data)[len];
#else
        checksum += (((uint8_t *)data)[len]) << 8;
#endif
    }
    
    stop = (uint16_t *)(((uint8_t *)data) + len);
    
    while(buf < stop)
    {
        checksum += *buf++;
    }
    
    return checksum;
}


static inline uint16_t checksum_finalize(uint32_t checksum)
{
    while(checksum >> 16)
    {
        checksum = (checksum & 0x0000FFFF) + (checksum >> 16);
    }

    KDEBUG("checksum_finalize: 0x%x (0x%x)\n", htons((uint16_t)~checksum), ((uint16_t)~checksum));
    
    return htons((uint16_t)~checksum);
}


uint16_t buf_checksum(void *buf1, uint32_t len1, void *buf2, uint32_t len2)
{
    uint32_t checksum;
    
    checksum = checksum_adder(0, buf1, len1);
    checksum = checksum_adder(checksum, buf2, len2);
    
    return checksum_finalize(checksum);
}


uint16_t checksum(void *buf, uint32_t len)
{
    uint32_t checksum;
    
    checksum = checksum_adder(0, buf, len);
    
    return checksum_finalize(checksum);
}


uint16_t icmp6_checksum(struct packet_t *p)
{
    struct ipv6_hdr_t *iph = (struct ipv6_hdr_t *)p->data;
    struct icmp6_hdr_t *icmph = (struct icmp6_hdr_t *)p->transport_hdr;
    uint32_t proto_len = p->count - 
                            ((uintptr_t)icmph - (uintptr_t)p->data);
    struct ipv6_pseudo_hdr_t pseudo;

    IPV6_COPY(pseudo.src.s6_addr, iph->src.s6_addr);
    IPV6_COPY(pseudo.dest.s6_addr, iph->dest.s6_addr);
    pseudo.len = htonl(proto_len);
    pseudo.proto = IPPROTO_ICMPV6;
    pseudo.zero[0] = 0;
    pseudo.zero[1] = 0;
    pseudo.zero[2] = 0;
    
    return buf_checksum(&pseudo, sizeof(struct ipv6_pseudo_hdr_t), 
                        icmph, proto_len);
}


uint16_t icmp4_checksum(struct packet_t *p)
{
    struct icmp4_hdr_t *icmph = (struct icmp4_hdr_t *)p->transport_hdr;
    
    icmph->checksum = 0;
    icmph->checksum = htons(checksum(icmph, p->count - 
                                     ((uintptr_t)icmph - (uintptr_t)p->data)));
    return 0;
}


uint16_t tcp_checksum_ipv4(struct packet_t *p)
{
    struct ipv4_hdr_t *iph = (struct ipv4_hdr_t *)p->data;
    struct tcp_hdr_t *tcph = (struct tcp_hdr_t *)p->transport_hdr;
    struct ipv4_pseudo_hdr_t pseudo;
    uint32_t proto_len;
    
    if(p->sock)
    {
        // outgoing packet
        KDEBUG("tcp_checksum_ipv4: outgoing src ");
        KDEBUG_IPV4_ADDR(htonl(p->sock->local_addr.ipv4.s_addr));
        KDEBUG(", dest ");
        KDEBUG_IPV4_ADDR(htonl(p->sock->remote_addr.ipv4.s_addr));
        KDEBUG("\n");

        pseudo.src.s_addr = p->sock->local_addr.ipv4.s_addr;
        pseudo.dest.s_addr = p->sock->remote_addr.ipv4.s_addr;
    }
    else
    {
        // incoming packet
        pseudo.src.s_addr = iph->src.s_addr;
        pseudo.dest.s_addr = iph->dest.s_addr;
    }
    
    proto_len = p->count - ((uintptr_t)tcph - (uintptr_t)iph);
    pseudo.zero = 0;
    pseudo.proto = IPPROTO_TCP;
    pseudo.len = htons(proto_len);

    return buf_checksum(&pseudo, sizeof(struct ipv4_pseudo_hdr_t), 
                        tcph, proto_len);
}


uint16_t tcp_checksum_ipv6(struct packet_t *p)
{
    struct ipv6_hdr_t *iph = (struct ipv6_hdr_t *)p->data;
    struct tcp_hdr_t *tcph = (struct tcp_hdr_t *)p->transport_hdr;
    struct ipv6_pseudo_hdr_t pseudo;
    uint32_t proto_len;
    
    if(p->sock)
    {
        // outgoing packet
        IPV6_COPY(pseudo.src.s6_addr, p->sock->local_addr.ipv6.s6_addr);
        IPV6_COPY(pseudo.dest.s6_addr, p->sock->remote_addr.ipv6.s6_addr);
    }
    else
    {
        // incoming packet
        IPV6_COPY(pseudo.src.s6_addr, iph->src.s6_addr);
        IPV6_COPY(pseudo.dest.s6_addr, iph->dest.s6_addr);
    }
    
    proto_len = p->count - ((uintptr_t)tcph - (uintptr_t)iph);
    pseudo.zero[0] = 0;
    pseudo.zero[1] = 0;
    pseudo.zero[2] = 0;
    pseudo.proto = IPPROTO_TCP;
    pseudo.len = htonl(proto_len);

    return buf_checksum(&pseudo, sizeof(struct ipv6_pseudo_hdr_t), 
                        tcph, proto_len);
}


uint16_t udp_checksum_ipv4(struct packet_t *p)
{
    struct ipv4_hdr_t *iph = (struct ipv4_hdr_t *)p->data;
    struct udp_hdr_t *udph = (struct udp_hdr_t *)p->transport_hdr;
    struct ipv4_pseudo_hdr_t pseudo;
    struct socket_t *s = p->sock;
    uint32_t proto_len;
    
    /* If the IPv4 packet contains a Routing header, the Destination
     * Address used in the pseudo-header is that of the final destination.
     */
    if(s)
    {
        // outgoing packet
        pseudo.src.s_addr = s->local_addr.ipv4.s_addr;
        pseudo.dest.s_addr = s->remote_addr.ipv4.s_addr;
    }
    else
    {
        // incoming packet
        pseudo.src.s_addr = iph->src.s_addr;
        pseudo.dest.s_addr = iph->dest.s_addr;
    }

    proto_len = p->count - ((uintptr_t)udph - (uintptr_t)iph);
    pseudo.len = htons(proto_len);
    pseudo.proto = IPPROTO_UDP;
    pseudo.zero = 0;
    
    return buf_checksum(&pseudo, sizeof(struct ipv4_pseudo_hdr_t), 
                        udph, proto_len);
}


uint16_t udp_checksum_ipv6(struct packet_t *p)
{
    struct ipv6_hdr_t *iph = (struct ipv6_hdr_t *)p->data;
    struct udp_hdr_t *udph = (struct udp_hdr_t *)p->transport_hdr;
    struct ipv6_pseudo_hdr_t pseudo;
    struct socket_t *s = p->sock;
    uint32_t proto_len;
    
    /* If the IPv6 packet contains a Routing header, the Destination
     * Address used in the pseudo-header is that of the final destination.
     */
    if(s)
    {
        // outgoing packet
        IPV6_COPY(pseudo.src.s6_addr, s->local_addr.ipv6.s6_addr);
        
        if(ipv6_is_unspecified(p->remote_addr.ipv6.s6_addr))
        {
            IPV6_COPY(pseudo.dest.s6_addr, s->remote_addr.ipv6.s6_addr);
        }
        else
        {
            IPV6_COPY(pseudo.dest.s6_addr, p->remote_addr.ipv6.s6_addr);
        }
    }
    else
    {
        // incoming packet
        IPV6_COPY(pseudo.src.s6_addr, iph->src.s6_addr);
        IPV6_COPY(pseudo.dest.s6_addr, iph->dest.s6_addr);
    }

    proto_len = p->count - ((uintptr_t)udph - (uintptr_t)iph);
    pseudo.len = htonl(proto_len);
    pseudo.proto = IPPROTO_UDP;
    pseudo.zero[0] = 0;
    pseudo.zero[1] = 0;
    pseudo.zero[2] = 0;
    
    return buf_checksum(&pseudo, sizeof(struct ipv6_pseudo_hdr_t), 
                        udph, proto_len);
}

