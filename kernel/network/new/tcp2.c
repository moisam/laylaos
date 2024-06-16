/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
 * 
 *    file: tcp.c
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
 *  \file tcp.c
 *
 *  Transmission Control Protocol (TCP) implementation.
 */

//#define __DEBUG
#define _GNU_SOURCE
#include <endian.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <kernel/laylaos.h>
#include <kernel/mutex.h>
#include <kernel/timer.h>
#include <kernel/net.h>
#include <kernel/net/packet.h>
#include <kernel/net/netif.h>
#include <kernel/net/socket.h>
#include <kernel/net/protocol.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/tcp.h>
#include <kernel/net/raw.h>
#include <kernel/net/checksum.h>
#include <kernel/net/notify.h>

#include "iovec.c"

#define TCPSTATE(so)        (((struct socket_t *)so)->state & SOCKET_STATE_TCP)

#ifndef ABS
#define ABS(x)                      (((x) < 0) ? -(x) : (x))
#endif

struct netif_queue_t tcp_inq = { 0, };

static void tcp_clear_queues(struct socket_tcp_t *tsock);
static void tcp_clear_timers(struct socket_tcp_t *tsock);
static void tcp_rearm_rto_timer(struct socket_tcp_t *tsock);
static int tcp_transmit_packet(struct socket_t *so, 
                               struct packet_t *p, uint32_t seqno);
static int tcp_queue_transmit_packet(struct socket_t *so, struct packet_t *p);
static int tcp_send_ack(struct socket_tcp_t *tsock);


int tcp_open(int domain, struct socket_t **res)
{
    struct socket_tcp_t *tsock;
    
    *res = NULL;

    if(!(tsock = kmalloc(sizeof(struct socket_tcp_t))))
    {
        return -ENOBUFS;
    }
	    
    A_memset(tsock, 0, sizeof(struct socket_tcp_t));
    
    tsock->sock.timestamp = ticks;
    tsock->sock.domain = domain;
    tsock->sack_ok = 1;
    tsock->rmss = 1460;
    tsock->smss = 536;
    
    //tsock->tcp_inq.sizemax = TCP_DEFAULT_QUEUE_SIZE;
    //tsock->tcp_outq.sizemax = TCP_DEFAULT_QUEUE_SIZE;
    tsock->ofoq.max = TCP_DEFAULT_QUEUE_SIZE;
    //tsock->rto = TCP_RTO_MIN;
    //tsock->linger_timeout = SOCKET_LINGER_TIMEOUT;

    *res = (struct socket_t *)tsock;

    return 0;
}


int tcp_done(struct socket_tcp_t *tsock)
{
    struct socket_t *so = (struct socket_t *)tsock;

    so->state = SOCKET_STATE_CLOSING;
    tcp_clear_timers(tsock);
    tcp_clear_queues(tsock);
    unblock_tasks(&so->recvsel);
    unblock_tasks(&so->sendsel);
    selwakeup(&so->recvsel);
    selwakeup(&so->sendsel);
    return 0;
}


void tcp_enter_time_wait(struct socket_tcp_t *tsock)
{
    struct socket_t *so = (struct socket_t *)tsock;

    //so->state &= 0x00ff;
    //so->state |= SOCKET_STATE_TCP_TIME_WAIT;
    so->state = SOCKET_STATE_CLOSING | SOCKET_STATE_TCP_TIME_WAIT;
    
    tcp_clear_timers(tsock);


    //////////////////////////////////////
    tcp_clear_queues(tsock);
    unblock_tasks(&so->recvsel);
    unblock_tasks(&so->sendsel);
    selwakeup(&so->recvsel);
    selwakeup(&so->sendsel);
    //////////////////////////////////////


    tsock->linger_timer_due = ticks + (60000 / MSECS_PER_TICK);
}


static void tcp_send_delack(struct socket_tcp_t *tsock)
{
    tsock->delacks = 0;
    tsock->delack_timer_due = 0;
    tcp_send_ack(tsock);
}


static void tcp_rtt(struct socket_tcp_t *tsock)
{
    int r, k;

    if(tsock->backoff > 0 || !tsock->retransmit_timer_due)
    {
        // Karn's Algorithm: Don't measure retransmissions
        return;
    }

    r = ticks - (tsock->retransmit_timer_due - (tsock->rto / MSECS_PER_TICK));
    r *= MSECS_PER_TICK;
    
    if(r < 0)
    {
        return;
    }
    
    if(!tsock->srtt)
    {
        // RFC6298 2.2 first measurement is made
        tsock->srtt = r;
        tsock->rttvar = r / 2;
    }
    else
    {
        // RFC6298 2.3 a subsequent measurement is made
        double beta = 0.25;
        double alpha = 0.125;
        
        tsock->rttvar = (1 - beta) * tsock->rttvar + beta * ABS(tsock->srtt - r);
        tsock->srtt = (1 - alpha) * tsock->srtt + alpha * r;
    }
    
    k = 4 * tsock->rttvar;
    
    // RFC6298 says RTO should be at least 1 second. Linux uses 200ms
    if(k < 200)
    {
        k = 200;
    }
    
    tsock->rto = tsock->srtt + k;
}


static void tcp_parse_opts(struct socket_tcp_t *tsock, struct tcp_hdr_t *h)
{
    uint8_t *ptr = (uint8_t *)h + TCP_HLEN;
    uint8_t optlen = (h->len >> 2) - TCP_HLEN;
    uint8_t sack_seen= 0;
    uint8_t tsopt_seen = 0;
    uint16_t mss;
    
    while(optlen > 0 && optlen < 20)
    {
        switch(*ptr)
        {
            case TCPOPT_MAXSEG:
                mss = ntohs(((ptr[2] << 8) | ptr[3]));
                
                if(mss > 536 && mss <= 1460)
                {
                    tsock->smss = mss;
                }
                
                ptr += 4;
                optlen -= 4;
                break;

            case TCPOPT_EOL:
                optlen--;
                break;

            case TCPOPT_NOP:
                ptr++;
                optlen--;
                break;

            case TCPOPT_SACK_PERMITTED:
                sack_seen = 1;
                optlen--;
                break;

            case TCPOPT_TIMESTAMP:
                tsopt_seen = 1;
                optlen--;
                break;

            default:
                printk("tcp: ignoring unrecognized option 0x%x\n", *ptr);
                optlen--;
                //optlen -= ptr[1] ? ptr[1] : 1;
                //ptr += ptr[1] ? ptr[1] : 1;
                break;
        }
    }
    
    if(sack_seen && tsock->sack_ok)
    {
        // There's room for 4 sack blocks without TS OPT
        tsock->sacks_allowed = tsopt_seen ? 3 : 4;
    }
    else
    {
        tsock->sack_ok = 0;
    }
}


