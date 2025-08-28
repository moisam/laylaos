/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
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

#include <netinet/tcp.h>
#include <kernel/net.h>
#include <kernel/net/protocol.h>
#include <kernel/net/socket.h>
#include <kernel/net/packet.h>
#include <kernel/net/nettimer.h>
#include <kernel/net/tcp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/checksum.h>
#include <mm/kheap.h>

#include "iovec.c"

#ifndef ABS
#define ABS(a)          ((a) < 0 ? -(a) : (a))
#endif

static int tcp_send_syn(struct socket_tcp_t *tsock);
static int tcp_send_reset(struct socket_tcp_t *tsock);
static int tcp_send_ack(struct socket_tcp_t *tsock);

static void tcp_linger(void *arg);

static int tcp_queue_tansmit(struct socket_tcp_t *tsock, struct packet_t *p);
static int tcp_transmit(struct socket_tcp_t *tsock, struct packet_t *p, uint32_t seqno);

static void tcp_rearm_rto_timer(struct socket_tcp_t *tsock);
static void tcp_rearm_user_timeout(struct socket_tcp_t *tsock);

static void tcp_release_rto_timer(struct socket_tcp_t *tsock);
static void tcp_release_delack_timer(struct socket_tcp_t *tsock);


#define DROP_AND_RETURN(p)              \
    printk("tcp: dropping packet\n");   \
    free_packet(p);                     \
    return;


static struct socket_t *tcp_socket(void)
{
    struct socket_tcp_t *tsock;
    
    if(!(tsock = kmalloc(sizeof(struct socket_tcp_t))))
    {
        return NULL;
    }
	    
    A_memset(tsock, 0, sizeof(struct socket_tcp_t));
    
    //tsock->sock.timestamp = ticks;
    tsock->sackok = 1;
    tsock->rmss = 1460;
    tsock->smss = 536;
    tsock->tcpstate = TCPSTATE_CLOSE;
    tsock->linger_ticks = TCP_2MSL_TICKS;
    tsock->ofoq.max = SOCKET_DEFAULT_QUEUE_SIZE;

    return (struct socket_t *)tsock;
}


static long tcp_connect(struct socket_t *so)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;
    long res;

    tsock->iss = genrand_int32();
    tsock->snd_wnd = 0;
    tsock->snd_wl1 = 0;
    tsock->snd_una = tsock->iss;
    tsock->snd_up = tsock->iss;
    tsock->snd_nxt = tsock->iss;
    tsock->rcv_nxt = 0;
    tsock->rcv_wnd = 44477;

    res = tcp_send_syn(tsock);
    tsock->snd_nxt++;

    return res;
}


