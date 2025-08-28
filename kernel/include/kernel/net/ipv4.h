/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
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

#include <netinet/ip.h>

#define IPv4_HLEN               20

// extract header length from IP header
#define GET_IP_FLAGS(h)         ((h)->offset & ~IP_OFFMASK)
#define GET_IP_OFFSET(h)        ((h)->offset & IP_OFFMASK)
#define SET_IP_OFFSET(h, o)     (h)->offset = GET_IP_FLAGS(h) | (o)

#define IPv4_HDR(p)             (struct ipv4_hdr_t *)((p)->head + ETHER_HLEN)

// Macros to enqueue/dequeue IP fragments
#define IPFRAG_ENQUEUE(q, p)                \
{                                           \
    (p)->next = NULL;                       \
    if((q)->tail == NULL)                   \
        (q)->head = (p);                    \
    else                                    \
        (q)->tail->next = (p);              \
    (q)->tail = (p);                        \
    (q)->count++;                           \
}

#define IPFRAG_DEQUEUE(q, p)                \
{                                           \
    (p) = (q)->head;                        \
    if(p)                                   \
    {                                       \
        (q)->head = (p)->next;              \
        if((q)->head == NULL)               \
            (q)->tail = NULL;               \
        (p)->next = NULL;                   \
        (q)->count--;                       \
    }                                       \
}

#define IPFRAG_FULL(q)                      ((q)->count >= (q)->max)


/**
 * @struct ipv4_header_t
 * @brief The ipv4_header_t structure.
 *
 * A structure to represent an IPv4 packet header.
 */
struct ipv4_hdr_t
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t hlen:4;             /**< Header length */
    uint8_t ver:4;              /**< Version */
#else
    uint8_t ver:4;              /**< Version */
    uint8_t hlen:4;             /**< Header length */
#endif
    uint8_t tos;                /**< Type of service */
    uint16_t tlen;              /**< Total length */
    uint16_t id;                /**< Identification */
    uint16_t offset;            /**< Fragment offset / flags */
    uint8_t ttl;                /**< Time to live */
    uint8_t proto;              /**< Protocol */
    uint16_t checksum;          /**< Checksum */
    uint32_t src, dest;         /**< Source and destination IP addresses */
    uint8_t data[];             /**< Data payload */
} __attribute__((packed));

/**
 * @struct ip_timestamp_t
 * @brief The ip_timestamp_t structure.
 *
 * A structure to represent an IP Timestamp option.
 */
struct ip_timestamp_t
{
    uint8_t code;		/**< IPOPT_TS */
    uint8_t len;		/**< size of structure (variable) */
    uint8_t ptr;		/**< index of current entry */

#define TIMESTAMP_FLAGS(x)              ((x)->flags & 0x0F)
#define TIMESTAMP_OVERFLOW(x)           (((x)->flags & 0xF0) >> 4)
#define TIMESTAMP_OVERFLOW_SET(x, o)    \
        (x)->flags = (TIMESTAMP_FLAGS(x) | ((o) << 4))
    uint8_t flags;      /**< flags */

    union ipt_timestamp_t
    {
        uint32_t ipt_time[1];   /**< time */

        struct ipt_ta
        {
            struct in_addr ipt_addr;    /**< IP address */
            uint32_t ipt_time;          /**< ipt time */
        } ipt_ta[1];
    } ipt_timestamp;
} __attribute__((packed));

/**
 * @struct ipv4_pseudo_hdr_t
 * @brief The ipv4_pseudo_hdr_t structure.
 *
 * A structure to represent an IPv4 pseudo-header.
 */
struct ipv4_pseudo_hdr_t
{
    uint32_t src, dest;
    uint8_t zero;
    uint8_t proto;
    uint16_t len;
} __attribute__((packed));

/**
 * @struct ip_fraglist_t
 * @brief The ip_fraglist_t structure.
 *
 * A structure to represent an IP fragment.
 */
struct ipv4_fraglist_t
{
    struct packet_t *plist;     /**< List of packets containing fragments */
    uint16_t id;                /**< Identification */
    uint8_t proto;              /**< Protocol */

#define	IPFRAGTTL	        60		/* time to live for frags, slowhz */
    uint8_t ttl;                /**< Time to live */

    uint32_t src, dest;         /**< Source and destination IP addresses */
    struct ipv4_fraglist_t *next; /**< Next fragment in list */
};


/**********************************
 * Functions defined in ipv4.c
 **********************************/

/**
 * @brief Initialize IP.
 *
 * Initialize the Internet Protocol (IP) layer.
 *
 * @return  nothing.
 */
void ip_init(void);

/**
 * @brief IPv4 receive.
 *
 * Process incoming IPv4 packet.
 *
 * @param   p               pointer to packet
 *
 * @return  nothing.
 */
void ipv4_recv(struct packet_t *p);

/**
 * @brief IPv4 send.
 *
 * Handle sending IPv4 packets.
 *
 * @param   p           packet to send
 * @param   src         source IP address
 * @param   dest        destination IP address
 * @param   proto       protocol
 * @param   ttl         time to live
 *
 * @return  zero on success, -(errno) on failure.
 */
int ipv4_send(struct packet_t *p, uint32_t src, uint32_t dest, 
                                  uint8_t proto, uint8_t ttl);

#endif      /* NET_IPv4_H */