int tcp_send_reset(struct socket_tcp_t *tsock)
{
    struct packet_t *p;
    struct tcp_hdr_t *h;
    struct socket_t *so = (struct socket_t *)tsock;
    int res;

    if(!(p = packet_alloc(TCP_HLEN, PACKET_IP)))
    {
        netstats.tcp.memerr++;
        return -ENOMEM;
    }
    
    p->sock = so;
    p->ifp = so->ifp;
    p->transport_hdr = p->data;
    A_memset(p->transport_hdr, 0, sizeof(struct tcp_hdr_t));
    packet_add_header(p, -TCP_HLEN);

    h = (struct tcp_hdr_t *)p->transport_hdr;
    h->flags = TCP_RST;
    h->seqno = htonl(tsock->snd_nxt);
    
    tsock->snd_una = tsock->snd_nxt;
    res = tcp_transmit_packet(so, p, tsock->snd_nxt);
    packet_free(p);
    
    return res;
}


static int tcp_send_ack(struct socket_tcp_t *tsock)
{
    struct packet_t *p;
    struct tcp_hdr_t *h;
    struct socket_t *so = (struct socket_t *)tsock;
    int res;

    if(TCPSTATE(tsock) == SOCKET_STATE_TCP_CLOSED)
    {
        return 0;
    }

    if(!(p = packet_alloc(TCP_HLEN, PACKET_IP)))
    {
        netstats.tcp.memerr++;
        return -ENOMEM;
    }
    
    p->sock = so;
    p->ifp = so->ifp;
    p->transport_hdr = p->data;
    A_memset(p->transport_hdr, 0, sizeof(struct tcp_hdr_t));
    packet_add_header(p, -TCP_HLEN);

    h = (struct tcp_hdr_t *)p->transport_hdr;
    h->flags = TCP_ACK;
    h->seqno = htonl(tsock->snd_nxt);

    res = tcp_transmit_packet(so, p, tsock->snd_nxt);
    packet_free(p);
    
    return res;
}


int tcp_send_synack(struct socket_tcp_t *tsock)
{
    struct packet_t *p;
    struct tcp_hdr_t *h;
    struct socket_t *so = (struct socket_t *)tsock;
    int res;

    if(TCPSTATE(tsock) != SOCKET_STATE_TCP_SYN_SENT)
    {
        printk("tcp: SYNACK when socket in wrong state\n");
        return -EINVAL;
    }

    if(!(p = packet_alloc(TCP_HLEN, PACKET_IP)))
    {
        netstats.tcp.memerr++;
        return -ENOMEM;
    }
    
    p->sock = so;
    p->ifp = so->ifp;
    p->transport_hdr = p->data;
    A_memset(p->transport_hdr, 0, sizeof(struct tcp_hdr_t));
    packet_add_header(p, -TCP_HLEN);

    h = (struct tcp_hdr_t *)p->transport_hdr;
    h->flags = TCP_ACK | TCP_SYN;
    h->seqno = htonl(tsock->snd_nxt);

    res = tcp_transmit_packet(so, p, tsock->snd_nxt);
    packet_free(p);
    
    return res;
}


int tcp_queue_fin(struct socket_tcp_t *tsock)
{
    struct packet_t *p;
    struct tcp_hdr_t *h;
    struct socket_t *so = (struct socket_t *)tsock;

    if(!(p = packet_alloc(TCP_HLEN, PACKET_IP)))
    {
        netstats.tcp.memerr++;
        return -ENOMEM;
    }
    
    p->sock = so;
    p->ifp = so->ifp;
    p->transport_hdr = p->data;
    A_memset(p->transport_hdr, 0, sizeof(struct tcp_hdr_t));
    packet_add_header(p, -TCP_HLEN);

    h = (struct tcp_hdr_t *)p->transport_hdr;
    h->flags = TCP_FIN | TCP_ACK;
    
    return tcp_queue_transmit_packet(so, p);
}


void tcp_send_next(struct socket_tcp_t *tsock, int count)
{
    struct socket_t *so = (struct socket_t *)tsock;
    struct packet_t *p;
    int i = 0;

    kernel_mutex_lock(&so->outq.lock);

    for(p = so->outq.head; p != NULL; p = p->next)
    {
        if(++i > count)
        {
            break;
        }
        
        ((struct tcp_hdr_t *)p->transport_hdr)->seqno = htonl(tsock->snd_nxt);
        tcp_transmit_packet(so, p, tsock->snd_nxt);
        tsock->snd_nxt += p->count;

        KDEBUG("tcp_send_next: snd_nxt old 0x%x, new 0x%x\n", tsock->snd_nxt - p->count, tsock->snd_nxt);
        
        if(((struct tcp_hdr_t *)p->transport_hdr)->flags & TCP_FIN)
        {
            tsock->snd_nxt++;
        }
    }

    kernel_mutex_unlock(&so->outq.lock);
}


static void tcp_clear_timers(struct socket_tcp_t *tsock)
{
    tsock->retransmit_timer_due = 0;
    tsock->backoff = 0;
    tsock->delack_timer_due = 0;
    //tsock->keepalive_timer_due = 0;
    tsock->linger_timer_due = 0;
    tsock->delack_timer_due = 0;
}


static void queue_free(struct netif_queue_t *q)
{
    struct packet_t *p, *next = NULL;

    kernel_mutex_lock(&q->lock);

    for(p = q->head; p != NULL; p = next)
    {
        next = p->next;
        packet_free(p);
    }
    
    q->head = NULL;
    q->tail = NULL;
    q->count = 0;

    kernel_mutex_unlock(&q->lock);
}


static void tcp_clean_rto_queue(struct socket_tcp_t *tsock, uint32_t una)
{
    struct packet_t *p;
    struct socket_t *so = (struct socket_t *)tsock;
    struct tcp_hdr_t *h;
    
    KDEBUG("tcp_clean_rto_queue: una 0x%x\n", una);

    kernel_mutex_lock(&so->outq.lock);
    
    p = so->outq.head;
    
    while(p)
    {
        h = (struct tcp_hdr_t *)p->transport_hdr;

        KDEBUG("tcp_clean_rto_queue: seqno 0x%x, total 0x%x, una 0x%x\n", ntohl(h->seqno), ntohl(h->seqno) + p->count, una);

        if(ntohl(h->seqno) > 0 &&
           ntohl(h->seqno) + p->count <= una)
        {
            so->outq.head = p->next;
            so->outq.count--;
            p->next = NULL;
            packet_free(p);
            
            if(so->outq.tail == p)
            {
                so->outq.tail = NULL;
            }
            
            if(tsock->inflight > 0)
            {
                tsock->inflight--;
            }
            
            p = so->outq.head;
        }
        else
        {
            break;
        }
    }
    
    kernel_mutex_unlock(&so->outq.lock);

    KDEBUG("tcp_clean_rto_queue: packets %d, inflight %d\n", so->outq.count, tsock->inflight);
    
    if(!p || tsock->inflight == 0)
    {
        tsock->retransmit_timer_due = 0;
        tsock->backoff = 0;
    }
}


static void tcp_clear_queues(struct socket_tcp_t *tsock)
{
    queue_free(&tsock->ofoq);
}


void socket_tcp_cleanup(struct socket_t *so)
{
    struct socket_tcp_t *tsock;

    if(so && so->proto && so->proto->protocol == IPPROTO_TCP)
    {
        tsock = (struct socket_tcp_t *)so;
        tcp_clear_timers(tsock);
        tcp_clear_queues(tsock);
    }
}


