/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: arp.c
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
 *  \file arp.c
 *
 *  Address Resolution Protocol (ARP) implementation.
 */

//#define __DEBUG
#include <errno.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/timer.h>
#include <kernel/net.h>
#include <kernel/net/packet.h>
#include <kernel/net/ether.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/notify.h>
#include <kernel/task.h>
#include <mm/kheap.h>
#include <fs/procfs.h>

#define NR_ARP                  128
#define NR_ARP_POSTPONED        16

// max age for an ARP entry is 60 * 20 = 1200 secs = 20 mins
#define APR_MAXAGE          (1200 * PIT_FREQUENCY)

#define SET_EXPIRY(arp)     (arp)->expiry = ticks + APR_MAXAGE

#define ARP_STATUS_REACHABLE    0
#define ARP_STATUS_PERMANENT    1
#define ARP_STATUS_STALE        2

struct arp_entry_t
{
    struct ether_addr_t ethernet_addr;
    struct in_addr ip_addr;
    int status;
    unsigned long long expiry;
    //unsigned long long timestamp;
    struct netif_t *ifp;
};

static struct arp_entry_t arp_entries[NR_ARP];
static struct packet_t *postponed_arp_packets[NR_ARP_POSTPONED] = { 0, };
static struct task_t *arp_task = NULL;

void arp_postpone(struct packet_t *p)
{
    int i;
    
    for(i = 0; i < NR_ARP_POSTPONED; i++)
    {
        if(!postponed_arp_packets[i])
        {
            if(p->nfailed < 4)
            {
                KDEBUG("arp_postpone: saving %d\n", i);
                postponed_arp_packets[i] = p;
            }
            else
            {
                // Ethernet layer will not expect a result from us, so 
                // free the packet
                packet_free(p);
            }
            
            return;
        }
    }
    
    // Ethernet layer will not expect a result from us, so free the packet
    packet_free(p);
}


static void arp_send_postponed(void)
{
    int i;

    KDEBUG("arp_send_postponed:\n");
    
    for(i = 0; i < NR_ARP_POSTPONED; i++)
    {
        if(!postponed_arp_packets[i])
        {
            continue;
        }

        KDEBUG("arp_send_postponed: adding %d to eth outq\n", i);
        
        IFQ_ENQUEUE(&ethernet_outq, postponed_arp_packets[i]);
        postponed_arp_packets[i] = NULL;
    }
}


static int arp_add(struct netif_t *ifp, uint32_t ipaddr, 
                   struct ether_addr_t *ethaddr)
{
    int i;

    KDEBUG("arp_add:\n");
    
    for(i = 0; i < NR_ARP; i++)
    {
        if(arp_entries[i].ip_addr.s_addr == 0)
        {
            KDEBUG("arp_add: adding entry %d\n", i);

            arp_entries[i].ip_addr.s_addr = ipaddr;
            COPY_ETHER_ADDR(arp_entries[i].ethernet_addr.addr, ethaddr->addr);
            arp_entries[i].ifp = ifp;
            arp_entries[i].status = ARP_STATUS_REACHABLE;
            //arp_entries[i].timestamp = ticks;
            SET_EXPIRY(&arp_entries[i]);
            
            arp_send_postponed();
            return 0;
        }
    }
    
    return -ENOBUFS;
}


/*
 * Returns 1 if the ARP entry was found, 0 otherwise.
 */
static int arp_lookup_entry(uint32_t ipaddr, struct ether_addr_t *ethaddr)
{
    int i;

    /*
    KDEBUG("arp_lookup_entry: ip ");
    KDEBUG_IPV4_ADDR(ntohl(ipaddr));
    KDEBUG(" - eth ");
    KDEBUG_ETHER_ADDR(ethaddr->addr);
    KDEBUG("\n");
    */

    for(i = 0; i < NR_ARP; i++)
    {
        if(arp_entries[i].ip_addr.s_addr == 0 ||
           arp_entries[i].ip_addr.s_addr != ipaddr)
        {
            continue;
        }
        
        if(arp_entries[i].status == ARP_STATUS_STALE)
        {
            KDEBUG("arp_lookup_entry: updating stale entry\n");
            // update stale entry
            arp_entries[i].status = ARP_STATUS_REACHABLE;
            //arp_entries[i].timestamp = ticks;
            SET_EXPIRY(&arp_entries[i]);
            
            arp_send_postponed();
            return 1;
        }
        else
        {
            KDEBUG("arp_lookup_entry: updating mac addr\n");
            // update MAC address
            COPY_ETHER_ADDR(arp_entries[i].ethernet_addr.addr, ethaddr->addr);
            //arp_entries[i].timestamp = ticks;
            SET_EXPIRY(&arp_entries[i]);
            return 1;
        }
    }
    
    KDEBUG("arp_lookup_entry: entry not found\n");
    return 0;
}