static long tcp_write(struct socket_t *so, struct msghdr *msg, int kernel)
{
    struct tcp_hdr_t *h;
    struct packet_t *p;
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;
    long res, total, slen, dlen = 0;
    int mss = tsock->smss;
    int tsoptlen = tsock->tsopt ? TCPOLEN_TIMESTAMP + 2 : 0;

    /*
    if(so->err != 0)
    {
        return so->err;
    }
    */

    if(tsock->tcpstate != TCPSTATE_ESTABLISHED &&
       tsock->tcpstate != TCPSTATE_CLOSE_WAIT)
    {
        return -EBADF;
    }

    if((total = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }

    slen = total;

    while(slen > 0)
    {
        dlen = (slen > mss) ? mss : slen;
        slen -= dlen;

        // reserve space in the header for the timestamp option
        if(!(p = alloc_packet(PACKET_SIZE_TCP(dlen + tsoptlen))))
        {
            printk("tcp: insufficient memory for sending packet\n");
            return -ENOMEM;
        }

        packet_add_header(p, -PACKET_SIZE_TCP(tsoptlen));

        if((res = read_iovec(msg->msg_iov, msg->msg_iovlen, p->data, 
                             p->count, kernel)) < 0)
        {
            free_packet(p);
            return res;
        }

        h = TCP_HDR(p);
        h->ack = 1;

        if(slen == 0)
        {
            h->psh = 1;
        }

        if((res = tcp_queue_tansmit(tsock, p)) != 0)
        {
            printk("tcp: error sending (err 0x%x)\n", res);
        }
    }

    tcp_rearm_user_timeout(tsock);

    return total;
}



static long tcp_read(struct socket_t *so, struct msghdr *msg, unsigned int flags)
{
    struct ipv4_hdr_t *iph;
    struct tcp_hdr_t *tcph;
    struct packet_t *p;
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;
    size_t size, read = 0;
    size_t plen, poff, prem;
    uint8_t psh;

    switch(tsock->tcpstate)
    {
        case TCPSTATE_CLOSE:
            return -EBADF;

        case TCPSTATE_LISTEN:
        case TCPSTATE_SYN_SENT:
        case TCPSTATE_SYN_RECV:
            break;

        case TCPSTATE_ESTABLISHED:
        case TCPSTATE_FIN_WAIT_1:
        case TCPSTATE_FIN_WAIT_2:
            break;

        case TCPSTATE_CLOSE_WAIT:
            if(so->inq.head != NULL)
            {
                break;
            }

            if(tsock->flags & TCP_FIN)
            {
                tsock->flags &= ~TCP_FIN;
                return 0;
            }

            break;

        case TCPSTATE_CLOSING:
        case TCPSTATE_LAST_ACK:
        case TCPSTATE_TIME_WAIT:
            if(so->inq.head != NULL)
            {
                break;
            }

            return tsock->sock.err;

        default:
            return -EBADF;
    }

    if((size = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }

    p = so->inq.head;
    poff = (flags & MSG_PEEK) ? so->peek_offset : 0;

    /*
     * If MSG_PEEK is passed, we need to "fast forward" to the last peek
     * offset, so that we can carry on reading from there.
     */
    while(p && poff != 0)
    {
        if(poff < p->count)
        {
            break;
        }

        poff -= p->count;
        p = p->next;
    }

    while(read < size)
    {
        if(!p)
        {
            if((flags & MSG_DONTWAIT) || (so->flags & SOCKET_FLAG_NONBLOCK))
            {
                if(read == 0)
                {
                    read = -EAGAIN;
                }

                if(!(tsock->flags & TCP_FIN))
                {
                    //so->poll_events &= ~POLLIN;
                    __sync_and_and_fetch(&so->poll_events, ~POLLIN);
                }

                return read;
            }

            // blocking socket -- wait for data
            selrecord(&so->selrecv);
            SOCKET_UNLOCK(so);
            this_core->cur_task->woke_by_signal = 0;
            block_task(so, 1);
            SOCKET_LOCK(so);

            if(this_core->cur_task->woke_by_signal)
            {
                // TODO: should we return -ERESTARTSYS and restart the read?
                return -EINTR;
            }

            p = so->inq.head;
            continue;
        }

        iph = IPv4_HDR(p);
        tcph = (struct tcp_hdr_t *)((char *)iph + (iph->hlen * 4));
        prem = p->count - poff;
        plen = (read + prem) > size ? (size - read) : prem;
        psh = tcph->psh;

        if(write_iovec(msg->msg_iov, msg->msg_iovlen, p->data + poff, plen, 0) != 0)
        {
            read += plen;
            socket_copy_remoteaddr(so, msg);

            if(!(flags & MSG_PEEK))
            {
                so->peek_offset = 0;
                poff = 0;
                packet_add_header(p, -plen);

                if(p->count == 0)
                {
                    IFQ_DEQUEUE(&so->inq, p);
                    free_packet(p);

                    p = so->inq.head;
                }
            }
            else
            {
                so->peek_offset += plen;

                if(plen == p->count)
                {
                    poff = 0;

                    if(!(p = p->next))
                    {
                        return read;
                    }
                }
                else
                {
                    poff += plen;
                }
            }
        }
        else
        {
            break;
        }

        if(psh)
        {
            break;
        }

        if(tsock->flags & TCP_FIN)
        {
            break;
        }
    }

    if(!so->inq.head && !(tsock->flags & TCP_FIN))
    {
        //so->poll_events &= ~POLLIN;
        __sync_and_and_fetch(&so->poll_events, ~POLLIN);
    }

    tcp_rearm_user_timeout(tsock);

    return read;
}


static long tcp_getsockopt(struct socket_t *so, int level, int optname,
                           void *optval, int *optlen)
{
    if(so->proto->protocol != IPPROTO_TCP)
    {
        return -EINVAL;
    }
    
    if(!optval || !optlen)
    {
        return -EFAULT;
    }

    if(level == SOL_SOCKET || level == IPPROTO_IP)
    {
        return socket_getsockopt(so, level, optname, optval, optlen);
    }
    else if(level == IPPROTO_TCP)
    {
        switch(optname)
        {
            case TCP_NODELAY:
                *(int *)optval = !!(so->flags & SOCKET_FLAG_TCPNODELAY);
                *optlen = sizeof(int);
                return 0;
        }
    }

    return -ENOPROTOOPT;
}


static long tcp_setsockopt(struct socket_t *so, int level, int optname,
                           void *optval, int optlen)
{
    int tmp;
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;

    if(so->proto->protocol != IPPROTO_TCP)
    {
        return -EINVAL;
    }
    
    if(!optval || optlen < (int)sizeof(int))
    {
        return -EINVAL;
    }

    /*
     * We can directly read the option value as the socket layer has copied
     * it from userspace for us.
     */
    tmp = *(int *)optval;

    if(level == SOL_SOCKET)
    {
        struct linger *li;

        switch(optname)
        {
            case SO_LINGER:
                if(optlen < (int)sizeof(struct linger))
                {
                    return -EINVAL;
                }

                li = (struct linger *)optval;
                
                if(li->l_onoff)
                {
                    // convert seconds to ticks
                    tsock->linger_ticks = li->l_linger * PIT_FREQUENCY;
                }
                else
                {
                    tsock->linger_ticks = TCP_2MSL_TICKS /* 0 */;
                }

                return 0;

            default:
                //return -ENOPROTOOPT;
                return socket_setsockopt(so, level, optname, optval, optlen);
        }
    }
    else if(level == IPPROTO_IP)
    {
        return socket_setsockopt(so, level, optname, optval, optlen);
    }
    else if(level == IPPROTO_TCP)
    {
        switch(optname)
        {
            case TCP_NODELAY:
                if(tmp)
                {
                    so->flags |= SOCKET_FLAG_TCPNODELAY;
                }
                else
                {
                    so->flags &= ~SOCKET_FLAG_TCPNODELAY;
                }

                return 0;

#if 0
            case TCP_KEEPCNT:
                tsock->keepalive_probes = tmp;
                return 0;

            case TCP_KEEPIDLE:
                // TODO: check the manpage -- is the arg given in msecs or secs?
                //tsock->keepalive_time = ticks + (tmp / MSECS_PER_TICK);
                tsock->keepalive_time = (tmp / MSECS_PER_TICK);
                return 0;

            case TCP_KEEPINTVL:
                tsock->keepalive_interval = tmp;
                return 0;
#endif

        }
    }

    return -ENOPROTOOPT;
}


static int tcp_verify_segment(struct socket_tcp_t *tsock, struct tcp_hdr_t *tcph, struct packet_t *p)
{
    size_t dlen = p->end_seq - p->seq;

    if(dlen > 0 && tsock->rcv_wnd == 0)
    {
        return 0;
    }

    if(tcph->seqno < tsock->rcv_nxt ||  // duplicate segment
       tcph->seqno > (tsock->rcv_nxt + tsock->rcv_wnd)) // out of scope segment
    {
        printk("tcp: received invalid segment (seqno 0x%x)\n", tcph->seqno);
        return 0;
    }

    return 1;
}


static void queue_free(struct netif_queue_t *q)
{
    struct packet_t *p, *next = NULL;

    //kernel_mutex_lock(&q->lock);

    for(p = q->head; p != NULL; p = next)
    {
        next = p->next;
        free_packet(p);
    }
    
    q->head = NULL;
    q->tail = NULL;
    q->count = 0;

    //kernel_mutex_unlock(&q->lock);
}


static void tcp_clean_rto_queue(struct socket_tcp_t *tsock, uint32_t una)
{
    struct packet_t *p;
    struct socket_t *so = (struct socket_t *)tsock;

    p = so->outq.head;

    while(p)
    {
        if(p->seq > 0 && p->end_seq <= una)
        {
            so->outq.head = p->next;
            so->outq.count--;
            p->next = NULL;
            free_packet(p);
            
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
    
    if(!p || tsock->inflight == 0)
    {
        // No unacknowledged packets, stop rto timer
        tcp_release_rto_timer(tsock);
    }
}


STATIC_INLINE void tcp_clear_queues(struct socket_tcp_t *tsock)
{
    queue_free(&tsock->ofoq);
}


STATIC_INLINE void tcp_clear_timers(struct socket_tcp_t *tsock)
{
    tcp_release_rto_timer(tsock);
    tsock->backoff = 0;
    tcp_release_delack_timer(tsock);
    nettimer_release(tsock->keepalive);
    tsock->keepalive = NULL;
    nettimer_release(tsock->linger);
    tsock->linger = NULL;
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


STATIC_INLINE void tcp_handle_fin_state(struct socket_tcp_t *tsock)
{
    switch(tsock->tcpstate)
    {
        case TCPSTATE_CLOSE_WAIT:
            tsock->tcpstate = TCPSTATE_LAST_ACK;
            break;

        case TCPSTATE_ESTABLISHED:
            tsock->tcpstate = TCPSTATE_FIN_WAIT_1;
            break;
    }
}


STATIC_INLINE void tcp_notify_user(struct socket_tcp_t *tsock)
{
    switch(tsock->tcpstate)
    {
        case TCPSTATE_CLOSE_WAIT:
            selwakeup(&(tsock->sock.sleep));
            break;
    }
}


STATIC_INLINE void tcp_done(struct socket_tcp_t *tsock)
{
    tsock->sock.state = TCPSTATE_CLOSING;
    tcp_clear_timers(tsock);
    tcp_clear_queues(tsock);
    selwakeup(&(tsock->sock.sleep));
    socket_delete((struct socket_t *)tsock, PIT_FREQUENCY * 60 * 2);
}


STATIC_INLINE void tcp_abort(struct socket_tcp_t *tsock)
{
    tcp_send_reset(tsock);
    tcp_done(tsock);
}


STATIC_INLINE void tcp_enter_time_wait(struct socket_tcp_t *tsock)
{
    tsock->tcpstate = TCPSTATE_TIME_WAIT;
    tcp_clear_timers(tsock);
    tsock->linger = nettimer_add(tsock->linger_ticks /* TCP_2MSL */, &tcp_linger, tsock);
}


static void tcp_send_delack(void *arg)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)arg;

    SOCKET_LOCK(&(tsock->sock));
    tsock->delacks = 0;
    tcp_release_delack_timer(tsock);
    tcp_send_ack(tsock);
    SOCKET_UNLOCK(&(tsock->sock));
}


static void tcp_linger(void *arg)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)arg;

    if(!sock_find((struct socket_t *)tsock))
    {
        return;
    }

    SOCKET_LOCK(&(tsock->sock));
    nettimer_release(tsock->linger);
    tsock->linger = NULL;
    tcp_done(tsock);
    SOCKET_UNLOCK(&(tsock->sock));
}


static void tcp_user_timeout(void *arg)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)arg;

    if(!sock_find((struct socket_t *)tsock))
    {
        return;
    }

    SOCKET_LOCK(&(tsock->sock));
    nettimer_release(tsock->linger);
    tsock->linger = NULL;
    tcp_abort(tsock);
    SOCKET_UNLOCK(&(tsock->sock));
}


