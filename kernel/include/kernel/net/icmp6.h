/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: icmp6.h
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
 *  \file icmp6.h
 *
 *  Functions and macros for handling Internet Control Message Protocol 
 *  (ICMP) v6 packets.
 */

#ifndef NET_ICMP6_H
#define NET_ICMP6_H

#include <kernel/net/ether.h>

// ICMP6 message types
#define ICMP6_MSG_DEST_UNREACH          1
#define ICMP6_MSG_PACKET_TOO_BIG        2
#define ICMP6_MSG_TIME_EXCEEDED         3
#define ICMP6_MSG_PARAM_PROBLEM         4
#define ICMP6_MSG_ECHO_REQUEST          128
#define ICMP6_MSG_ECHO_REPLY            129
#define ICMP6_MSG_ROUTER_SOLICIT        133
#define ICMP6_MSG_ROUTER_ADV            134
#define ICMP6_MSG_NEIGHBOR_SOLICIT      135
#define ICMP6_MSG_NEIGHBOR_ADV          136
#define ICMP6_MSG_REDIRECT              137

// Error codes for DESTUNREACH messages (type = 1)
#define ICMP6_DESTUNREACH_NOROUTE       0
#define ICMP6_DESTUNREACH_ADMIN         1
#define ICMP6_DESTUNREACH_SRCSCOPE      2
#define ICMP6_DESTUNREACH_ADDR          3
#define ICMP6_DESTUNREACH_PORT          4
#define ICMP6_DESTUNREACH_SRCFILTER     5
#define ICMP6_DESTUNREACH_REJROUTE      6

// Error codes for TIMEEXCEEDED messages (type = 3)
#define ICMP6_TIMEEXCEEDED_INTRANS      0
#define ICMP6_TIMEEXCEEDED_REASSEMBLY   1

// Error codes for PARAMPROBLEM messages (type = 4)
#define ICMP6_PARAMPROBLEM_HDRFIELD     0
#define ICMP6_PARAMPROBLEM_NXTHDR       1
#define ICMP6_PARAMPROBLEM_IPV6OPT      2

// extensions
#define ICMP6_ND_UNICAST                0

#define ICMP6_ND_SOLICITED              2
#define ICMP6_ND_DAD                    3


/*********************************
 * Neighbor discovery options
 *********************************/

#define ND_OPT_LLADDR_SRC               1
#define ND_OPT_LLADDR_TGT               2
#define ND_OPT_PREFIX                   3
#define ND_OPT_REDIRECT                 4
#define ND_OPT_MTU                      5
#define ND_OPT_RDNSS                    25
#define ND_OPT_ARO                      33
#define ND_OPT_6CO                      34
#define ND_OPT_ABRO                     35

// Neighbor discovery advertisement flags
#define ND_OVERRIDE                     0x20000000
#define ND_SOLICITED                    0x40000000
#define ND_ROUTER                       0x80000000

// check if solicited flag is set
#define IS_SOLICITED(x)                 \
    (ntohl(x->msg.info.neighbor_adv.rsor) & (ND_SOLICITED))

// check if override flag is set
#define IS_OVERRIDE(x)                  \
    (ntohl(x->msg.info.neighbor_adv.rsor) & (ND_OVERRIDE))

// check if router flag is set
#define IS_ROUTER(x)                    \
    (ntohl(x->msg.info.neighbor_adv.rsor) & (ND_ROUTER))


struct icmp6_hdr_t
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    
    union
    {
        // err messages
        union
        {
            struct
            {
                uint32_t unused;
            } dest_unreach;

            struct
            {
                uint32_t mtu;
            } too_big;

            struct
            {
                uint32_t unused;
            } time_exceeded;

            struct
            {
                uint32_t p;
            } param;
        } err;

        // info messages
        union
        {
            struct
            {
                uint16_t id, seq;
            } echo_request;

            struct
            {
                uint16_t id, seq;
            } echo_reply;

            struct
            {
                uint32_t unused;
            } router_solicit;

            struct
            {
                uint8_t hop, mor;
                uint16_t life_time;
                uint32_t reachable_time;
                uint32_t retransmit_time;
            } router_adv;

            struct
            {
                uint32_t unused;
                struct in6_addr target;
            } neighbor_solicit;

            struct
            {
                uint32_t rsor;
                struct in6_addr target;
            } neighbor_adv;

            struct
            {
                uint32_t res;
                struct in6_addr target, dest;
            } redirect;

            struct
            {
                uint16_t max_resp_time, res;
                struct in6_addr mcast_group;
                uint8_t res2, qqic;
                uint16_t nbr_src;
                struct in6_addr src[1];
            } mld;
        } info;
    } msg;
} __attribute__((packed));


struct icmp6_opt_lladdr_t
{
    uint8_t type, len;
    struct ether_addr_t addr;
} __attribute__((packed));


struct icmp6_opt_prefix_t
{
    uint8_t type, len;
    uint8_t prefix_len;
    uint8_t res : 6,
            aac : 1,
            onlink : 1;
    uint32_t val_lifetime;
    uint32_t pref_lifetime;
    uint32_t res1;
    struct in6_addr prefix;
} __attribute__((packed));


struct icmp6_opt_mtu_t
{
    uint8_t type, len;
    uint16_t res;
    uint32_t mtu;
} __attribute__((packed));


struct icmp6_opt_redirect_t
{
    uint8_t type, len;
    uint16_t res1;
    uint32_t res2;
} __attribute__((packed));


struct icmp6_opt_rdnss_t
{
    uint8_t type, len;
    uint16_t res;
    uint32_t lifetime;
    struct in6_addr *addr;
} __attribute__((packed));


// externs defined in icmp6.c
extern struct netif_queue_t icmp6_inq;


/**************************************
 * Functions defined in icmp6.c
 **************************************/

int icmp6_receive(struct packet_t *p);
int icmp6_router_solicit(struct netif_t *ifp, 
                         struct in6_addr *src, struct in6_addr *addr);
int icmp6_neighbor_solicit(struct netif_t *ifp, 
                           struct in6_addr *addr, int type);
int icmp6_neighbor_advertise(struct packet_t *p, struct in6_addr *addr);

int icmp6_port_unreachable(struct packet_t *p);
int icmp6_proto_unreachable(struct packet_t *p);
int icmp6_dest_unreachable(struct packet_t *p);
int icmp6_ttl_expired(struct packet_t *p);
int icmp6_frag_expired(struct packet_t *p);
int icmp6_packet_too_big(struct packet_t *p);
int icmp6_param_problem(struct packet_t *p, uint8_t problem, uint32_t ptr);

#endif      /* NET_ICMP6_H */
