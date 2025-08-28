/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
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

#include <limits.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <endian.h>
#include <kernel/laylaos.h>
#include <kernel/softint.h>
#include <kernel/net.h>
#include <kernel/timer.h>
#include <kernel/softint.h>
#include <kernel/net/route.h>
#include <kernel/net/arp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv4_addr.h>
#include <kernel/net/stats.h>
#include <kernel/net/dhcp.h>
#include <mm/kheap.h>
#include <fs/procfs.h>

// size of ARP entry table
#define NR_ARP              64

// how many incoming/outgoing packets we can keep in queue
#define MAX_ARP_PACKETS     128

// max age for an ARP entry is 60 * 20 = 1200 secs = 20 mins
//#define APR_MAXAGE          (7600 * PIT_FREQUENCY)
#define APR_MAXAGE          (1200 * PIT_FREQUENCY)

// age ARP entries every 60 * 5 = 300 secs = 5 mins
#define ARP_PRUNE           (300 * PIT_FREQUENCY)

#define SET_EXPIRY(arp)     (arp)->expiry = ticks + APR_MAXAGE

#define SET_ETHER_ADDR_BYTES(a, b)                      \
    for(int z = 0; z < ETHER_ADDR_LEN; z++) a[z] = b;

#define COPY_ETHER_ADDR(d, s)                           \
    for(int z = 0; z < ETHER_ADDR_LEN; z++) d[z] = s[z];

#define REMOVE_ENTRY(a)                                 \
    (a)->ip_addr = 0;                                   \
    (a)->expiry = 0;                                    \
    (a)->ifp = 0;                                       \
    SET_ETHER_ADDR_BYTES((a)->hwaddr, 0x00);

struct arp_entry_t
{
    uint32_t ip_addr;
    uint8_t hwaddr[ETHER_ADDR_LEN];
    unsigned long long expiry;
    struct netif_t *ifp;
};

struct arp_packet_t
{
    struct netif_t *ifp;
    struct packet_t *p;
    uint32_t ip_addr;
    unsigned long long expiry;
    struct arp_packet_t *next;
};

struct arp_packet_queue_t
{
    struct arp_packet_t *head, *tail;
    int count, max;
    volatile struct kernel_mutex_t lock;
};

struct arp_entry_t arp_entries[NR_ARP];
struct arp_packet_queue_t arp_out_queue = { 0, };
volatile struct task_t *arp_task = NULL;


static void check_delayed_packets(void);
static void arp_timer(void *unused);


/*
 * Initialize ARP.
 */
void arp_init(void)
{
    A_memset(arp_entries, 0, sizeof(arp_entries));
    init_kernel_mutex(&arp_out_queue.lock);
    arp_out_queue.max = MAX_ARP_PACKETS;

    // kick off the timer
    (void)start_kernel_task("arp", arp_timer, NULL, &arp_task, 0);
}


static void arp_timer(void *unused)
{
    UNUSED(unused);
    volatile struct arp_entry_t *a;

    while(1)
    {
        for(a = arp_entries; a < &arp_entries[NR_ARP]; a++)
        {
            if(a->ip_addr == 0x00)
            {
                continue;
            }
        
            if(a->expiry && a->expiry <= ticks)
            {
                REMOVE_ENTRY(a);
            }
        }

        block_task2(&arp_entries, ARP_PRUNE);
    }
}


int arp_resolve(struct rtentry_t *rt, uint32_t addr, uint8_t *dest)
{
    int i;

    if(addr == 0x00 || ipv4_is_broadcast(addr, rt->netmask))
    {
        COPY_ETHER_ADDR(dest, ethernet_broadcast);
        return 1;
    }
    else if(ipv4_is_multicast(addr))
    {
        dest[0] = 0x01;
        dest[1] = 0x00;
        dest[2] = 0x5e;
        dest[3] = ipaddr_byte(addr, 1) & 0x7F;
        dest[4] = ipaddr_byte(addr, 2);
        dest[5] = ipaddr_byte(addr, 3);
        return 1;
    }
    else if(addr == rt->dest)
    {
        COPY_ETHER_ADDR(dest, rt->ifp->hwaddr);
        return 1;
    }
    
    // if destination address is on the same network, search for its address,
    // otherwise, use the default gateway's address
    if((rt->flags & RT_GATEWAY) ||
       !ipv4_is_same_network(addr, rt->dest, rt->netmask))
    {
        addr = rt->gateway;
    }
    
    for(i = 0; i < NR_ARP; i++)
    {
        if(addr == arp_entries[i].ip_addr)
        {
            COPY_ETHER_ADDR(dest, arp_entries[i].hwaddr);
            return 1;
        }
    }
    
    arp_request(rt->ifp, rt->dest, addr);
    return 0;
}


