/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
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

#include <sys/socket.h>
#include <netinet/in.h>
#include <kernel/net/packet.h>
#include <kernel/net/stats.h>

struct netif_t;

#define ETHER_HLEN          14
#define ETHER_ADDR_LEN      6

// https://en.wikipedia.org/wiki/EtherType
#define	ETHERTYPE_PUP		0x0200	/* PUP protocol */
#define	ETHERTYPE_IP		0x0800	/* IPv4 protocol */
#define ETHERTYPE_ARP		0x0806	/* Addr. resolution protocol */
#define ETHERTYPE_REVARP	0x8035	/* Reverse Addr. resolution protocol */
#define	ETHERTYPE_IPv6		0x86DD	/* IPv6 protocol */

/**
 * @var ethernet_broadcast
 * @brief Ethernet broadcast address.
 *
 * Pointer to the Ethernet broadcast address.
 */
extern uint8_t ethernet_broadcast[];
  
/**
 * @struct ether_header_t
 * @brief The ether_header_t structure.
 *
 * A structure to represent an Ethernet packet header.
 */
struct ether_header_t
{
    uint8_t dest[ETHER_ADDR_LEN];   /**< destination hardware address */
    uint8_t src[ETHER_ADDR_LEN];    /**< source hardware address */
    uint16_t type;                  /**< header type */
} __attribute__((packed));


/**********************************
 * Function prototypes
 **********************************/

/**
 * @brief Interface attach.
 *
 * Add a network interface to our list of attached interfaces and call
 * dhcp_start() to start DHCP discovery and create a DHCP binding for the
 * given network interface.
 *
 * @param   ifp     network interface
 *
 * @return  zero on success, -(errno) on failure.
 */
int ethernet_attach(struct netif_t *ifp);

/**
 * @brief Ethernet receive.
 *
 * Handle received Ethernet packets.
 *
 * @param   ifp     network interface
 * @param   p       received packet
 *
 * @return  nothing.
 */
void ethernet_receive(struct netif_t *ifp, struct packet_t *p);

/**
 * @brief Ethernet send.
 *
 * Handle sending Ethernet packets.
 *
 * @param   ifp         network interface
 * @param   p           packet to send
 * @param   hwdest      destination Ethernet address
 *
 * @return  zero on success, -(errno) on failure.
 */
int ethernet_send(struct netif_t *ifp, struct packet_t *p, 
                  uint8_t *hwdest);

#endif      /* NET_ETHER_H */
