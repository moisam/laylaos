/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: tcp.h
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
 *  \file tcp.h
 *
 *  Functions and macros for handling TCP network packets.
 */

#ifndef NET_TCP_H
#define NET_TCP_H

#include <netinet/tcp.h>
#include <kernel/net/socket.h>
#include <kernel/net/netif.h>

// Length of the TCP header, excluding options
#define TCP_HLEN                20

// TCP flags
#define TCP_FIN                 0x01
#define TCP_SYN                 0x02
#define TCP_RST                 0x04
#define TCP_PSH                 0x08
#define TCP_ACK                 0x10
#define TCP_URG                 0x20

// TCP states
#define TCPSTATE_LISTEN         1
#define TCPSTATE_SYN_SENT       2
#define TCPSTATE_SYN_RECV       3
#define TCPSTATE_ESTABLISHED    4
#define TCPSTATE_FIN_WAIT_1     5
#define TCPSTATE_FIN_WAIT_2     6
#define TCPSTATE_CLOSE          7
#define TCPSTATE_CLOSE_WAIT     8
#define TCPSTATE_CLOSING        9
#define TCPSTATE_LAST_ACK       10
#define TCPSTATE_TIME_WAIT      11

#define TCP_SYN_BACKOFF         500
#define TCP_CONN_RETRIES        3

#define TCP_2MSL_TICKS          (PIT_FREQUENCY * 60 * 2)    /* 2 mins */
#define TCP_USER_TIMEOUT_TICKS  (PIT_FREQUENCY * 60 * 3)    /* 3 mins */

#define TCP_STATE(so)           ((struct socket_tcp_t *)so)->tcpstate

#define TCP_HDR(p)              (struct tcp_hdr_t *)((p)->head + ETHER_HLEN + IPv4_HLEN)


struct tcp_hdr_t
{
    uint16_t srcp, destp;
    uint32_t seqno, ackno;
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t reserved : 4;
    uint8_t hlen : 4;
    uint8_t fin : 1,
            syn : 1,
            rst : 1,
            psh : 1,
            ack : 1,
            urg : 1,
            ece : 1,
            cwr : 1;
#else
    uint8_t hlen : 4;
    uint8_t reserved : 4;
    uint8_t cwr : 1,
            ere : 1,
            urg : 1,
            ack : 1,
            psh : 1,
            rst : 1,
            syn : 1,
            fin : 1;
#endif
    uint16_t wnd;
    uint16_t checksum;
    uint16_t urgp;
    uint8_t data[];
} __attribute__((packed));

struct tcp_options_t
{
    uint16_t mss;
    uint8_t sack;
};

struct tcp_opt_mss_t
{
    uint8_t kind;
    uint8_t len;
    uint16_t mss;
} __attribute__((packed));

struct tcp_opt_ts_t
{
    uint8_t kind;
    uint8_t len;
    uint32_t tsval, tsecr;
} __attribute__((packed));

struct tcp_sack_block_t
{
    uint32_t left, right;
} __attribute__((packed));


struct socket_tcp_t
{
    struct socket_t sock;

    uint32_t tcpstate;
    uint32_t snd_una, snd_nxt, snd_wnd;
    uint32_t snd_up, snd_wl1, snd_wl2;
    uint32_t iss;
    uint32_t rcv_nxt, rcv_wnd, rcv_up;
    uint32_t irs;
    uint32_t tsrecent;

    uint8_t flags;
    uint8_t tsopt;
    uint8_t backoff;
    int32_t srtt, rttvar;
    uint32_t rto;

    struct nettimer_t *retransmit;
    struct nettimer_t *delack;
    struct nettimer_t *keepalive;
    struct nettimer_t *linger;

    uint8_t delacks;
    uint16_t rmss, smss;
    uint16_t cwnd;
    uint32_t inflight;

    unsigned long long linger_ticks;

    uint8_t sackok, sacks_allowed, sacklen;
    struct tcp_sack_block_t sacks[4];
    
    struct netif_queue_t ofoq;  // Out-of-order queue
};


// externs defined in tcp.c
extern struct sockops_t tcp_sockops;


/**************************************
 * Functions defined in network/tcp.c
 **************************************/

void tcp_input(struct packet_t *p);
void socket_tcp_cleanup(struct socket_t *so);
void tcp_notify_closing(struct socket_t *so);

#endif      /* NET_TCP_H */
