/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: udp.h
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
 *  \file udp.h
 *
 *  Functions and macros for handling User Datagram Protocol (UDP) packets.
 */

#ifndef NET_UDP_H
#define NET_UDP_H

#include <stdint.h>
#include <kernel/net/netif.h>

#define UDP_HLEN                8

struct udp_hdr_t
{
    uint16_t srcp;         // Source port number 
    uint16_t destp;        // Destination port number
    uint16_t len;          // Length
    uint16_t checksum;     // Checksum
} __attribute__((packed));


// externs defined in udp.c
extern struct netif_queue_t udp_inq;
extern struct netif_queue_t udp_outq;
extern struct sockops_t udp_sockops;


/*********************************************
 * Functions defined in network/udp.c
 *********************************************/

int udp_receive(struct packet_t *p);
int udp_push(struct packet_t *p);
int udp_process_out(struct packet_t *p);


/*********************************************
 * Functions defined in network/sockets/udp.c
 *********************************************/

int socket_udp_receive(struct sockport_t *sp, struct packet_t *p);

#endif      /* NET_UDP_H */
