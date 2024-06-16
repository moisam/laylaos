/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: ipv6.h
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
 *  \file ipv6.h
 *
 *  Functions and macros for handling Internet Protocol (IP) v6 packets.
 */

#ifndef NET_IPv6_H
#define NET_IPv6_H

#define IPv6_HLEN               (sizeof(struct ipv6_hdr_t))
#define IPv6_MIN_MTU            1280
#define IPv6_OPTLEN(x)          ((uint16_t)(((x + 1) << 3)))

#define IPV6_ZERO_SET(a)                        \
    {                                           \
        uint8_t *p = (uint8_t *)a;              \
        for(int x = 0; x < 16; x++)             \
            *p++ = '\0';                        \
    }

#define IPV6_COPY(a, b)                         \
    {                                           \
        uint8_t *p1 = (uint8_t *)a;             \
        const uint8_t *p2 = (const uint8_t *)b; \
        for(int x = 0; x < 16; x++)             \
            *p1++ = *p2++;                      \
    }

#define KDEBUG_IPV6_ADDR(a)                     \
    {                                           \
        for(int x = 0; x < 15; x++)             \
            KDEBUG("%02x:", a[x]);              \
        KDEBUG("%02x", a[15]);                  \
    }

// times in ticks, not msecs
#define ND_REACHABLE_TIME           (30000 / MSECS_PER_TICK)
#define ND_RETRANS_TIMER            (1000 / MSECS_PER_TICK)
#define ND_DELAY_FIRST_PROBE_TIME   (5000 / MSECS_PER_TICK)

// avoid circular includes by prototyping packet_t here
struct packet_t;

/**
 * @struct ipv6_header_t
 * @brief The ipv6_header_t structure.
 *
 * A structure to represent an IPv6 packet header.
 */
struct ipv6_hdr_t
{
    uint32_t vtf;
    uint16_t len;
    uint8_t proto;
    uint8_t ttl;
    struct in6_addr src, dest;
} __attribute__((packed));


struct ipv6_pseudo_hdr_t
{
    struct in6_addr src, dest;
    uint32_t len;
    uint8_t zero[3];
    uint8_t proto;
} __attribute__((packed));


struct ipv6_exthdr_t
{
    uint8_t next_hdr;
    
    union
    {
        struct
        {
            uint8_t len;
        } hopbyhop;

        struct
        {
            uint8_t len;
        } destopt;

        struct
        {
            uint8_t len;
            uint8_t routtype;
            uint8_t segleft;
        } routing;

        struct
        {
            uint8_t res;
            uint8_t om[2];
            uint8_t id[4];
        } frag;
    } ext;
} __attribute__((packed));


struct ipv6_link_t
{
    struct netif_t *ifp;
    struct in6_addr addr;
    struct in6_addr netmask;
    uint8_t is_tentative;
    uint8_t is_duplicate;
    uint16_t dup_detect_retrans;
    unsigned long long dad_expiry;
    unsigned long long link_expiry;
    struct ipv6_link_t *next;
};

struct ipv6_route_t
{
    struct in6_addr dest;
    struct in6_addr netmask;
    struct in6_addr gateway;
    struct ipv6_link_t *link;
    uint32_t metric;
    struct ipv6_route_t *next;
};

struct ipv6_nd_hostvars_t
{
    uint8_t routing, hop_limit;
    unsigned long long base_time, reachable_time, retrans_time;
};


// externs defined in ipv6.c
extern struct ipv6_link_t *ipv6_links;
extern struct ipv6_route_t *ipv6_routes;
extern struct netif_queue_t ipv6_inq;
extern struct netif_queue_t ipv6_outq;

// externs defined in ipv6_addr.c
extern const uint8_t IPv6_ANY[];
extern const uint8_t IPv6_LOCALHOST[];


/**************************************
 * Functions defined in ipv6.c
 **************************************/

int ipv6_source_find(struct in6_addr *res, struct in6_addr *addr);
struct netif_t *ipv6_source_ifp_find(struct in6_addr *addr);

struct ipv6_link_t *ipv6_link_get(struct in6_addr *addr);
struct ipv6_link_t *ipv6_linklocal_get(struct netif_t *ifp);
struct ipv6_link_t *ipv6_sitelocal_get(struct netif_t *ifp);
struct ipv6_link_t *ipv6_link_is_tentative(struct in6_addr *addr);
struct ipv6_link_t *ipv6_link_by_ifp(struct netif_t *ifp);
struct ipv6_link_t *ipv6_link_by_ifp_next(struct netif_t *ifp, 
                                          struct ipv6_link_t *last);
struct ipv6_link_t *ipv6_prefix_configured(struct in6_addr *prefix);

int ipv6_link_del(struct netif_t *ifp, struct in6_addr *addr);
int ipv6_link_add(struct netif_t *ifp, 
                  struct in6_addr *addr, struct in6_addr *netmask,
                  struct ipv6_link_t **res);
int ipv6_link_add_local(struct netif_t *ifp, 
                        struct in6_addr *prefix, struct ipv6_link_t **res);

int ipv6_push(struct packet_t *p, 
              struct in6_addr *dest, struct in6_addr *src,
              uint8_t proto, int is_dad);

int ipv6_receive(struct packet_t *p);
int ipv6_process_out(struct packet_t *p);
void ipv6_init(void);

int ipv6_route_add(struct ipv6_link_t *link, 
                   struct in6_addr *addr, struct in6_addr *netmask,
                   struct in6_addr *gateway, uint32_t metric);
int ipv6_route_gateway_get(struct in6_addr *gateway, struct in6_addr *addr);
void ipv6_router_down(struct in6_addr *addr);

void ipv6_ifp_routing_enable(struct netif_t *ifp);


/**************************************
 * Functions defined in ipv6_addr.c
 **************************************/

int ipv6_is_unspecified(const uint8_t addr[16]);
int ipv6_is_global(const uint8_t addr[16]);
int ipv6_is_linklocal(const uint8_t addr[16]);
int ipv6_is_sitelocal(const uint8_t addr[16]);
int ipv6_is_uniquelocal(const uint8_t addr[16]);
int ipv6_is_localhost(const uint8_t addr[16]);
int ipv6_is_multicast(const uint8_t addr[16]);
int ipv6_is_unicast(struct in6_addr *addr);
int ipv6_is_solnode_multicast(struct netif_t *ifp, const uint8_t addr[16]);
int ipv6_is_allhosts_multicast(const uint8_t addr[16]);

int ipv6_cmp(struct in6_addr *a, struct in6_addr *b);
int string_to_ipv6(const char *str, uint8_t *ip);


/**************************************
 * Functions defined in ipv6_nd.c
 **************************************/

struct ether_addr_t *ipv6_get_neighbor(struct packet_t *p);
void ipv6_nd_postpone(struct packet_t *p);
int ipv6_nd_recv(struct packet_t *p);
void ipv6_nd_check_expired(void);


/**************************************
 * Functions defined in ipv_frag.c
 **************************************/
void ipv6_process_fragment(struct packet_t *p, 
                           struct ipv6_exthdr_t *h, uint8_t proto);

#endif      /* NET_IPv6_H */
