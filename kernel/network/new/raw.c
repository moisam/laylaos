/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: raw.c
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
 *  \file raw.c
 *
 *  RAW socket interface.
 */

//#define __DEBUG
#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/net.h>
#include <kernel/net/packet.h>
#include <kernel/net/socket.h>
#include <kernel/net/protocol.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/raw.h>
#include <kernel/task.h>
#include <mm/kheap.h>

#include "iovec.c"


static struct socket_t *raw_socket(void)
{
    struct socket_t *so;
    
    if(!(so = kmalloc(sizeof(struct socket_t))))
    {
        return NULL;
    }
	    
    A_memset(so, 0, sizeof(struct socket_t));

    return so;
}


static long raw_write(struct socket_t *so, struct msghdr *msg, int kernel)
{
    struct packet_t *p;
    long total, res;
    int hdrincluded = (so->flags & SOCKET_FLAG_IPHDR_INCLUDED);
    int hdrsize = hdrincluded ? ETHER_HLEN : (ETHER_HLEN + IPv4_HLEN);

    if((total = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }

    if(!(p = alloc_packet(total + hdrsize)))
    {
        printk("raw: insufficient memory for sending packet\n");
        return -ENOMEM;
    }

    packet_add_header(p, -hdrsize);
    p->flags = hdrincluded ? PACKET_FLAG_HDRINCLUDED : 0;

    if((res = read_iovec(msg->msg_iov, msg->msg_iovlen, p->data, 
                         p->count, kernel)) < 0)
    {
        free_packet(p);
        return res;
    }

    if(so->domain == AF_INET)
    {
        res = ipv4_send(p, so->local_addr.ipv4,
                           so->remote_addr.ipv4, 
                           so->proto->protocol, so->ttl);
        return (res < 0) ? res : total;
    }

    /*
     * TODO: handle IPv6 packets.
     */
    free_packet(p);
    return -EAFNOSUPPORT;
}


static long raw_read(struct socket_t *so, struct msghdr *msg, unsigned int flags)
{
    struct packet_t *p;
    size_t size, read = 0;
    size_t plen;

    if((size = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }

try:

    p = so->inq.head;

    if(!p)
    {
        if((flags & MSG_DONTWAIT) || (so->flags & SOCKET_FLAG_NONBLOCK))
        {
            return -EAGAIN;
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

        goto try;
    }

    plen = p->count > size ? size : p->count;

    if(write_iovec(msg->msg_iov, msg->msg_iovlen, p->data, plen, 0) != 0)
    {
        read += plen;
        socket_copy_remoteaddr(so, msg);

        if(!(flags & MSG_PEEK))
        {
            packet_add_header(p, -plen);

            if(p->count == 0)
            {
                IFQ_DEQUEUE(&so->inq, p);
                free_packet(p);
            }
        }
    }

    if(!so->inq.head)
    {
        //so->poll_events &= ~POLLIN;
        __sync_and_and_fetch(&so->poll_events, ~POLLIN);
    }

    return read;
}


static long raw_getsockopt(struct socket_t *so, int level, int optname,
                           void *optval, int *optlen)
{
    return socket_getsockopt(so, level, optname, optval, optlen);
}


static long raw_setsockopt(struct socket_t *so, int level, int optname,
                           void *optval, int optlen)
{
    return socket_setsockopt(so, level, optname, optval, optlen);
}


#define DROP_PACKET(p)      \
    {                       \
        free_packet(p);     \
        netstats.raw.drop++;\
        netstats.raw.err++; \
    }


int raw_input(struct packet_t *p)
{
    struct ipv4_hdr_t *iph4 = IPv4_HDR(p);
    struct socket_t *so;
    int ipver = iph4->ver;
    uint8_t proto = iph4->proto;

    /*
     * FIXME: We only support IPv4 for now.
     */
    if(ipver != 4)
    {
        printk("raw: ignoring packet with ip version %d\n", ipver);
        return -EPROTONOSUPPORT;
    }

    kernel_mutex_lock(&sock_lock);

    for(so = sock_head.next; so != NULL; so = so->next)
    {
        if(!RAW_SOCKET(so) || so->proto->protocol != proto)
        {
            continue;
        }

        if(so->domain != AF_INET)
        {
            continue;
        }

        // deliver the incoming packet to a raw socket if:
        //   - its local address is the destination specified in the
        //     packet's destination address, or
        //   - it is unbound to any local address
        if(so->local_addr.ipv4 == 0 ||
           so->local_addr.ipv4 == iph4->dest)
        {
            netstats.raw.recv++;
            kernel_mutex_unlock(&sock_lock);
            SOCKET_LOCK(so);

            // remove the Ethernet header
            packet_add_header(p, -ETHER_HLEN);

            // User has called shutdown() specifying SHUT_RDWR or SHUT_RD.
            // Discard input.
            if(so->flags & SOCKET_FLAG_SHUT_REMOTE)
            {
                DROP_PACKET(p);
                SOCKET_UNLOCK(so);
                return 0;
            }

            if(!IFQ_FULL(&so->inq))
            {
                IFQ_ENQUEUE(&so->inq, p);
            }
            else
            {
                printk("raw: full input queue -- discarding packet\n");
                DROP_PACKET(p);
            }

            //so->poll_events |= (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND);
            __sync_or_and_fetch(&so->poll_events, (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND));
            SOCKET_UNLOCK(so);
            selwakeup(&(so->selrecv));

            return 0;
        }
    }

    kernel_mutex_unlock(&sock_lock);
    return -ENOENT;
}


struct sockops_t raw_sockops =
{
    .connect = NULL,
    .connect2 = NULL,
    .socket = raw_socket,
    .write = raw_write,
    .read = raw_read,
    .getsockopt = raw_getsockopt,
    .setsockopt = raw_setsockopt,
};

