/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
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

#define __DEBUG
#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/net.h>
#include <kernel/net/packet.h>
#include <kernel/net/socket.h>
#include <kernel/net/protocol.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/icmp6.h>
#include <kernel/net/raw.h>
#include <kernel/task.h>
#include <mm/kheap.h>

#include "../iovec.c"

struct netif_queue_t raw_inq = { 0, };


int raw_receive(struct packet_t *p)
{
    struct ipv4_hdr_t *iph4 = (struct ipv4_hdr_t *)p->data;
    struct ipv6_hdr_t *iph6 = (struct ipv6_hdr_t *)p->data;
    struct socket_t *so;
    int ipver = GET_IP_VER(iph4);
    uint8_t proto = iph4->proto;
    int found = 0;
    
    //KDEBUG("raw: received packet with ipv%d\n", ipver);

    kernel_mutex_lock(&sockraw_lock);
    
    for(so = raw_socks; so != NULL; so = so->next)
    {
        KDEBUG("raw: proto %d, %d\n", so->proto->protocol, proto);

        if(so->proto->protocol != proto)
        {
            continue;
        }

        if(ipver == 4 && so->domain != AF_INET)
        {
            continue;
        }

        if(ipver == 6 && so->domain != AF_INET6)
        {
            continue;
        }

        // deliver the incoming packet to a raw socket if:
        //   - its local address is the destination specified in the
        //     packet's destination address, or
        //   - it is unbound to any local address
        if(so->domain == AF_INET)
        {
            if(so->local_addr.ipv4.s_addr == 0 ||
               so->local_addr.ipv4.s_addr == iph4->dest.s_addr)
            {
                found = 1;
            }
        }
        else
        {
            if(!memcmp(so->local_addr.ipv6.s6_addr, IPv6_ANY, 16) ||
               !memcmp(so->local_addr.ipv6.s6_addr, iph6->dest.s6_addr, 16))
            {
                // see if we need to filter the packet
                if(ipver == 6 && iph6->proto == IPPROTO_ICMPV6)
                {
                    struct socket_raw_t *rso = (struct socket_raw_t *)so;
                    struct icmp6_hdr_t *icmph = (struct icmp6_hdr_t *)p->transport_hdr;

                    if(ICMP6_FILTER_WILLBLOCK(icmph->type, &rso->icmp6_filter))
                    {
                        // discard packet
                        KDEBUG("raw: filtering out packet\n");
                        kernel_mutex_unlock(&sockraw_lock);

                        packet_free(p);
                        netstats.raw.drop++;

                        return 0;
                    }
                }

                found = 1;
            }
        }

        KDEBUG("raw: found %d\n", found);

        if(found)
        {
            kernel_mutex_unlock(&sockraw_lock);

            //return raw_enqueue(so, p);

            kernel_mutex_lock(&so->inq.lock);

            if(!IFQ_FULL(&so->inq))
            {
                KDEBUG("raw: enqueuing packet\n");
                IFQ_ENQUEUE(&so->inq, p);
                kernel_mutex_unlock(&so->inq.lock);
                netstats.raw.recv++;

                unblock_tasks(&so->inq);
                unblock_tasks(&so->recvsel);
            }
            else
            {
                KDEBUG("raw: discarding packet\n");
                kernel_mutex_unlock(&so->inq.lock);

                packet_free(p);
                netstats.raw.drop++;
                netstats.raw.err++;
            }

            return 0;
        }
    }

    kernel_mutex_unlock(&sockraw_lock);
    return -ENOENT;
}


/*
 * Push a packet on the outgoing queue.
 * Called from the socket layer.
 */
int raw_push(struct packet_t *p)
{
    int res, count;

    p->frag = 0;
    count = p->count;
    
    if((res = ip_push(p)) == 0)
    {
        return count;
    }
    else
    {
        return res;
    }
}


int socket_raw_open(int domain, int type, struct socket_t **res)
{
    struct socket_t *so;
    
    UNUSED(domain);
    UNUSED(type);
    
    *res = NULL;

    if(!(so = kmalloc(sizeof(struct socket_raw_t))))
    {
        return -ENOBUFS;
    }
	    
    A_memset(so, 0, sizeof(struct socket_raw_t));
    *res = so;

    return 0;
}


