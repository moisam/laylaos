/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: icmp4.h
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
 *  \file icmp4.h
 *
 *  Functions and macros for handling Internet Control Message Protocol 
 *  (ICMP) v4 packets.
 */

#ifndef NET_ICMP4_H
#define NET_ICMP4_H

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

// Error codes for TIMEEXCEEDED messages (type = 11)
#define ICMP_TIMEEXCEEDED_INTRANS       0
#define ICMP_TIMEEXCEEDED_REASSEMBLY    1


struct icmp4_hdr_t
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    
    union
    {
        uint8_t ptr;
        struct in_addr gwaddr;
        
        struct
        {
            uint16_t id, seq;
        } idseq;
        
        uint32_t null;

        struct
        {
            uint16_t null, nmtu;
        } pmtu;

        struct
        {
            uint8_t numgw, wpa;
            uint16_t lifetime;
        } rta;
    } hun;
} __attribute__((packed));


// externs defined in icmp4.c
extern struct netif_queue_t icmp4_inq;


/**************************************
 * Functions defined in icmp4.c
 **************************************/

int icmp4_receive(struct packet_t *p);
int icmp4_param_problem(struct packet_t *p, uint8_t problem);
int icmp4_port_unreachable(struct packet_t *p);
int icmp4_proto_unreachable(struct packet_t *p);
int icmp4_dest_unreachable(struct packet_t *p);
int icmp4_packet_too_big(struct packet_t *p);
int icmp4_ttl_expired(struct packet_t *p);
int icmp4_frag_expired(struct packet_t *p);

//int icmp4_send_echo(struct in_addr *dest);

#endif      /* NET_ICMP4_H */