static void tcp_connect_rto(struct socket_tcp_t *tsock)
{
    struct socket_t *so = (struct socket_t *)tsock;

    tsock->initconn_timer_due = 0;

    if(TCPSTATE(tsock) == SOCKET_STATE_TCP_SYN_SENT)
    {
        if(tsock->backoff > 3)
        {
            printk("tcp: RTO timeout 3 times\n");
            tcp_done(tsock);

            /*
            tsock->err = -ETIMEDOUT;
            so->state &= 0x00ff;
            so->state |= SOCKET_STATE_TCP_CLOSING;
            tcp_clear_timers(tsock);
            tcp_clear_queues(tsock);
            unblock_tasks(&so->recvsel);
            unblock_tasks(&so->sndsel);
            socket_delete(so);
            */
        }
        else
        {
            printk("tcp: RTO timeout - resending packet\n");
            kernel_mutex_lock(&so->outq.lock);
        
            if(so->outq.head)
            {
                ((struct tcp_hdr_t *)
                    so->outq.head->transport_hdr)->seqno =
                                            htonl(tsock->snd_una);
                tcp_transmit_packet(so, so->outq.head, tsock->snd_una);
                tsock->backoff++;
                tcp_rearm_rto_timer(tsock);
            }

            kernel_mutex_unlock(&so->outq.lock);
        }
    }
    else
    {
        printk("tcp: connect RTO when not in SYNSENT state!\n");
    }
}


static void tcp_retransmission_timeout(struct socket_tcp_t *tsock)
{
    struct socket_t *so = (struct socket_t *)tsock;
    struct tcp_hdr_t *h;
    uint8_t flags;

    tsock->retransmit_timer_due = 0;

    kernel_mutex_lock(&so->outq.lock);
        
    if(so->outq.head == NULL)
    {
        tsock->backoff = 0;
        kernel_mutex_unlock(&so->outq.lock);
        
        if(TCPSTATE(tsock) == SOCKET_STATE_TCP_CLOSE_WAIT)
        {
            unblock_tasks(&so->recvsel);
            unblock_tasks(&so->sendsel);
            selwakeup(&so->recvsel);
            selwakeup(&so->sendsel);
            return;
        }
    }

    h = (struct tcp_hdr_t *)so->outq.head->transport_hdr;
    flags = h->flags;

    printk("tcp_retransmission_timeout: una 0x%x\n", tsock->snd_una);

    ((struct tcp_hdr_t *)so->outq.head->transport_hdr)->seqno = htonl(tsock->snd_una);
    tcp_transmit_packet(so, so->outq.head, tsock->snd_una);
    kernel_mutex_unlock(&so->outq.lock);
    
    if(tsock->rto > 60000)
    {
        tcp_done(tsock);
        return;
    }
    else
    {
        tsock->rto <<= 1;
        tsock->backoff++;
        tsock->retransmit_timer_due = ticks + (tsock->rto / MSECS_PER_TICK);

        if(flags & TCP_FIN)
        {
            if(TCPSTATE(tsock) == SOCKET_STATE_TCP_CLOSE_WAIT)
            {
                so->state &= 0x00ff;
                so->state |= SOCKET_STATE_TCP_LAST_ACK;
            }
            else if(TCPSTATE(tsock) == SOCKET_STATE_TCP_ESTABLISHED)
            {
                so->state &= 0x00ff;
                so->state |= SOCKET_STATE_TCP_FIN_WAIT1;
            }
        }
    }
}


/*
static void tcp_abort(struct socket_tcp_t *tsock)
{
    if(TCPSTATE(tsock) != SOCKET_STATE_TCP_TIME_WAIT)
    {
        tcp_send_reset(tsock);
    }
    
    tcp_done(tsock);
}
*/


static int tcp_user_timeout(struct socket_tcp_t *tsock)
{
    tsock->linger_timer_due = 0;

    //tcp_abort(tsock);
    if(TCPSTATE(tsock) == SOCKET_STATE_TCP_TIME_WAIT)
    {
        kernel_mutex_unlock(&sockport_lock);
        socket_delete((struct socket_t *)tsock);
        kernel_mutex_lock(&sockport_lock);
        return 1;
    }
    else
    {
        tcp_send_reset(tsock);
        tcp_done(tsock);
        return 0;
    }
}


static void tcp_rearm_rto_timer(struct socket_tcp_t *tsock)
{
    if(TCPSTATE(tsock) == SOCKET_STATE_TCP_SYN_SENT)
    {
        tsock->initconn_timer_due = 
            ticks + ((500 << tsock->backoff) / MSECS_PER_TICK);
        tsock->retransmit_timer_due = 0;
    }
    else
    {
        tsock->initconn_timer_due = 0;
        tsock->retransmit_timer_due = ticks + (tsock->rto / MSECS_PER_TICK);
    }
}


void tcp_rearm_user_timeout(struct socket_tcp_t *tsock)
{
    if(TCPSTATE(tsock) == SOCKET_STATE_TCP_TIME_WAIT)
    {
        return;
    }

    tsock->linger_timer_due = ticks + (180000 / MSECS_PER_TICK);
}


void tcp_queue_tail(struct netif_queue_t *q, struct packet_t *p)
{
    kernel_mutex_lock(&q->lock);
    
    if(q->head == NULL)
    {
        IFQ_ENQUEUE(q, p);
    }
    else
    {
        p->next = NULL;
        q->tail->next = p;
        q->tail = p;
        q->count++;
    }

    kernel_mutex_unlock(&q->lock);
}


static void tcp_add_opt(struct socket_tcp_t *tsock, struct packet_t *p,
                        uint8_t flags, size_t optsz)
{
    //uint32_t tsval = htonl((uint32_t)(ticks * MSECS_PER_TICK));
    //uint32_t tsec = htonl(tsock->ts_next);
    uint8_t *ptr = (uint8_t *)p->transport_hdr + TCP_HLEN;
    uint8_t *lptr = ptr + optsz;
    
    // fill with no-op to begin with
    A_memset(ptr, TCPOPT_NOP, optsz);
    
    if(flags & TCP_SYN)
    {
        *ptr++ = TCPOPT_MAXSEG;
        *ptr++ = TCPOLEN_MAXSEG;
        *ptr++ = (uint8_t)((tsock->rmss >> 8) & 0xff);
        *ptr++ = (uint8_t)(tsock->rmss & 0xff);
        *ptr++ = TCPOPT_SACK_PERMITTED;
        *ptr++ = TCPOLEN_SACK_PERMITTED;
    }
    
    /*
    *ptr++ = TCPOPT_WINDOW;
    *ptr++ = TCPOLEN_WINDOW;
    *ptr++ = (uint8_t)(tsock->wnd_scale);
    
    if((flags & TCP_SYN) || tsock->sack_ok)
    {
        *ptr++ = TCPOPT_TIMESTAMP;
        *ptr++ = TCPOLEN_TIMESTAMP;
        
        memcpy(ptr, &tsval, 4);
        ptr += 4;
        memcpy(ptr, &tsec, 4);
        ptr += 4;
    }
    */
    
    if((flags & TCP_ACK) && tsock->sack_ok && tsock->sack_len > 0)
    {
        int i;
        struct tcp_sack_block_t *sb;

        *ptr++ = TCPOPT_SACK;
        *ptr++ = 2 + (tsock->sack_len * 8);
        sb = (struct tcp_sack_block_t *)ptr;

        for(i = tsock->sack_len - 1; i >= 0; i--)
        {
            sb->left = htonl(tsock->sacks[i].left);
            sb->right = htonl(tsock->sacks[i].right);
            tsock->sacks[i].left = 0;
            tsock->sacks[i].right = 0;
            
            sb++;
            ptr += sizeof(struct tcp_sack_block_t);
        }
        
        tsock->sack_len = 0;
    }
    
    if(ptr < lptr)
    {
        lptr[-1] = TCPOPT_EOL;
    }
}


