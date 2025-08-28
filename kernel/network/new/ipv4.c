/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: ipv4.c
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
 *  \file ipv4.c
 *
 *  Internet Protocol (IP) version 4 implementation.
 */

//#define __DEBUG

#include <kernel/net.h>
#include <kernel/net/packet.h>
#include <kernel/net/ether.h>
#include <kernel/net/route.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv4_addr.h>
#include <kernel/net/icmpv4.h>
#include <kernel/net/arp.h>
#include <kernel/net/tcp.h>
#include <kernel/net/udp.h>
#include <kernel/net/raw.h>
#include <kernel/net/dhcp.h>
#include <kernel/net/checksum.h>

// how many fragments we can keep in queue
#define MAX_FRAGS                   128

struct ip_frag_queue_t
{
    struct ipv4_fraglist_t *head, *tail;
    int count, max;
    volatile struct kernel_mutex_t lock;
};

struct ip_frag_queue_t fragq = { 0, };
volatile uint16_t ip_id = 0;
volatile struct task_t *ip_slowtimo_task = NULL;

static void ip_slowtimo(void *arg);
static void remove_from_fqueue(struct ipv4_fraglist_t *ifq);
static struct packet_t *ip_reassemble(struct packet_t *p, struct ipv4_fraglist_t *ifq);
static int ip_do_options(struct packet_t *p);


/*
 * Initialize IP.
 */
void ip_init(void)
{
    init_kernel_mutex(&fragq.lock);
    fragq.max = MAX_FRAGS;
    ip_id = now() & 0xFFFF;
    (void)start_kernel_task("ipslowto", ip_slowtimo, NULL, &ip_slowtimo_task, 0);
}


/*
 * IP slow timeout function.
 */
static void ip_slowtimo(void *arg)
{
    UNUSED(arg);

    while(1)
    {
        struct ipv4_fraglist_t *ifq, *prev = NULL, *next;
        struct packet_t *p, *p2;

        kernel_mutex_lock(&fragq.lock);

        for(ifq = fragq.head; ifq != NULL; )
        {
            next = ifq->next;
            ifq->ttl--;

            if(ifq->ttl == 0)
            {
                netstats.ip.err++;

                for(p = ifq->plist; p != NULL; )
                {
                    p2 = p->next;
                    free_packet(p);
                    p = p2;
                }

                if(prev)
                {
                    prev->next = next;
                }
                else
                {
                    fragq.head = next;
                }
            
                kfree(ifq);
                fragq.count--;
            }
            else
            {
                prev = ifq;
            }
        
            ifq = next;
        }

        kernel_mutex_unlock(&fragq.lock);

        // schedule every 500ms
        block_task2(&ip_slowtimo_task, PIT_FREQUENCY / 2);
    }
}


STATIC_INLINE uint32_t iptime(void)
{
    struct timeval atv;
    uint32_t t;

    microtime(&atv);
    t = (atv.tv_sec % (24 * 60 * 60)) * 1000 + atv.tv_usec / 1000;

    return (htonl(t));
}


STATIC_INLINE int do_send(struct rtentry_t *rt, struct packet_t *p, uint32_t dest)
{
    int i;
    uint8_t dest_ethernet[ETHER_ADDR_LEN];

    netstats.ip.xmit++;

    /*
     * NOTE: If the address is resolved, arp_resolve() returns 1 and we carry 
     *       on to send the packet. If it wasn't, 0 is returned and the packet
     *       is queued for delayed send (therefore we don't free the packet 
     *       here).
     */
    if(!arp_resolve(rt, dest, dest_ethernet))
    {
        if((i = packet_add_header(p, ETHER_HLEN)) < 0 ||
           (i = arp_queue(rt->ifp, p, dest)) < 0)
        {
            printk("ip: failed to queue ARP packet (err %d)\n", i);
            free_packet(p);
            netstats.link.drop++;
            netstats.link.memerr++;
            return i;
        }

        return 0;
    }

    /*
     * NOTE: it is the sending function's duty to free the packet!
     *       we leave this to the sending function, as it may queue the
     *       packet instead of sending it right away.
     */

    if((i = ethernet_send(rt->ifp, p, dest_ethernet)) < 0)
    {
        printk("ip: failed to send packet (err %d)\n", i);
        return i;
    }
    
    return 0;
}


