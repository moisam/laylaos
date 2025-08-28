/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
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

//#define __DEBUG

#define __USE_MISC

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>         // IFF_* flags
#include <kernel/net/netif.h>
#include <kernel/net/ipv4.h>
#include <kernel/mutex.h>
#include <fs/procfs.h>
#include <mm/kheap.h>

volatile struct kernel_mutex_t netif_lock;
struct netif_t *netif_list = NULL;
static int last_index = 0;


void netif_init(void)
{
    init_kernel_mutex(&netif_lock);
}


/*
 * Interface attach.
 */
int netif_attach(struct netif_t *ifp)
{
    struct netif_t *tmp;
    
    if(!ifp)
    {
        return -EINVAL;
    }

    /*
    if(ifp->addr == NULL)
    {
        if(!(ifp->addr = kmalloc(sizeof(struct netifaddr_t))))
        {
            return -ENOMEM;
        }

        A_memset(ifp->addr, 0, sizeof(struct netifaddr_t));
    }
    */

    kernel_mutex_lock(&netif_lock);

    // do not reattach the interface if it is already in the list
    for(tmp = netif_list; tmp != NULL; tmp = tmp->next)
    {
        if(tmp == ifp)
        {
            kernel_mutex_unlock(&netif_lock);
            return -EEXIST;
        }
    }
    
    ifp->next = netif_list;
    ifp->index = ++last_index;
    netif_list = ifp;
    kernel_mutex_unlock(&netif_lock);

    return 0;
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

    kernel_mutex_lock(&netif_lock);
    
    for(ifp = netif_list; ifp != NULL && i > 0; ifp = ifp->next, i--)
    {
        ;
    }
    
    kernel_mutex_unlock(&netif_lock);
    return ifp;
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
    
    kernel_mutex_lock(&netif_lock);

    for(ifp = netif_list; ifp != NULL; ifp = ifp->next)
    {
        if(strcmp(name, ifp->name) == 0)
        {
            kernel_mutex_unlock(&netif_lock);
            return ifp;
        }
    }
    
    kernel_mutex_unlock(&netif_lock);
    return NULL;
}


/*
 * Read /proc/net/dev.
 */
size_t get_net_dev_stats(char **buf)
{
    struct netif_t *ifp;
    size_t len, count = 0, bufsz = 1024;
    char tmp[156];
    char *p;

    PR_MALLOC(*buf, bufsz);
    p = *buf;

    ksprintf(p, 156, " Inter- |   Receive                                                |  Transmit\n");
    len = strlen(p);
    count += len;
    p += len;

    ksprintf(p, 156, "  face  |bytes    packets errs drop fifo frame compressed multicast|"
                              "bytes    packets errs drop fifo colls carrier compressed\n");
    len = strlen(p);
    count += len;
    p += len;

    kernel_mutex_lock(&netif_lock);

    for(ifp = netif_list; ifp != NULL; ifp = ifp->next)
    {
        ksprintf(tmp, 156, "%8s: %7u %7u %4u %4u %4u %5u %10u %9u ",
                            ifp->name,
                            ifp->stats.rx_bytes, ifp->stats.rx_packets,
                            ifp->stats.rx_errors, ifp->stats.rx_dropped,
                            0, 0, 0,
                            ifp->stats.multicast);
        len = strlen(tmp);
        ksprintf(tmp + len, 156, "%7u  %7u %4u %4u %4u %5u %7u %10u\n",
                            ifp->stats.tx_bytes, ifp->stats.tx_packets,
                            ifp->stats.tx_errors, ifp->stats.tx_dropped,
                            0, 0, 0, 0);
        len = strlen(tmp);

        if(count + len >= bufsz)
        {
            char *tmp;
            if(!(tmp = (char *)krealloc(*buf, bufsz * 2)))
            {
                kernel_mutex_unlock(&netif_lock);
                return count;
            }

            bufsz *= 2;
            *buf = tmp;
            p = *buf + count;
        }

        count += len;
        strcpy(p, tmp);
        p += len;
    }

    kernel_mutex_unlock(&netif_lock);
    return count;
}