void arp_update_entry(struct netif_t *ifp, uint32_t ipaddr, 
                      struct ether_addr_t *ethaddr)
{
    /*
    KDEBUG("arp_update_entry: adding ip ");
    KDEBUG_IPV4_ADDR(ntohl(ipaddr));
    KDEBUG(" - eth ");
    KDEBUG_ETHER_ADDR(ethaddr->addr);
    KDEBUG("\n");
    */

    if(!arp_lookup_entry(ipaddr, ethaddr))
    {
        arp_add(ifp, ipaddr, ethaddr);
    }
}


static void arp_process_input(struct packet_t *p, 
                              struct arp_header_t *h, int found)
{
    KDEBUG("arp_process_input:\n");

    struct in_addr dest;
    struct ipv4_link_t *link;

    // RFC 826 says we can optionally check these fields
    if(ntohs(h->hwtype) != 1 ||
       h->hwlen != ETHER_ADDR_LEN ||
       ntohs(h->proto) != ETHERTYPE_IP ||
       h->protolen != sizeof(struct in_addr))
    {
        KDEBUG("arp: discarding ARP packet with invalid header field(s)\n");
        //KDEBUG("arp_intr: hwtype %d, hwlen %d (%d), proto %d (%d), protolen %d (%d)\n", h->hwtype, h->hwlen, ETHER_ADDR_LEN, h->proto, ETHERTYPE_IP, h->protolen, sizeof(struct in_addr));

        packet_free(p);
        return;
    }
    
    // source MAC address must not be a multicast or broadcast address
    if(h->sha.addr[0] & 0x01)
    {
        KDEBUG("arp: discarding broadcast/multicast packet\n");
        packet_free(p);
        return;
    }

    /*
    KDEBUG("arp_process_input: spa ");
    KDEBUG_IPV4_ADDR(ntohl(h->spa.s_addr));
    KDEBUG(", tpa ");
    KDEBUG_IPV4_ADDR(ntohl(h->tpa.s_addr));
    KDEBUG("\n");
    */
    
    dest.s_addr = h->tpa.s_addr;
    
    // avoid ARP flooding
    if((link = ipv4_link_find(&dest)))
    {
        /*
        KDEBUG("arp_process_input: found link %lx\n", link);
        KDEBUG("arp_process_input: h->code = ARP_%s\n", ntohs(h->opcode) == ARP_REQUEST ? "REQUEST" : "REPLY");
        KDEBUG("arp_process_input: link->ifp %lx, p->ifp %lx\n", link->ifp, p->ifp);
        */
        
        // discard if going down the wrong ifp
        if(link->ifp != p->ifp)
        {
            KDEBUG("arp: packet sent down the wrong interface\n");
            packet_free(p);
            return;
        }
    }
    else
    {
        KDEBUG("arp: cannot find link -- discarding packet\n");
        packet_free(p);
        return;
    }
    
    // add ARP entry if there is none
    if(!found && arp_add(p->ifp, h->spa.s_addr, &h->sha) != 0)
    {
        KDEBUG("arp: full table -- discarding packet\n");
        packet_free(p);
        return;
    }
    
    // send ARP REPLY if this is a QUERY
    if(ntohs(h->opcode) == ARP_REQUEST)
    {
        KDEBUG("arp_process_input: sending reply\n");

        // modify the packet and send it back
        h->opcode = htons(ARP_REPLY);

        COPY_ETHER_ADDR(h->tha.addr, h->sha.addr);
        COPY_ETHER_ADDR(h->sha.addr, p->ifp->ethernet_addr.addr);
        COPY_ETHER_ADDR(h->ether_header.dest.addr, h->tha.addr);
        COPY_ETHER_ADDR(h->ether_header.src.addr, h->sha.addr);

        h->tpa.s_addr = h->spa.s_addr;
        h->spa.s_addr = dest.s_addr;

        h->hwtype = htons(1);
        h->hwlen = ETHER_ADDR_LEN;
        h->proto = htons(ETHERTYPE_IP);
        h->protolen = sizeof(struct in_addr);
        h->ether_header.type = htons(ETHERTYPE_ARP);
        
        if(p->ifp->transmit(p->ifp, p) == 0)
        {
            return;
        }
    }

