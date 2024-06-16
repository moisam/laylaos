/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: raw.h
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
 *  \file raw.h
 *
 *  Functions and macros for handling RAW network packets.
 */

#ifndef NET_RAW_H
#define NET_RAW_H

#include <netinet/icmp6.h>

struct socket_raw_t
{
    struct socket_t sock;
    struct icmp6_filter icmp6_filter;
};


/**
 * @var raw_inq
 * @brief RAW in queue.
 *
 * Queue of incoming RAW packets.
 * Defined in raw.c.
 */
extern struct netif_queue_t raw_inq;

/**
 * @var raw_outq
 * @brief RAW out queue.
 *
 * Queue of outgoing RAW packets.
 * Defined in raw.c.
 */
//extern struct netif_queue_t raw_outq;

/**
 * @var raw_sockops
 * @brief RAW socket operations.
 *
 * Pointer to the socket operations structure for the RAW protocol.
 * Defined in raw.c.
 */
extern struct sockops_t raw_sockops;


/*********************************************
 * Functions defined in network/raw.c
 *********************************************/

/**
 * @brief RAW push.
 *
 * Handle sending RAW packets.
 *
 * @param   p           packet to push on outgoing queue
 *
 * @return  zero on success, -(errno) on failure.
 */
int raw_push(struct packet_t *p);

/**
 * @brief RAW receive.
 *
 * Handle received RAW packets.
 *
 * @param   p       received packet
 *
 * @return  zero on success, -(errno) on failure.
 */
int raw_receive(struct packet_t *p);

#endif      /* NET_RAW_H */
