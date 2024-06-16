/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: icmp6.c
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
 *  \file icmp6.c
 *
 *  Internet Control Message Protocol (ICMP) v6 implementation.
 */

#define __DEBUG
#include <errno.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/net.h>
#include <kernel/net/ipv6.h>
#include <kernel/net/icmp6.h>
#include <kernel/net/ether.h>
#include <kernel/net/packet.h>
#include <kernel/net/checksum.h>
#include <kernel/net/raw.h>


struct netif_queue_t icmp6_inq = { 0, };
//struct netif_queue_t icmp6_outq = { 0, };


int icmp6_receive(struct packet_t *p)
{
    struct icmp6_hdr_t *icmph = (struct icmp6_hdr_t *)p->transport_hdr;
    struct icmp6_hdr_t *icmph2;
    struct ipv6_hdr_t *iph = (struct ipv6_hdr_t *)p->data;
    struct in6_addr src = { 0, };
    struct in6_addr dest = { 0, };
    struct packet_t *p2;
    uint32_t proto_len = p->count - 
                            ((uintptr_t)icmph - (uintptr_t)p->data);

    // try raw sockets first
    if(raw_receive(p) == 0)
    {
        // a raw socket consumed the packet
        return 0;
    }
    
    switch(icmph->type)
    {
        case ICMP6_MSG_DEST_UNREACH:
            return socket_error(p, iph->proto);

        case ICMP6_MSG_ECHO_REQUEST:
            if(!(p2 = packet_alloc(proto_len, PACKET_IP)))
            {
                packet_free(p);
                return -ENOMEM;
            }
            
            p2->ifp = p->ifp;
            p2->transport_hdr = p2->data;
            icmph2 = (struct icmp6_hdr_t *)p2->data;
            icmph2->type = ICMP6_MSG_ECHO_REPLY;
            icmph2->code = 0;
            icmph2->msg.info.echo_reply.id = icmph->msg.info.echo_request.id;
            icmph2->msg.info.echo_reply.seq = icmph->msg.info.echo_request.seq;
            
            A_memcpy((void *)((uintptr_t)icmph2 + 8),
                     (void *)((uintptr_t)icmph + 8), proto_len - 8);
            icmph2->checksum = 0;
            icmph2->checksum = htons(icmp6_checksum(p2));
            
            // swap src & dest
            IPV6_COPY(dest.s6_addr, iph->src.s6_addr);
            IPV6_COPY(src.s6_addr, iph->dest.s6_addr);
            
            return ipv6_push(p2, &dest, &src, IPPROTO_ICMPV6, 0);

        case ICMP6_MSG_ECHO_REPLY:
            packet_free(p);
            return 0;
        
        default:
            return ipv6_nd_recv(p);
    }
}


/*
 * Provide a Source Link-Layer Address Option (SLLAO) or a Destination 
 * Link-Layer Address Option (DLLAO).
 */
static int icmp6_llao(struct netif_t *ifp,
                      struct icmp6_opt_lladdr_t *llao, uint8_t type)
{
    llao->type = type;
    
    if(ifp->ethernet_addr.addr[0])
    {
        COPY_ETHER_ADDR(llao->addr.addr, ifp->ethernet_addr.addr);
        llao->len = 1;
        return 0;
    }
    
    return -EINVAL;
}


static struct packet_t *icmp6_neighbor_solicit_prep(struct netif_t *ifp, 
                                                    struct in6_addr *addr, 
                                                    uint16_t len)
{
    struct icmp6_hdr_t *icmph;
    struct packet_t *p;

    if(!(p = packet_alloc(len, PACKET_IP)))
    {
        return NULL;
    }

    p->ifp = ifp;
    p->transport_hdr = p->data;
    icmph = (struct icmp6_hdr_t *)p->data;
    icmph->type = ICMP6_MSG_NEIGHBOR_SOLICIT;
    icmph->code = 0;
    icmph->msg.info.neighbor_solicit.unused = 0;
    IPV6_COPY(icmph->msg.info.neighbor_solicit.target.s6_addr, addr->s6_addr);
    
    return p;
}


int icmp6_neighbor_solicit(struct netif_t *ifp, 
                           struct in6_addr *addr, int type)
{
    uint16_t len;
    struct packet_t *p;
    struct icmp6_hdr_t *icmph;
    struct icmp6_opt_lladdr_t *llao;
    struct in6_addr dest =
    {
        .s6_addr =
        {
            0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00,
        }
    };

    if(ipv6_is_multicast(addr->s6_addr))
    {
        return -EINVAL;
    }
    
    // calculate frame size
    len = 24;
    
    if(type != ICMP6_ND_DAD)
    {
        len = (uint16_t)(len + 8);
    }
    