static void tcp_reconnect_rto(void *arg)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)arg;

    if(!sock_find((struct socket_t *)tsock))
    {
        return;
    }

    SOCKET_LOCK(&(tsock->sock));
    tcp_release_rto_timer(tsock);

    if(tsock->tcpstate == TCPSTATE_SYN_SENT)
    {
        if(tsock->backoff > TCP_CONN_RETRIES)
        {
            tsock->sock.err = -ETIMEDOUT;
            //tsock->sock.poll_events |= (POLLOUT | POLLERR | POLLHUP);
            __sync_or_and_fetch(&tsock->sock.poll_events, (POLLOUT | POLLERR | POLLHUP));
            tcp_done(tsock);
        }
        else
        {
            struct packet_t *p = tsock->sock.outq.head;

            if(p)
            {
                //reset_packet_header(p);
                tcp_transmit(tsock, p, tsock->snd_una);
                tsock->backoff++;
                tcp_rearm_rto_timer(tsock);
            }
        }
    }
    else
    {
        printk("tcp: connect RTO triggered while not in SYN_SENT\n");
    }

    SOCKET_UNLOCK(&(tsock->sock));
}


static void tcp_retransmission_timeout(void *arg)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)arg;
    struct packet_t *p;
    struct tcp_hdr_t *h;

    if(!sock_find((struct socket_t *)tsock))
    {
        return;
    }

    SOCKET_LOCK(&(tsock->sock));
    tcp_release_rto_timer(tsock);

    if(!(p = tsock->sock.outq.head))
    {
        tsock->backoff = 0;
        printk("tcp: RTO queue empty\n");
        tcp_notify_user(tsock);
        SOCKET_UNLOCK(&(tsock->sock));
        return;
    }

    h = TCP_HDR(p);
    //reset_packet_header(p);
    tcp_transmit(tsock, p, tsock->snd_una);

    // time out after 3 mins
    if(tsock->rto > PIT_FREQUENCY * 60 * 3)
    {
        tcp_done(tsock);
        tsock->sock.err = -ETIMEDOUT;
        //tsock->sock.poll_events |= (POLLOUT | POLLERR | POLLHUP);
        __sync_or_and_fetch(&tsock->sock.poll_events, (POLLOUT | POLLERR | POLLHUP));
        SOCKET_UNLOCK(&(tsock->sock));
        return;
    }

    tsock->rto *= 2;
    tsock->backoff++;
    tsock->retransmit = nettimer_add(tsock->rto, &tcp_retransmission_timeout, tsock);

    if(h->fin)
    {
        tcp_handle_fin_state(tsock);
    }

    SOCKET_UNLOCK(&(tsock->sock));
}


static void tcp_release_delack_timer(struct socket_tcp_t *tsock)
{
    nettimer_release(tsock->delack);
    tsock->delack = NULL;
}


static void tcp_release_rto_timer(struct socket_tcp_t *tsock)
{
    nettimer_release(tsock->retransmit);
    tsock->retransmit = NULL;
}


static void tcp_rearm_rto_timer(struct socket_tcp_t *tsock)
{
    tcp_release_rto_timer(tsock);

    if(tsock->tcpstate == TCPSTATE_SYN_SENT)
    {
        tsock->retransmit = nettimer_add(TCP_SYN_BACKOFF << tsock->backoff, &tcp_reconnect_rto, tsock);
    }
    else
    {
        tsock->retransmit = nettimer_add(tsock->rto, &tcp_retransmission_timeout, tsock);
    }
}


static void tcp_rearm_user_timeout(struct socket_tcp_t *tsock)
{
    if(tsock->tcpstate == TCPSTATE_TIME_WAIT)
    {
        return;
    }

    nettimer_release(tsock->linger);
    tsock->linger = nettimer_add(tsock->linger_ticks /* TCP_USER_TIMEOUT */, &tcp_user_timeout, tsock);
}