void arp_request(struct netif_t *ifp, uint32_t src, uint32_t dest)
{
    struct packet_t *p;
    struct arp_header_t *h;
    int i;

    if(!(p = alloc_packet(sizeof(struct arp_header_t))))
    {
        /*
         * TODO: should we sleep here and wait until memory is available?
         */
        printk("%s: insufficient memory for ARP package\n", ifp->name);
        return;
    }
    
    h = (struct arp_header_t *)p->data;
    h->opcode = htons(ARP_REQUEST);
    h->tpa = dest;
    h->spa = src;
    h->hwtype = htons(1);
    h->hwlen = ETHER_ADDR_LEN;
    h->proto = htons(ETHERTYPE_IP);
    h->protolen = sizeof(uint32_t);

    // RFC 826 doesn'says we can set 'tha' to anything, though it suggests
    // we might set it to the Ethernet broadcast address (all ones)
    SET_ETHER_ADDR_BYTES(h->tha, 0x00);
    SET_ETHER_ADDR_BYTES(h->ether_header.dest, 0xFF);
    COPY_ETHER_ADDR(h->sha, ifp->hwaddr);
    COPY_ETHER_ADDR(h->ether_header.src, ifp->hwaddr);

    h->ether_header.type = htons(ETHERTYPE_ARP);

    /*
     * NOTE: it is the transmitting function's duty to free the packet!
     *       we leave this to the transmitting function, as it may queue the
     *       packet instead of sending it right away.
     */
    
    if((i = ifp->transmit(ifp, p)) < 0)
    {
        printk("%s: failed to send ARP packet (err %d)\n", ifp->name, i);
        netstats.link.drop++;
        return;
    }
}


int arp_queue(struct netif_t *ifp, struct packet_t *p, uint32_t ipaddr)
{
    struct arp_packet_t *arpp;
    int res = 0;
    
    if(!(arpp = kmalloc(sizeof(struct arp_packet_t))))
    {
        return -ENOMEM;
    }
    
    arpp->ifp = ifp;
    arpp->p = p;
    arpp->ip_addr = ipaddr;
    arpp->next = NULL;
    arpp->expiry = ticks + 1000;

    kernel_mutex_lock(&arp_out_queue.lock);
    
    if(ARP_FULL(&arp_out_queue))
    {
        netstats.link.drop++;
        res = -ENOMEM;
    }
    else
    {
        ARP_ENQUEUE(&arp_out_queue, arpp);
    }

    kernel_mutex_unlock(&arp_out_queue.lock);
    return res;
}


/*
 * ARP interrupt handler.
 */
