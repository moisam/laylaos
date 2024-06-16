/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: ipv4.h
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
 *  \file ipv4.h
 *
 *  Functions and macros for handling Internet Protocol (IP) v4 packets.
 */

#ifndef NET_IPv4_H
#define NET_IPv4_H

#define IPv4_HLEN               20

// extract header length from IP header
#define GET_IP_HLEN(h)          ((h)->ver_hlen & 0x0F)
#define GET_IP_VER(h)           (((h)->ver_hlen >> 4) & 0x0F)
#define GET_IP_FLAGS(h)         ((h)->offset & ~IP_OFFMASK)
#define GET_IP_OFFSET(h)        ((h)->offset & IP_OFFMASK)

// fragment flags
#define IP_RF           0x8000       // Reserved fragment flag
#define IP_DF           0x4000       // Dont fragment flag
#define IP_MF           0x2000       // More fragments flag
#define IP_OFFMASK      0x1fff       // Mask for fragmenting bits

#define KDEBUG_IPV4_ADDR(a)                     \
    KDEBUG("%3d.%3d.%3d.%3d",                   \
            (a >> 24) & 0xff, (a >> 16) & 0xff, \
            (a >> 8 ) & 0xff, (a >> 0 ) & 0xff);

// avoid circular includes by prototyping packet_t here
struct packet_t;


/**
 * @struct ipv4_header_t
 * @brief The ipv4_header_t structure.
 *
 * A structure to represent an IPv4 packet header.
 */
struct ipv4_hdr_t
{
    uint8_t ver_hlen;           /**< Version / header length */
    uint8_t tos;                /**< Type of service */
    uint16_t len;               /**< Total length */
    uint16_t id;                /**< Identification */
    uint16_t offset;            /**< Fragment offset / flags */
    uint8_t ttl;                /**< Time to live */
    uint8_t proto;              /**< Protocol */
    uint16_t checksum;          /**< Checksum */
    struct in_addr src, dest;   /**< Source and destination IP addresses */
} __attribute__((packed));

struct ipv4_pseudo_hdr_t
{
    struct in_addr src, dest;
    uint8_t zero;
    uint8_t proto;
    uint16_t len;
} __attribute__((packed));


struct ipv4_link_t
{
    struct netif_t *ifp;
    struct in_addr addr;
    struct in_addr netmask;
    struct ipv4_link_t *next;
};

struct ipv4_route_t
{
    struct in_addr dest;
    struct in_addr netmask;
    struct in_addr gateway;
    struct ipv4_link_t *link;
    uint32_t metric;
    struct ipv4_route_t *next;
};


// externs defined in ipv4.c
extern struct ipv4_link_t *ipv4_links;
extern struct ipv4_route_t *ipv4_routes;
extern struct netif_queue_t ipv4_inq;
extern struct netif_queue_t ipv4_outq;


/**************************************
 * Functions defined in ipv4.c
 **************************************/

int ipv4_source_find(struct in_addr *res, struct in_addr *addr);
struct netif_t *ipv4_source_ifp_find(struct in_addr *addr);

struct ipv4_link_t *ipv4_link_find(struct in_addr *addr);
struct ipv4_link_t *ipv4_link_get(struct in_addr *addr);
struct ipv4_link_t *ipv4_link_by_ifp(struct netif_t *ifp);
int ipv4_link_del(struct netif_t *ifp, struct in_addr *addr);
int ipv4_link_add(struct netif_t *ifp, 
                  struct in_addr *addr, struct in_addr *netmask);
int ipv4_cleanup_links(struct netif_t *ifp);

void ipv4_route_set_broadcast_link(struct ipv4_link_t *link);

int ipv4_route_gateway_get(struct in_addr *gateway, struct in_addr *addr);
int ipv4_route_add(struct ipv4_link_t *link, struct in_addr *addr, 
                   struct in_addr *netmask, struct in_addr *gateway, 
                   uint32_t metric);
int ipv4_cleanup_routes(struct ipv4_link_t *link);

int ipv4_push(struct packet_t *p, struct in_addr *dest, uint8_t proto);
int ipv4_receive(struct packet_t *p);
int ipv4_process_out(struct packet_t *p);
void ipv4_init(void);

int ip_push(struct packet_t *p);


/**************************************
 * Functions defined in ipv4_addr.c
 **************************************/

int ipv4_is_multicast(uint32_t addr);
int ipv4_is_broadcast(uint32_t addr);
int ipv4_cmp(struct in_addr *a, struct in_addr *b);

int string_to_ipv4(const char *str, uint32_t *ip);


/**************************************
 * Functions defined in ipv_frag.c
 **************************************/

void ip_fragment_check_expired(void);
void ipv4_process_fragment(struct packet_t *p, 
                           struct ipv4_hdr_t *h, uint8_t proto);

#endif      /* NET_IPv4_H */
