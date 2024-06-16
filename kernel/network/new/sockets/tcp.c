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
 *  TCP socket interface.
 */

//#define __DEBUG
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <kernel/laylaos.h>
#include <kernel/net/protocol.h>
#include <kernel/net/socket.h>
#include <kernel/net/packet.h>
#include <kernel/net/tcp.h>
#include <kernel/net/ipv4.h>
#include <kernel/user.h>

#include "../iovec.c"


void socket_tcp_delete(struct socket_t *so)
{
    if(so && so->parent)
    {
        so->parent->pending_connections--;
    }
}


static int socket_tcp_open(int domain, int type, struct socket_t **res)
{
    UNUSED(type);
    
    return tcp_open(domain, res);
}


static int socket_tcp_getsockopt(struct socket_t *so, int level, int optname,
                                 void *optval, int *optlen)
{
    struct socket_tcp_t *tsock = (struct socket_tcp_t *)so;

    if(so->proto->protocol != IPPROTO_TCP)
    {
        return -EINVAL;
    }
    
    if(!optval || !optlen)
    {
        return -EFAULT;
    }

    if(level == SOL_SOCKET)
    {
        switch(optname)
        {
            case SO_ERROR:
                /*
                 * TODO: implement socket errors properly
                 */
            	if(so->state & SOCKET_STATE_CONNECTED)
            	{
                    *(int *)optval = 0;
                    *optlen = sizeof(int);
                    return 0;
            	}
            	
            	return -EHOSTUNREACH;

            case SO_ACCEPTCONN:
                // return 1 if socket is listening, 0 if not
                *(int *)optval = 
                            ((so->state & SOCKET_STATE_TCP) == 
                                        SOCKET_STATE_TCP_LISTEN);
                *optlen = sizeof(int);
                return 0;

            default:
                return socket_getsockopt(so, level, optname, optval, optlen);
        }
    }
    else if(level == IPPROTO_IP)
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


static int socket_tcp_setsockopt(struct socket_t *so, int level, int optname,
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
    //COPY_VAL_FROM_USER(&tmp, (int *)optval);

    if(level == SOL_SOCKET)
    {
        struct linger li;

        switch(optname)
        {
            case SO_LINGER:
                if(optlen < (int)sizeof(struct linger))
                {
                    return -EINVAL;
                }

                /*
                COPY_FROM_USER(&li, optval, sizeof(struct linger));
                
                if(li.l_onoff)
                {
                    // convert seconds to ticks
                    tsock->linger_timeout = li.l_linger * PIT_FREQUENCY;
                }
                else
                {
                    tsock->linger_timeout = 0;
                }
                */

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


static struct socket_t *socket_tcp_receive_ipv4(struct socket_t *so,
                                                struct packet_t *p)
{
    struct ipv4_hdr_t *iph = (struct ipv4_hdr_t *)p->data;
    struct tcp_hdr_t *tcph = (struct tcp_hdr_t *)p->transport_hdr;
    
    if((so->remote_port == tcph->srcp) &&
       (so->remote_addr.ipv4.s_addr == iph->src.s_addr) &&
       ((so->local_addr.ipv4.s_addr == INADDR_ANY) ||
        (so->local_addr.ipv4.s_addr == iph->dest.s_addr)))
    {
        // either local socket is ANY, or matches destination
        return so;
    }
    else if((so->remote_port == 0) && // listening, not connected
            ((so->local_addr.ipv4.s_addr == INADDR_ANY) ||
             (so->local_addr.ipv4.s_addr == iph->dest.s_addr)))
    {
        // either local socket is ANY, or matches destination
        return so;
    }
    
    return NULL;
}


static struct socket_t *socket_tcp_receive_ipv6(struct socket_t *so,
                                                struct packet_t *p)
{
    struct ipv6_hdr_t *iph = (struct ipv6_hdr_t *)p->data;
    struct tcp_hdr_t *tcph = (struct tcp_hdr_t *)p->transport_hdr;
    
    if((so->remote_port == tcph->srcp) &&
       (!memcmp(so->remote_addr.ipv6.s6_addr, iph->src.s6_addr, 16)) &&
       ((!memcmp(so->local_addr.ipv6.s6_addr, IPv6_ANY, 16)) ||
        (!memcmp(so->local_addr.ipv6.s6_addr, iph->dest.s6_addr, 16))))
    {
        // either local socket is ANY, or matches destination
        return so;
    }
    else if((so->remote_port == 0) && // listening, not connected
            ((!memcmp(so->local_addr.ipv6.s6_addr, IPv6_ANY, 16)) ||
             (!memcmp(so->local_addr.ipv6.s6_addr, iph->dest.s6_addr, 16))))
    {
        // either local socket is ANY, or matches destination
        return so;
    }
    
    return NULL;
}


int socket_tcp_receive(struct sockport_t *sp, struct packet_t *p)
{
    struct socket_t *so, *found = NULL;

    if(!sp || !p)
    {
        goto err;
    }

    for(so = sp->sockets; so != NULL; so = so->next)
    {
        if(GET_IP_VER((struct ipv4_hdr_t *)p->data) == 4)
        {
            found = socket_tcp_receive_ipv4(so, p);
        }
        else if(GET_IP_VER((struct ipv4_hdr_t *)p->data) == 6)
        {
            found = socket_tcp_receive_ipv6(so, p);
        }
        
        if(found)
        {
            if(found->remote_port)
            {
                // only deliver if socket is connected
                tcp_input(so, p);

                unblock_tasks(&so->recvsel);
                
                if(so->pending_events && so->wakeup)
                {
                    so->wakeup(so, so->pending_events);
                    
                    if(!so->parent)
                    {
                        so->pending_events = 0;
                    }
                }
                
                return 0;
            }
        }
    }
    
    KDEBUG("tcp: cannot find socket to receive packet\n");

err:

    packet_free(p);
    netstats.tcp.proterr++;
    return -EINVAL;
}


int socket_tcp_recvmsg(struct socket_t *so, struct msghdr *msg,
                       unsigned int flags)
{
    printk("%s: 1\n", __func__);
    
    int size, res;
    struct socket_tcp_t *tsock;

    if((size = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }

    printk("socket_tcp_recvmsg: size %d\n", size);

    if(so->proto->protocol != IPPROTO_TCP)
    {
        return -EINVAL;
    }

try:

    tsock = (struct socket_tcp_t *)so;
    kernel_mutex_lock(&so->inq.lock);

    printk("socket_tcp_recvmsg: so->state 0x%x\n", so->state);

    if((so->state & SOCKET_STATE_SHUT_REMOTE) &&
       (so->inq.count == 0))
    {
        kernel_mutex_unlock(&so->inq.lock);
        return -ESHUTDOWN;
    }

    KDEBUG("socket_tcp_recvmsg: packets %d\n", ((struct socket_tcp_t *)so)->tcp_inq.packets);
    
    if(so->inq.count == 0)
    {
        kernel_mutex_unlock(&so->inq.lock);

        if((flags & MSG_DONTWAIT) || (so->flags & SOCKET_FLAG_NONBLOCK))
        {
            return -EAGAIN;
        }

        // wait for input
        // TODO: sleep upto user-defined timeout, instead of indefinitely
        res = block_task2(&so->recvsel, 0);
        
        if(res != 0)
        {
            KDEBUG("socket_tcp_recvmsg: aborting - res %d\n", res);
            return (res == EWOULDBLOCK) ? -ETIMEDOUT : -EINTR;
        }

    	if(!(so->state & SOCKET_STATE_BOUND))
    	{
            KDEBUG("socket_tcp_recvmsg: aborting - so->state 0x%x\n", so->state);
            return -EADDRNOTAVAIL;
    	}
    	
    	goto try;
    }
    
    kernel_mutex_unlock(&so->inq.lock);

    KDEBUG("socket_tcp_recvmsg: reading\n");

    res = tcp_read(so, msg, flags);
    
    if(res == 0)
    {
        printk("socket_tcp_recvmsg: no luck -- retrying\n");

        // for some reason, we didn't read anything.
        // try again if the socket is still connected.
        goto try;
    }
    
    return res;
}


struct sockops_t tcp_sockops =
{
    .connect2 = NULL,
    .socket = socket_tcp_open,
    .getsockopt = socket_tcp_getsockopt,
    .setsockopt = socket_tcp_setsockopt,
    .recvmsg = socket_tcp_recvmsg,
};