void arp_recv(struct packet_t *p)
{
    struct rtentry_t *rt;
    struct arp_header_t *h;
    int i;
    int flag = 0;
    uint32_t dest;
    
    netstats.link.recv++;
    h = (struct arp_header_t *)p->data;

    // RFC 826 says we can optionally check these fields
    if(ntohs(h->hwtype) != 1 ||
       h->hwlen != ETHER_ADDR_LEN ||
       ntohs(h->proto) != ETHERTYPE_IP ||
       h->protolen != sizeof(uint32_t))
    {
        printk("arp: discarding packet with invalid header field(s)\n");
        free_packet(p);
        return;
    }

    // source MAC address must not be a multicast or broadcast address
    if(h->sha[0] & 0x01)
    {
        printk("arp: discarding broadcast/multicast packet\n");
        free_packet(p);
        return;
    }

    // Here we follow RFC 826's algorithm:
    //   - Set flag to false (done above)
    //   - If an entry already exists, update our table and set flag
    //     to true
    //   - Check if the packet is for us, discard if not
    //   - If the flag is false, add an ARP entry to our table
    //   - Check the header's opcode
    //   - Reply to the sender (if the opcode is REQUEST)

    flag = update_arp_entry(p->ifp, h->spa, h->sha);

    // check if we are the intended recipient of this packet
    if(!(rt = route_for_ipv4(h->tpa)))
    {
        // not for us
        printk("arp: cannot find link -- discarding packet\n");
        free_packet(p);
        return;
    }

    if(!flag)
    {
        add_arp_entry(p->ifp, h->spa, h->sha);
        flag = 1;
    }

    switch(ntohs(h->opcode))
    {
        case ARP_REQUEST:
            // avoid ARP flooding by limiting address requests to 1/sec
            // as mandated by RFC 1122
            if(rt->ifp->last_arp_request_time >= (ticks - PIT_FREQUENCY))
            {
                break;
            }

            rt->ifp->last_arp_request_time = ticks;
            dest = h->tpa;

            // modify the packet and send it back
            h->opcode = htons(ARP_REPLY);
            h->tpa = h->spa;
            h->spa = dest;

            h->hwtype = htons(1);
            h->hwlen = ETHER_ADDR_LEN;
            h->proto = htons(ETHERTYPE_IP);
            h->protolen = sizeof(uint32_t);
            h->ether_header.type = htons(ETHERTYPE_ARP);

            COPY_ETHER_ADDR(h->tha, h->sha);
            COPY_ETHER_ADDR(h->sha, rt->ifp->hwaddr);
            COPY_ETHER_ADDR(h->ether_header.dest, h->tha);
            COPY_ETHER_ADDR(h->ether_header.src, rt->ifp->hwaddr);

            /*
             * NOTE: it is the transmitting function's duty to free the packet!
             *       we leave this to the transmitting function, as it may queue the
             *       packet instead of sending it right away.
             */
            if((i = rt->ifp->transmit(rt->ifp, p)) < 0)
            {
                printk("arp: failed to send packet (err %d)\n", i);
                netstats.link.drop++;
            }

            break;

        case ARP_REPLY:
            dhcp_arp_reply(h->spa);
            free_packet(p);
            break;
    }

    if(flag)
    {
        check_delayed_packets();
    }
}


/*
 * Check if an entry exists in the ARP table with the given IP address and
 * Ethernet address. Update the entry ONLY if it exists. That is, don't create
 * a new entry.
 *
 * Returns 1 if an entry is found and updated, 0 otherwise.
 */
int update_arp_entry(struct netif_t *ifp, uint32_t ip, uint8_t *eth)
{
    struct arp_entry_t *a;

    for(a = arp_entries; a < &arp_entries[NR_ARP]; a++)
    {
        if(ip == a->ip_addr)
        {
            a->ifp = ifp;
            COPY_ETHER_ADDR(a->hwaddr, eth);
            SET_EXPIRY(a);
            return 1;
        }
    }
    
    return 0;
}


void add_arp_entry(struct netif_t *ifp, uint32_t ip, uint8_t *eth)
{
    struct arp_entry_t *a;
    
    // Try to find an ARP entry with the same IP address.
    // If found, we update the entry and return.
    for(a = arp_entries; a < &arp_entries[NR_ARP]; a++)
    {
        if(ip == a->ip_addr)
        {
            a->ifp = ifp;
            COPY_ETHER_ADDR(a->hwaddr, eth);
            SET_EXPIRY(a);
            return;
        }
    }
    
    // No matching entry found. Try to locate an empty entry in the table
    // and fill it up with the new data.
    for(a = arp_entries; a < &arp_entries[NR_ARP]; a++)
    {
        if(a->ip_addr == 0x00)
        {
            break;
        }
    }
    
    // Table is full. Try to find the oldest entry, discard it and take
    // its place.
    if(a == &arp_entries[NR_ARP])
    {
        struct arp_entry_t *oa = arp_entries;
        unsigned long long expiry = ULLONG_MAX;

        for(a = arp_entries; a < &arp_entries[NR_ARP]; a++)
        {
            if(a->expiry < expiry)
            {
                expiry = a->expiry;
                oa = a;
            }
        }
        
        a = oa;
    }
    
    // Fill in the new data.
    a->ip_addr = ip;
    a->ifp = ifp;
    COPY_ETHER_ADDR(a->hwaddr, eth);
    SET_EXPIRY(a);
}


