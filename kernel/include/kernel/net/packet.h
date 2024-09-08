/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: packet.h
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
 *  \file packet.h
 *
 *  Functions and macros for handling network packets.
 */

#ifndef NET_PACKET_H
#define NET_PACKET_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <kernel/net/socket.h>

#define PACKET_LINK             1
#define PACKET_IP               2
#define PACKET_TRANSPORT        3
#define PACKET_RAW              4

#define PACKET_FLAG_BROADCAST   0x01
#define PACKET_FLAG_SACKED      0x02

/**
 * @struct packet_t
 * @brief The packet_t structure.
 *
 * A structure to represent a network packet.
 */
struct packet_t
{
    void *data;                 /**< data buffer */
    void *transport_hdr;        /**< pointer to transport-layer header
                                     (for incoming packets) */
    void *incoming_iphdr;       /**< pointer to IP header
                                     (for incoming packets) */
    size_t count;               /**< bytes in this buffer */
    size_t malloced;            /**< malloced bytes in total */
    int flags;                  /**< flags */
    int transport_flags_saved;
    uint16_t frag;              /**< payload fragmentation info */
    //int refs;                   /**< reference count */
    //struct sockaddr src;        /**< source IP address */
    struct netif_t *ifp;        /**< network interface */
    void (*free_packet)(struct packet_t *p);    /**< free function */
    struct socket_t *sock;
    struct packet_t *next;      /**< next packet buffer */
    
    unsigned long long timestamp;

    union ip_addr_t remote_addr;    /**< remote UDP IP address */
    uint16_t remote_port;           /**< remote UDP port */

    int nfailed;                    /**< failed transmission tries */
};


/**********************************
 * Function prototypes
 **********************************/

/**
 * @brief Allocate packet.
 *
 * Allocate memory for a new network packet.
 *
 * @param   len     requested size
 * @param   type    network layer requesting packet
 *
 * @return  pointer to new alloced packet on success, NULL on failure.
 */
struct packet_t *packet_alloc(size_t len, int type);

/**
 * @brief Free packet.
 *
 * Free the memory used by the given packet.
 *
 * @param   p       packet to free
 *
 * @return  nothing.
 */
void packet_free(struct packet_t *p);

/**
 * @brief Add header.
 *
 * Add space at the front of the packet data to accommodate a new header.
 *
 * @param   p           network packet
 * @param   hdr_len     header size
 *
 * @return  zero on success, -(errno) on failure.
 */
int packet_add_header(struct packet_t *p, size_t hdr_len);

/**
 * @brief Duplicate a packet.
 *
 * Allocate memory for a new network packet and copy the given packet to it.
 *
 * @param   p       network packet to copy
 *
 * @return  pointer to new alloced packet on success, NULL on failure.
 */
struct packet_t *packet_duplicate(struct packet_t *p);

#endif      /* NET_PACKET_H */
