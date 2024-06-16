/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
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
 *  UDP socket interface.
 */

//#define __DEBUG
#include <errno.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/net.h>
#include <kernel/net/protocol.h>
#include <kernel/net/packet.h>
#include <kernel/net/socket.h>
#include <kernel/net/udp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/dhcp.h>
#include <kernel/user.h>
#include <mm/kheap.h>

#include "../iovec.c"


static int do_enqueue_ipv4(struct socket_t *so, struct packet_t *p)
{
    int hlen = ((uintptr_t)p->transport_hdr - (uintptr_t)p->data) + UDP_HLEN;
    struct ipv4_hdr_t *iph = (struct ipv4_hdr_t *)p->data;
    struct udp_hdr_t *udph = (struct udp_hdr_t *)p->transport_hdr;
    
    p->remote_addr.ipv4.s_addr = iph->src.s_addr;
    p->remote_port = udph->srcp;

    if(so->wakeup == dhcp_client_wakeup)
    {
        // for DHCP, update our ARP table with the sender's address
        // (could be handy if the sender does not reply to ARP requests, e.g.
        //  some gateways/routers)
        arp_update_entry(p->ifp, iph->src.s_addr,
                         &(((struct ether_header_t *)
                            ((uintptr_t)p->data - ETHER_HLEN))->src));
    }

    packet_add_header(p, -hlen);
    kernel_mutex_lock(&so->inq.lock);

    if(!IFQ_FULL(&so->inq))
    {
        IFQ_ENQUEUE(&so->inq, p);
        kernel_mutex_unlock(&so->inq.lock);
        netstats.udp.recv++;
        
        unblock_tasks(&so->recvsel);
        
        if(so->wakeup)
        {
            so->wakeup(so, SOCKET_EV_RD);
        }
        
        return 0;
    }
    else
    {
        kernel_mutex_unlock(&so->inq.lock);
        packet_free(p);
        netstats.udp.drop++;
        netstats.udp.err++;
        return -ENOBUFS;
    }
}


static int socket_udp_receive_ipv4(struct socket_t *so, struct packet_t *p)
{
    struct ipv4_hdr_t *iph = (struct ipv4_hdr_t *)p->data;

    KDEBUG("socket_udp_receive_ipv4: src ");
    KDEBUG_IPV4_ADDR(ntohl(iph->src.s_addr));
    KDEBUG(", dest ");
    KDEBUG_IPV4_ADDR(ntohl(iph->dest.s_addr));
    KDEBUG("\n");
    
    if(ipv4_is_broadcast(iph->dest.s_addr) ||
       ipv4_is_multicast(iph->dest.s_addr))
    {
        struct ipv4_link_t *link;
        
        KDEBUG("udp: received a broad/multi-cast ipv4 packet\n");

        /*
         * TODO: process multicast packets
         */
        
        if((link = ipv4_link_find(&so->local_addr.ipv4)))
        {
            // process if our local IP is ANY or if the source of the
            // broadcast is a neighbour
            if(so->local_addr.ipv4.s_addr == INADDR_ANY || link->ifp == p->ifp)
            {
                return do_enqueue_ipv4(so, p);
            }
        }
    }
    else if(so->local_addr.ipv4.s_addr == INADDR_ANY ||
            so->local_addr.ipv4.s_addr == iph->dest.s_addr)
    {
        KDEBUG("udp: received a unicast ipv4 packet on socket (port %d)\n", ntohs(so->local_port));
        
        return do_enqueue_ipv4(so, p);
    }

    packet_free(p);
    netstats.udp.drop++;
    netstats.udp.proterr++;
    return -EINVAL;
}


static int socket_udp_receive_ipv6(struct socket_t *so, struct packet_t *p)
{
    struct ipv6_hdr_t *iph = (struct ipv6_hdr_t *)p->data;
    struct udp_hdr_t *udph = (struct udp_hdr_t *)p->transport_hdr;
    struct in6_addr dest;
    
    IPV6_COPY(dest.s6_addr, iph->dest.s6_addr);
    IPV6_COPY(p->remote_addr.ipv6.s6_addr, iph->src.s6_addr);
    p->remote_port = udph->srcp;
    
    if(ipv6_is_multicast(iph->dest.s6_addr))
    {
        KDEBUG("udp: ignoring broad/multi-cast ipv6 packet\n");
        /*
         * TODO: process multicast packets
         */
    }
    else if(ipv6_is_unspecified(so->local_addr.ipv6.s6_addr) ||
            ipv6_cmp(&so->local_addr.ipv6, &dest) == 0)
    {
        KDEBUG("udp: received a unicast ipv6 packet on socket (port %d)\n", ntohs(so->local_port));

        int hlen = ((uintptr_t)p->transport_hdr - (uintptr_t)iph) + UDP_HLEN;
        packet_add_header(p, -hlen);
        kernel_mutex_lock(&so->inq.lock);

        if(!IFQ_FULL(&so->inq))
        {
            IFQ_ENQUEUE(&so->inq, p);
            kernel_mutex_unlock(&so->inq.lock);
            netstats.udp.recv++;
            
            unblock_tasks(&so->recvsel);

            if(so->wakeup)
            {
                so->wakeup(so, SOCKET_EV_RD);
            }
            
            return 0;
        }
        else
        {
            kernel_mutex_unlock(&so->inq.lock);
            packet_free(p);
            netstats.udp.drop++;
            netstats.udp.err++;
            return -ENOBUFS;
        }
    }

    packet_free(p);
    netstats.udp.drop++;
    netstats.udp.proterr++;
    return -EINVAL;
}