static void tcp_write_options(struct socket_tcp_t *tsock, struct tcp_hdr_t *h)
{
    struct tcp_sack_block_t *sb;
    struct tcp_opt_ts_t *ts;
    uint8_t *p = h->data;
    int i;

    if(tsock->tsopt)
    {
        ts = (struct tcp_opt_ts_t *)p;
        ts->kind = TCPOPT_TIMESTAMP;
        ts->len = TCPOLEN_TIMESTAMP;
        ts->tsval = ticks;
        ts->tsecr = tsock->tsrecent;

        p += sizeof(struct tcp_opt_ts_t);
        *p++ = TCPOPT_NOP;
        *p++ = TCPOPT_NOP;

        if((h->hlen * 4) == (TCP_HLEN + TCPOLEN_TIMESTAMP + 2))
        {
            return;
        }
    }
    else if((h->hlen * 4) == TCP_HLEN)
    {
        return;
    }

    if(!tsock->sackok || tsock->sacks[0].left == 0)
    {
        return;
    }

    *p++ = TCPOPT_NOP;
    *p++ = TCPOPT_NOP;
    *p++ = TCPOPT_SACK;
    *p++ = 2 + tsock->sacklen * 8;

    sb = (struct tcp_sack_block_t *)p;

    for(i = tsock->sacklen - 1; i >= 0; i--)
    {
        sb->left = htonl(tsock->sacks[i].left);
        sb->right = htonl(tsock->sacks[i].right);
        tsock->sacks[i].left = 0;
        tsock->sacks[i].right = 0;
        sb++;
    }

    tsock->sacklen = 0;
}


static int tcp_transmit(struct socket_tcp_t *tsock, struct packet_t *p, uint32_t seqno)
{
    struct packet_t *copy = dup_packet(p);
    struct tcp_hdr_t *h;

    if(!copy)
    {
        printk("tcp: insufficient memory to send packet\n");
        return -ENOMEM;
    }

    h = TCP_HDR(copy);

    if(h->hlen == 0)
    {
        int tsoptlen = tsock->tsopt ? TCPOLEN_TIMESTAMP + 2 : 0;
        h->hlen = (TCP_HLEN + tsoptlen) / 4;
    }

    packet_add_header(copy, h->hlen * 4);

    h->srcp = tsock->sock.local_port;
    h->destp = tsock->sock.remote_port;
    h->seqno = htonl(seqno);
    h->ackno = htonl(tsock->rcv_nxt);
    h->reserved = 0;
    h->wnd = htons(tsock->rcv_wnd);
    h->urgp = 0;

    tcp_write_options(tsock, h);

    if(tsock->sock.domain == AF_INET)
    {
        return ipv4_send(copy, tsock->sock.local_addr.ipv4,
                               tsock->sock.remote_addr.ipv4, 
                               IPPROTO_TCP, tsock->sock.ttl);
    }

    /*
     * TODO: handle IPv6 packets.
     */
    free_packet(copy);
    return -EAFNOSUPPORT;
}


static int tcp_queue_tansmit(struct socket_tcp_t *tsock, struct packet_t *p)
{
    struct tcp_hdr_t *h = TCP_HDR(p);
    int res = 0;

    if(tsock->sock.outq.head == NULL)
    {
        tcp_rearm_rto_timer(tsock);
    }

    if(tsock->inflight == 0)
    {
        res = tcp_transmit(tsock, p, tsock->snd_nxt);
        tsock->inflight++;
        p->seq = tsock->snd_nxt;
        tsock->snd_nxt += p->count;
        p->end_seq = tsock->snd_nxt;

        if(h->fin)
        {
            tsock->snd_nxt++;
        }
    }

    //kernel_mutex_lock(&tsock->sock.outq.lock);

    if(IFQ_FULL(&tsock->sock.outq))
    {
        //kernel_mutex_unlock(&tsock->sock.outq.lock);
        free_packet(p);
        res = -ENOBUFS;
    }
    else
    {
        IFQ_ENQUEUE(&tsock->sock.outq, p);
        //kernel_mutex_unlock(&tsock->sock.outq.lock);
    }

    return res;
}


static int tcp_options_len(struct socket_tcp_t *tsock)
{
    int i, optlen = 0;

    if(tsock->tsopt)
    {
        optlen += TCPOLEN_TIMESTAMP + 2;
    }

    if(tsock->sackok && tsock->sacklen > 0)
    {
        for(i = 0; i < tsock->sacklen; i++)
        {
            if(tsock->sacks[i].left != 0)
            {
                optlen += 8;
            }
        }

        optlen += 2;
    }

    while(optlen % 4)
    {
        optlen++;
    }

    return optlen;
}


static int tcp_syn_options(struct socket_tcp_t *tsock, struct tcp_options_t *opts)
{
    int optlen = TCPOLEN_MAXSEG + TCPOLEN_TIMESTAMP + 2;

    opts->mss = tsock->rmss;

    if(tsock->sackok)
    {
        opts->sack = 1;
        optlen += TCPOLEN_SACK_PERMITTED + 2;
    }
    else
    {
        opts->sack = 0;
    }

    return optlen;
}


static void tcp_write_syn_options(struct tcp_hdr_t *h, struct tcp_options_t *opts, int optlen)
{
    struct tcp_opt_mss_t *optmss = (struct tcp_opt_mss_t *)h->data;
    struct tcp_opt_ts_t *ts;
    int i = 0;

    optmss->kind = TCPOPT_MAXSEG;
    optmss->len = TCPOLEN_MAXSEG;
    optmss->mss = htons(opts->mss);
    i += sizeof(struct tcp_opt_mss_t);

    ts = (struct tcp_opt_ts_t *)((uint8_t *)h->data + 4);
    ts->kind = TCPOPT_TIMESTAMP;
    ts->len = TCPOLEN_TIMESTAMP;
    ts->tsval = ticks;
    ts->tsecr = 0;
    i += sizeof(struct tcp_opt_ts_t);

    if(opts->sack)
    {
        h->data[i++] = TCPOPT_SACK_PERMITTED;
        h->data[i++] = TCPOLEN_SACK_PERMITTED;
    }
    else
    {
        h->data[i++] = TCPOPT_NOP;
        h->data[i++] = TCPOPT_NOP;
    }

    h->hlen = (TCP_HLEN + optlen) / 4;
}


