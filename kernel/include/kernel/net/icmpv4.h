/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: icmpv4.h
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
 *  \file icmpv4.h
 *
 *  Functions and macros for handling Internet Control Message Protocol 
 *  (ICMP) version 4 packets.
 */

#ifndef NET_ICMPv4_H
#define NET_ICMPv4_H

#include <netinet/in.h>

#define ICMP_HLEN                       8

// Message types
#define ICMP_MSG_ECHOREPLY              0  // Echo reply
#define ICMP_MSG_DESTUNREACH            3  // Destination unreachable
#define ICMP_MSG_SRCQUENCH              4  // Source quench
#define ICMP_MSG_REDIRECT               5  // Redirect
#define ICMP_MSG_ECHO                   8  // Echo
#define ICMP_MSG_TIMEEXCEEDED           11 // Time exceeded
#define ICMP_MSG_PARAMPROBLEM           12 // Parameter problem
#define ICMP_MSG_TIMESTAMP              13 // Timestamp
#define ICMP_MSG_TIMESTAMPREPLY         14 // Timestamp reply
#define ICMP_MSG_INFOREQUEST            15 // Information request
#define ICMP_MSG_INFOREQUESTREPLY       16 // Information reply

// Error codes for DESTUNREACH messages (type = 3)
#define ICMP_DESTUNREACH_NET            0  // Net unreachable
#define ICMP_DESTUNREACH_HOST           1  // Host unreachable
#define ICMP_DESTUNREACH_PROTO          2  // Protocol unreachable
#define ICMP_DESTUNREACH_PORT           3  // Port unreachable
#define ICMP_DESTUNREACH_FRAG           4  // Fragmentation needed and DF set
#define ICMP_DESTUNREACH_SRCFAIL        5  // Source route failed

// Destination Unreachable Message header
// same as the Source Quench Message header
struct icmp_du_header_t
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint32_t unused;
    // These fields are followed by:
    // Internet Header + 64 bits of Original Data Datagram
} __attribute__((packed));

// Time Exceeded Message header
struct icmp_te_header_t
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    // These fields are followed by:
    // Internet Header + 64 bits of Original Data Datagram
} __attribute__((packed));

// Parameter Problem Message header
struct icmp_pp_header_t
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint8_t pointer;
    uint8_t unused[3];
    // These fields are followed by:
    // Internet Header + 64 bits of Original Data Datagram
} __attribute__((packed));

// Redirect Message header
struct icmp_redirect_header_t
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint32_t gateway;
    // These fields are followed by:
    // Internet Header + 64 bits of Original Data Datagram
} __attribute__((packed));

// Echo or Echo Reply Message header
// same as Information Request or Information Reply Message header, except
// the latter doesn't include data from the original packet
struct icmp_echo_header_t
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id, seq;
    // These fields are followed by data
} __attribute__((packed));

// Timestamp or Timestamp Reply Message header
struct icmp_timestamp_header_t
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint32_t timestamp_originate, timestamp_receive, timestamp_transmit;
    // These fields are followed by data
} __attribute__((packed));


/**
 * \def icmpv4_dest_unreach
 * Send a Destination Unreachable ICMP message
 */
#define icmpv4_dest_unreach(p, code)    icmpv4_send(p, ICMP_MSG_DESTUNREACH, code)

/**
 * \def icmpv4_time_exceeded
 * Send a Time Exceeded ICMP message
 */
#define icmpv4_time_exceeded(p, code)   icmpv4_send(p, ICMP_MSG_TIMEEXCEEDED, code)


/**********************************
 * Function prototypes
 **********************************/

/**
 * @brief ICMP input.
 *
 * Handle incoming ICMP packets.
 *
 * @param   p       received packet
 *
 * @return  zero on success, -(errno) on failure.
 */
int icmpv4_input(struct packet_t *p);

/**
 * @brief ICMP send.
 *
 * Handle sending ICMP packets.
 *
 * @param   p       packet to send
 * @param   type    ICMP packet type
 * @param   code    ICMP packet code
 *
 * @return  zero on success, -(errno) on failure.
 */
void icmpv4_send(struct packet_t *p, uint8_t type, uint8_t code);

#endif      /* NET_ICMPv4_H */