static void ip_forward(struct packet_t *p)
{
    struct ipv4_hdr_t *h;
    struct rtentry_t *rt;

    h = IPv4_HDR(p);

    // find where to route the packet
    if(!(rt = route_for_ipv4(h->dest)))
    {
        printk("ip: cannot find forwarding route (0x%x)\n", h->dest);
        netstats.ip.rterr++;
        free_packet(p);
        return;
    }

    // don't route broadcasts
    if(ipv4_is_broadcast(h->dest, rt->netmask))
    {
        free_packet(p);
        return;
    }
    
    // don't forward packets to the same interface they arrived on
    if(rt->ifp == p->ifp)
    {
        printk("ip: not forwarding packet to same arrival interface\n");
        netstats.ip.rterr++;
        free_packet(p);
        return;
    }
    
    // decrement ttl and if it is 0, send an error ICMP, unless the packet
    // itself is an ICMP packet
    h->ttl = h->ttl - 1;

    //KDEBUG("ip_forward: ttl = %d\n", h->ttl);
    
    if(h->ttl == 0)
    {
        if(h->proto != IPPROTO_ICMP)
        {
            //KDEBUG("ip_forward: sending icmp\n");
            icmpv4_time_exceeded(p, 0);
        }
        
        free_packet(p);
        return;
    }
    
    // incrementally update the header checksum as we changed ttl
    if(h->checksum >= htons(0xFFFF - 0x100))
    {
        h->checksum = h->checksum + htons(0x100) + 1;
    }
    else
    {
        h->checksum = h->checksum + htons(0x100);
    }
    
    printk("ip: forwarding packet to 0x%x\n", h->dest);
    netstats.ip.fw++;
    netstats.ip.xmit++;

    do_send(rt, p, h->dest);
}


STATIC_INLINE void hdr_convert(struct ipv4_hdr_t *h)
{
    h->tlen = ntohs(h->tlen);
    h->id = ntohs(h->id);
    h->offset = ntohs(h->offset);
    //h->src = ntohl(h->src);
    //h->dest = ntohl(h->dest);
}