size_t tcp_opt_size(struct socket_tcp_t *tsock, uint8_t flags)
{
    size_t sz = 0;
    
    if(flags & TCP_SYN)
    {
        // get length of all the options
        sz = TCPOLEN_MAXSEG + 4 /* SACK */ /* + TCPOLEN_WINDOW + TCPOLEN_TIMESTAMP */;
    }
    else
    {
        /*
        // wnd scale (WS) option always included
        sz += TCPOLEN_WINDOW;
        
        if(tsock->sack_ok)
        {
            // TIMESTAMP option
            sz += TCPOLEN_TIMESTAMP;
        }
        
        // END option
        sz++;
        */
    }
    
    if((flags & TCP_ACK) && tsock->sack_ok && tsock->sack_len > 0)
    {
        int i;

        sz += 2;
        
        for(i = 0; i < tsock->sack_len; i++)
        {
            if(tsock->sacks[i].left != 0)
            {
                sz += 8;
            }
        }
    }
    
    return (((sz + 3) >> 2) << 2);
}


static int tcp_transmit_packet(struct socket_t *so, struct packet_t *p, uint32_t seqno)
{
    struct packet_t *copy;
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;
    struct tcp_hdr_t *h = (struct tcp_hdr_t *)p->transport_hdr;
    size_t opt_len = tcp_opt_size(tsock, h->flags);
    size_t tmph_len = opt_len + TCP_HLEN;
    uint8_t flags = h->flags;

    if(!(copy = packet_alloc(tmph_len + p->count, PACKET_IP)))
    {
        return -ENOMEM;
    }
    
    copy->ifp = p->ifp;
    copy->sock = p->sock;
    copy->transport_hdr = copy->data;
    A_memcpy(&copy->remote_addr, &p->remote_addr, sizeof(p->remote_addr));
    copy->remote_port = p->remote_port;

    tcp_add_opt(tsock, copy, flags, tmph_len - TCP_HLEN);
    
    printk("tcp: sending packet - seqno 0x%x, ackno 0x%x\n", seqno, tsock->rcv_nxt);

    if(p->count)
    {
        A_memcpy((void *)((uintptr_t)copy->transport_hdr + tmph_len),
                     p->data, p->count);
    }

    h = (struct tcp_hdr_t *)copy->data;
    
    h->len = (uint8_t)(tmph_len << 2);
    h->srcp = so->local_port;
    h->destp = so->remote_port;
    h->seqno = htonl(seqno);
    h->ackno = htonl(tsock->rcv_nxt);
    h->wnd = htons(tsock->rcv_wnd);
    h->flags = flags;
    h->urgp = 0;
    h->checksum = 0;
    h->checksum = (so->domain == AF_INET) ? 
                   htons(tcp_checksum_ipv4(copy)) :
                   htons(tcp_checksum_ipv6(copy));

    return ip_push(copy);
}


static int tcp_queue_transmit_packet(struct socket_t *so, struct packet_t *p)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;
    struct tcp_hdr_t *h = (struct tcp_hdr_t *)p->transport_hdr;
    int res = 0;
    
    if(so->outq.head == NULL)
    {
        tcp_rearm_rto_timer(tsock);
    }
    
    KDEBUG("tcp: inflight %d\n", tsock->inflight);

    if(tsock->inflight == 0)
    {
        h->seqno = htonl(tsock->snd_nxt);
        res = tcp_transmit_packet(so, p, tsock->snd_nxt);
        tsock->inflight++;
        tsock->snd_nxt += p->count;
        //p->end_seqno = tsock->snd_nxt;
        
        if(h->flags & TCP_FIN)
        {
            tsock->snd_nxt++;
        }
    }
    
    tcp_queue_tail(&so->outq, p);
    
    return res;
}


int tcp_init_connection(struct socket_t *so)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;
    struct packet_t *p;
    struct tcp_hdr_t *h;
    int res;
    
    tsock->iss = genrand_int32();
    tsock->snd_wnd = 0;
    tsock->snd_wl1 = 0;
    tsock->snd_una = tsock->iss;
    tsock->snd_up = tsock->iss;
    tsock->snd_nxt = tsock->iss;
    tsock->rcv_nxt = 0;
    tsock->rcv_wnd = 44477;
    
    if(TCPSTATE(so) != 0 &&
       TCPSTATE(so) != SOCKET_STATE_TCP_SYN_SENT &&
       TCPSTATE(so) != SOCKET_STATE_TCP_CLOSED &&
       TCPSTATE(so) != SOCKET_STATE_TCP_LISTEN)
    {
        return -EINVAL;
    }
    
    if(!(p = packet_alloc(TCP_HLEN, PACKET_IP)))
    {
        netstats.tcp.memerr++;
        return -ENOMEM;
    }
    
    p->sock = so;
    p->ifp = so->ifp;
    p->transport_hdr = p->data;
    A_memset(p->transport_hdr, 0, sizeof(struct tcp_hdr_t));
    packet_add_header(p, -TCP_HLEN);

    h = (struct tcp_hdr_t *)p->transport_hdr;
    h->flags = TCP_SYN;
    
    //so->state &= 0x00ff;
    //so->state |= SOCKET_STATE_TCP_SYN_SENT;
    so->state |= (SOCKET_STATE_CONNECTING | SOCKET_STATE_TCP_SYN_SENT);
    
    res = tcp_queue_transmit_packet(so, p);
    tsock->snd_nxt++;

    if(res < 0)
    {
        return res;
    }

    // wait for connection to be established if this is a blocking socket
    if(!(so->flags & SOCKET_FLAG_NONBLOCK))
    {
        //void (*old_wakeup)(struct socket_t *, uint16_t);

        //old_wakeup = so->wakeup;
        //so->wakeup = socket_wakeup;
        block_task2(so, 0);
        //so->wakeup = old_wakeup;

        if(TCPSTATE(so) != SOCKET_STATE_TCP_ESTABLISHED)
        {
            return -ETIMEDOUT;
        }
        
        return 0;
    }

    return -EINPROGRESS;
}


/*
 * Push a packet on the outgoing queue.
 * Called from the socket layer.
 */