int socket_udp_receive(struct sockport_t *sp, struct packet_t *p)
{
    //KDEBUG("socket_udp_receive:\n");
    
    struct socket_t *so;

    if(!sp || !p)
    {
        goto err;
    }

    if((so = sp->sockets))
    //for(so = sp->sockets; so != NULL; so = so->next)
    {
        if(GET_IP_VER((struct ipv4_hdr_t *)p->data) == 4)
        {
            return socket_udp_receive_ipv4(so, p);
        }
        else if(GET_IP_VER((struct ipv4_hdr_t *)p->data) == 6)
        {
            return socket_udp_receive_ipv6(so, p);
        }
    }

err:

    packet_free(p);
    netstats.udp.drop++;
    netstats.udp.proterr++;
    return -EINVAL;
}


int socket_udp_open(int domain, int type, struct socket_t **res)
{
    struct socket_t *so;
    
    UNUSED(domain);
    UNUSED(type);
    
    *res = NULL;

    if(!(so = kmalloc(sizeof(struct socket_t))))
    {
        return -ENOBUFS;
    }
	    
    A_memset(so, 0, sizeof(struct socket_t));
    *res = so;

    return 0;
}


static int socket_udp_getsockopt(struct socket_t *so, int level, int optname,
                                 void *optval, int *optlen)
{
    if(so->proto->protocol != IPPROTO_UDP)
    {
        return -EINVAL;
    }

    return socket_getsockopt(so, level, optname, optval, optlen);
}


static int socket_udp_setsockopt(struct socket_t *so, int level, int optname,
                                 void *optval, int optlen)
{
    if(so->proto->protocol != IPPROTO_UDP)
    {
        return -EINVAL;
    }

    return socket_setsockopt(so, level, optname, optval, optlen);
}


int socket_udp_recvmsg(struct socket_t *so, struct msghdr *msg,
                       unsigned int flags)
{
    int size, res;
    struct packet_t *p;

    if((size = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }
    
    //KDEBUG("socket_udp_recvmsg: port %u, size %d\n", ntohs(so->local_port), size);

try:

    kernel_mutex_lock(&so->inq.lock);
    
    if((p = so->inq.head) == NULL)
    {
        kernel_mutex_unlock(&so->inq.lock);
        
        if((flags & MSG_DONTWAIT) || (so->flags & SOCKET_FLAG_NONBLOCK))
        {
            return -EAGAIN;
        }
        
        // wait for input
        KDEBUG("socket_udp_recvmsg: empty queue - sleeping\n");

        // TODO: sleep upto user-defined timeout, instead of indefinitely
        res = block_task2(&so->recvsel, 0);

        if(res != 0)
        {
            KDEBUG("socket_udp_recvmsg: res %d\n", res);
            return (res == EWOULDBLOCK) ? -ETIMEDOUT : -EINTR;
        }

    	if(!(so->state & SOCKET_STATE_BOUND))
    	{
            KDEBUG("socket_udp_recvmsg: socket not bound\n");
            return -EADDRNOTAVAIL;
    	}
        
        goto try;
    }

    KDEBUG("socket_udp_recvmsg: p->count %u, size %d\n", p->count, size);
    
    if(p->count > (size_t)size)
    {
        res = write_iovec(msg->msg_iov, msg->msg_iovlen, p->data, size, 0);

        KDEBUG("socket_udp_recvmsg: got %d bytes\n", res);
        
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

        KDEBUG("socket_udp_recvmsg: got %d bytes\n", res);
        
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

    KDEBUG("socket_udp_recvmsg: res %d\n", res);
    
    return res;
}


struct sockops_t udp_sockops =
{
    .connect2 = NULL,
    .socket = socket_udp_open,
    .getsockopt = socket_udp_getsockopt,
    .setsockopt = socket_udp_setsockopt,
    .recvmsg = socket_udp_recvmsg,
};