static int tcp_send_syn(struct socket_tcp_t *tsock)
{
    struct packet_t *p;
    struct tcp_hdr_t *h;
    struct tcp_options_t opts = { 0, };
    int optlen;

    if(tsock->tcpstate != TCPSTATE_SYN_SENT &&
       tsock->tcpstate != TCPSTATE_CLOSE &&
       tsock->tcpstate != TCPSTATE_LISTEN)
    {
        printk("tcp: socket in incorrect state for SYN\n");
        return -EINVAL;
    }

    optlen = tcp_syn_options(tsock, &opts);

    if(!(p = alloc_packet(PACKET_SIZE_TCP(optlen))))
    {
        printk("tcp: insufficient memory for SYN packet\n");
        return -ENOMEM;
    }

    packet_add_header(p, -PACKET_SIZE_TCP(optlen));
    p->seq = 0;

    h = TCP_HDR(p);

    tcp_write_syn_options(h, &opts, optlen);
    h->syn = 1;
    tsock->tcpstate = TCPSTATE_SYN_SENT;

    return tcp_queue_tansmit(tsock, p);
}


static int tcp_send_reset(struct socket_tcp_t *tsock)
{
    struct packet_t *p;
    struct tcp_hdr_t *h;
    int res;

    if(!(p = alloc_packet(PACKET_SIZE_TCP(0))))
    {
        printk("tcp: insufficient memory for RST packet\n");
        return -ENOMEM;
    }

    packet_add_header(p, -PACKET_SIZE_TCP(0));
    h = TCP_HDR(p);
    h->rst = 1;
    tsock->snd_una = tsock->snd_nxt;

    res = tcp_transmit(tsock, p, tsock->snd_nxt);
    free_packet(p);

    return res;
}


static int tcp_send_challenge_ack(struct socket_tcp_t *tsock, struct packet_t *p)
{
    /*
     * TODO: implement this.
     */
    return 0;
}


static int tcp_send_ack(struct socket_tcp_t *tsock)
{
    struct packet_t *p;
    struct tcp_hdr_t *h;
    int optlen, res;

    if(tsock->tcpstate == TCPSTATE_CLOSE)
    {
        printk("tcp: socket in incorrect state for ACK\n");
        return -EINVAL;
    }

    optlen = tcp_options_len(tsock);

    if(!(p = alloc_packet(PACKET_SIZE_TCP(optlen))))
    {
        printk("tcp: insufficient memory for SYN packet\n");
        return -ENOMEM;
    }

    packet_add_header(p, -PACKET_SIZE_TCP(optlen));
    p->seq = 0;

    h = TCP_HDR(p);
    h->ack = 1;
    h->hlen = (TCP_HLEN + optlen) / 4;

    res = tcp_transmit(tsock, p, tsock->snd_nxt);
    free_packet(p);

    return res;
}


static int tcp_send_synack(struct socket_tcp_t *tsock)
{
    struct packet_t *p;
    struct tcp_hdr_t *h;
    int res;
    int tsoptlen = tsock->tsopt ? TCPOLEN_TIMESTAMP + 2 : 0;

    if(tsock->tcpstate != TCPSTATE_SYN_SENT)
    {
        printk("tcp: socket in incorrect state for SYN-ACK\n");
        return -EINVAL;
    }

    if(!(p = alloc_packet(PACKET_SIZE_TCP(tsoptlen))))
    {
        printk("tcp: insufficient memory for SYN-ACK packet\n");
        return -ENOMEM;
    }

    packet_add_header(p, -PACKET_SIZE_TCP(tsoptlen));
    h = TCP_HDR(p);
    h->syn = 1;
    h->ack = 1;

    res = tcp_transmit(tsock, p, tsock->snd_nxt);
    free_packet(p);

    return res;
}


static int tcp_queue_fin(struct socket_tcp_t *tsock)
{
    struct packet_t *p;
    struct tcp_hdr_t *h;
    int tsoptlen = tsock->tsopt ? TCPOLEN_TIMESTAMP + 2 : 0;

    if(!(p = alloc_packet(PACKET_SIZE_TCP(tsoptlen))))
    {
        printk("tcp: insufficient memory for FIN packet\n");
        return -ENOMEM;
    }

    packet_add_header(p, -PACKET_SIZE_TCP(tsoptlen));
    h = TCP_HDR(p);
    h->fin = 1;
    h->ack = 1;

    return tcp_queue_tansmit(tsock, p);
}


static void tcp_reset(struct socket_tcp_t *tsock)
{
    tsock->sock.poll_events = (POLLOUT | POLLWRNORM | POLLERR | POLLHUP);

    switch(tsock->tcpstate)
    {
        case TCPSTATE_SYN_SENT:
            tsock->sock.err = -ECONNREFUSED;
            break;

        case TCPSTATE_CLOSE_WAIT:
            tsock->sock.err = -EPIPE;
            break;

        case TCPSTATE_CLOSE:
            return;

        default:
            tsock->sock.err = -ECONNRESET;
            break;
    }

    tcp_done(tsock);
}


static void tcp_parse_timestamp(struct socket_tcp_t *tsock, struct tcp_hdr_t *tcph)
{
    struct tcp_opt_ts_t *ts;
    uint8_t *p = tcph->data;
    int optlen = (tcph->hlen * 4) - 20;

    while(optlen > 0 && optlen < 20)
    {
        switch(*p)
        {
            case TCPOPT_EOL:
                optlen = 0;
                break;

            case TCPOPT_NOP:
                p++;
                optlen--;
                break;

            case TCPOPT_TIMESTAMP:
                ts = (struct tcp_opt_ts_t *)p;
                tsock->tsrecent = ts->tsval;
                p += TCPOLEN_TIMESTAMP;
                optlen -= TCPOLEN_TIMESTAMP;
                break;

            default:
                printk("tcp: unrecognised option 0x%x\n", *p);
                // the 'kind' byte is followed by a 'len' byte
                optlen -= p[1];
                p += p[1];
                break;
        }
    }
}


static void tcp_parse_opts(struct socket_tcp_t *tsock, struct tcp_hdr_t *tcph)
{
    uint8_t *p = tcph->data;
    int optlen = (tcph->hlen * 4) - 20;
    int sack_seen = 0;
    int tsopt_seen = 0;
    uint16_t mss;
    struct tcp_opt_mss_t *optmss = NULL;

    while(optlen > 0 && optlen < 20)
    {
        switch(*p)
        {
            case TCPOPT_EOL:
                optlen = 0;
                break;

            case TCPOPT_MAXSEG:
                optmss = (struct tcp_opt_mss_t *)p;
                mss = htons(optmss->mss);

                if(mss > 536 && mss <= 1460)
                {
                    tsock->smss = mss;
                }

                p += TCPOLEN_MAXSEG;
                optlen -= TCPOLEN_MAXSEG;
                break;

            case TCPOPT_NOP:
                p++;
                optlen--;
                break;

            case TCPOPT_SACK_PERMITTED:
                sack_seen = 1;
                p += TCPOLEN_SACK_PERMITTED;
                optlen -= TCPOLEN_SACK_PERMITTED;
                break;

            case TCPOPT_TIMESTAMP:
                tsopt_seen = 1;
                p += TCPOLEN_TIMESTAMP;
                optlen -= TCPOLEN_TIMESTAMP;
                break;

            default:
                printk("tcp: unrecognised option 0x%x\n", *p);
                // the 'kind' byte is followed by a 'len' byte
                optlen -= p[1];
                p += p[1];
                break;
        }
    }

    if(!tsopt_seen)
    {
        tsock->tsopt = 0;
    }

    if(sack_seen && tsock->sackok)
    {
        if(tsock->tsopt)
        {
            tsock->sacks_allowed = 3;
        }
        else
        {
            tsock->sacks_allowed = 4;
        }
    }
    else
    {
        tsock->sackok = 0;
    }
}