int tcp_push(struct packet_t *p)
{
    KDEBUG("%s: 1\n", __func__);
    struct tcp_hdr_t *h = (struct tcp_hdr_t *)p->transport_hdr;
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)p->sock;
    struct socket_t *so = (struct socket_t *)p->sock;
    int res;
    int len = p->count;

    if(TCPSTATE(so) != SOCKET_STATE_TCP_ESTABLISHED &&
       TCPSTATE(so) != SOCKET_STATE_TCP_CLOSE_WAIT)
    {
        printk("tcp_push: socket in invalid state 0x%x\n", so->state);
        return -EBADF;
    }
    
    A_memset(h, 0, sizeof(struct tcp_hdr_t));
    h->flags = TCP_ACK;
    
    if(p->count == 0)
    {
        h->flags |= TCP_PSH;
    }
    
    if((res = tcp_queue_transmit_packet(so, p)) < 0)
    {
        printk("tcp: enqueue failed\n");
        packet_free(p);
        netstats.tcp.err++;
        return res;
    }

    tcp_rearm_user_timeout(tsock);
    
    return (res < 0) ? res : len;
}


int tcp_receive(struct packet_t *p)
{
    struct tcp_hdr_t *tcph = (struct tcp_hdr_t *)p->transport_hdr;
    struct sockport_t *sp;
    int ipver = GET_IP_VER((struct ipv4_hdr_t *)p->data);
    
    KDEBUG("tcp_receive: ipv%d\n", ipver);

    if(!tcph)
    {
        KDEBUG("tcp: discarding packet with invalid TCP header\n");
        goto err;
    }
    
    if(ipver == 4)
    {
        if(tcp_checksum_ipv4(p) != 0)
        {
            printk("tcp: checksum 0x%x\n", tcp_checksum_ipv4(p));
            printk("tcp: discarding IPv4 packet with invalid checksum\n");

            //__asm__ __volatile__("cli\nhlt":::);
            //for(;;);

            goto err;
        }
    }
    else if(ipver == 6)
    {
        if(tcp_checksum_ipv6(p) != 0)
        {
            KDEBUG("tcp: discarding IPv6 packet with invalid checksum\n");
            goto err;
        }
    }
    else
    {
        KDEBUG("tcp: discarding packet with invalid IP version\n");
        goto err;
    }

    // try raw sockets first
    if(raw_receive(p) == 0)
    {
        // a raw socket consumed the packet
        return 0;
    }
    
    if(!(sp = get_sockport(IPPROTO_TCP, tcph->destp)))
    {
        KDEBUG("tcp: cannot find port %d\n", ntohs(tcph->destp));
        
        if(p->flags & PACKET_FLAG_BROADCAST)
        {
            notify_socket_unreachable(p, (ipver == 6));
        }
        
        goto err;
    }
    
    return socket_tcp_receive(sp, p);

err:

    packet_free(p);
    netstats.tcp.proterr++;
    return -EINVAL;
}


static int tcp_synsent(struct socket_t *so, struct packet_t *p)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;
    struct tcp_hdr_t *tcph = (struct tcp_hdr_t *)p->transport_hdr;

    if(tcph->flags & TCP_ACK)
    {
        if(ntohl(tcph->ackno) <= tsock->iss ||
           ntohl(tcph->ackno) > tsock->snd_nxt)
        {
            KDEBUG("tcp: ack is unacceptable - 0x%x\n", ntohl(tcph->ackno));
            
            if(tcph->flags & TCP_RST)
            {
                goto discard;
            }
            
            goto reset_and_discard;
        }
        
        if(ntohl(tcph->ackno) < tsock->snd_una ||
           ntohl(tcph->ackno) > tsock->snd_nxt)
        {
            KDEBUG("tcp: ack is unacceptable - 0x%x\n", ntohl(tcph->ackno));
            
            goto reset_and_discard;
        }
    }
    
    if(tcph->flags & TCP_RST)
    {
        if(TCPSTATE(so) != SOCKET_STATE_TCP_CLOSED)
        {
            tcp_done(tsock);
        }
        
        goto discard;
    }
    
    if(!(tcph->flags & TCP_SYN))
    {
        goto discard;
    }
    
    tsock->rcv_nxt = ntohl(tcph->seqno) + 1;
    tsock->irs = ntohl(tcph->seqno);
    tsock->initconn_timer_due = 0;

    if(tcph->flags & TCP_ACK)
    {
        tsock->snd_una = ntohl(tcph->ackno);
        tcp_clean_rto_queue(tsock, tsock->snd_una);
    }

    KDEBUG("tcp_synsent: una 0x%x, iss 0x%x\n", tsock->snd_una, tsock->iss);
    
    if(tsock->snd_una > tsock->iss)
    {
        //so->state &= 0x00ff;
        //so->state |= SOCKET_STATE_TCP_ESTABLISHED;
        so->state &= ~(SOCKET_STATE_TCP_SYN_SENT | SOCKET_STATE_CONNECTING);
        so->state |= (SOCKET_STATE_CONNECTED | SOCKET_STATE_TCP_ESTABLISHED);
        tsock->snd_una = tsock->snd_nxt;
        tsock->backoff = 0;
        tsock->rto = 1000;
        tcp_send_ack(tsock);
        tcp_rearm_user_timeout(tsock);
        tcp_parse_opts(tsock, tcph);
    }
    else
    {
        so->state &= 0x00ff;
        so->state &= ~SOCKET_STATE_CONNECTING;
        so->state |= SOCKET_STATE_TCP_SYN_RECV;
        tsock->snd_una = tsock->iss;
        tcp_send_synack(tsock);
    }
    
    // wakeup tasks waiting for the connect (first call is for blocking
    // sockets, second one is for non-blocking sockets)
    unblock_tasks(so);
    selwakeup(&so->sendsel);

    KDEBUG("tcp_synsent: new state 0x%x\n", so->state);

discard:
    packet_free(p);
    return 0;

reset_and_discard:
    // TODO: reset
    packet_free(p);
    return 0;
}


static int tcp_verify_segment(struct socket_tcp_t *tsock, 
                              struct packet_t *p, size_t *res_payload_len)
{
    struct tcp_hdr_t *tcph = (struct tcp_hdr_t *)p->transport_hdr;
    /*
    uint16_t hlen = (tcph->len >> 2);
    uintptr_t payload = (uintptr_t)p->transport_hdr + hlen;
    uintptr_t packet_end = (uintptr_t)p->data + p->count;
    size_t payload_len = packet_end - payload;
    */
    size_t iphlen = (uintptr_t)p->transport_hdr - (uintptr_t)p->data;
    size_t hlen = iphlen + (tcph->len >> 2);
    size_t payload_len = p->count - hlen;
    uintptr_t payload = (uintptr_t)p->data + hlen;
    uintptr_t packet_end = (uintptr_t)p->data + p->count;
    
    *res_payload_len = payload_len;
    
    if((payload > packet_end) ||
       (payload_len > 0 && tsock->rcv_wnd == 0))
    {
        KDEBUG("tcp: received invalid segment - payload_len 0x%x\n", payload_len);
        return 0;
    }
    
    if(ntohl(tcph->seqno) < tsock->rcv_nxt ||
       ntohl(tcph->seqno) > (tsock->rcv_nxt + tsock->rcv_wnd))
    {
        KDEBUG("tcp: received invalid segment - seqno 0x%x, exp 0x%x\n", ntohl(tcph->seqno), tsock->rcv_nxt);
        return 0;
    }
    
