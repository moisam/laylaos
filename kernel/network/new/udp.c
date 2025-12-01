/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: udp.c
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
 *  \file udp.c
 *
 *  User Datagram Protocol (UDP) implementation.
 */

#include <kernel/net.h>
#include <kernel/net/socket.h>
#include <kernel/net/packet.h>
#include <kernel/net/protocol.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/udp.h>
#include <kernel/net/checksum.h>
#include <mm/kheap.h>

#include "iovec.c"


static struct socket_t *udp_socket(void)
{
    struct socket_t *so;
    
    if(!(so = kmalloc(sizeof(struct socket_t))))
    {
        return NULL;
    }
	    
    A_memset(so, 0, sizeof(struct socket_t));

    return so;
}


static long udp_write(struct socket_t *so, struct msghdr *msg, int kernel)
{
    struct packet_t *p;
    struct udp_hdr_t *h;
    long total, res;

    if((total = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }

    if(!(p = alloc_packet(PACKET_SIZE_UDP(total))))
    {
        printk("udp: insufficient memory for sending packet\n");
        return -ENOMEM;
    }

    packet_add_header(p, -PACKET_SIZE_UDP(0));

    if((res = read_iovec(msg->msg_iov, msg->msg_iovlen, p->data, 
                         p->count, kernel)) < 0)
    {
        free_packet(p);
        return res;
    }

    packet_add_header(p, UDP_HLEN);
    h = UDP_HDR(p);
    h->len = htons(p->count);
    h->srcp = so->local_port;
    h->destp = so->remote_port;

    if(so->domain == AF_INET)
    {
        res = ipv4_send(p, so->local_addr.ipv4,
                           so->remote_addr.ipv4, 
                           IPPROTO_UDP, so->ttl);
        return (res < 0) ? res : total;
    }

    /*
     * TODO: handle IPv6 packets.
     */
    free_packet(p);
    return -EAFNOSUPPORT;
}


static long udp_read(struct socket_t *so, struct msghdr *msg, unsigned int flags)
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

        /*
        this_core->cur_task->woke_by_signal = 0;
        block_task(so, 1);
        */
        set_task_waking_signal(this_core->cur_task, 0);
        set_task_waitchan(this_core->cur_task, so);
        set_task_state(this_core->cur_task, TASK_SLEEPING);
        scheduler();

        SOCKET_LOCK(so);

        if(get_task_waking_signal(this_core->cur_task))
        //if(this_core->cur_task->woke_by_signal)
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


static long udp_getsockopt(struct socket_t *so, int level, int optname,
                           void *optval, int *optlen)
{
    if(so->proto->protocol != IPPROTO_UDP)
    {
        return -EINVAL;
    }

    return socket_getsockopt(so, level, optname, optval, optlen);
}


static long udp_setsockopt(struct socket_t *so, int level, int optname,
                           void *optval, int optlen)
{
    if(so->proto->protocol != IPPROTO_UDP)
    {
        return -EINVAL;
    }

    return socket_setsockopt(so, level, optname, optval, optlen);
}


#define DROP_PACKET(p)      \
    {                       \
        free_packet(p);     \
        netstats.udp.drop++;\
        netstats.udp.err++; \
    }


void udp_input(struct packet_t *p)
{
    struct ipv4_hdr_t *iph;
    struct udp_hdr_t *udph;
    struct socket_t *so;
    int hlen;

    netstats.udp.recv++;
    iph = IPv4_HDR(p);
    udph = (struct udp_hdr_t *)((char *)iph + (iph->hlen * 4));

    /*
    udph->srcp = ntohs(udph->srcp);
    udph->destp = ntohs(udph->destp);
    udph->checksum = ntohs(udph->checksum);
    udph->len = ntohs(udph->len);
    */

    if(!(so = sock_lookup(IPPROTO_UDP, udph->srcp, udph->destp)))
    {
        printk("udp: cannot find socket for src %d and dest %d\n", udph->srcp, udph->destp);
        DROP_PACKET(p);
        return;
    }

    SOCKET_LOCK(so);

    // User has called shutdown() specifying SHUT_RDWR or SHUT_RD.
    // Discard input.
    if(so->flags & SOCKET_FLAG_SHUT_REMOTE)
    {
        DROP_PACKET(p);
        SOCKET_UNLOCK(so);
        return;
    }

    hlen = ETHER_HLEN + (iph->hlen * 4) + UDP_HLEN;
    packet_add_header(p, -hlen);

    if(IFQ_FULL(&so->inq))
    {
        printk("udp: full input queue -- discarding packet\n");
        DROP_PACKET(p);
    }
    else
    {
        IFQ_ENQUEUE(&so->inq, p);
    }

    //so->poll_events |= (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND);
    __sync_or_and_fetch(&so->poll_events, (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND));
    SOCKET_UNLOCK(so);
    selwakeup(&(so->selrecv));
}


struct sockops_t udp_sockops =
{
    .connect = NULL,
    .connect2 = NULL,
    .socket = udp_socket,
    .write = udp_write,
    .read = udp_read,
    .getsockopt = udp_getsockopt,
    .setsockopt = udp_setsockopt,
};

