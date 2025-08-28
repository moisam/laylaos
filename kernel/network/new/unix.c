/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
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
 *  Unix socket interface.
 */

//#define __DEBUG
#include <errno.h>
#include <fcntl.h>
#include <kernel/laylaos.h>
#include <kernel/syscall.h>
#include <kernel/net.h>
#include <kernel/net/packet.h>
#include <kernel/net/socket.h>
#include <kernel/net/protocol.h>
#include <kernel/net/unix.h>
#include <mm/kheap.h>

#include "iovec.c"
#include "../../kernel/task_funcs.c"


static struct socket_t *unix_socket(void)
{
    struct socket_t *so;
    
    if(!(so = kmalloc(sizeof(struct socket_t))))
    {
        return NULL;
    }
	    
    A_memset(so, 0, sizeof(struct socket_t));

    return so;
}


static long unix_write(struct socket_t *so, struct msghdr *msg, int kernel)
{
    struct socket_t *so2;
    struct packet_t *p;
    long total, res;

    if(!(so2 = so->pairedsock))
    {
        return -EPIPE;
    }

    if((total = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }

    if(!(p = alloc_packet(total)))
    {
        printk("unix: insufficient memory for sending packet\n");
        return -ENOMEM;
    }

    if((res = read_iovec(msg->msg_iov, msg->msg_iovlen, p->data, 
                         p->count, kernel)) < 0)
    {
        free_packet(p);
        return res;
    }

    SOCKET_UNLOCK(so);
    SOCKET_LOCK(so2);
    IFQ_ENQUEUE(&so2->inq, p);
    //so2->poll_events |= POLLIN;
    __sync_or_and_fetch(&so2->poll_events, POLLIN);
    SOCKET_UNLOCK(so2);
    SOCKET_LOCK(so);

    selwakeup(&so2->selrecv);

    return total;
}


static long unix_read(struct socket_t *so, struct msghdr *msg, unsigned int flags)
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
        // don't wait if peer has disconnected
        if(!so->pairedsock)
        {
            return 0; //-ECONNRESET;
        }

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


static long unix_getsockopt(struct socket_t *so, int level, int optname,
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
            case SO_PEERCRED:
                {
                    // We can manipulate optval directly as we are called
                    // from kernel space
                    int i;
                    struct xucred *creds = (struct xucred *)optval;
                    volatile struct task_t *t;

                    if(!optval || *optlen < (int)sizeof(struct xucred))
                    {
                        return -EINVAL;
                    }
                    
                    if(!so->pairedsock ||
                       !(t = get_task_by_id(so->pairedsock->pid)))
                    {
                        return -EINVAL;
                    }

                    creds->cr_version = 1;
                    creds->cr_pid = t->pid;
                    creds->cr_uid = t->uid;
                    creds->cr_ngroups = 0;

                    for(i = 0; i < NGROUPS_MAX; i++)
                    {
                        if(t->extra_groups[i] != (gid_t)-1)
                        {
                            creds->cr_groups[i] = t->extra_groups[i];
                            creds->cr_ngroups++;
                        }
                        else
                        {
                            creds->cr_groups[i] = 0;
                        }
                    }
                    
                    return 0;
                }
        }
    }

    return socket_getsockopt(so, level, optname, optval, optlen);
}


static long unix_setsockopt(struct socket_t *so, int level, int optname,
                            void *optval, int optlen)
{
    return socket_setsockopt(so, level, optname, optval, optlen);
}


static long unix_connect2(struct socket_t *s1, struct socket_t *s2)
{
    if(!s1 || !s2)
    {
        return -EINVAL;
    }

    s1->pairedsock = s2;
    s2->pairedsock = s1;
    
    return 0;
}


long socket_unix_bind(struct socket_t *so, 
                      struct sockaddr *name, socklen_t namelen)
{
    struct sockaddr_un *sun;
    struct fs_node_t *node;
    char *p, *p2;
    long res;

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
    A_memcpy(&so->local_addr.sun, name, sizeof(sa_family_t) + p - sun->sun_path);
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


static void cancel_socket(struct socket_t *so)
{
    struct socket_t *prev;

    for(prev = &sock_head; prev->next != NULL; prev = prev->next)
    {
        if(prev->next == so)
        {
            prev->next = so->next;
            so->next = NULL;
            kfree(so);
            return;
        }
    }
}


long socket_unix_connect(struct socket_t *so, 
                         struct sockaddr *name, socklen_t namelen)
{
    struct socket_t *serversock, *newsock;
    struct sockaddr_un *sun;
    struct fs_node_t *node;
    char *p, *p2;
    long res;

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
    A_memcpy(&so->remote_addr.sun, name, sizeof(sa_family_t) + p - sun->sun_path);
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
        release_node(node);
        return -ECONNREFUSED;
	}

    if(has_access(node, WRITE, 0) != 0)
    {
        A_memset(&so->remote_addr.sun, 0, sizeof(struct sockaddr_un));
        release_node(node);
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

    newsock->pid = this_core->cur_task->pid;
    newsock->uid = this_core->cur_task->euid;
    newsock->gid = this_core->cur_task->egid;

    // find the server socket
    kernel_mutex_lock(&sock_lock);

    for(serversock = sock_head.next; serversock != NULL; serversock = serversock->next)
    {
        if(serversock->domain != AF_UNIX ||
           serversock->state != SOCKSTATE_LISTENING)
        {
            continue;
        }

        if(memcmp(serversock->local_addr.sun.sun_path,
                  so->remote_addr.sun.sun_path,
                  p - sun->sun_path + 1) == 0)
        {
            break;
        }
    }

    if(serversock == NULL)
    {
        cancel_socket(newsock);
        kernel_mutex_unlock(&sock_lock);
        A_memset(&so->remote_addr.sun, 0, sizeof(struct sockaddr_un));
        return -ECONNREFUSED;
    }

    if(serversock->max_backlog &&
       serversock->pending_connections >= serversock->max_backlog)
    {
        cancel_socket(newsock);
        kernel_mutex_unlock(&sock_lock);
        A_memset(&so->remote_addr.sun, 0, sizeof(struct sockaddr_un));
        return -EAGAIN;
    }

    A_memcpy(&newsock->local_addr.sun, &serversock->local_addr.sun, 
             sizeof(struct sockaddr_un));
    A_memcpy(&newsock->remote_addr.sun, &so->local_addr.sun,
             sizeof(struct sockaddr_un));
    newsock->state = SOCKSTATE_CONNECTED;
    newsock->poll_events = (POLLOUT | POLLWRNORM | POLLWRBAND);
    newsock->parent = serversock;
    newsock->pairedsock = so;
    //so->state |= SOCKET_STATE_CONNECTED;
    so->pairedsock = newsock;
    serversock->pending_connections++;

    kernel_mutex_unlock(&sock_lock);

    // wakeup waiters blocked in an accept() call
    unblock_tasks(&serversock->pending_connections);

    // wakeup waiters who are polling/selecting to know when connections
    // are pending
    selwakeup(&serversock->selrecv);

    return 0;
}


struct sockops_t unix_sockops =
{
    .connect = NULL,
    .connect2 = unix_connect2,
    .socket = unix_socket,
    .write = unix_write,
    .read = unix_read,
    .getsockopt = unix_getsockopt,
    .setsockopt = unix_setsockopt,
};