    return 1;
}


static void tcp_data_insert(struct netif_queue_t *q, 
                            struct packet_t *p /* , size_t payload_len */)
{
    struct packet_t *prev = NULL, *next;
    struct tcp_hdr_t *ph = (struct tcp_hdr_t *)p->transport_hdr;
    struct tcp_hdr_t *nexth;
    uint32_t seqno = ntohl(ph->seqno);
    
    kernel_mutex_lock(&q->lock);
    
    for(next = q->head; next != NULL; prev = next, next = next->next)
    {
        nexth = (struct tcp_hdr_t *)next->transport_hdr;
        
        if(seqno < ntohl(nexth->seqno))
        {
            if(seqno + p->count > ntohl(nexth->seqno))
            {
                // TODO: join segments
                printk("tcp: could not join segments\n");
            }
            else
            {
                if(prev)
                {
                    prev->next = p;
                }
                else
                {
                    q->head = p;
                }

                p->next = next;
                q->count++;

                kernel_mutex_unlock(&q->lock);
                return;
            }
        }
        else if(seqno == ntohl(nexth->seqno))
        {
            // duplicate segment
            kernel_mutex_unlock(&q->lock);
            packet_free(p);
            return;
        }
    }
    
    kernel_mutex_unlock(&q->lock);
    tcp_queue_tail(q, p);
}


static void tcp_calc_sacks(struct socket_tcp_t *tsock)
{
    struct tcp_sack_block_t *sb = &tsock->sacks[tsock->sack_len];
    struct packet_t *next;
    struct tcp_hdr_t *h;
    uint32_t seqno;
    
    sb->left = 0;
    sb->right = 0;

    kernel_mutex_lock(&tsock->ofoq.lock);
    
    for(next = tsock->ofoq.head; next != NULL; next = next->next)
    {
        h = (struct tcp_hdr_t *)next->transport_hdr;
        seqno = ntohl(h->seqno);
        
        if(sb->left == 0)
        {
            sb->left = seqno;
            tsock->sack_len++;
        }
        
        if(sb->right == 0)
        {
            sb->right = seqno + next->count;
        }
        else if(sb->right == seqno)
        {
            sb->right = seqno + next->count;
        }
        else
        {
            if(tsock->sack_len >= tsock->sacks_allowed)
            {
                break;
            }
            
            sb = &tsock->sacks[tsock->sack_len];
            sb->left = seqno;
            sb->right = seqno + next->count;
            tsock->sack_len++;
        }
    }
    
    kernel_mutex_unlock(&tsock->ofoq.lock);
}


int tcp_data_queue(struct socket_tcp_t *tsock, struct packet_t *p)
{
    struct socket_t *so = (struct socket_t *)tsock;
    struct tcp_hdr_t *tcph = (struct tcp_hdr_t *)p->transport_hdr;
    size_t iphlen = (uintptr_t)p->transport_hdr - (uintptr_t)p->data;
    size_t hlen = iphlen + (tcph->len >> 2);
    size_t payload_len = p->count - hlen;
    int expected;

    if(!tsock->rcv_wnd)
    {
        packet_free(p);
        return -EINVAL;
    }

    expected = (ntohl(tcph->seqno) == tsock->rcv_nxt);
    
    if(expected)
    {
        struct packet_t *ofop;
        struct tcp_hdr_t *ofoh;

        tsock->rcv_nxt += payload_len;
        packet_add_header(p, -hlen);
        tcp_queue_tail(&so->inq, p);
        
        // transform out-of-order segments into order
        kernel_mutex_lock(&tsock->ofoq.lock);

        while((ofop = tsock->ofoq.head))
        {
            ofoh = (struct tcp_hdr_t *)ofop->transport_hdr;
            
            if(tsock->rcv_nxt != ntohl(ofoh->seqno))
            {
                break;
            }
            
            // packet in-order, put it in receive queue
            /*
            size_t l1 = (uintptr_t)ofop->transport_hdr - (uintptr_t)ofop->data;
            size_t l2 = l1 + (ofoh->len >> 2);

            tsock->rcv_nxt += (ofop->count - l2);
            */
            tsock->rcv_nxt += ofop->count;
            
            // dequeue
            tsock->ofoq.head = ofop->next;
            tsock->ofoq.count--;
            
            if(tsock->ofoq.tail == ofop)
            {
                tsock->ofoq.tail = NULL;
            }
            
            // enqueue (don't hold two locks simultaneously)
            kernel_mutex_unlock(&tsock->ofoq.lock);
            tcp_queue_tail(&so->inq, ofop);
            kernel_mutex_lock(&tsock->ofoq.lock);
        }

        kernel_mutex_unlock(&tsock->ofoq.lock);

        // notify user
        unblock_tasks(&so->recvsel);
        selwakeup(&so->recvsel);
    }
    else
    {
        // Segment passed validation, hence it is in-window but not the
        // left-most sequence. Put into out-of-order queue for later 
        // processing
        packet_add_header(p, -hlen);
        tcp_data_insert(&tsock->ofoq, p /* , payload_len */);
        
        if(tsock->sack_ok)
        {
            tcp_calc_sacks(tsock);
        }
        
        // RFC5581: A TCP receiver SHOULD send an immediate duplicate ACK 
        // when an out-of-order segment arrives. The purpose of this ACK is
        // to inform the sender that a segment was received out-of-order and
        // which sequence number is expected
        tcp_send_ack(tsock);
    }
    
    return 0;
}


