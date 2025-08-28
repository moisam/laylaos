/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024, 2025 (c)
 * 
 *    file: route.c
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
 *  \file route.c
 *
 *  Internet route implementation.
 */

#include <kernel/mutex.h>
#include <kernel/net/route.h>
#include <mm/kheap.h>

volatile struct kernel_mutex_t route_lock;
struct rtentry_t route_head = { 0, };

void route_init(void)
{
    init_kernel_mutex(&route_lock);
}

void route_add_ipv4(uint32_t dest, uint32_t gateway, uint32_t netmask,
                    uint8_t flags, uint32_t metric, struct netif_t *ifp)
{
    struct rtentry_t *rt, *newrt;

    if(!(newrt = kmalloc(sizeof(struct rtentry_t))))
    {
        return;
    }

    newrt->dest = dest;
    newrt->gateway = gateway;
    newrt->netmask = netmask;
    newrt->flags = flags;
    newrt->metric = metric;
    newrt->ifp = ifp;
    newrt->next = NULL;

    kernel_mutex_lock(&route_lock);

    for(rt = &route_head; rt->next != NULL; rt = rt->next)
    {
        ;
    }

    rt->next = newrt;

    kernel_mutex_unlock(&route_lock);
}


struct rtentry_t *route_for_ipv4(uint32_t addr)
{
    struct rtentry_t *rt;

    kernel_mutex_lock(&route_lock);

    for(rt = route_head.next; rt != NULL; rt = rt->next)
    {
        if((addr & rt->netmask) == (rt->dest & rt->netmask))
        {
            kernel_mutex_unlock(&route_lock);
            return rt;
        }
    }

    // no match found, try the default gateway
    for(rt = route_head.next; rt != NULL; rt = rt->next)
    {
        if(rt->flags & RT_GATEWAY)
        {
            kernel_mutex_unlock(&route_lock);
            return rt;
        }
    }

    kernel_mutex_unlock(&route_lock);
    return NULL;
}


struct rtentry_t *route_for_ifp(struct netif_t *ifp)
{
    struct rtentry_t *rt;

    kernel_mutex_lock(&route_lock);

    for(rt = route_head.next; rt != NULL; rt = rt->next)
    {
        if(rt->ifp == ifp)
        {
            kernel_mutex_unlock(&route_lock);
            return rt;
        }
    }

    kernel_mutex_unlock(&route_lock);
    return NULL;
}


/*
void route_free_ipv4(uint32_t dest, uint32_t gateway, uint32_t netmask)
{
    struct rtentry_t *rt, *next;

    kernel_mutex_lock(&route_lock);

    for(rt = &route_head; rt->next != NULL; rt = rt->next)
    {
        if(rt->next->dest == dest &&
           rt->next->gateway == gateway &&
           rt->next->netmask == netmask)
        {
            next = rt->next;
            rt->next = next->next;
            kfree(next);
            break;
        }
    }

    kernel_mutex_unlock(&route_lock);
}
*/


void route_free_for_ifp(struct netif_t *ifp)
{
    struct rtentry_t *rt, *next;

    kernel_mutex_lock(&route_lock);

    for(rt = &route_head; rt->next != NULL; rt = rt->next)
    {
        if(rt->next->ifp == ifp)
        {
            next = rt->next;
            rt->next = next->next;
            kfree(next);
        }
    }

    kernel_mutex_unlock(&route_lock);
}

