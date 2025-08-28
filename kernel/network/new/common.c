/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: common.c
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
 *  \file common.c
 *
 *  Helper functions used by different components in the socket layer.
 */

#define __DEBUG
#include <errno.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/net/protocol.h>
#include <kernel/net/socket.h>
#include <kernel/net/packet.h>
#include <kernel/net/ipv4.h>
#include <kernel/user.h>

#define MIN_QUEUE_SIZE              (128)


#define RETURN_FLAG(flag)                       \
    *(int *)optval = !!(so->flags & (flag));    \
    *optlen = sizeof(int);                      \
    return 0;


long socket_getsockopt(struct socket_t *so, int level, int optname,
                       void *optval, int *optlen)
{
    if(!optval || !optlen)
    {
        return -EFAULT;
    }

    if(level == SOL_SOCKET)
    {
        switch(optname)
        {
            case SO_ERROR:
                *(int *)optval = so->err;
                *optlen = sizeof(int);
                return 0;

            case SO_DOMAIN:
                *(int *)optval = so->domain;
                *optlen = sizeof(int);
                return 0;

            case SO_PROTOCOL:
                *(int *)optval = so->proto->protocol;
                *optlen = sizeof(int);
                return 0;

            case SO_TYPE:
                *(int *)optval = so->type;
                *optlen = sizeof(int);
                return 0;

            case SO_RCVBUF:
                *(int *)optval = so->inq.max;
                *optlen = sizeof(int);
                return 0;

            case SO_SNDBUF:
                *(int *)optval = so->outq.max;
                *optlen = sizeof(int);
                return 0;

            case SO_BROADCAST:
                RETURN_FLAG(SOCKET_FLAG_BROADCAST);

            case SO_ACCEPTCONN:
                // return 1 if socket is listening, 0 if not
                *(int *)optval = !!(so->state == SOCKSTATE_LISTENING);
                *optlen = sizeof(int);
                return 0;
        }
    }
    else if(level == IPPROTO_IP)
    {
        // must be an IPv4 socket
        if(so->domain != AF_INET)
        {
            return -EINVAL;
        }

        switch(optname)
        {
            case IP_TOS:
                *(int *)optval = so->tos;
                *optlen = sizeof(int);
                return 0;

            case IP_TTL:
                *(int *)optval = so->ttl;
                *optlen = sizeof(int);
                return 0;

            case IP_HDRINCL:
                // this only works on raw sockets
                if(so->type != SOCK_RAW)
                {
                    return -EINVAL;
                }

                RETURN_FLAG(SOCKET_FLAG_IPHDR_INCLUDED);

            case IP_RECVOPTS:
                // does not work on stream sockets
                if(so->type == SOCK_STREAM)
                {
                    return -EINVAL;
                }

                RETURN_FLAG(SOCKET_FLAG_RECVOPTS);

            case IP_RECVTTL:
                // does not work on stream sockets
                if(so->type == SOCK_STREAM)
                {
                    return -EINVAL;
                }

                RETURN_FLAG(SOCKET_FLAG_RECVTTL);

            case IP_RECVTOS:
                RETURN_FLAG(SOCKET_FLAG_RECVTOS);
        }
    }
    else if(level == IPPROTO_IPV6)
    {
        // must be an IPv6 socket
        if(so->domain != AF_INET6)
        {
            return -EINVAL;
        }

        switch(optname)
        {
            case IPV6_UNICAST_HOPS:
                *(int *)optval = so->ttl;
                *optlen = sizeof(int);
                return 0;

            case IPV6_RECVHOPLIMIT:
            case IPV6_HOPLIMIT:
                // does not work on stream sockets
                if(so->type == SOCK_STREAM)
                {
                    return -EINVAL;
                }

                RETURN_FLAG(SOCKET_FLAG_RECVTTL);
        }
    }

    return -ENOPROTOOPT;
}


#define TOGGLE_SOCKET_FLAG(so, flag, val)   \
    if(val) so->flags |= flag;              \
    else so->flags &= ~flag;


long socket_setsockopt(struct socket_t *so, int level, int optname,
                       void *optval, int optlen)
{
    int tmp;

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
        switch(optname)
        {
            case SO_RCVBUF:
                if(tmp < MIN_QUEUE_SIZE)
                {
                    return -EINVAL;
                }

                so->inq.max = tmp;
                return 0;

            case SO_SNDBUF:
                if(tmp < MIN_QUEUE_SIZE)
                {
                    return -EINVAL;
                }

                so->inq.max = tmp;
                return 0;

            case SO_BROADCAST:
                // this only works on datagram sockets
                if(so->type != SOCK_DGRAM && so->type != SOCK_RAW)
                {
                    return -EINVAL;
                }

                TOGGLE_SOCKET_FLAG(so, SOCKET_FLAG_BROADCAST, tmp);
                return 0;
        }
    }
    else if(level == IPPROTO_IP)
    {
        // must be an IPv4 socket
        if(so->domain != AF_INET)
        {
            return -EINVAL;
        }

        switch(optname)
        {
            case IP_TOS:
                if(tmp < 0)
                {
                    return -EINVAL;
                }

                so->tos = tmp;
                return 0;

            case IP_TTL:
                if(tmp < -1 || tmp > 255)
                {
                    return -EINVAL;
                }

                so->ttl = tmp;
                return 0;

            case IP_HDRINCL:
                // this only works on raw sockets
                if(so->type != SOCK_RAW)
                {
                    return -EINVAL;
                }

                TOGGLE_SOCKET_FLAG(so, SOCKET_FLAG_IPHDR_INCLUDED, tmp);
                return 0;

            case IP_RECVOPTS:
                // does not work on stream sockets
                if(so->type == SOCK_STREAM)
                {
                    return -EINVAL;
                }

                TOGGLE_SOCKET_FLAG(so, SOCKET_FLAG_RECVOPTS, tmp);
                return 0;

            case IP_RECVTTL:
                // does not work on stream sockets
                if(so->type == SOCK_STREAM)
                {
                    return -EINVAL;
                }

                TOGGLE_SOCKET_FLAG(so, SOCKET_FLAG_RECVTTL, tmp);
                return 0;

            case IP_RECVTOS:
                TOGGLE_SOCKET_FLAG(so, SOCKET_FLAG_RECVTOS, tmp);
                return 0;
        }
    }
    else if(level == IPPROTO_IPV6)
    {
        // must be an IPv6 socket
        if(so->domain != AF_INET6)
        {
            return -EINVAL;
        }

        switch(optname)
        {
            case IPV6_UNICAST_HOPS:
                if(tmp < -1 || tmp > 255)
                {
                    return -EINVAL;
                }

                so->ttl = tmp;
                return 0;

            case IPV6_RECVHOPLIMIT:
            case IPV6_HOPLIMIT:
                // does not work on stream sockets
                if(so->type == SOCK_STREAM)
                {
                    return -EINVAL;
                }

                TOGGLE_SOCKET_FLAG(so, SOCKET_FLAG_RECVTTL, tmp);
                return 0;
        }
    }

    return -ENOPROTOOPT;
}

