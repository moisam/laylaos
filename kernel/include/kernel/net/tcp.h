/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
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

#define TCP_DEFAULT_QUEUE_SIZE      (8 * 1024)
//#define TCP_MIN_QUEUE_SIZE          (128)

// Length of the TCP header, excluding options
#define TCP_HLEN            20

// TCP flags
#define TCP_FIN             0x01
#define TCP_SYN             0x02
#define TCP_RST             0x04
#define TCP_PSH             0x08
#define TCP_ACK             0x10
#define TCP_URG             0x20

// TCP flag combinations
#define TCP_SYNACK          (TCP_SYN | TCP_ACK)
#define TCP_PSHACK          (TCP_PSH | TCP_ACK)
#define TCP_FINACK          (TCP_FIN | TCP_ACK)
#define TCP_FINPSHACK       (TCP_FIN | TCP_PSH | TCP_ACK)
#define TCP_RSTACK          (TCP_RST | TCP_ACK)

struct tcp_hdr_t
{
    uint16_t srcp, destp;
    uint32_t seqno, ackno;
    uint8_t  len, flags;
    uint16_t wnd;
    uint16_t checksum;
    uint16_t urgp;
} __attribute__((packed));

struct tcp_sack_block_t
{
    uint32_t left, right;
} __attribute__((packed));

struct socket_tcp_t
{
    struct socket_t sock;
    
    uint32_t snd_una, snd_nxt, snd_wnd;
    uint32_t snd_up, snd_wl1, snd_wl2;
    uint32_t iss;
    uint32_t rcv_nxt, rcv_wnd, rcv_up;
    uint32_t irs;
    
    uint8_t backoff;
    int32_t srtt, rttvar;
    uint32_t rto;
    
    unsigned long long initconn_timer_due;
    unsigned long long retransmit_timer_due;
    unsigned long long linger_timer_due;
    unsigned long long delack_timer_due;
    
    uint8_t delacks;
    uint16_t rmss, smss;
    uint16_t cwnd;
    uint32_t inflight;
    
    uint8_t sack_ok, sacks_allowed, sack_len;
    struct tcp_sack_block_t sacks[4];
    
    struct netif_queue_t ofoq;
};

// externs defined in udp.c
extern struct netif_queue_t tcp_inq;
extern struct netif_queue_t tcp_outq;
extern struct kernel_mutex_t tcp_lock;
extern struct sockops_t tcp_sockops;


/**************************************
 * Functions defined in network/tcp.c
 **************************************/

int tcp_receive(struct packet_t *p);
int tcp_input(struct socket_t *so, struct packet_t *p);
int tcp_output(struct socket_t *so, int loop_score);
int tcp_read(struct socket_t *so, struct msghdr *msg, unsigned int flags);
int tcp_check_listen_close(struct socket_t *so);
void tcp_notify_closing(struct socket_t *so);
int tcp_open(int domain, struct socket_t **res);
int tcp_init_connection(struct socket_t *so);
int tcp_push(struct packet_t *p);
int tcp_process_out(struct packet_t *p);

void tcp_init(void);

/*********************************************
 * Functions defined in network/sockets/tcp.c
 *********************************************/

void socket_tcp_delete(struct socket_t *so);
void socket_tcp_cleanup(struct socket_t *so);
int socket_tcp_receive(struct sockport_t *sp, struct packet_t *p);

#endif      /* NET_TCP_H */