void ipv4_recv(struct packet_t *p)
{
    struct rtentry_t *rt;
    struct ipv4_hdr_t *h;

    netstats.ip.recv++;
    h = IPv4_HDR(p);

    // check IP version - we only accept IPv4 packets
    if(h->ver != 4)
    {
        printk("ip: dropped packet with invalid version number: %d\n", h->ver);
        netstats.ip.err++;
        netstats.ip.drop++;
        free_packet(p);
        return;
    }

    // check header length is valid, should be at least 5, which equates
    // to 20 bytes, and no more than 60 bytes (hlen is expressed in terms
    // of dwords, see RFC 791)
    if(h->hlen < 5 || h->hlen > 15 || (h->hlen * 4) > p->count)
    {
        printk("ip: dropped packet with invalid length: %d\n", h->hlen);
        netstats.ip.lenerr++;
        netstats.ip.drop++;
        free_packet(p);
        return;
    }

    // check the checksum
    if(inet_chksum((uint16_t *)(p->head + ETHER_HLEN), h->hlen * 4, 0) != 0)
    {
        printk("ip: dropped packet with invalid header checksum\n");
        netstats.ip.chkerr++;
        netstats.ip.drop++;
        free_packet(p);
        return;
    }

    // convert multibyte fields to host representation
    hdr_convert(h);

    // check if this is a DHCP packet, as those need to be processed to get
    // our IP address
    if(h->proto == IPPROTO_UDP &&
       ((struct udp_hdr_t *)((char *)h + (h->hlen * 4)))->srcp == DHCP_SERVER_PORT)
    {
        // add or update ARP table entry
        struct ether_header_t *eh = (struct ether_header_t *)p->data;
        add_arp_entry(p->ifp, h->src, eh->src);

        // pass over to the transport layer
        udp_input(p);
        return;
    }

    for(rt = route_head.next; rt != NULL; rt = rt->next)
    {
        if(rt->dest == h->dest ||
           (ipv4_is_broadcast(h->dest, rt->netmask) &&
            ipv4_is_same_network(h->src, rt->dest, rt->netmask)) ||
            h->dest == 0xffffffff ||
            rt->dest == 0x00)
        {
            break;
        }
    }

    if(!rt)
    {
        // packet not for us, either forward or discard

        // convert multibyte fields to network representation
        hdr_convert(h);

        // this will free the packet buf
        ip_forward(p);
        return;
    }

    // add or update ARP table entry if the source IP is on the local network
    if(ipv4_is_same_network(h->src, rt->dest, rt->netmask))
    {
        struct ether_header_t *eh = (struct ether_header_t *)p->data;
        add_arp_entry(rt->ifp, h->src, eh->src);
    }

    /*
     * RFC 1122 says:
     * 
     *    A host SHOULD silently discard a datagram that is received via
     *    a link-layer broadcast (see Section 2.4) but does not specify
     *    an IP multicast or broadcast destination address.
     */
    if((p->flags & PACKET_FLAG_BROADCAST) &&
       !ipv4_is_broadcast(h->dest, rt->netmask) &&
       !ipv4_is_multicast(h->dest))
    {
        printk("ip: dropped broadcast packet\n");
        netstats.ip.drop++;
        free_packet(p);
        return;
    }

    /*
     * Handle IP options and possibly forward the packet.
     * If the packet is forwarded, or there is an error processing
     * options, packet will be freed, so we don't need to do this here.
     */
    if((h->hlen * 4) > sizeof(struct ipv4_hdr_t) && ip_do_options(p))
    {
        return;
    }

    /*
     * Reassemble fragmented packets
     */
    if((h->offset & (IP_OFFMASK | IP_MF)) != 0)
    {
        struct ipv4_fraglist_t *ifq;

        kernel_mutex_lock(&fragq.lock);

        /*
         * RFC 791 says we should identify fragment buffers via the
         * combination of 4 fields: source, destination, protocol and
         * identification fields.
         */
        for(ifq = fragq.head; ifq != NULL; ifq = ifq->next)
        {
            if(ifq->id == h->id && ifq->proto == h->proto &&
               ifq->src == h->src && ifq->dest == h->dest)
            {
                break;
            }
        }

        kernel_mutex_unlock(&fragq.lock);

        // if all fragments are reassembled, we get a packet back
        if((p = ip_reassemble(p, ifq)) == NULL)
        {
            KDEBUG("ip: incomplete fragmented packet\n");
            return;
        }
    }

    // check if a raw socket wants this packet
    if(raw_input(p) == 0)
    {
        KDEBUG("ip: packet consumed by raw socket\n");
        return;
    }

    // route up to a higher layer
    switch(h->proto)
    {
        case IPPROTO_UDP:
            udp_input(p);
            break;

        case IPPROTO_TCP:
            tcp_input(p);
            break;

        case IPPROTO_ICMP:
            icmpv4_input(p);
            break;

        default:
            printk("ip: dropping packet with unsupported protocol (0x%x)\n", h->proto);
            netstats.ip.proterr++;
            netstats.ip.drop++;

            if(!ipv4_is_broadcast(h->dest, rt->netmask) &&
               !ipv4_is_multicast(h->dest))
            {
                // this will free the packet buf
                icmpv4_dest_unreach(p, ICMP_DESTUNREACH_PROTO);
            }
            else
            {
                free_packet(p);
            }
            break;
    }
}


