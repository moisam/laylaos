/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: arp.h
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
 *  \file arp.h
 *
 *  Functions and macros for working with the Address Resolution Protocol (ARP).
 */

#ifndef NET_ARP_H
#define NET_ARP_H

#include "ether.h"

#define ARP_REQUEST         1
#define ARP_REPLY           2

// Macros to enqueue/dequeue ARP packets
#define ARP_ENQUEUE(q, p)                   \
{                                           \
    (p)->next = NULL;                       \
    if((q)->tail == NULL)                   \
        (q)->head = (p);                    \
    else                                    \
        (q)->tail->next = (p);              \
    (q)->tail = (p);                        \
    (q)->count++;                           \
}

#define ARP_DEQUEUE(q, p)                   \
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

#define ARP_FULL(q)                         ((q)->count >= (q)->max)

/**
 * @struct arp_header_t
 * @brief The arp_header_t structure.
 *
 * A structure to represent an ARP packet header.
 * RFC 826 defines the structure of an ARP packet.
 */
struct arp_header_t
{
    struct ether_header_t ether_header;     /**< Ethernet header */
    uint16_t hwtype;                        /**< Hardware type */
    uint16_t proto;                         /**< Protocol type */
    //unsigned short protolen;              // Protocol address length */
    uint8_t hwlen;                          /**< Hardware address length */
    uint8_t protolen;                       /**< Protocol address length */
    uint16_t opcode;                        /**< Opcode */
    uint8_t sha[ETHER_ADDR_LEN];            /**< Source hardware address */
    uint32_t spa;                           /**< Source protocol address */
    uint8_t tha[ETHER_ADDR_LEN];            /**< Target hardware address */
    uint32_t tpa;                           /**< Target protocol address */
} __attribute__((packed));


/**********************************
 * Function prototypes
 **********************************/

/**
 * @brief Initialize ARP.
 *
 * Initialize the Address Resolution Protocol (ARP) layer.
 *
 * @return  nothing.
 */
void arp_init(void);

/**
 * @brief ARP resolution.
 *
 * Perform an ARP address resolution. If the address is resolved, the function
 * returns 1 and the caller can carry on to send the packet. If the function
 * fails to resolve the address, it returns 0 and the packet is queued for 
 * delayed send (therefore the caller should notfree the packet).
 *
 * @param   rt      network route
 * @param   addr    IP address to resolve
 * @param   dest    destination Ethernet address is returned here
 *
 * @return  1 if ARP resolution succeeds, 0 otherwise.
 */
int arp_resolve(struct rtentry_t *rt, uint32_t addr, uint8_t *dest);

/**
 * @brief ARP request.
 *
 * Broadcast an ARP request.
 *
 * @param   ifp             network interface
 * @param   src             source IP address
 * @param   dest            destination IP address
 *
 * @return  nothing.
 */
void arp_request(struct netif_t *ifp, uint32_t src, uint32_t dest);

/**
 * @brief ARP queue.
 *
 * Queue an ARP request for later.
 *
 * @param   ifp             network interface
 * @param   p               pointer to packet
 * @param   ipaddr          destination IP address
 *
 * @return  zero on success, -(errno) on failure.
 */
int arp_queue(struct netif_t *ifp, struct packet_t *p, uint32_t ipaddr);

/**
 * @brief ARP receive.
 *
 * Process incoming ARP packet.
 *
 * @param   p               pointer to packet
 *
 * @return  nothing.
 */
void arp_recv(struct packet_t *p);

/**
 * @brief Update ARP entry.
 *
 * Check if an entry exists in the ARP table with the given IP address and
 * Ethernet address. Update the entry ONLY if it exists. That is, don't create
 * a new entry.
 *
 * @param   ifp     network interface
 * @param   ip      IP address
 * @param   eth     ethernet address
 *
 * @return  1 if an entry is found and updated, 0 otherwise.
 */
int update_arp_entry(struct netif_t *ifp, uint32_t ip, uint8_t *eth);

/**
 * @brief Add ARP entry.
 *
 * Add a new ARP entry to the ARP table.
 *
 * @param   ifp     network interface
 * @param   ip      IP address
 * @param   eth     ethernet address
 *
 * @return  nothing.
 */
void add_arp_entry(struct netif_t *ifp, uint32_t ip, uint8_t *eth);

/**
 * @brief Set ARP entry expiry.
 *
 * Set the expiry tick count for an ARP entry.
 *
 * @param   ip      IP address
 * @param   expiry  tick count
 *
 * @return  nothing.
 */
void arp_set_expiry(uint32_t ip, unsigned long long expiry);

/**
 * @brief Remove ARP entry.
 *
 * Remove the given ARP entry from the ARP table.
 *
 * @param   ip      IP address
 *
 * @return  nothing.
 */
void remove_arp_entry(uint32_t ip);

/**
 * @brief Get ARP entry.
 *
 * Check if an entry exists in the ARP table with the given IP address and
 * return its Ethernet address.
 *
 * @param   ip      IP address
 * @param   eth     Ethernet address will be stored here
 *
 * @return  1 if an entry is found, 0 otherwise.
 */
int arp_to_eth(uint32_t ip, uint8_t *eth);

#endif      /* NET_ARP_H */