static void tcp_synsent(struct socket_tcp_t *tsock, struct tcp_hdr_t *tcph, struct packet_t *p)
{
    if(tcph->ack)
    {
        if(tcph->ackno <= tsock->iss || tcph->ackno > tsock->snd_nxt)
        {
            printk("tcp: unacceptable ackno 0x%x\n", tcph->ackno);

            if(tcph->rst)
            {
                free_packet(p);
                return;
            }

            // TODO: reset and discard
            free_packet(p);
            return;
        }

        if(tcph->ackno < tsock->snd_una || tcph->ackno > tsock->snd_nxt)
        {
            printk("tcp: unacceptable ackno 0x%x\n", tcph->ackno);
            // TODO: reset and discard
            free_packet(p);
            return;
        }
    }

    if(tcph->rst)
    {
        tcp_reset(tsock);
        free_packet(p);
        return;
    }

    // TODO: check security and precedence

    if(!tcph->syn)
    {
        free_packet(p);
        return;
    }

    tsock->rcv_nxt = tcph->seqno + 1;
    tsock->irs = tcph->seqno;

    if(tcph->ack)
    {
        tsock->snd_una = tcph->ackno;
        // Any packets in RTO queue that are acknowledged here should be removed
        tcp_clean_rto_queue(tsock, tsock->snd_una);
        tcp_parse_timestamp(tsock, tcph);
    }

    if(tsock->snd_una > tsock->iss)
    {
        tsock->tcpstate = TCPSTATE_ESTABLISHED;
        tsock->snd_una = tsock->snd_nxt;
        tsock->backoff = 0;
        // RFC 6298: Sender SHOULD set RTO <- 1 second
        tsock->rto = PIT_FREQUENCY;
        tcp_send_ack(tsock);
        tcp_rearm_user_timeout(tsock);
        tcp_parse_opts(tsock, tcph);
        sock_connected((struct socket_t *)tsock);
    }
    else
    {
        tsock->tcpstate = TCPSTATE_SYN_RECV;
        tsock->snd_una = tsock->iss;
        tcp_send_synack(tsock);
    }

    free_packet(p);
}


static void tcp_data_insert_ordered(struct netif_queue_t *q, struct packet_t *p)
{
    struct packet_t *prev = NULL, *next;

    for(next = q->head; next != NULL; prev = next, next = next->next)
    {
        if(p->seq < next->seq)
        {
            if(p->end_seq > next->seq)
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

                return;
            }
        }
        else if(p->seq == next->seq)
        {
            // duplicate segment
            free_packet(p);
            return;
        }
    }

    IFQ_ENQUEUE(q, p);
}


static void tcp_consume_ofo_queue(struct socket_tcp_t *tsock)
{
    struct packet_t *p;

    while((p = tsock->ofoq.head))
    {
        if(tsock->rcv_nxt != p->seq)
        {
            break;
        }
            
        // packet in-order, put it in receive queue
        tsock->rcv_nxt += (p->end_seq - p->seq);

        // dequeue
        tsock->ofoq.head = p->next;
        tsock->ofoq.count--;

        if(tsock->ofoq.tail == p)
        {
            tsock->ofoq.tail = NULL;
        }

        // enqueue
        IFQ_ENQUEUE(&(tsock->sock.inq), p);
    }
}


static void tcp_rtt(struct socket_tcp_t *tsock)
{
    int r, k;

    if(tsock->backoff > 0 || !tsock->retransmit)
    {
        // Karn's Algorithm: Don't measure retransmissions
        return;
    }

    r = ticks - (tsock->retransmit->expires - tsock->rto);

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

    // RFC6298 says RTO should be at least 1 second. Linux uses 200ms,
    // which is equal to 20 ticks
    if(k < 20)
    {
        k = 20;
    }

    tsock->rto = tsock->srtt + k;
}


static void tcp_calc_sacks(struct socket_tcp_t *tsock)
{
    struct tcp_sack_block_t *sb = &tsock->sacks[tsock->sacklen];
    struct packet_t *next;
    
    sb->left = 0;
    sb->right = 0;

    for(next = tsock->ofoq.head; next != NULL; next = next->next)
    {
        if(sb->left == 0)
        {
            sb->left = next->seq;
            tsock->sacklen++;
        }
        
        if(sb->right == 0)
        {
            sb->right = next->end_seq;
        }
        else if(sb->right == next->seq)
        {
            sb->right = next->end_seq;
        }
        else
        {
            if(tsock->sacklen >= tsock->sacks_allowed)
            {
                break;
            }
            
            sb = &tsock->sacks[tsock->sacklen];
            sb->left = next->seq;
            sb->right = next->end_seq;
            tsock->sacklen++;
        }
    }
}


static void tcp_data_queue(struct socket_tcp_t *tsock, struct tcp_hdr_t *tcph, struct packet_t *p)
{
    struct ipv4_hdr_t *iph;
    int expected, hlen;

    if(!tsock->rcv_wnd)
    {
        free_packet(p);
        return;
    }

    iph = IPv4_HDR(p);
    hlen = ETHER_HLEN + (iph->hlen * 4) + (tcph->hlen * 4);
    packet_add_header(p, -hlen);

    expected = (p->seq == tsock->rcv_nxt);

    if(expected)
    {
        tsock->rcv_nxt += (p->end_seq - p->seq);
        IFQ_ENQUEUE(&(tsock->sock.inq), p);

        // transform out-of-order segments into order
        tcp_consume_ofo_queue(tsock);

        //tsock->sock.poll_events |= (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND);
        __sync_or_and_fetch(&tsock->sock.poll_events, (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND));
        selwakeup(&(tsock->sock.selrecv));
    }
    else
    {
        /*
         * Segment passed validation, hence it is in-window
         * but not the left-most sequence. Put into out-of-order queue
         * for later processing
         */
        tcp_data_insert_ordered(&tsock->ofoq, p);

        if(tsock->sackok)
        {
            tcp_calc_sacks(tsock);
        }

        /*
         * RFC5581: A TCP receiver SHOULD send an immediate duplicate ACK when
         * an out-of-order segment arrives. The purpose of this ACK is to
         * inform the sender that a segment was received out-of-order and
         * which sequence number is expected
         */
        tcp_send_ack(tsock);
    }
}


