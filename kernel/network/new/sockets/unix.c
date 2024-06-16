/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: unix.c
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
 *  \file unix.c
 *
 *  UNIX socket interface.
 */

#define __DEBUG
#include <errno.h>
#include <fcntl.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/net.h>
#include <kernel/net/protocol.h>
#include <kernel/net/packet.h>
#include <kernel/net/socket.h>
#include <mm/kheap.h>

#include "../../../kernel/task_funcs.c"
#include "../iovec.c"

#define UNIX_MIN_QUEUE_SIZE         (128)


int socket_unix_bind(struct socket_t *so, 
                     struct sockaddr *name, socklen_t namelen)
{
    struct sockaddr_un *sun;
    struct fs_node_t *node;
    char *p, *p2;
    int res;

    if(namelen < sizeof(sa_family_t) || namelen > sizeof(struct sockaddr_un))
    {
        return -EFAULT;
    }

    sun = (struct sockaddr_un *)name;

    if(sun->sun_family != AF_UNIX)
    {
        return -EAFNOSUPPORT;
    }
        
    // ensure the passed path is null-terminated
    p2 = &sun->sun_path[namelen - sizeof(sa_family_t)];

    for(p = sun->sun_path; p < p2; p++)
    {
        if(*p == '\0')
        {
            break;
        }
    }

    // null char not found, bail out
    if(p == p2)
    {
        return -ENAMETOOLONG;   // TODO: is this the right errno here?
    }

    A_memset(&so->local_addr.sun, 0, sizeof(struct sockaddr_un));
    A_memcpy(&so->local_addr.sun, name, p - sun->sun_path);
    so->local_port = 0;

#define open_flags      OPEN_KERNEL_CALLER | OPEN_NOFOLLOW_SYMLINK

    // create the socket
    if((res = vfs_mknod(so->local_addr.sun.sun_path,
                        (S_IFSOCK | 0666), 0, AT_FDCWD, 
                        open_flags, &node)) != 0)
    {
        return (res == -EEXIST) ? -EADDRINUSE : res;
    }

#undef open_flags

    //node->data = so;
    release_node(node);
    
    return 0;
}


int socket_unix_connect(struct socket_t *so, 
                        struct sockaddr *name, socklen_t namelen)
{
    struct socket_t *serversock, *newsock;
    struct sockaddr_un *sun;
    struct fs_node_t *node;
    char *p, *p2;
    int res;

    if(namelen < sizeof(sa_family_t) || namelen > sizeof(struct sockaddr_un))
    {
        return -EFAULT;
    }

    sun = (struct sockaddr_un *)name;

    if(sun->sun_family != AF_UNIX)
    {
        return -EAFNOSUPPORT;
    }

    // ensure the passed path is null-terminated
    p2 = &sun->sun_path[namelen - sizeof(sa_family_t)];

    for(p = sun->sun_path; p < p2; p++)
    {
        if(*p == '\0')
        {
            break;
        }
    }

    // null char not found, bail out
    if(p == p2)
    {
        return -ENAMETOOLONG;   // TODO: is this the right errno here?
    }

    A_memset(&so->remote_addr.sun, 0, sizeof(struct sockaddr_un));
    A_memcpy(&so->remote_addr.sun, name, p - sun->sun_path);
    so->remote_port = 0;

#define open_flags      OPEN_KERNEL_CALLER | OPEN_NOFOLLOW_SYMLINK

    // find the remote socket
	if((res = vfs_open_internal(so->remote_addr.sun.sun_path,
	                            AT_FDCWD, &node, open_flags)) != 0)
	{
        A_memset(&so->remote_addr.sun, 0, sizeof(struct sockaddr_un));
        return res;
	}
	
	if(!IS_SOCKET(node))
	{
        A_memset(&so->remote_addr.sun, 0, sizeof(struct sockaddr_un));
        return -ECONNREFUSED;
	}