void arp_set_expiry(uint32_t ip, unsigned long long expiry)
{
    struct arp_entry_t *a;

    for(a = arp_entries; a < &arp_entries[NR_ARP]; a++)
    {
        if(ip == a->ip_addr)
        {
            a->expiry = expiry;
            break;
        }
    }
}


void remove_arp_entry(uint32_t ip)
{
    struct arp_entry_t *a;

    for(a = arp_entries; a < &arp_entries[NR_ARP]; a++)
    {
        if(ip == a->ip_addr)
        {
            REMOVE_ENTRY(a);
            break;
        }
    }
}


STATIC_INLINE int __arp_to_eth(uint32_t ip, uint8_t *eth)
{
    int i;
    
    for(i = 0; i < NR_ARP; i++)
    {
        if(ip == arp_entries[i].ip_addr)
        {
            COPY_ETHER_ADDR(eth, arp_entries[i].hwaddr);
            return 1;
        }
    }
    
    return 0;
}


int arp_to_eth(uint32_t ip, uint8_t *eth)
{
    return __arp_to_eth(ip, eth);
}


/*
 * We check for, and send, any packets that were delayed waiting for the
 * destination's ethernet address. If we do this in the add_arp_entry()
 * function, we can end in a deadlock: the network device interrupt is 
 * triggered, which calls ethernet_receive(), which calls add_arp_entry(),
 * which tries to send the delayed packet by calling the device's transmit
 * function, and the device hangs.
 */
static void check_delayed_packets(void)
{
    uint8_t eth[ETHER_ADDR_LEN];
    struct arp_packet_t *p, *pp;
    struct ether_header_t *h;
    int i;

    // Check to see if there is a delayed transmission awaiting this ARP
    // resolution. If so, we send the packet now.
    kernel_mutex_lock(&arp_out_queue.lock);

    if(arp_out_queue.count == 0)
    {
        kernel_mutex_unlock(&arp_out_queue.lock);
        return;
    }
    
    for(p = arp_out_queue.head, pp = NULL; p != NULL; )
    {
        if(!p->ifp || !p->p || !__arp_to_eth(p->ip_addr, eth))
        {
            pp = p;
            p = p->next;
            continue;
        }
        
        // remove packet from queue
        if(pp)
        {
            pp->next = p->next;
        }
        else
        {
            arp_out_queue.head = p->next;
        }
        
        arp_out_queue.count--;

        h = (struct ether_header_t *)p->p->data;
        h->type = htons(ETHERTYPE_IP);

        COPY_ETHER_ADDR(h->dest, eth);
        COPY_ETHER_ADDR(h->src, p->ifp->hwaddr);

        /*
         * NOTE: it is the transmitting function's duty to free the packet!
         *       we leave this to the transmitting function, as it may queue 
         *       the packet instead of sending it right away.
         */
    
        if((i = p->ifp->transmit(p->ifp, p->p)) < 0)
        {
            printk("%s: failed to send delayed ARP packet (err %d)\n", 
                    p->ifp->name, i);
        }
        
        kfree(p);
        p = pp ? pp : arp_out_queue.head;
    }
    
    kernel_mutex_unlock(&arp_out_queue.lock);
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
        if(arp_entries[i].ip_addr == 0)
        {
            continue;
        }

        ksprintf(tmp, 64, "%u.%u.%u.%u",
                          ADDR_BYTE(arp_entries[i].ip_addr,  0),
                          ADDR_BYTE(arp_entries[i].ip_addr,  8),
                          ADDR_BYTE(arp_entries[i].ip_addr, 16),
                          ADDR_BYTE(arp_entries[i].ip_addr, 24));
        len = strlen(tmp);

        while(len < 16)
        {
            tmp[len++] = ' ';
            tmp[len] = '\0';
        }

        ksprintf(tmp + len, 64 - len,
                 "0x1       %02x:%02x:%02x:%02x:%02x:%02x",
                 arp_entries[i].hwaddr[0],
                 arp_entries[i].hwaddr[1],
                 arp_entries[i].hwaddr[2],
                 arp_entries[i].hwaddr[3],
                 arp_entries[i].hwaddr[4],
                 arp_entries[i].hwaddr[5]);
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

