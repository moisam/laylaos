/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: ether.h
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
 *  \file ether.h
 *
 *  Functions and macros for handling Ethernet packets.
 */

#ifndef NET_ETHER_H
#define NET_ETHER_H

#include <netinet/in.h>

// avoid circular includes by prototyping these here
struct netif_t;
struct packet_t;

#define ETHER_HLEN          14
#define ETHER_ADDR_LEN      6

// https://en.wikipedia.org/wiki/EtherType
#define	ETHERTYPE_PUP		0x0200	/* PUP protocol */
#define	ETHERTYPE_IP		0x0800	/* IPv4 protocol */
#define ETHERTYPE_ARP		0x0806	/* Addr. resolution protocol (ARP) */
#define ETHERTYPE_REVARP	0x8035	/* Reverse ARP */
#define	ETHERTYPE_IPv6		0x86DD	/* IPv6 protocol */

#define COPY_ETHER_ADDR(d, s)                           \
    for(int z = 0; z < ETHER_ADDR_LEN; z++) (d)[z] = (s)[z];

#define SET_ETHER_ADDR_BYTES(a, b)                      \
    for(int z = 0; z < ETHER_ADDR_LEN; z++) a[z] = b;

#define KDEBUG_ETHER_ADDR(a)                            \
    KDEBUG("%02x.%02x.%02x.%02x.%02x.%02x",             \
           a[0], a[1], a[2], a[3], a[4], a[5]);

#define ARP_REQUEST         1
#define ARP_REPLY           2


/**
 * @struct ether_addr_t
 * @brief The ether_addr_t structure.
 *
 * A structure to represent an Ethernet address.
 */
struct ether_addr_t
{
    unsigned char addr[ETHER_ADDR_LEN];     /**< Ethernet hardware address */
} __attribute__((packed));
  
/**
 * @struct ether_header_t
 * @brief The ether_header_t structure.
 *
 * A structure to represent an Ethernet packet header.
 */
struct ether_header_t
{
    struct ether_addr_t dest;   /**< destination hardware address */
    struct ether_addr_t src;    /**< source hardware address */
    uint16_t type;              /**< header type */
} __attribute__((packed));

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
    struct ether_addr_t sha;                /**< Source hardware address */
    struct in_addr spa;                     /**< Source protocol address */
    struct ether_addr_t tha;                /**< Target hardware address */
    struct in_addr tpa;                     /**< Target protocol address */
} __attribute__((packed));


/**
 * @var ethernet_inq
 * @brief Ethernet in queue.
 *
 * Queue of incoming Ethernet packets.
 */
extern struct netif_queue_t ethernet_inq;

/**
 * @var ethernet_outq
 * @brief Ethernet out queue.
 *
 * Queue of outgoing Ethernet packets.
 */
extern struct netif_queue_t ethernet_outq;


/**********************************
 * Ethernet function prototypes
 **********************************/

/**
 * @brief Interface add.
 *
 * Add a network interface to our list of attached interfaces and call
 * dhcp_start() to start DHCP discovery and create a DHCP binding for the
 * given network interface.
 *
 * @param   ifp     network interface
 *
 * @return  zero on success, -(errno) on failure.
 */
int ethernet_add(struct netif_t *ifp);

/**
 * @brief Ethernet receive.
 *
 * Handle received Ethernet packets.
 *
 * @param   p       received packet
 *
 * @return  zero on success, -(errno) on failure.
 */
int ethernet_receive(struct packet_t *p);

/**
 * @brief Ethernet send.
 *
 * Handle sending Ethernet packets.
 *
 * @param   p           packet to send
 *
 * @return  zero on success, -(errno) on failure.
 */
int ethernet_send(struct packet_t *p);


/**********************************
 * ARP function prototypes
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
 * @brief ARP request.
 *
 * Broadcast an ARP request.
 *
 * @param   ifp             network interface
 * @param   dest            destination IP address
 *
 * @return  nothing.
 */
void arp_request(struct netif_t *ifp, struct in_addr *dest);

/**
 * @brief ARP get.
 *
 * Get the Ethernet address of the detination IP address for the given packet
 * from the ARP table.
 *
 * @param   p       IP packet
 *
 * @return  Ethernet address on success, NULL on error.
 */
struct ether_addr_t *arp_get(struct packet_t *p);

/**
 * @brief ARP postpone.
 *
 * Postpone sending ARP packets.
 *
 * @param   p       packet to postpone
 *
 * @return  nothing.
 */
void arp_postpone(struct packet_t *p);

/**
 * @brief ARP receive.
 *
 * Handle received ARP packets.
 *
 * @param   p       received packet
 *
 * @return  nothing.
 */
void arp_receive(struct packet_t *p);

/**
 * @brief Update ARP entry.
 *
 * Check if an entry exists in the ARP table with the given IP address and
 * Ethernet address. Update the entry ONLY if it exists. That is, don't create
 * a new entry.
 *
 * @param   ifp             network interface
 * @param   ip              IP address
 * @param   eth             ethernet address
 *
 * @return  1 if an entry is found and updated, 0 otherwise.
 */
void arp_update_entry(struct netif_t *ifp, uint32_t ip, 
                      struct ether_addr_t *eth);

#endif      /* NET_ETHER_H */