/*
 * Process IP options.
 *
 * Returns 0 if the options processed successfully, -errno if there was an
 * error, or if the packet was forwarded. An ICMP packet is sent in case of
 * an error.
 */
static int ip_do_options(struct packet_t *p)
{
    struct rtentry_t *rt;
    struct ipv4_hdr_t *h;
    struct ip_timestamp_t *ipt;
    uint8_t *opts, opt, code, opt_len = 0;
    uint8_t off, icmp_type = ICMP_MSG_PARAMPROBLEM;
    int hlen, bytes;
    int forward = 0;
    uint32_t ntime, dest, ipaddr, *sin;

    h = IPv4_HDR(p);
    hlen = h->hlen * 4;
    dest = h->dest;
    opts = (uint8_t *)p->data + hlen;
    bytes = hlen - sizeof(struct ipv4_hdr_t);

    for( ; bytes > 0; bytes -= opt_len, opts += opt_len)
    {
        opt = opts[IPOPT_OPTVAL];
        
        if(opt == IPOPT_EOL)
        {
            break;
        }
        else if(opt == IPOPT_NOP)
        {
            opt_len = 1;
        }
        else
        {
            opt_len = opts[IPOPT_OLEN];
            
            if(opt_len == 0 || opt_len > bytes)
            {
                code = &opts[IPOPT_OLEN] - (uint8_t *)p->data;
                goto err;
            }
        }
        
        switch(opt)
        {
            /*
             * Loose or strict source routing. Check if the destination
             * address is on this machine. If the destination address is
             * not on this machine, drop the packet if source routed, or
             * do nothing if loosely routed.
             */
            case IPOPT_LSRR:
            case IPOPT_SSRR:
                // offset should be >= 4
                if((off = opts[IPOPT_OFFSET]) < IPOPT_MINOFF)
                {
                    code = &opts[IPOPT_OFFSET] - (uint8_t *)p->data;
                    goto err;
                }

                if(!(rt = route_for_ipv4(h->dest)))
                {
                    // strictly routed - send ICMP DESTUNREACH msg
                    if(opt == IPOPT_SSRR)
                    {
                        icmp_type = ICMP_MSG_DESTUNREACH;
                        code = ICMP_DESTUNREACH_SRCFAIL;
                        goto err;
                    }
                    
                    // loosely routed - do nothing (just forward)
                    break;
                }
                
                //off--;
                
                if(off >= (opt_len - sizeof(uint32_t)))
                {
                    // end of source route
                    //save_route(opts, h->src);
                    break;
                }
                
                // find outgoing interface
                ipaddr = ((uint32_t *)(opts + off))[0];

                if(opt == IPOPT_SSRR)
                {
                    rt = route_for_ipv4(ipaddr);
                }
                else
                {
                    /*
                     * TODO: find route address
                     *       See netinet/ip_input.c in 4.3BSD source
                     */
                    icmp_type = ICMP_MSG_DESTUNREACH;
                    code = ICMP_DESTUNREACH_SRCFAIL;
                    goto err;
                }
                
                if(!rt)
                {
                    icmp_type = ICMP_MSG_DESTUNREACH;
                    code = ICMP_DESTUNREACH_SRCFAIL;
                    goto err;
                }
                
                h->dest = ipaddr;
                ((uint32_t *)(opts + off))[0] = rt->dest;
                opts[IPOPT_OFFSET] += sizeof(uint32_t);
                forward = !ipv4_is_multicast(ipaddr);
                break;
            
            case IPOPT_RR:
                if((off = opts[IPOPT_OFFSET]) < IPOPT_MINOFF)
                {
                    code = &opts[IPOPT_OFFSET] - (uint8_t *)p->data;
                    goto err;
                }

                //off--;

                if(off >= (opt_len - sizeof(uint32_t)))
                {
                    break;
                }

                if(!(rt = route_for_ipv4(h->dest)))
                {
                    icmp_type = ICMP_MSG_DESTUNREACH;
                    code = ICMP_DESTUNREACH_HOST;
                    goto err;
                }

                ((uint32_t *)(opts + off))[0] = rt->dest;
                opts[IPOPT_OFFSET] += sizeof(uint32_t);
                break;

            case IPOPT_TS:
                code = opts - (uint8_t *)p->data;
                ipt = (struct ip_timestamp_t *)opts;
                
                if(ipt->len < 5)
                {
                    goto err;
                }
                
                if(ipt->ptr > ipt->len - 4)
                {
                    TIMESTAMP_OVERFLOW_SET(ipt, TIMESTAMP_OVERFLOW(ipt) + 1);

                    if(TIMESTAMP_OVERFLOW(ipt) == 0)
                    {
                        goto err;
                    }
                    
                    break;
                }
                
                sin = (uint32_t *)(opts + ipt->ptr - 1);
                
                switch(TIMESTAMP_FLAGS(ipt))
                {
                    case IPOPT_TS_TSONLY:
                        break;

                    case IPOPT_TS_TSANDADDR:
                        if(ipt->ptr + 4 + sizeof(uint32_t) > ipt->len)
                        {
                            goto err;
                        }
                        
                        if(!(rt = route_for_ipv4(dest)))
                        {
                            continue;
                        }
                        
                        *sin = rt->dest;
                        ipt->ptr += sizeof(uint32_t);
                        break;

                    case IPOPT_TS_PRESPEC:
                        if(ipt->ptr + 4 + sizeof(uint32_t) > ipt->len)
                        {
                            goto err;
                        }

                        if(!(route_for_ipv4(*sin)))
                        {
                            continue;
                        }

                        ipt->ptr += sizeof(uint32_t);
                        break;

                    default:
                        goto err;
                }
                
                ntime = iptime();
                ((uint32_t *)(opts + ipt->ptr - 1))[0] = ntime;
                ipt->ptr += 4;
                break;

            default:
                break;
        }
    }
    
    if(forward)
    {
        // convert multibyte fields to network representation
        hdr_convert(h);

        // this will free the packet buf
        ip_forward(p);
        return -1;
    }
    
    return 0;

err:

    // this will free the packet buf
    icmpv4_send(p, icmp_type, code);
    netstats.ip.opterr++;
    return -EINVAL;
}


