/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: netif.c
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
 *  \file netif.c
 *
 *  The network interface card driver.
 *
 *  The driver's code is split between these files:
 *    - netif.c => general driver functions
 *    - netif_ioctl.c => driver ioctl()function
 */

#define __USE_MISC

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>         // IFF_* flags
#include <kernel/laylaos.h>
#include <kernel/timer.h>
#include <kernel/net/netif.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>
#include <kernel/net/icmp6.h>
#include <mm/kheap.h>
#include <fs/procfs.h>

struct netif_t *netif_list = NULL;
struct kernel_mutex_t netif_lock = { 0, };
int last_index = 0;


/*
 * Interface attach.
 */
int netif_add(struct netif_t *ifp)
{
    struct netif_t *tmp;
    struct ipv6_link_t *link;
    int i;
    struct in6_addr linklocal =
    {
        .s6_addr =
        {
            0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
            0xaa, 0xaa, 0xaa, 0xff, 0xfe, 0xaa, 0xaa, 0xaa
        }
    };
    
    if(!ifp)
    {
        return -EINVAL;
    }
    
    if(!ifp->mtu)
    {
        ifp->mtu = 1500;
    }
    
    for(i = 0; i < ETHER_ADDR_LEN; i++)
    {
        // break if we find a non-zero byte
        if(ifp->ethernet_addr.addr[i])
        {
            break;
        }
    }
    
    if(i == ETHER_ADDR_LEN)
    {
        // all zeroes - MAC address not set
        if((i = netif_ipv6_random_ll(ifp)) != 0)
        {
            return i;
        }
    }
    else
    {
        // MAC address set
        if((i = ipv6_link_add_local(ifp, &linklocal, &link)) != 0)
        {
            return i;
        }
    }

    kernel_mutex_lock(&netif_lock);

    // do not reattach the interface if it is already in the list
    for(tmp = netif_list; tmp != NULL; tmp = tmp->next)
    {
        if(tmp == ifp)
        {
            kernel_mutex_unlock(&netif_lock);
            return 0;
        }
    }
    
    ifp->next = netif_list;
    ifp->index = ++last_index;
    netif_list = ifp;

    kernel_mutex_unlock(&netif_lock);

    return 0;
}


int netif_ipv6_random_ll(struct netif_t *ifp)
{
    int i;
    struct in6_addr linklocal =
    {
        .s6_addr =
        {
            0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
            0xaa, 0xaa, 0xaa, 0xff, 0xfe, 0xaa, 0xaa, 0xaa
        }
    };

    struct in6_addr netmask =
    {
        .s6_addr =
        {
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        }
    };

    if(strcmp(ifp->name, "lo0"))
    {
        do
        {
            // privacy extension + unset universal/local and 
            // individual/group bit
            uint32_t r1 = genrand_int32();
            uint32_t r2 = genrand_int32();
            
            linklocal.s6_addr[8] = ((r1 & 0xff) & (uint8_t)~3);
            linklocal.s6_addr[9] = (r1 >> 8) & 0xff;
            linklocal.s6_addr[10] = (r1 >> 16) & 0xff;
            linklocal.s6_addr[11] = (r1 >> 24) & 0xff;
            linklocal.s6_addr[12] = r2 & 0xff;
            linklocal.s6_addr[13] = (r2 >> 8) & 0xff;
            linklocal.s6_addr[14] = (r2 >> 16) & 0xff;
            linklocal.s6_addr[15] = (r2 >> 24) & 0xff;
        } while(ipv6_link_get(&linklocal));
        
        if((i = ipv6_link_add(ifp, &linklocal, &netmask, NULL)) != 0)
        {
            return i;
        }
    }
    
    return 0;
}


/*
 * Get the network interface with the given name.
 */
struct netif_t *netif_by_name(char *name)
{
    struct netif_t *ifp;
    
    if(!name || !*name)
    {
        return NULL;
    }
    
    for(ifp = netif_list; ifp != NULL; ifp = ifp->next)
    {
        if(strcmp(name, ifp->name) == 0)
        {
            return ifp;
        }
    }
    
    return NULL;
}


/*
 * Get the network interface with the given index.
 */
struct netif_t *netif_by_index(int i)
{
    struct netif_t *ifp;
    
    if(i-- == 0)
    {
        return NULL;
    }
    
    for(ifp = netif_list; ifp != NULL && i > 0; ifp = ifp->next, i--)
    {
        ;
    }
    
    return ifp;
}


/*
 * Read /proc/net/dev.
 */
size_t get_net_dev_stats(char **buf)
{
    struct netif_t *ifp;
    size_t len, count = 0, bufsz = 1024;
    char tmp[128];
    char *p;

    PR_MALLOC(*buf, bufsz);
    p = *buf;

    ksprintf(p, 128, " Inter- |   Receive                          |  Transmit\n");
    len = strlen(p);
    count += len;
    p += len;

    ksprintf(p, 128, "  face  |bytes    packets errs drop multicast|"
                     "bytes   packets errs drop\n");
    len = strlen(p);
    count += len;
    p += len;

    for(ifp = netif_list; ifp != NULL; ifp = ifp->next)
    {
        ksprintf(tmp, 128, "%8s: %7u %7u %4u %4u %9u %7u %7u %4u %4u\n",
                            ifp->name,
                            ifp->stats.rx_bytes, ifp->stats.rx_packets,
                            ifp->stats.rx_errors, ifp->stats.rx_dropped,
                            ifp->stats.multicast,
                            ifp->stats.tx_bytes, ifp->stats.tx_packets,
                            ifp->stats.tx_errors, ifp->stats.tx_dropped);
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

    return count;
}