    if(has_access(node, WRITE, 0) != 0)
    {
        A_memset(&so->remote_addr.sun, 0, sizeof(struct sockaddr_un));
        return -EPERM;
    }

    release_node(node);

    // create a new socket
    if((res = sock_create(AF_UNIX, so->type,
                          so->proto->protocol, &newsock)) != 0)
    {
        A_memset(&so->remote_addr.sun, 0, sizeof(struct sockaddr_un));
        return res;
    }

#undef open_flags

    newsock->pid = cur_task->pid;
    newsock->uid = cur_task->euid;
    newsock->gid = cur_task->egid;

    // find the server socket
    kernel_mutex_lock(&sockunix_lock);

    for(serversock = unix_socks; serversock != NULL; serversock = serversock->next)
    {
        if(memcmp(serversock->local_addr.sun.sun_path,
                  so->remote_addr.sun.sun_path,
                  p - sun->sun_path + 1) == 0)
        {
            break;
        }
    }

    if(serversock == NULL || !(serversock->state & SOCKET_STATE_LISTENING))
    {
        kernel_mutex_unlock(&sockunix_lock);
        kfree(newsock);
        A_memset(&so->remote_addr.sun, 0, sizeof(struct sockaddr_un));
        return -ECONNREFUSED;
    }

    if(serversock->max_backlog &&
       serversock->pending_connections >= serversock->max_backlog)
    {
        kernel_mutex_unlock(&sockunix_lock);
        kfree(newsock);
        A_memset(&so->remote_addr.sun, 0, sizeof(struct sockaddr_un));
        return -EAGAIN;
    }

    A_memcpy(&newsock->local_addr.sun, &serversock->local_addr.sun, 
             sizeof(struct sockaddr_un));
    A_memcpy(&newsock->remote_addr.sun, &so->local_addr.sun,
             sizeof(struct sockaddr_un));
    newsock->state = SOCKET_STATE_BOUND | SOCKET_STATE_CONNECTED;
    newsock->parent = serversock;
    newsock->pairedsock = so;
    //so->state |= SOCKET_STATE_CONNECTED;
    so->pairedsock = newsock;
    serversock->pending_connections++;

    kernel_mutex_unlock(&sockunix_lock);

    socket_add(newsock);
    unblock_tasks(&serversock->pending_connections);

    return 0;
}


int socket_unix_open(int domain, int type, struct socket_t **res)
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


static int socket_unix_getsockopt(struct socket_t *so, int level, int optname,
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
            case SO_ACCEPTCONN:
                // return 1 if socket is listening, 0 if not
                return !!(so->state & SOCKET_STATE_LISTENING);

            case SO_DOMAIN:
                *(int *)optval = so->domain;
                *optlen = sizeof(int);
                return 0;

            case SO_PROTOCOL:
                *(int *)optval = 0;     // XXX
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

            case SO_PEERCRED:
                {
                    int i;
                    struct xucred creds;
                    struct task_t *t;

                    if(!optval || *optlen < (int)sizeof(creds))
                    {
                        return -EINVAL;
                    }
                    
                    if(!so->pairedsock ||
                       !(t = get_task_by_id(so->pairedsock->pid)))
                    {
                        return -EINVAL;
                    }
                    
                    creds.cr_uid = t->uid;
                    creds.cr_ngroups = 0;

                    for(i = 0; i < NGROUPS_MAX; i++)
                    {
                        if(t->extra_groups[i] != (gid_t)-1)
                        {
                            creds.cr_groups[i] = t->extra_groups[i];
                            creds.cr_ngroups++;
                        }
                        else
                        {
                            creds.cr_groups[i] = 0;
                        }
                    }
                    
                    return copy_to_user(optval, &creds, sizeof(creds));
                }

            default:
                return -ENOPROTOOPT;
        }
    }

    return -ENOPROTOOPT;
}