/*
 * Try to reassemble fragments to form a full packet. If we are still waiting
 * for more fragments to come, this fragment is added to the waiting queue,
 * which is passed to us in *ifq. If *ifq == NULL, this is the first fragment
 * and we create a new queue for it.
 */
static struct packet_t *ip_reassemble(struct packet_t *p, struct ipv4_fraglist_t *ifq)
{
    struct ipv4_hdr_t *h, *h2;
    struct packet_t *next, *prev;
    int hlen;
    uint16_t offset;
    uint8_t *buf;

    h = IPv4_HDR(p);
    hlen = h->hlen * 4;

    // subtract ip header length from fragment's length, and convert
    // the offset count into bytes
    h->tlen -= hlen;
    SET_IP_OFFSET(h, GET_IP_OFFSET(h) * 8);
    p->next = NULL;
    
    // create a new queue if this is the first fragment
    if(ifq == NULL)
    {
        if(!(ifq = kmalloc(sizeof(struct ipv4_fraglist_t))))
        {
            netstats.ip.drop++;
            free_packet(p);
            return NULL;
        }
        
        kernel_mutex_lock(&fragq.lock);
        IPFRAG_ENQUEUE(&fragq, ifq)
        kernel_mutex_unlock(&fragq.lock);
        
        ifq->ttl = IPFRAGTTL;
        ifq->proto = h->proto;
        ifq->id = h->id;
        ifq->src = h->src;
        ifq->dest = h->dest;
        ifq->plist = p;
        //ifq->next = NULL;
        return NULL;
    }
    