    KDEBUG("arp_process_input: done\n");
    
    packet_free(p);
}


void arp_receive(struct packet_t *p)
{
    struct arp_header_t *h = (struct arp_header_t *)p->data;
    
    arp_process_input(p, h, arp_lookup_entry(h->spa.s_addr, &h->sha));
}


static void arp_unreachable(struct in_addr *addr)
{
    int i;
    struct ipv4_hdr_t *h;
    struct in_addr src = { 0 };
    struct in_addr dest = { 0 };
    struct in_addr gateway = { 0 };
    
    for(i = 0; i < NR_ARP_POSTPONED; i++)
    {
        if(!postponed_arp_packets[i])
        {
            continue;
        }
        
        h = (struct ipv4_hdr_t *)postponed_arp_packets[i]->data;
        dest.s_addr = h->dest.s_addr;
        gateway.s_addr = h->dest.s_addr;
        
        ipv4_route_gateway_get(&gateway, &dest);
        
        if(gateway.s_addr == addr->s_addr)
        {
            src.s_addr = h->src.s_addr;
            
            // check src is not a local addr
            if(src.s_addr != INADDR_ANY && !ipv4_link_find(&src))
            {
                notify_dest_unreachable(postponed_arp_packets[i], 0);
            }
        }
    }
}


static void arp_retry(struct packet_t *p, struct in_addr *where)
{
    if(++p->nfailed < 4)
    {
        KDEBUG("arp: retry #%d\n", p->nfailed);
        arp_request(p->ifp, where);
    }
    else
    {
        arp_unreachable(where);
    }
}


struct ether_addr_t *arp_get(struct packet_t *p)
{
    struct ipv4_hdr_t *h = (struct ipv4_hdr_t *)p->data;
    struct in_addr dest = { h->dest.s_addr };
    struct in_addr gateway = { 0 };
    struct in_addr *where;
    struct ipv4_link_t *link;
    int i;
    
    /*
    KDEBUG("arp_get: addr ");
    KDEBUG_IPV4_ADDR(ntohl(h->dest.s_addr));
    KDEBUG("\n");
    */
    
    if((link = ipv4_link_get(&dest)))
    {
        KDEBUG("arp_get: found local addr\n");
        // addr is ours
        return &link->ifp->ethernet_addr;
    }
    
    ipv4_route_gateway_get(&gateway, &dest);

    if(gateway.s_addr != 0)
    {
        where = &gateway;
    }
    else
    {
        where = &dest;
    }

    /*
    KDEBUG("arp_get: gateway ");
    KDEBUG_IPV4_ADDR(ntohl(gateway.s_addr));
    KDEBUG(", dest ");
    KDEBUG_IPV4_ADDR(ntohl(dest.s_addr));
    KDEBUG("\n");
    */
    
    // lookup the table
    for(i = 0; i < NR_ARP; i++)
    {
        if(arp_entries[i].ip_addr.s_addr == where->s_addr &&
           arp_entries[i].status != ARP_STATUS_STALE)
        {
            break;
        }
    }
    
    // not found
    if(i == NR_ARP)
    {
        arp_retry(p, where);
        return NULL;
    }
    
    return &arp_entries[i].ethernet_addr;
}


void arp_request(struct netif_t *ifp, struct in_addr *dest)
{
    struct packet_t *p;
    struct arp_header_t *h;
    struct in_addr src;

    /*
    KDEBUG("arp_request: addr ");
    KDEBUG_IPV4_ADDR(ntohl(dest->s_addr));
    KDEBUG("\n");
    */

    if(!(p = packet_alloc(sizeof(struct arp_header_t), PACKET_RAW)))
    {
        /*
         * TODO: should we sleep here and wait until memory is available?
         */
        KDEBUG("arp: insufficient memory for package\n");
        netstats.link.drop++;
        return;
    }
    
    if(ipv4_source_find(&src, dest) != 0)
    {
        KDEBUG("arp: cannot find route\n");
        packet_free(p);
        netstats.link.drop++;
        return;
    }
    
    h = (struct arp_header_t *)p->data;
    h->opcode = htons(ARP_REQUEST);
    
    h->tpa.s_addr = dest->s_addr;
    h->spa.s_addr = src.s_addr;

    //KDEBUG("%s: h->spa.s_addr %x\n", __func__, h->spa.s_addr);
    //KDEBUG("%s: h->tpa.s_addr %x\n", __func__, h->tpa.s_addr);

    h->hwtype = htons(1);
    h->hwlen = ETHER_ADDR_LEN;

    h->proto = htons(ETHERTYPE_IP);
    h->protolen = sizeof(struct in_addr);

    // RFC 826 doesn'says we can set 'tha' to anything, though it suggests
    // we might set it to the Ethernet broadcast address (all ones)
    SET_ETHER_ADDR_BYTES(h->tha.addr, 0x00);
    SET_ETHER_ADDR_BYTES(h->ether_header.dest.addr, 0xFF);

    COPY_ETHER_ADDR(h->sha.addr, ifp->ethernet_addr.addr);
    COPY_ETHER_ADDR(h->ether_header.src.addr, ifp->ethernet_addr.addr);

    h->ether_header.type = htons(ETHERTYPE_ARP);

    /*
     * NOTE: it is the transmitting function's duty to free the packet!
     *       we leave this to the transmitting function, as it may queue the
     *       packet instead of sending it right away.
     */
    
    if(ifp->transmit(ifp, p) < 0)
    {
        printk("%s: failed to send ARP packet\n", ifp->name);
        //packet_free(p);
        netstats.link.drop++;
        return;
    }
}