int tcp_input(struct socket_t *so, struct packet_t *p)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;
    struct tcp_hdr_t *tcph = (struct tcp_hdr_t *)p->transport_hdr;
    int expected, do_free = 1;
    size_t payload_len = 0;
    uint8_t flags = tcph->flags;

    p->sock = so;
    p->timestamp = ticks;
    so->timestamp = ticks;

    printk("tcp_input: received segment - seqno 0x%x, exp 0x%x, flags 0x%x\n", ntohl(tcph->seqno), tsock->rcv_nxt, tcph->flags);
    
    switch(TCPSTATE(so))
    {
        case SOCKET_STATE_TCP_CLOSED:
            packet_free(p);

            if(flags & TCP_RST)
            {
                return 0;
            }
            
            return tcp_send_reset(tsock);

        case SOCKET_STATE_TCP_LISTEN:
            packet_free(p);
            return 0;

        case SOCKET_STATE_TCP_SYN_SENT:
            printk("tcp: received packet in state SYNSENT\n");
            return tcp_synsent(so, p);
    }
    
    // check seqno
    if(!tcp_verify_segment(tsock, p, &payload_len))
    {
        if(!(flags & TCP_RST))
        {
            tcp_send_ack(tsock);
        }

        packet_free(p);
        return 0;
    }
    
    // check RST
    if(flags & TCP_RST)
    {
        packet_free(p);
        tcp_enter_time_wait(tsock);
        unblock_tasks(&so->recvsel);
        selwakeup(&so->recvsel);
        return 0;
    }
    
    // TODO: check security and precedence
    
    // check SYN
    if(flags & TCP_SYN)
    {
        // RFC 5961 Section 4.2
        // TODO: implement tcp_send_challenge_ack()
        packet_free(p);
        return 0;
    }
    
    // check ACK
    if(!(flags & TCP_ACK))
    {
        packet_free(p);
        return 0;
    }
    
    // ACK is set
    switch(TCPSTATE(so))
    {
        case SOCKET_STATE_TCP_SYN_RECV:
            if(tsock->snd_una <= ntohl(tcph->ackno) &&
               ntohl(tcph->ackno) < tsock->snd_nxt)
            {
                so->state &= 0x00ff;
                so->state |= SOCKET_STATE_TCP_ESTABLISHED;
            }
            else
            {
                packet_free(p);
                return 0;
            }

        case SOCKET_STATE_TCP_ESTABLISHED:
        case SOCKET_STATE_TCP_FIN_WAIT1:
        case SOCKET_STATE_TCP_FIN_WAIT2:
        case SOCKET_STATE_TCP_CLOSE_WAIT:
        case SOCKET_STATE_TCP_CLOSING:
        case SOCKET_STATE_TCP_LAST_ACK:
            printk("tcp_input: una 0x%x, ackno 0x%x, snd_nxt 0x%x\n", tsock->snd_una, ntohl(tcph->ackno), tsock->snd_nxt);
            if(tsock->snd_una <= ntohl(tcph->ackno) &&
               ntohl(tcph->ackno) <= tsock->snd_nxt)
            {
                tsock->snd_una = ntohl(tcph->ackno);
                // remove ACKed segments on the retransmission queue
                tcp_rtt(tsock);
                tcp_clean_rto_queue(tsock, tsock->snd_una);
            }
            
            if(ntohl(tcph->ackno) < tsock->snd_una)
            {
                printk("tcp_input: ignoring dup ACK\n");
                // ignore duplicate ACK
                packet_free(p);
                return 0;
            }

            if(ntohl(tcph->ackno) > tsock->snd_nxt)
            {
                printk("tcp_input: ignoring future ACK\n");
                // ACK for segment not yet sent
                //tcp_send_ack(tsock);
                packet_free(p);
                return 0;
            }

            if(tsock->snd_una < ntohl(tcph->ackno) &&
               ntohl(tcph->ackno) <= tsock->snd_nxt)
            {
                // TODO: update send window
            }
            
            break;
    }

    // If the write queue is empty, it means our FIN was acked
    if(so->outq.head == NULL)
    {
        printk("tcp_input: --- 1 state 0x%x\n", so->state);

        switch(TCPSTATE(so))
        {
            case SOCKET_STATE_TCP_FIN_WAIT1:
                so->state &= 0x00ff;
                so->state |= SOCKET_STATE_TCP_FIN_WAIT2;

            case SOCKET_STATE_TCP_FIN_WAIT2:
                break;

            case SOCKET_STATE_TCP_CLOSING:
                // In addition to the processing for the ESTABLISHED state, if
                // the ACK acknowledges our FIN then enter the TIME-WAIT state,
                // otherwise ignore the segment
                so->state &= 0x00ff;
                so->state |= SOCKET_STATE_TCP_TIME_WAIT;
                break;

            case SOCKET_STATE_TCP_LAST_ACK:
                // The only thing that can arrive in this state is an 
                // acknowledgment of our FIN. If our FIN is now acknowledged,
                // delete the TCB, enter the CLOSED state, and return
                return tcp_done(tsock);

            case SOCKET_STATE_TCP_TIME_WAIT:
                // TODO: The only thing that can arrive in this state is a
                // retransmission of the remote FIN. Acknowledge it, and 
                // restart the 2 MSL timeout
                if(tsock->rcv_nxt == ntohl(tcph->seqno))
                {
                    tcp_send_ack(tsock);
                }
                
                break;
        }
    }

    // TODO: check URG
    if(flags & TCP_URG)
    {
    }

    expected = (ntohl(tcph->seqno) == tsock->rcv_nxt);
    KDEBUG("tcp_input: expected %d\n", expected);
    
    // process the segment
    switch(TCPSTATE(so))
    {
        case SOCKET_STATE_TCP_ESTABLISHED:
        case SOCKET_STATE_TCP_FIN_WAIT1:
        case SOCKET_STATE_TCP_FIN_WAIT2:
            if((flags & TCP_PSH) || payload_len > 0)
            {
                tcp_data_queue(tsock, p);
                do_free = 0;
            }
            break;

        case SOCKET_STATE_TCP_CLOSE_WAIT:
        case SOCKET_STATE_TCP_CLOSING:
        case SOCKET_STATE_TCP_LAST_ACK:
        case SOCKET_STATE_TCP_TIME_WAIT:
            // This should not occur, since a FIN has been received from the
            // remote side. Ignore the segment text
            break;
    }
    
    // check FIN
    if((flags & TCP_FIN) && expected)
    {
        printk("tcp: received in sequence FIN\n");
        printk("tcp_input: --- 2 state 0x%x\n", so->state);

        switch(TCPSTATE(so))
        {
            case SOCKET_STATE_TCP_CLOSED:
            case SOCKET_STATE_TCP_LISTEN:
            case SOCKET_STATE_TCP_SYN_SENT:
                // Do not process, since SEG.SEQ cannot be validated
                goto drop;
        }
        
        tsock->rcv_nxt++;
        tcp_send_ack(tsock);
        unblock_tasks(&so->recvsel);
        selwakeup(&so->recvsel);

        switch(TCPSTATE(so))
        {
            case SOCKET_STATE_TCP_SYN_RECV:
            case SOCKET_STATE_TCP_ESTABLISHED:
                so->state &= 0x00ff;
                so->state |= SOCKET_STATE_SHUT_REMOTE;
                so->state |= SOCKET_STATE_TCP_CLOSE_WAIT;
                break;

            case SOCKET_STATE_TCP_FIN_WAIT1:
                // If our FIN has been ACKed (perhaps in this segment), then
                // enter TIME-WAIT, start the time-wait timer, turn off the 
                // other timers; otherwise enter the CLOSING state
                if(so->outq.head == NULL)
                {
                    tcp_enter_time_wait(tsock);
                }
                else
                {
                    so->state &= 0x00ff;
                    so->state |= SOCKET_STATE_TCP_CLOSING;
                }

                break;

            case SOCKET_STATE_TCP_FIN_WAIT2:
                // Enter the TIME-WAIT state. Start the time-wait timer, turn
                // off the other timers
                tcp_enter_time_wait(tsock);
                break;

            case SOCKET_STATE_TCP_CLOSE_WAIT:
            case SOCKET_STATE_TCP_CLOSING:
            case SOCKET_STATE_TCP_LAST_ACK:
                break;

            case SOCKET_STATE_TCP_TIME_WAIT:
                // TODO: restart the 2 MSL timeout
                break;
        }
    }
    
    // Congestion control and delacks
    switch(TCPSTATE(so))
    {
        case SOCKET_STATE_TCP_ESTABLISHED:
        case SOCKET_STATE_TCP_FIN_WAIT1:
        case SOCKET_STATE_TCP_FIN_WAIT2:
            if(expected)
            {
                tsock->delack_timer_due = 0;

                int pending = MIN(so->outq.count, 3);
                
                // RFC1122:  A TCP SHOULD implement a delayed ACK, but an 
                // ACK should not be excessively delayed; in particular, the 
                // delay MUST be less than 0.5 seconds, and in a stream of
                // full-sized segments there SHOULD be an ACK for at least
                // every second segment
                if(tsock->inflight == 0 && pending > 0)
                {
                    tcp_send_next(tsock, pending);
                    tsock->inflight += pending;
                    tcp_rearm_rto_timer(tsock);
                }
                else if((flags & TCP_PSH) ||
                        (payload_len > 1000 && ++tsock->delacks > 1))
                {
                    tsock->delacks = 0;
                    tcp_send_ack(tsock);
                }
                else if(payload_len > 0)
                {
                    tsock->delacks = 0;
                    tcp_send_ack(tsock);
                    //tsock->delack_timer_due = ticks + (200 / MSECS_PER_TICK);
                }
            }
            
            break;
    }
    