    prev = ifq->plist;
    
    // find a fragment that begins after this one
    for(next = ifq->plist; next != NULL; next = next->next)
    {
        if(GET_IP_OFFSET(IPv4_HDR(next)) > GET_IP_OFFSET(h))
        {
            break;
        }

        prev = next;
    }
    
    /*
     * If there is a preceding fragment, check if its data overlaps with
     * ours. If the preceding fragment covers some of our data, update our
     * pointers. If the preceding fragment covers all of our data, drop us.
     */
    if(prev)
    {
        h2 = IPv4_HDR(prev);
        offset = GET_IP_OFFSET(h2) + h2->tlen - GET_IP_OFFSET(h);

        if(offset > 0)
        {
            if(offset >= h->tlen)
            {
                netstats.ip.drop++;
                free_packet(p);
                return NULL;
            }
            
            h->offset += offset;
            h->tlen -= offset;
        }
    }
    
    // add the new fragment to its queue
    if(prev)
    {
        prev->next = p;
    }
    else
    {
        ifq->plist = p;
    }
    
    p->next = next;
    
    // now check to see if we've got all fragments
    offset = 0;
    prev = ifq->plist;
    
    for(next = ifq->plist; next != NULL; next = next->next)
    {
        // there is a gap here, wait for next fragment to come in
        h2 = IPv4_HDR(next);

        if(GET_IP_OFFSET(h2) != offset)
        {
            return NULL;
        }

        offset += h2->tlen;
        prev = next;
    }
    
    // check if the last fragment is actually the last fragment in packet
    if(GET_IP_FLAGS(IPv4_HDR(prev)) & IP_MF)
    {
        return NULL;
    }
    
    // we've got all fragments - time to assemble
    if(!(p = alloc_packet(PACKET_SIZE_IP(offset))))
    {
        /*
         * TODO: we don't have enogh memory for reassembly - should we 
         *       send an ICMP packet back to the sender here?
         */
        for(next = ifq->plist; next != NULL; )
        {
            prev = next->next;
            free_packet(next);
            next = prev;
        }

        remove_from_fqueue(ifq);
        netstats.ip.drop++;

        return NULL;
    }

    //packet_add_header(p, -ETHER_HLEN);
    h = IPv4_HDR(p);
    h2 = IPv4_HDR(ifq->plist);
    h->ver = 4;
    h->hlen = (IPv4_HLEN / 4);
    h->tos = h2->tos;
    h->tlen = offset + IPv4_HLEN;
    h->id = ifq->id;

    // get flags from first fragment, but turn the More Fragments bit off
    h->offset = 0 | GET_IP_FLAGS(h2);
    h->offset &= ~IP_MF;

    h->ttl = h2->ttl;
    h->proto = ifq->proto;
    h->checksum = 0;
    h->src = ifq->src;
    h->dest = ifq->dest;

    offset = 0;
    buf = (uint8_t *)p->data + IPv4_HLEN;
    
    // copy all fragment data into the new packet
    for(next = ifq->plist; next != NULL; )
    {
        h2 = IPv4_HDR(next);
        A_memcpy(buf, (uint8_t *)next->data + (h2->hlen * 4), h2->tlen);
        buf += h2->tlen;
        prev = next->next;
        free_packet(next);
        next = prev;
    }
    
    // remove from the queue
    ifq->plist = NULL;
    remove_from_fqueue(ifq);
    
    return p;
}