static int socket_raw_getsockopt(struct socket_t *so, int level, int optname,
                                  void *optval, int *optlen)
{
    if(level == IPPROTO_ICMPV6)
    {
        // must be an IPv6 socket
        if(so->domain != AF_INET6 || so->type != SOCK_RAW)
        {
            return -EINVAL;
        }

        switch(optname)
        {
            case ICMP6_FILTER:
            {
                struct socket_raw_t *rso = (struct socket_raw_t *)so;

                if(!optval || !optlen || *optlen < (int)sizeof(struct icmp6_filter))
                {
                    return -EFAULT;
                }

                A_memcpy(optval, &rso->icmp6_filter, sizeof(struct icmp6_filter));
                *optlen = sizeof(struct icmp6_filter);

                return 0;
            }
        }

        return -ENOPROTOOPT;
    }

    return socket_getsockopt(so, level, optname, optval, optlen);
}


static int socket_raw_setsockopt(struct socket_t *so, int level, int optname,
                                  void *optval, int optlen)
{
    if(level == IPPROTO_ICMPV6)
    {
        // must be an IPv6 socket
        if(so->domain != AF_INET6 || so->type != SOCK_RAW)
        {
            return -EINVAL;
        }

        switch(optname)
        {
            case ICMP6_FILTER:
            {
                struct socket_raw_t *rso = (struct socket_raw_t *)so;

                if(!optval || optlen > (int)sizeof(struct icmp6_filter))
                {
                    return -EINVAL;
                }

                A_memcpy(&rso->icmp6_filter, optval, optlen);

                return 0;
            }
        }

        return -ENOPROTOOPT;
    }

    return socket_setsockopt(so, level, optname, optval, optlen);
}


int socket_raw_recvmsg(struct socket_t *so, struct msghdr *msg,
                        unsigned int flags)
{
    int size, res;
    struct packet_t *p;

    if((size = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }

try:

    kernel_mutex_lock(&so->inq.lock);
    
    if((p = so->inq.head) == NULL)
    {
        kernel_mutex_unlock(&so->inq.lock);
        
        if((flags & MSG_DONTWAIT) || (so->flags & SOCKET_FLAG_NONBLOCK))
        {
            return -EAGAIN;
        }

        KDEBUG("socket_raw_recvmsg: empty queue - sleeping\n");

        // TODO: sleep upto user-defined timeout, instead of indefinitely
        //res = block_task2(so, 0);
        res = block_task2(&so->inq, 0);
        
        if(res != 0)
        {
            KDEBUG("socket_raw_recvmsg: res %d\n", res);
            return (res == EWOULDBLOCK) ? -ETIMEDOUT : -EINTR;
        }

    	if(!(so->state & SOCKET_STATE_BOUND))
    	{
            KDEBUG("socket_raw_recvmsg: socket not bound\n");
            return -EADDRNOTAVAIL;
    	}
        
        goto try;
    }

    KDEBUG("socket_raw_recvmsg: p->count %u, size %d\n", p->count, size);
    
    if(p->count > (size_t)size)
    {
        res = write_iovec(msg->msg_iov, msg->msg_iovlen, p->data, size, 0);

        KDEBUG("socket_raw_recvmsg: got %d bytes\n", res);
        
        if(res >= 0)
        {
            if(!(flags & MSG_PEEK))
            {
                packet_add_header(p, -res);
            }
            
            kernel_mutex_unlock(&so->inq.lock);
            packet_copy_remoteaddr(so, p, msg);
        }
        else
        {
            kernel_mutex_unlock(&so->inq.lock);
        }
    }
    else
    {
        res = write_iovec(msg->msg_iov, msg->msg_iovlen, p->data, p->count, 0);

        KDEBUG("socket_raw_recvmsg: got %d bytes\n", res);
        
        if(res >= 0)
        {
            if(!(flags & MSG_PEEK))
            {
                IFQ_DEQUEUE(&so->inq, p);
            }

            kernel_mutex_unlock(&so->inq.lock);
            packet_copy_remoteaddr(so, p, msg);
            packet_free(p);
        }
        else
        {
            kernel_mutex_unlock(&so->inq.lock);
        }
    }

    KDEBUG("socket_raw_recvmsg: res %d\n", res);
    
    return res;
}


/*
void raw_init(void)
{
}
*/


struct sockops_t raw_sockops =
{
    .connect2 = NULL,
    .socket = socket_raw_open,
    .getsockopt = socket_raw_getsockopt,
    .setsockopt = socket_raw_setsockopt,
    .recvmsg = socket_raw_recvmsg,
};