    // prep neighbor solicit msg
    if((p = icmp6_neighbor_solicit_prep(ifp, addr, len)))
    {
        icmph = (struct icmp6_hdr_t *)p->data;
        
        // provide SLLAO if it's neighbor solicitation for DAD
        llao = (struct icmp6_opt_lladdr_t *)
                (((uint8_t *)&icmph->msg.info.neighbor_solicit) + 20);

        if(type != ICMP6_ND_DAD && icmp6_llao(ifp, llao, ND_OPT_LLADDR_SRC) != 0)
        {
            packet_free(p);
            return -EINVAL;
        }
        
        // get dest addr
        if(type == ICMP6_ND_SOLICITED || type == ICMP6_ND_DAD)
        {
            for(int i = 1; i <= 3; i++)
            {
                dest.s6_addr[16 - i] = addr->s6_addr[16 - i];
            }
        }
        else
        {
            IPV6_COPY(dest.s6_addr, addr->s6_addr);
        }
        
        ipv6_push(p, &dest, NULL, IPPROTO_ICMPV6, (type == ICMP6_ND_DAD));
        return 0;
    }
    
    return -EINVAL;
}


int icmp6_neighbor_advertise(struct packet_t *p, struct in6_addr *addr)
{
    struct icmp6_opt_lladdr_t *opt;
    struct packet_t *p2;
    struct ipv6_hdr_t *iph = (struct ipv6_hdr_t *)p->data;
    struct icmp6_hdr_t *icmph;
    struct in6_addr src;
    struct in6_addr dest =
    {
        .s6_addr =
        {
            0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        }
    };

    if(!(p2 = packet_alloc(32, PACKET_IP)))
    {
        return -ENOMEM;
    }

    p2->ifp = p->ifp;
    p2->transport_hdr = p2->data;
    icmph = (struct icmp6_hdr_t *)p2->data;
    icmph->type = ICMP6_MSG_NEIGHBOR_ADV;
    icmph->code = 0;
    IPV6_COPY(icmph->msg.info.neighbor_adv.target.s6_addr, addr->s6_addr);
    // -> !router && solicited && override
    icmph->msg.info.neighbor_adv.rsor = htonl(0x60000000);

    IPV6_COPY(src.s6_addr, iph->src.s6_addr);
    
    if(ipv6_is_unspecified(src.s6_addr))
    {
        // solicited = clear && dst = all-nodes address (scope link-local)
        icmph->msg.info.neighbor_adv.rsor ^= htonl(0x40000000);
    }
    else
    {
        // solicited = set && dst = source of solicitation
        IPV6_COPY(dest.s6_addr, iph->src.s6_addr);
    }
    
    opt = (struct icmp6_opt_lladdr_t *)(((uint8_t *)&icmph->msg) + 20);
    opt->type = ND_OPT_LLADDR_TGT;
    opt->len = 1;
    COPY_ETHER_ADDR(opt->addr.addr, p->ifp->ethernet_addr.addr);
    
    // packet src is set in frame_push, checksum calculated there
    ipv6_push(p2, &dest, NULL, IPPROTO_ICMPV6, 0);
    
    return 0;
}


int icmp6_router_solicit(struct netif_t *ifp, 
                         struct in6_addr *src, struct in6_addr *addr)
{
    UNUSED(addr);
    
    uint16_t len;
    struct packet_t *p;
    struct icmp6_hdr_t *icmph;
    struct icmp6_opt_lladdr_t *llao;
    struct in6_addr dest =
    {
        .s6_addr =
        {
            0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
        }
    };
    
    len = 8;
    
    if(!ipv6_is_unspecified(src->s6_addr))
    {
        len += 8;
    }

    if(!(p = packet_alloc(len, PACKET_IP)))
    {
        return -ENOMEM;
    }

    p->ifp = ifp;
    p->transport_hdr = p->data;
    icmph = (struct icmp6_hdr_t *)p->data;
    icmph->type = ICMP6_MSG_ROUTER_SOLICIT;
    icmph->code = 0;

    if(!ipv6_is_unspecified(src->s6_addr))
    {
        llao = (struct icmp6_opt_lladdr_t *)
                (((uint8_t *)&icmph->msg.info.router_solicit) + 4);

        if(icmp6_llao(ifp, llao, ND_OPT_LLADDR_SRC) != 0)
        {
            packet_free(p);
            return -EINVAL;
        }
    }

    ipv6_push(p, &dest, NULL, IPPROTO_ICMPV6, 0);
    return 0;
}