static void remove_from_fqueue(struct ipv4_fraglist_t *ifq)
{
    struct ipv4_fraglist_t *ifq2;
    
    kernel_mutex_lock(&fragq.lock);
    
    if(fragq.head == ifq)
    {
        fragq.head = ifq->next;
        ifq2 = NULL;
    }
    else
    {
        ifq2 = fragq.head;
        
        while(ifq2 && ifq2->next != ifq)
        {
            ifq2 = ifq2->next;
        }
        
        // this shouldn't happen as we know the fragment list is in the queue
        if(!ifq2)
        {
            kpanic("ip: invalid fragment queue pointer");
        }
        
        ifq2->next = ifq->next;
    }

    if(fragq.tail == ifq)
    {
        fragq.tail = ifq2;
    }
    
    fragq.count--;
    kfree(ifq);

    kernel_mutex_unlock(&fragq.lock);
}


STATIC_INLINE void set_hdr_src(struct netif_t *ifp, struct ipv4_hdr_t *h, uint32_t src)
{
    if(src == 0x00)
    {
        struct rtentry_t *rt = route_for_ifp(ifp);

        if(!rt)
        {
            printk("ip: %s: net interface has no IP address\n", ifp->name);
            h->src = 0x00;
        }
        else
        {
            h->src = rt->dest;
        }
    }
    else
    {
        h->src = src;
    }
}


/*
 * Handle sending IP packets.
 */
int ipv4_send(struct packet_t *p, 
              uint32_t src, uint32_t dest,
              uint8_t proto, uint8_t ttl)
{
    struct ipv4_hdr_t *h;
    struct rtentry_t *rt;

    h = IPv4_HDR(p);

    if(p->flags & PACKET_FLAG_HDRINCLUDED)
    {
        dest = h->dest;
    }

    if(!dest || !(rt = route_for_ipv4(dest)))
    {
        printk("ip: cannot find a route (dest 0x%x)\n", dest);
        netstats.ip.rterr++;
        free_packet(p);
        return -EHOSTUNREACH /* EROUTE */;
    }

    /*
    if(rt->flags & RT_GATEWAY)
    {
        dest = rt->gateway;
    }
    */

    if(!(p->flags & PACKET_FLAG_HDRINCLUDED))
    {
        if(packet_add_header(p, IPv4_HLEN) != 0)
        {
            printk("ip: insufficient memory for packet header\n");
            netstats.ip.err++;
            free_packet(p);
            return -ENOBUFS;
        }

        h->ttl = ttl;
        h->proto = proto;
        h->dest = dest;
        h->tos = 0;
        h->ver = 4;
        h->hlen = 5;     // hlen of 5 (=20 bytes)
        h->tlen = htons((uint16_t)p->count);
        h->offset = htons(IP_DF);
        h->id = htons(ip_id);
        ip_id++;

        set_hdr_src(rt->ifp, h, src);

        if(h->proto == IPPROTO_UDP)
        {
            struct udp_hdr_t *udph = (struct udp_hdr_t *)((char *)h + (h->hlen * 4));

            packet_add_header(p, -IPv4_HLEN);
            udph->checksum = 0;
            udph->checksum = udp_v4_checksum(p, h->src, h->dest);
            packet_add_header(p, IPv4_HLEN);
        }
        else if(h->proto == IPPROTO_TCP)
        {
            struct tcp_hdr_t *tcph = (struct tcp_hdr_t *)((char *)h + (h->hlen * 4));

            packet_add_header(p, -IPv4_HLEN);
            tcph->checksum = 0;
            tcph->checksum = tcp_v4_checksum(p, h->src, h->dest);
            packet_add_header(p, IPv4_HLEN);
        }
    }
    else
    {
        h->tlen = htons((uint16_t)p->count);

        // fill these fields if needed
        if(h->src == 0x00)
        {
            set_hdr_src(rt->ifp, h, src);
        }

        if(h->id == 0)
        {
            h->id = htons(ip_id);
            ip_id++;
        }
    }

    h->checksum = 0;
    h->checksum = inet_chksum((uint16_t *)p->data, IPv4_HLEN, 0);

    return do_send(rt, p, dest);
}