static int socket_unix_setsockopt(struct socket_t *so, int level, int optname,
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
    //COPY_VAL_FROM_USER(&tmp, (int *)optval);

    KDEBUG("socket_unix_setsockopt: level %d, optname %d, optval %d\n", level, optname, tmp);

    if(level == SOL_SOCKET)
    {
        switch(optname)
        {
            case SO_RCVBUF:
                if(tmp < UNIX_MIN_QUEUE_SIZE)
                {
                    return -EINVAL;
                }

                so->inq.max = tmp;

                return 0;

            case SO_SNDBUF:
                if(tmp < UNIX_MIN_QUEUE_SIZE)
                {
                    return -EINVAL;
                }

                so->inq.max = tmp;

                return 0;
            
            default:
                return -ENOPROTOOPT;
        }
    }

    return -ENOPROTOOPT;
}


int socket_unix_recvmsg(struct socket_t *so, struct msghdr *msg,
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
        
        // don't wait if peer has disconnected
        if(!so->pairedsock)
        {
            return -ECONNRESET;
        }
        
        if((flags & MSG_DONTWAIT) || (so->flags & SOCKET_FLAG_NONBLOCK))
        {
            return -EAGAIN;
        }

        // wait for input
        KDEBUG("socket_unix_recvmsg: empty queue - sleeping\n");


        // A race condition happens here when we are receiving lots of
        // packets and we go to sleep. The sender would try to wake us, but
        // as we are in the process of sleeping, we sleep forever.
        // The current workaround is to sleep for a few seconds and then
        // wakeup and check the queue. A better solution should avoid the
        // race condition altogether!
        res = block_task2(&so->inq, PIT_FREQUENCY * 2);
        
        if(res == EINTR)
        {
            return -EINTR;
        }

    	if(!(so->state & SOCKET_STATE_BOUND))
    	{
            KDEBUG("socket_unix_recvmsg: socket not bound\n");
            return -EADDRNOTAVAIL;
    	}
        
        goto try;
    }

    KDEBUG("socket_unix_recvmsg: p->count %u, size %d\n", p->count, size);
    
    if(p->count > (size_t)size)
    {
        res = write_iovec(msg->msg_iov, msg->msg_iovlen, p->data, size, 0);

        KDEBUG("socket_unix_recvmsg: got %d bytes\n", res);
        
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

        KDEBUG("socket_unix_recvmsg: got %d bytes\n", res);
        
        if(res >= 0)
        {
            if(!(flags & MSG_PEEK))
            {
                IFQ_DEQUEUE(&so->inq, p);
                kernel_mutex_unlock(&so->inq.lock);
                packet_copy_remoteaddr(so, p, msg);
                packet_free(p);
            }
            else
            {
                kernel_mutex_unlock(&so->inq.lock);
                packet_copy_remoteaddr(so, p, msg);
            }
        }
        else
        {
            kernel_mutex_unlock(&so->inq.lock);
        }
    }

    KDEBUG("socket_unix_recvmsg: res %d\n", res);
    
    return res;
}


/*
 * Push a packet on the outgoing queue.
 * Called from the socket layer.
 */
int unix_push(struct packet_t *p)
{
    int size = p->count;
    struct socket_t *s2;

    if(!p->sock || !(s2 = p->sock->pairedsock))
    {
        return -EPIPE;
    }
    
    kernel_mutex_lock(&s2->inq.lock);
    IFQ_ENQUEUE(&s2->inq, p);
    kernel_mutex_unlock(&s2->inq.lock);

    unblock_tasks(&s2->inq);
    selwakeup(&s2->recvsel);

    return size;
}


/*
void unix_init(void)
{
}
*/


struct sockops_t unix_sockops =
{
    .connect2 = NULL,
    .socket = socket_unix_open,
    .getsockopt = socket_unix_getsockopt,
    .setsockopt = socket_unix_setsockopt,
    .recvmsg = socket_unix_recvmsg,
};