drop:

    if(do_free)
    {
        packet_free(p);
    }
    
    return 0;
}


void tcp_notify_closing(struct socket_t *so)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;

    printk("tcp_notify_closing: state 0x%x\n", so->state);
    
    switch(TCPSTATE(so))
    {
        case SOCKET_STATE_TCP_CLOSED:
        case SOCKET_STATE_TCP_CLOSING:
        case SOCKET_STATE_TCP_LAST_ACK:
        case SOCKET_STATE_TCP_TIME_WAIT:
        case SOCKET_STATE_TCP_FIN_WAIT1:
        case SOCKET_STATE_TCP_FIN_WAIT2:
            break;

        case SOCKET_STATE_TCP_LISTEN:
        case SOCKET_STATE_TCP_SYN_SENT:
        case SOCKET_STATE_TCP_SYN_RECV:
            tcp_done(tsock);
            break;

        case SOCKET_STATE_TCP_ESTABLISHED:
            // Queue this until all preceding SENDs have been segmentized, 
            // then form a FIN segment and send it. In any case, enter 
            // FIN-WAIT-1 state
            so->state &= 0x00ff;
            so->state |= SOCKET_STATE_TCP_FIN_WAIT1;
            tcp_queue_fin(tsock);
            break;

        case SOCKET_STATE_TCP_CLOSE_WAIT:
            // Queue this request until all preceding SENDs have been 
            // segmentized; then send a FIN segment, enter LAST_ACK state
            tcp_queue_fin(tsock);
            break;

        default:
            printk("tcp: close in unknown TCP state\n");
            break;
    }
}


int tcp_read(struct socket_t *so, struct msghdr *msg, unsigned int flags)
{
    KDEBUG("%s: 1\n", __func__);
    struct packet_t *p;
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;
    size_t size, read = 0;
    size_t plen;
    uint8_t pflags;

    if((size = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }

    switch(TCPSTATE(so))
    {
        case SOCKET_STATE_TCP_CLOSED:
            return -EBADF;

        case SOCKET_STATE_TCP_CLOSING:
        case SOCKET_STATE_TCP_LAST_ACK:
        case SOCKET_STATE_TCP_TIME_WAIT:
            return -ENOTCONN;

        case SOCKET_STATE_TCP_LISTEN:
        case SOCKET_STATE_TCP_SYN_SENT:
        case SOCKET_STATE_TCP_SYN_RECV:
            break;

        case SOCKET_STATE_TCP_ESTABLISHED:
        case SOCKET_STATE_TCP_FIN_WAIT1:
        case SOCKET_STATE_TCP_FIN_WAIT2:
            break;

        case SOCKET_STATE_TCP_CLOSE_WAIT:
            if(so->inq.head != NULL)
            {
                break;
            }

            return -ENOTCONN;

        default:
            return -EBADF;
    }
    
    kernel_mutex_lock(&so->inq.lock);
    p = so->inq.head;

    while(read < size)
    {
        KDEBUG("tcp_read: read %d, size %d, packets %d\n", read, size, so->inq.count);
        
        if(!p)
        {
            break;
        }
        
        plen = (read + p->count) > size ? (size - read) : p->count;
        pflags = ((struct tcp_hdr_t *)p->transport_hdr)->flags;
        
        if(write_iovec(msg->msg_iov, msg->msg_iovlen, p->data, plen, 0) != 0)
        {
            read += plen;
            packet_copy_remoteaddr(so, p, msg);
            
            if(!(flags & MSG_PEEK))
            {
                packet_add_header(p, -plen);
                
                if(p->count == 0)
                {
                    KDEBUG("tcp_read: discarding segment (plen %u, seqno 0x%x)\n", plen, ntohl(((struct tcp_hdr_t *)p->transport_hdr)->seqno));
                    
                    /*
                    if(((tcp_hdr_t *)p->transport_hdr)->flags & TCP_PSH)
                    {
                        tsock->flags |= TCP_PSH;
                    }
                    */
                    
                    IFQ_DEQUEUE(&so->inq, p);
                    packet_free(p);
                    
                    p = so->inq.head;
                }
            }
            else
            {
                p = p->next;
            }
        }
        else
        {
            break;
        }
        
        /*
        if(tsock->flags & TCP_PSH)
        {
            tsock->flags &= ~TCP_PSH;
            break;
        }
        */
        if(pflags & TCP_PSH)
        {
            break;
        }
    }

    kernel_mutex_unlock(&so->inq.lock);

    KDEBUG("tcp_read: done\n");
    
    if(read > 0)
    {
        tcp_rearm_user_timeout(tsock);
    }
    
    return read;
}


struct task_t *tcp_timeout_task = NULL;


void tcp_timeout(void *unused)
{
    struct sockport_t *sp;
    struct socket_t *so;
    struct socket_tcp_t *tsock;

    UNUSED(unused);

    for(;;)
    {
        kernel_mutex_lock(&sockport_lock);

        //KDEBUG("tcp_timeout:\n");

loop:

        for(sp = tcp_ports; sp != NULL; sp = sp->next)
        {
            for(so = sp->sockets; so != NULL; so = so->next)
            {
                tsock = (struct socket_tcp_t *)so;

                if(tsock->initconn_timer_due &&
                   tsock->initconn_timer_due < ticks)
                {
                    tcp_connect_rto(tsock);
                    continue;
                }

                /*
                if(tsock->retransmit_timer_due &&
                   tsock->retransmit_timer_due < ticks)
                {
                    tcp_retransmission_timeout(tsock);
                    continue;
                }
                */

                if(tsock->linger_timer_due &&
                   tsock->linger_timer_due < ticks)
                {
                    if(tcp_user_timeout(tsock))
                    {
                        goto loop;
                    }

                    continue;
                }

                /*
                if(tsock->delack_timer_due &&
                   tsock->delack_timer_due < ticks)
                {
                    tcp_send_delack(tsock);
                    continue;
                }
                */
            }
        }

        kernel_mutex_unlock(&sockport_lock);
        
        //KDEBUG("tcp_timeout: sleeping\n");

        block_task2(&tcp_timeout_task, PIT_FREQUENCY / 5);
    }
}


void tcp_init(void)
{
    (void)start_kernel_task("tcp", tcp_timeout, NULL, &tcp_timeout_task, 0);
}

