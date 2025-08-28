/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024, 2025 (c)
 * 
 *    file: route.h
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
 *  \file route.h
 *
 *  Functions and macros for working with internet routes.
 */

#ifndef NET_ROUTE_H
#define NET_ROUTE_H

#include <kernel/net/netif.h>

#define RT_LOOPBACK         0x01
#define RT_GATEWAY          0x02
#define RT_HOST             0x04

struct rtentry_t
{
    uint32_t dest;
    uint32_t gateway;
    uint32_t netmask;
    uint32_t flags;
    uint32_t metric;
    struct netif_t *ifp;
    struct rtentry_t *next;
};


extern volatile struct kernel_mutex_t route_lock;
extern struct rtentry_t route_head;


void route_init(void);

void route_add_ipv4(uint32_t dest, uint32_t gateway, uint32_t netmask,
                    uint8_t flags, uint32_t metric, struct netif_t *ifp);

struct rtentry_t *route_for_ipv4(uint32_t addr);
struct rtentry_t *route_for_ifp(struct netif_t *ifp);

void route_free_for_ifp(struct netif_t *ifp);

#endif      /* NET_ROUTE_H */