static void tcp_send_next(struct socket_tcp_t *tsock, int amount)
{
    struct packet_t *p;
    struct tcp_hdr_t *tcph;
    struct ipv4_hdr_t *iph;
    int i = 0;

    for(p = tsock->sock.outq.head; p != NULL; p = p->next)
    {
        if(++i > amount)
        {
            break;
        }

        tcp_transmit(tsock, p, tsock->snd_nxt);

        p->seq = tsock->snd_nxt;
        tsock->snd_nxt += p->count;
        p->end_seq = tsock->snd_nxt;

        iph = IPv4_HDR(p);
        //tcph = (struct tcp_hdr_t *)iph->data;
        tcph = (struct tcp_hdr_t *)((char *)iph + (iph->hlen * 4));

        if(tcph->fin)
        {
            tsock->snd_nxt++;
        }
    }
}


static void tcp_input_state(struct socket_t *so, struct tcp_hdr_t *tcph, struct packet_t *p)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;
    int expected, do_free = 1;

    switch(tsock->tcpstate)
    {
        case TCPSTATE_CLOSE:
            free_packet(p);

            if(!(tcph->rst))
            {
                tcp_send_reset(tsock);
            }
            
            return;

        case TCPSTATE_LISTEN:
            free_packet(p);
            return;

        case TCPSTATE_SYN_SENT:
            tcp_synsent(tsock, tcph, p);
            return;
    }

    // 1 - check sequence number
    if(!tcp_verify_segment(tsock, tcph, p))
    {
        /* 
         * RFC793: If an incoming segment is not acceptable, an acknowledgment
         * should be sent in reply (unless the RST bit is set, if so drop
         *  the segment and return)
         */
        if(!tcph->rst)
        {
            tcp_send_ack(tsock);
        }

        DROP_AND_RETURN(p);
    }

    // 2 - check RST
    if(tcph->rst)
    {
        free_packet(p);
        tcp_enter_time_wait(tsock);
        selwakeup(&so->selrecv);
        return;
    }

    // 3 - security and precedence
    // TODO:

    // 4 - check SYN
    if(tcph->syn)
    {
        tcp_send_challenge_ack(tsock, p);
        DROP_AND_RETURN(p);
    }

    // 5 - check ACK is set
    if(!tcph->ack)
    {
        DROP_AND_RETURN(p);
    }

    switch(tsock->tcpstate)
    {
        case TCPSTATE_SYN_RECV:
            if(tsock->snd_una <= tcph->ackno && tcph->ackno < tsock->snd_nxt)
            {
                tsock->tcpstate = TCPSTATE_ESTABLISHED;
            }
            else
            {
                DROP_AND_RETURN(p);
            }

        case TCPSTATE_ESTABLISHED:
        case TCPSTATE_FIN_WAIT_1:
        case TCPSTATE_FIN_WAIT_2:
        case TCPSTATE_CLOSE_WAIT:
        case TCPSTATE_CLOSING:
        case TCPSTATE_LAST_ACK:
            if(tsock->snd_una < tcph->ackno && tcph->ackno <= tsock->snd_nxt)
            {
                tsock->snd_una = tcph->ackno;

                // parse timestamps if this is enabled for the connection
                if(tsock->tsopt)
                {
                    tcp_parse_timestamp(tsock, tcph);
                }

                tcp_rtt(tsock);

                // clear retransmission queue from acknowledged segments
                tcp_clean_rto_queue(tsock, tsock->snd_una);
            }

            if(tcph->ackno < tsock->snd_una)
            {
                // ignore duplicate ACK
                DROP_AND_RETURN(p);
            }

            if(tcph->ackno > tsock->snd_nxt)
            {
                // ACK for segment not sent yet
                DROP_AND_RETURN(p);
            }

            if(tsock->snd_una < tcph->ackno && tcph->ackno <= tsock->snd_nxt)
            {
                // TODO: update send window
            }

            break;
    }

    // if the write queue is empty, then our FIN was acknowledged
    if(so->outq.head == NULL)
    {
        switch(tsock->tcpstate)
        {
            case TCPSTATE_FIN_WAIT_1:
                tsock->tcpstate = TCPSTATE_FIN_WAIT_2;
                break;

            case TCPSTATE_FIN_WAIT_2:
                break;

            case TCPSTATE_CLOSING:
                // In addition to the processing for the ESTABLISHED state, 
                // if the ACK acknowledges our FIN then enter the TIME-WAIT
                // state, otherwise ignore the segment
                tsock->tcpstate = TCPSTATE_TIME_WAIT;
                break;

            case TCPSTATE_LAST_ACK:
                // The only thing that can arrive in this state is an
                // acknowledgment of our FIN. If our FIN is now acknowledged, 
                // delete the TCB, enter the CLOSED state, and return
                free_packet(p);
                tcp_done(tsock);
                return;

            case TCPSTATE_TIME_WAIT:
                // The only thing that can arrive in this state is a
                // retransmission of the remote FIN. Acknowledge it, and restart
                // the 2 MSL timeout
                if(tsock->rcv_nxt == tcph->seqno)
                {
                    tsock->flags |= TCP_FIN;
                    tcp_send_ack(tsock);
                }

                break;
        }
    }

    // 6 - check URG
    if(tcph->urg)
    {
        // TODO:
    }

    // 7 - process the segment payload
    expected = (p->seq == tsock->rcv_nxt);

    switch(tsock->tcpstate)
    {
        case TCPSTATE_ESTABLISHED:
        case TCPSTATE_FIN_WAIT_1:
        case TCPSTATE_FIN_WAIT_2:
            if(tcph->psh || (p->end_seq - p->seq) > 0)
            {
                // User has called shutdown() specifying SHUT_RDWR or SHUT_RD.
                // Discard input and notify the other end.
                /*
                if(so->flags & SOCKET_FLAG_SHUT_REMOTE)
                {
                    tcp_send_reset(tsock);
                }
                else
                */
                {
                    tcp_data_queue(tsock, tcph, p);
                    do_free = 0;
                }
            }
            break;

        case TCPSTATE_CLOSE_WAIT:
        case TCPSTATE_CLOSING:
        case TCPSTATE_LAST_ACK:
        case TCPSTATE_TIME_WAIT:
            // This should not occur, since a FIN has been received from the
            // remote side. Ignore the segment
            break;
    }

    // 8 - check FIN
    if(tcph->fin && expected)
    {
        switch(tsock->tcpstate)
        {
            case TCPSTATE_CLOSE:
            case TCPSTATE_LISTEN:
            case TCPSTATE_SYN_SENT:
                // Do not process, since SEG.SEQ cannot be validated
                free_packet(p);
                return;
        }

        tsock->rcv_nxt++;
        tsock->flags |= TCP_FIN;
        //so->poll_events |= (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND);
        __sync_or_and_fetch(&so->poll_events, (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND));
        tcp_send_ack(tsock);
        selwakeup(&so->selrecv);

        switch(tsock->tcpstate)
        {
            case TCPSTATE_SYN_RECV:
            case TCPSTATE_ESTABLISHED:
                tsock->tcpstate = TCPSTATE_CLOSE_WAIT;
                break;

            case TCPSTATE_FIN_WAIT_1:
                // If our FIN has been ACKed (perhaps in this segment), then
                // enter TIME-WAIT, start the time-wait timer, turn off the other
                // timers; otherwise enter the CLOSING state
                if(so->outq.head == NULL)
                {
                    tcp_enter_time_wait(tsock);
                }
                else
                {
                    tsock->tcpstate = TCPSTATE_CLOSING;
                }
                break;

            case TCPSTATE_FIN_WAIT_2:
                // Enter the TIME-WAIT state.  Start the time-wait timer, turn
                // off the other timers
                tcp_enter_time_wait(tsock);
                break;

            case TCPSTATE_CLOSE_WAIT:
            case TCPSTATE_CLOSING:
            case TCPSTATE_LAST_ACK:
                break;

            case TCPSTATE_TIME_WAIT:
                // Restart the 2 MSL time-wait timeout
                tcp_enter_time_wait(tsock);
                break;
        }
    }

    // congestion control and delacks
    switch(tsock->tcpstate)
    {
        case TCPSTATE_ESTABLISHED:
        case TCPSTATE_FIN_WAIT_1:
        case TCPSTATE_FIN_WAIT_2:
            if(expected)
            {
                tcp_release_delack_timer(tsock);

                /*
                 * RFC1122:  A TCP SHOULD implement a delayed ACK, but an ACK 
                 * should not be excessively delayed; in particular, the delay
                 * MUST be less than 0.5 seconds, and in a stream of full-sized
                 * segments there SHOULD be an ACK for at least every second
                 * segment
                 */
                int pending = MIN(so->outq.count, 3);

                if(tsock->inflight == 0 && pending > 0)
                {
                    tcp_send_next(tsock, pending);
                    tsock->inflight += pending;
                    tcp_rearm_rto_timer(tsock);
                }
                else if(tcph->psh ||
                        ((p->end_seq - p->seq) > 1000 && ++tsock->delacks > 1))
                {
                    tsock->delacks = 0;
                    tcp_send_ack(tsock);
                }
                else if((p->end_seq - p->seq) > 0)
                {
                    tsock->delack = nettimer_add(20, &tcp_send_delack, tsock);
                }
            }
            break;
    }

    if(do_free)
    {
        free_packet(p);
    }
}