static int icmp6_notify(struct packet_t *p, uint8_t type, uint8_t code, uint32_t ptr)
{
    struct packet_t *p2;
    struct ipv6_hdr_t *h;
    struct icmp6_hdr_t *icmph;
    struct in6_addr src = { 0, };
    uint16_t len;

    if(!p)
    {
        return -EINVAL;
    }
    
    h = (struct ipv6_hdr_t *)p->data;
    len = ntohs(h->len) + IPv6_HLEN;

    if(type != ICMP6_MSG_DEST_UNREACH && type != ICMP6_MSG_PACKET_TOO_BIG &&
       type != ICMP6_MSG_TIME_EXCEEDED && type != ICMP6_MSG_PARAM_PROBLEM)
    {
        return -EINVAL;
    }

    // do not exceed min IPv6 MTU
    if((IPv6_HLEN + 8 + len) > IPv6_MIN_MTU)
    {
        len = IPv6_MIN_MTU - (IPv6_HLEN + 8);
    }

    if(!(p2 = packet_alloc(len + 8, PACKET_IP)))
    {
        return -ENOMEM;
    }

    p2->transport_hdr = p2->data;
    icmph = (struct icmp6_hdr_t *)p2->data;
    
    switch(type)
    {
        case ICMP6_MSG_DEST_UNREACH:
            icmph->msg.err.dest_unreach.unused = 0;
            break;

        case ICMP6_MSG_PACKET_TOO_BIG:
            icmph->msg.err.too_big.mtu = htonl(p->ifp->mtu);
            break;

        case ICMP6_MSG_TIME_EXCEEDED:
            icmph->msg.err.time_exceeded.unused = 0;
            break;

        case ICMP6_MSG_PARAM_PROBLEM:
            icmph->msg.err.param.p = htonl(ptr);
            break;

        default:
            return -EINVAL;
    }
    
    icmph->type = type;
    icmph->code = code;
    A_memcpy((char *)p2->data + 8, p->data, len);

    netstats.icmp.xmit++;

    IPV6_COPY(src.s6_addr, h->src.s6_addr);
    ipv6_push(p2, &src, NULL, IPPROTO_ICMPV6, 0);
    return 0;
}


int icmp6_port_unreachable(struct packet_t *p)
{
    struct ipv6_hdr_t *h = (struct ipv6_hdr_t *)p->data;
    
    if(ipv6_is_multicast(h->dest.s6_addr))
    {
        return 0;
    }

    return icmp6_notify(p, ICMP6_MSG_DEST_UNREACH, ICMP6_DESTUNREACH_PORT, 0);
}


int icmp6_proto_unreachable(struct packet_t *p)
{
    struct ipv6_hdr_t *h = (struct ipv6_hdr_t *)p->data;
    
    if(ipv6_is_multicast(h->dest.s6_addr))
    {
        return 0;
    }

    return icmp6_notify(p, ICMP6_MSG_DEST_UNREACH, ICMP6_DESTUNREACH_ADDR, 0);
}


int icmp6_dest_unreachable(struct packet_t *p)
{
    struct ipv6_hdr_t *h = (struct ipv6_hdr_t *)p->data;
    
    if(ipv6_is_multicast(h->dest.s6_addr))
    {
        return 0;
    }

    return icmp6_notify(p, ICMP6_MSG_DEST_UNREACH, ICMP6_DESTUNREACH_ADDR, 0);
}


int icmp6_ttl_expired(struct packet_t *p)
{
    struct ipv6_hdr_t *h = (struct ipv6_hdr_t *)p->data;
    
    if(ipv6_is_multicast(h->dest.s6_addr))
    {
        return 0;
    }

    return icmp6_notify(p, ICMP6_MSG_TIME_EXCEEDED, ICMP6_TIMEEXCEEDED_INTRANS, 0);
}


int icmp6_frag_expired(struct packet_t *p)
{
    struct ipv6_hdr_t *h = (struct ipv6_hdr_t *)p->data;
    
    if(ipv6_is_multicast(h->dest.s6_addr))
    {
        return 0;
    }

    return icmp6_notify(p, ICMP6_MSG_TIME_EXCEEDED, ICMP6_TIMEEXCEEDED_REASSEMBLY, 0);
}


int icmp6_packet_too_big(struct packet_t *p)
{
    struct ipv6_hdr_t *h = (struct ipv6_hdr_t *)p->data;
    
    if(ipv6_is_multicast(h->dest.s6_addr))
    {
        return 0;
    }

    return icmp6_notify(p, ICMP6_MSG_PACKET_TOO_BIG, 0, 0);
}


int icmp6_param_problem(struct packet_t *p, uint8_t problem, uint32_t ptr)
{
    return icmp6_notify(p, ICMP6_MSG_PARAM_PROBLEM, problem, ptr);
}