void arp_check_expired(void)
{
    int i;
    
    for(i = 0; i < NR_ARP; i++)
    {
        if(arp_entries[i].ip_addr.s_addr == 0)
        {
            continue;
        }
        
        if(ticks >= arp_entries[i].expiry)
        {
            KDEBUG("arp: table entry %d expired\n", i);
            arp_entries[i].status = ARP_STATUS_STALE;
            arp_request(arp_entries[i].ifp, &arp_entries[i].ip_addr);
        }
    }
}


/*
 * ARP task function.
 */
void arp_task_func(void *arg)
{
    UNUSED(arg);

    for(;;)
    {
        //KDEBUG("arp_task_func:\n");

        arp_check_expired();
        
        //KDEBUG("arp_task_func: sleeping\n");

        // schedule every 60 seconds - TODO: is this the right interval?
        block_task2(&arp_task, PIT_FREQUENCY * 60);
    }
}


void arp_init(void)
{
    A_memset(arp_entries, 0, sizeof(arp_entries));
    
    // fork the arp task
    (void)start_kernel_task("arp", arp_task_func, NULL, &arp_task, 0);
}


/*
 * Read /proc/net/arp.
 */
size_t get_arp_list(char **buf)
{
    size_t len, count = 0, bufsz = 1024;
    char tmp[64];
    char *p;
    int i;

    PR_MALLOC(*buf, bufsz);
    p = *buf;
    *p = '\0';

    ksprintf(p, 64, "IP address      HW type   HW address          Device\n");
    len = strlen(p);
    count += len;
    p += len;

#define ADDR_BYTE(addr, shift)      (((addr) >> (shift)) & 0xff)

    for(i = 0; i < NR_ARP; i++)
    {
        if(arp_entries[i].ip_addr.s_addr == 0)
        {
            continue;
        }

        ksprintf(tmp, 64, "%u.%u.%u.%u",
                          ADDR_BYTE(arp_entries[i].ip_addr.s_addr, 0 ),
                          ADDR_BYTE(arp_entries[i].ip_addr.s_addr, 8 ),
                          ADDR_BYTE(arp_entries[i].ip_addr.s_addr, 16),
                          ADDR_BYTE(arp_entries[i].ip_addr.s_addr, 24));
        len = strlen(tmp);

        while(len < 16)
        {
            tmp[len++] = ' ';
            tmp[len] = '\0';
        }

        ksprintf(tmp + len, 64 - len,
                 "0x1       %02x:%02x:%02x:%02x:%02x:%02x",
                 arp_entries[i].ethernet_addr.addr[0],
                 arp_entries[i].ethernet_addr.addr[1],
                 arp_entries[i].ethernet_addr.addr[2],
                 arp_entries[i].ethernet_addr.addr[3],
                 arp_entries[i].ethernet_addr.addr[4],
                 arp_entries[i].ethernet_addr.addr[5]);
        len = strlen(tmp);

        ksprintf(tmp + len, 64 - len, "   %s\n",
                 arp_entries[i].ifp ? arp_entries[i].ifp->name : "?");
        len = strlen(tmp);

        if(count + len >= bufsz)
        {
            PR_REALLOC(*buf, bufsz, count);
            p = *buf + count;
        }

        count += len;
        strcpy(p, tmp);
        p += len;
    }

#undef ADDR_BYTE

    return count;
}