#define DROP_PACKET(p)      \
    {                       \
        free_packet(p);     \
        netstats.tcp.drop++;\
        netstats.tcp.err++; \
    }


void tcp_input(struct packet_t *p)
{
    struct ipv4_hdr_t *iph;
    struct tcp_hdr_t *tcph;
    struct socket_t *so;
    size_t dlen;

    netstats.tcp.recv++;
    iph = IPv4_HDR(p);
    //tcph = (struct tcp_hdr_t *)iph->data;
    tcph = (struct tcp_hdr_t *)((char *)iph + (iph->hlen * 4));

    tcph->srcp = tcph->srcp;
    tcph->destp = tcph->destp;
    tcph->seqno = htonl(tcph->seqno);
    tcph->ackno = htonl(tcph->ackno);
    tcph->wnd = htons(tcph->wnd);
    tcph->checksum = htons(tcph->checksum);
    tcph->urgp = htons(tcph->urgp);

    dlen = iph->tlen - (iph->hlen * 4) - (tcph->hlen * 4);
    p->seq = tcph->seqno;
    p->end_seq = p->seq + dlen;

    if(!(so = sock_lookup(IPPROTO_TCP, tcph->srcp, tcph->destp)))
    {
        printk("tcp: cannot find socket for src %d and dest %d\n", tcph->srcp, tcph->destp);
        DROP_PACKET(p);
        return;
    }

    SOCKET_LOCK(so);
    tcp_input_state(so, tcph, p);
    SOCKET_UNLOCK(so);
}


void tcp_notify_closing(struct socket_t *so)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;

    if(so->outq.head == NULL)
    {
        tsock->inflight = 0;
    }

    switch(tsock->tcpstate)
    {
        case TCPSTATE_CLOSE:
        case TCPSTATE_CLOSING:
        case TCPSTATE_LAST_ACK:
        case TCPSTATE_TIME_WAIT:
        case TCPSTATE_FIN_WAIT_1:
        case TCPSTATE_FIN_WAIT_2:
            so->err = -EBADF;
            break;

        case TCPSTATE_LISTEN:
        case TCPSTATE_SYN_SENT:
            tcp_done(tsock);
            break;

        case TCPSTATE_SYN_RECV:
        case TCPSTATE_ESTABLISHED:
            // Queue this until all preceding SENDs have been segmentized, 
            // then form a FIN segment and send it. In any case, enter 
            // FIN-WAIT-1 state
            tsock->tcpstate = TCPSTATE_FIN_WAIT_1;
            tcp_queue_fin(tsock);
            break;

        case TCPSTATE_CLOSE_WAIT:
            // Queue this request until all preceding SENDs have been 
            // segmentized; then send a FIN segment, enter LAST_ACK state
            tsock->tcpstate = TCPSTATE_LAST_ACK;
            tcp_queue_fin(tsock);
            break;

        default:
            printk("tcp: close in unknown TCP state (0x%x)\n", tsock->tcpstate);
            break;
    }
}


struct sockops_t tcp_sockops =
{
    .connect = tcp_connect,
    .connect2 = NULL,
    .socket = tcp_socket,
    .write = tcp_write,
    .read = tcp_read,
    .getsockopt = tcp_getsockopt,
    .setsockopt = tcp_setsockopt,
};

