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
 *  Helper functions for working with network packets.
 */

#ifndef NET_PACKET_H
#define NET_PACKET_H

#include <errno.h>
#include <kernel/laylaos.h>
#include <mm/kheap.h>

#define ETHER_HLEN              14
#define IPv4_HLEN               20
#define UDP_HLEN                8
#define TCP_HLEN                20

#define PACKET_SIZE_IP(s)       (ETHER_HLEN + IPv4_HLEN + (s))
#define PACKET_SIZE_TCP(s)      (ETHER_HLEN + IPv4_HLEN + TCP_HLEN + (s))
#define PACKET_SIZE_UDP(s)      (ETHER_HLEN + IPv4_HLEN + UDP_HLEN + (s))

#define PACKET_FLAG_BROADCAST   0x01
#define PACKET_FLAG_HDRINCLUDED 0x02    /* for RAW sockets */

/**
 * @struct packet_t
 * @brief The packet_t structure.
 *
 * A structure to represent a network packet.
 */
struct packet_t
{
    uint8_t *data;              /**< data buffer */
    uint8_t *head;              /**< data head */
    uint8_t *end;               /**< data end */
    size_t count;               /**< bytes in buffer */
    int flags;                  /**< flags */
    int refs;                   /**< reference count */
    uint32_t seq, end_seq;      /**< Starting and ending sequence numbers */
    struct netif_t *ifp;        /**< network interface */
    void (*free_packet)(struct packet_t *p);    /**< free function */
    struct packet_t *next;      /**< next packet buffer */
};


/**
 * @brief Allocate packet.
 *
 * Allocate memory for a new network packet.
 *
 * @param   len     requested size
 *
 * @return  pointer to new alloced packet on success, NULL on failure.
 */
STATIC_INLINE struct packet_t *alloc_packet(size_t len)
{
    struct packet_t *p;

    if(!(p = kmalloc(sizeof(struct packet_t) + len)))
    {
        return NULL;
    }

    A_memset(p, 0, sizeof(struct packet_t) + len);
    p->data = ((uint8_t *)p + sizeof(struct packet_t));
    p->head = p->data;
    p->end = p->data + len;
    p->refs = 1;
    p->count = len;

    return p;
}


/**
 * @brief Duplicate a packet.
 *
 * Allocate memory for a new network packet and copy the given packet to it.
 *
 * @param   p       network packet to copy
 *
 * @return  pointer to new alloced packet on success, NULL on failure.
 */
STATIC_INLINE struct packet_t *dup_packet(struct packet_t *p)
{
    struct packet_t *p2;
    size_t tlen = p->end - (uint8_t *)p;

    if(!(p2 = kmalloc(tlen)))
    {
        return NULL;
    }

    A_memcpy(p2, p, tlen);
    p2->head = (uint8_t *)p2 + (p->head - (uint8_t *)p);
    p2->data = (uint8_t *)p2 + (p->data - (uint8_t *)p);
    p2->end = (uint8_t *)p2 + (p->end - (uint8_t *)p);
    p2->refs = 1;
    p2->next = NULL;
    p2->free_packet = NULL;

    return p2;
}


/**
 * @brief Free packet.
 *
 * Free the memory used by the given packet.
 *
 * @param   p       packet to free
 *
 * @return  nothing.
 */
STATIC_INLINE void free_packet(struct packet_t *p)
{
    if(!p)
    {
        return;
    }
    
    p->refs--;
    
    if(p->refs == 0)
    {
        // some modules (e.g. ne2000) have their own packet free functions
        if(p->free_packet)
        {
            p->free_packet(p);
        }
        else
        {
            kfree(p);
        }
    }
}


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
STATIC_INLINE int packet_add_header(struct packet_t *p, size_t hdr_len)
{
    uint8_t *data = p->data - hdr_len;
    uint8_t *not_below = (uint8_t *)p + sizeof(struct packet_t);

    if(data < not_below)
    {
        return -ENOBUFS;
    }

    p->count += hdr_len;
    p->data = data;

    return 0;
}


/*
STATIC_INLINE void reset_packet_header(struct packet_t *p)
{
    p->data = p->end - p->count;
}
*/

#endif      /* NET_PACKET_H */
