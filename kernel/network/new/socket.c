/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: socket.c
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
 *  \file socket.c
 *
 *  The kernel's socket layer implementation.
 */

//#define __DEBUG

#define __USE_XOPEN_EXTENDED
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/sockops.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <kernel/laylaos.h>
#include <kernel/syscall.h>
#include <kernel/user.h>
#include <kernel/mutex.h>
#include <kernel/net.h>
#include <kernel/net/socket.h>
#include <kernel/net/protocol.h>
#include <kernel/net/packet.h>
#include <kernel/net/netif.h>
#include <kernel/net/route.h>
#include <kernel/net/tcp.h>
#include <kernel/net/raw.h>
#include <kernel/net/unix.h>
#include <kernel/net/nettimer.h>
#include <fs/sockfs.h>

#include "iovec.c"

static long syscall_sendto_internal(int s, void *buf, size_t len, int flags,
                                    struct sockaddr *to, socklen_t tolen);
static long syscall_recvfrom_internal(int s, void *buf, size_t len, int flags,
                                      struct sockaddr *from, 
                                      socklen_t *fromlenaddr);


#define asz(x)              ((x) * sizeof(uintptr_t))

static int argsz[18] =
{
    0, asz(3), asz(3), asz(3), asz(2), asz(3), asz(3),
       asz(3), asz(4), asz(4), asz(4), asz(6), asz(6),
       asz(2), asz(5), asz(5), asz(3), asz(3)
};

static long (*sockops_table[18])() =
{
    [SOCK_socket] = syscall_socket,
    [SOCK_bind] = syscall_bind,
    [SOCK_connect] = syscall_connect,
    [SOCK_listen] = syscall_listen,
    [SOCK_accept] = syscall_accept,
    [SOCK_getsockname] = syscall_getsockname,
    [SOCK_getpeername] = syscall_getpeername,
    [SOCK_socketpair] = syscall_socketpair,
    [SOCK_send] = syscall_sendto_internal,
    [SOCK_recv] = syscall_recvfrom_internal,
    [SOCK_sendto] = syscall_sendto_internal,
    [SOCK_recvfrom] = syscall_recvfrom_internal,
    [SOCK_shutdown] = syscall_shutdown,
    [SOCK_setsockopt] = syscall_setsockopt,
    [SOCK_getsockopt] = syscall_getsockopt,
    [SOCK_sendmsg] = syscall_sendmsg,
    [SOCK_recvmsg] = syscall_recvmsg,
};

struct socket_t sock_head = { 0, };
volatile struct kernel_mutex_t sock_lock = { 0, };


/*
 * Handler for syscall socketcall().
 */
long syscall_socketcall(int call, uintptr_t *args)
{
    uintptr_t a[asz(6)] = { 0, };
    unsigned int len;

    if(call < 1 || call > SOCK_recvmsg)
    {
        return -EINVAL;
    }

	if(!args)
	{
        return -EINVAL;
	}
    
    len = argsz[call];
    
    if(copy_from_user(a, args, len) != 0)
    {
        return -EFAULT;
        SYSCALL_EFAULT(args);
    }

    return sockops_table[call](a[0], a[1], a[2], a[3], a[4], a[5]);
}


static void socket_clean_queue(struct netif_queue_t *q)
{
    struct packet_t *p;

    IFQ_DEQUEUE(q, p);

    while(p)
    {
        free_packet(p);
        IFQ_DEQUEUE(q, p);
    }

    q->head = NULL;
    q->tail = NULL;
    q->count = 0;
}


struct socket_t *sock_lookup(uint16_t proto, uint16_t remoteport, uint16_t localport)
{
    struct socket_t *so;

    kernel_mutex_lock(&sock_lock);

    for(so = sock_head.next; so != NULL; so = so->next)
    {
        if(so->local_port == localport && so->remote_port == remoteport &&
           so->proto && so->proto->protocol == proto)
        {
            kernel_mutex_unlock(&sock_lock);
            return so;
        }
    }

    kernel_mutex_unlock(&sock_lock);
    return NULL;
}


struct socket_t *sock_find(struct socket_t *find)
{
    struct socket_t *so;

    kernel_mutex_lock(&sock_lock);

    for(so = sock_head.next; so != NULL; so = so->next)
    {
        if(so == find)
        {
            kernel_mutex_unlock(&sock_lock);
            return so;
        }
    }

    kernel_mutex_unlock(&sock_lock);
    return NULL;
}


static void sock_free(struct socket_t *find)
{
    struct socket_t *so;

    kernel_mutex_lock(&sock_lock);

    for(so = &sock_head; so->next != NULL; so = so->next)
    {
        if(so->next == find)
        {
            if(find->refs == 0)
            {
                so->next = find->next;
                find->next = NULL;

                kernel_mutex_unlock(&sock_lock);

                SOCKET_LOCK(find);
                socket_clean_queue(&find->inq);
                socket_clean_queue(&find->outq);
                socket_tcp_cleanup(find);
                selwakeup(&find->sleep);
                SOCKET_UNLOCK(find);
                kfree(find);
                return;
            }

            break;
        }
    }

    kernel_mutex_unlock(&sock_lock);
}


static void socket_garbage_collect(void *arg)
{
    struct socket_t *so;

    if((so = sock_find(arg)))
    {
        sock_free(so);
    }
}


void socket_delete(struct socket_t *so, uint32_t expiry)
{
    if(so->state != SOCKSTATE_DISCONNECTING)
    {
        so->state = SOCKSTATE_DISCONNECTING;
        nettimer_oneshot(expiry, &socket_garbage_collect, so);
    }
}


void socket_copy_remoteaddr(struct socket_t *so, struct msghdr *msg)
{
    if(msg->msg_name == NULL)
    {
        return;
    }
    
   	if(so->domain == AF_INET)
   	{
   	    struct sockaddr_in sin = { 0, };

   	    sin.sin_family = AF_INET;
   	    sin.sin_addr.s_addr = so->remote_addr.ipv4;
   	    sin.sin_port = so->remote_port;
   	    A_memcpy(msg->msg_name, &sin, sizeof(sin));
   	    msg->msg_namelen = sizeof(sin);
   	}
   	else if(so->domain == AF_INET6)
   	{
        /*
         * FIXME: We only support IPv4 for now.
         */
   	}
}


STATIC_INLINE void *malloced_copy(void *p, int count)
{
    void *buf = kmalloc(count);

    if(buf)
    {
        if(copy_from_user(buf, p, count) != 0)
        {
            kfree(buf);
            return NULL;
        }
    }

    return buf;
}


STATIC_INLINE long getsock(int fd, struct socket_t **so)
{
    struct file_t *fp;
    
    *so = NULL;
    
    if(fd >= (int)NR_OPEN || !this_core->cur_task->ofiles || 
       (fp = this_core->cur_task->ofiles->ofile[fd]) == NULL)
    {
        return -EBADF;
    }
    
    if(!fp->node || !IS_SOCKET(fp->node))
    {
        return -ENOTSOCK;
    }
    
    *so = (struct socket_t *)fp->node->data;

    return 0;
}


STATIC_INLINE int is_ipv4_port_free(uint16_t proto, uint16_t port, uint32_t addr)
{
    struct socket_t *so;

    if(proto == IPPROTO_RAW)
    {
        return 1;
    }

    for(so = sock_head.next; so != NULL; so = so->next)
    {
        if(so->domain == AF_INET &&
           so->proto && so->proto->protocol == proto &&
           so->local_port == port)
        {
            if(so->local_addr.ipv4 == INADDR_ANY ||
               so->local_addr.ipv4 == addr)
            {
                return 0;
            }
        }
    }
    
    return 1;
}


STATIC_INLINE uint16_t socket_high_port(int domain, uint16_t proto)
{
    volatile uint16_t port;

    if(proto != IPPROTO_UDP && proto != IPPROTO_TCP && proto != IPPROTO_RAW)
    {
        return 0;
    }

    while(1)
    {
        uint32_t r = genrand_int32();
        
        port = (uint16_t)(r & 0xffff);
        port = htons((uint16_t)((port % (0xffff - 0x400)) + 0x400));

        kernel_mutex_lock(&sock_lock);

        if(domain == AF_INET &&
           is_ipv4_port_free(proto, port, INADDR_ANY))
        {
            kernel_mutex_unlock(&sock_lock);
            return port;
        }

        kernel_mutex_unlock(&sock_lock);
    }
}


/*
 * Helper function to create a new socket.
 */
long sock_create(int domain, int type, int protocol, struct socket_t **res)
{
    struct proto_t *proto;
    struct socket_t *so;

	*res = NULL;
	
    if(protocol)
    {
        proto = find_proto(domain, protocol, type);
    }
    else
    {
        proto = find_proto_by_type(domain, type);
    }

    if(!proto)
    {
        return -EPROTONOSUPPORT;
    }
	    
    if(proto->sock_type != type)
    {
        return -EPROTOTYPE;
    }

    if(!(so = proto->sockops->socket()))
    {
        return -ENOMEM;
    }

    so->type = type;
    so->domain = domain;
    so->proto = proto;
    so->state = SOCKSTATE_UNCONNECTED;
    so->ttl = RAW_SOCKET(so) ? RAWTTL : IPDEFTTL;

    so->pid = this_core->cur_task->pid;
    so->uid = this_core->cur_task->euid;
    so->gid = this_core->cur_task->egid;
    so->refs = 1;
    
    so->inq.max = SOCKET_DEFAULT_QUEUE_SIZE;
    so->outq.max = SOCKET_DEFAULT_QUEUE_SIZE;
    so->poll_events = 0;
    init_kernel_mutex(&so->lock);

    kernel_mutex_lock(&sock_lock);
    so->next = sock_head.next;
    sock_head.next = so;
    kernel_mutex_unlock(&sock_lock);

	*res = so;
	return 0;
}


static long sock_createf(int domain, int type, int protocol,
                         unsigned int flags, struct socket_t *so)
{
    long res;
    int fd;
    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;

    if(!this_core->cur_task || !this_core->cur_task->ofiles)
    {
        return -EINVAL;
    }

    /*
     * FIXME: We only support IPv4 for now.
     */
    if(domain == AF_INET6)
    {
        return -EAFNOSUPPORT;
    }

	if((res = falloc(&fd, &f)) != 0)
	{
	    return res;
	}

    if(!(node = sockfs_get_node()))
    {
    	this_core->cur_task->ofiles->ofile[fd] = NULL;
    	f->refs = 0;
    	return -ENOSPC;
    }

	if(so == NULL)
	{
	    if((res = sock_create(domain, type, protocol, &so)) != 0)
	    {
	        goto err;
	    }
	}

    node->data = (void *)so;

    // set the close-on-exec flag
    if(flags & O_CLOEXEC)
    {
        cloexec_set(this_core->cur_task, fd);
    }
    
    if(flags & O_NONBLOCK)
    {
        so->flags |= SOCKET_FLAG_NONBLOCK;
    }

    if(so->proto->protocol == IPPROTO_RAW)
    {
        so->flags |= SOCKET_FLAG_IPHDR_INCLUDED;
    }

    f->mode = node->mode;
    f->flags = flags;
    f->refs = 1;
    f->node = node;
    f->pos = 0;

	return fd;

err:

    this_core->cur_task->ofiles->ofile[fd] = NULL;
    f->refs = 0;
    release_node(node);
    return res;
}


/*
 * Handler for syscall socket().
 */
long syscall_socket(int domain, int type, int protocol)
{
    unsigned int flags = O_RDWR | O_NOATIME;
    
    if(type & SOCK_CLOEXEC)
    {
        type &= ~SOCK_CLOEXEC;
        flags |= O_CLOEXEC;
    }

    if(type & SOCK_NONBLOCK)
    {
        type &= ~SOCK_NONBLOCK;
        flags |= O_NONBLOCK;
    }

    return sock_createf(domain, type, protocol, flags, NULL);
}


/*
 * Handler for syscall bind().
 */
long syscall_bind(int s, struct sockaddr *_name, socklen_t namelen)
{
	struct socket_t *so;
	long res;
	uint16_t port;
	struct sockaddr *name;
    struct sockaddr_in *sin;

    if(!_name || !namelen)
    {
        SYSCALL_EFAULT(_name);
    }
    
	if((res = getsock(s, &so)))
	{
		return res;
	}
	
	if(!(name = malloced_copy(_name, namelen)))
	{
		return -ENOBUFS;
	}

	if(so->domain == AF_UNIX)      // UNIX
	{
        res = socket_unix_bind(so, name, namelen);
        kfree(name);

        if(res == -EFAULT)
        {
            SYSCALL_EFAULT(_name);
        }

        return res;
	}

	if(so->domain != AF_INET && so->domain != AF_INET6)
	{
        so->err = -EAFNOSUPPORT;
        goto out;
	}

    sin = (struct sockaddr_in *)name;
	
	// validate local address
	if(so->domain == AF_INET)       // IPv4
	{
        if(namelen < sizeof(struct sockaddr_in))
        {
        	kfree(name);
            SYSCALL_EFAULT(_name);
        }

        if(sin->sin_family != AF_INET && sin->sin_family != AF_UNSPEC)
        {
            so->err = -EAFNOSUPPORT;
            goto out;
        }

	    if(sin->sin_addr.s_addr != INADDR_ANY)
	    {
	        if(!route_for_ipv4(sin->sin_addr.s_addr))
	        {
                so->err = -EINVAL;
                goto out;
	        }
	    }
	    
	    port = sin->sin_port;
	}
	else if(so->domain == AF_INET6)     // IPv6
	{
        /*
         * FIXME: We only support IPv4 for now.
         */
        so->err = -EAFNOSUPPORT;
        goto out;
	}

	// if port == 0 and not raw socket, choose a random high port
	if(port == 0 && !RAW_SOCKET(so))
	{
	    if((port = socket_high_port(so->domain, SOCK_PROTO(so))) == 0)
	    {
            so->err = -EADDRINUSE;
            goto out;
	    }
	}

	if(so->domain == AF_INET)       // IPv4
	{
        kernel_mutex_lock(&sock_lock);

        if(!is_ipv4_port_free(SOCK_PROTO(so), port, sin->sin_addr.s_addr))
        {
            kernel_mutex_unlock(&sock_lock);
            so->err = -EADDRINUSE;
            goto out;
        }

        so->local_port = port;
        so->local_addr.ipv4 = sin->sin_addr.s_addr;
        so->err = 0;
        kernel_mutex_unlock(&sock_lock);
	}

out:

    kfree(name);
    return so->err;
}


void sock_connected(struct socket_t *so)
{
    so->err = 0;
    so->state = SOCKSTATE_CONNECTED;
    so->poll_events = (POLLOUT | POLLWRNORM | POLLWRBAND);
    selwakeup(&so->sleep);
}


/*
 * Handler for syscall connect().
 */
long syscall_connect(int fd, struct sockaddr *_name, socklen_t namelen)
{
	struct socket_t *so;
	long res;
	struct sockaddr *name;

    if(!_name || !namelen)
    {
        SYSCALL_EFAULT(_name);
    }

	if((res = getsock(fd, &so)))
	{
		return res;
	}

	if(!(name = malloced_copy(_name, namelen)))
	{
        so->err = -ENOBUFS;
        return so->err;
	}

    if(so->state == SOCKSTATE_CONNECTED)
    {
        so->err = -EISCONN;
        goto err;
    }

    if(so->state == SOCKSTATE_CONNECTING)
    {
        so->err = -EALREADY;
        goto err;
    }

    if(so->state != SOCKSTATE_UNCONNECTED)
    {
        so->err = -EINVAL;
        goto err;
    }

	if(so->domain == AF_UNIX)      // UNIX
	{
        res = socket_unix_connect(so, name, namelen);
        kfree(name);
        
        if(res == -EFAULT)
        {
            SYSCALL_EFAULT(_name);
        }
	    
	    if(res == 0)
	    {
            sock_connected(so);
    	}

    	return res;
	}

	if(so->domain != AF_INET && so->domain != AF_INET6)
	{
        so->err = -EAFNOSUPPORT;
        goto err;
	}

	// validate local address
	if(so->domain == AF_INET)       // IPv4
	{
	    struct sockaddr_in *sin;

        if(namelen < sizeof(struct sockaddr_in))
        {
        	kfree(name);
            SYSCALL_EFAULT(_name);
        }

        sin = (struct sockaddr_in *)name;

        if(sin->sin_family != AF_INET && sin->sin_family != AF_UNSPEC)
        {
            so->err = -EAFNOSUPPORT;
            goto err;
        }

        so->remote_addr.ipv4 = sin->sin_addr.s_addr;
	    so->remote_port = sin->sin_port;
	}
	else if(so->domain == AF_INET6)     // IPv6
	{
        /*
         * FIXME: We only support IPv4 for now.
         */
        so->err = -EAFNOSUPPORT;
        goto err;
	}

	// if port == 0 and not raw socket, choose a random high port
	if(so->local_port == 0 && !RAW_SOCKET(so))
	{
	    if((so->local_port = socket_high_port(so->domain, SOCK_PROTO(so))) == 0)
	    {
        	so->err = -EADDRINUSE;
        	goto err;
	    }
	}
	
	kfree(name);
    SOCKET_LOCK(so);

	if(so->proto->protocol == IPPROTO_TCP)
	{
	    if(TCP_STATE(so) != TCPSTATE_CLOSE)
        {
            so->err = -EISCONN;
            SOCKET_UNLOCK(so);
            return so->err;
        }

        so->proto->sockops->connect(so);
        so->state = SOCKSTATE_CONNECTING;
        so->err = -EINPROGRESS;

        // only wait for connection to be established if this is a blocking socket
        if(so->flags & SOCKET_FLAG_NONBLOCK)
        {
            SOCKET_UNLOCK(so);
            return so->err;
        }

        while(so->state == SOCKSTATE_CONNECTING && so->err == -EINPROGRESS)
        {
            selrecord(&so->sleep);
            SOCKET_UNLOCK(so);
            //block_task2(so, PIT_FREQUENCY / 2);
            block_task(so, 1);
            SOCKET_LOCK(so);
        }

        if(so->err == 0)
        {
	        so->state = SOCKSTATE_CONNECTED;
	    }
    }
    else
    {
        sock_connected(so);
    }

    SOCKET_UNLOCK(so);
	return so->err;

err:

    kfree(name);
    return so->err;
}


STATIC_INLINE int check_namelen(struct socket_t *so, socklen_t len)
{
	if(so->domain == AF_INET)
	{
	    if(len < sizeof(struct sockaddr_in))
	    {
	        return -1;
	    }
	}
	else if(so->domain == AF_INET6)
	{
        /*
         * FIXME: We only support IPv4 for now.
         */
        return -1;
	}
	else if(so->domain == AF_UNIX)
	{
	    if(len < sizeof(struct sockaddr_un))
	    {
	        return -1;
	    }
	}
	
	return 0;
}


STATIC_INLINE long copy_sockname_to_user(struct socket_t *so, void *dest, void *src)
{
	if(so->domain == AF_INET)
	{
	    return copy_to_user(dest, src, sizeof(struct sockaddr_in));
	}
	else if(so->domain == AF_INET6)
	{
        /*
         * FIXME: We only support IPv4 for now.
         */
        return -EAFNOSUPPORT;
	}
	
	return 0;
}


STATIC_INLINE long copy_sockname_from_user(struct socket_t *so, void *dest, void *src)
{
	if(so->domain == AF_INET)
	{
	    return copy_from_user(dest, src, sizeof(struct sockaddr_in));
	}
	else if(so->domain == AF_INET6)
	{
        /*
         * FIXME: We only support IPv4 for now.
         */
        return -EAFNOSUPPORT;
	}
	
	return 0;
}


STATIC_INLINE long do_sendto(struct socket_t *so, struct msghdr *msg, int kernel)
{
    long res;

    SOCKET_LOCK(so);
    res = so->proto->sockops->write(so, msg, kernel);

    if(res < 0)
    {
        so->err = res;
    }

    SOCKET_UNLOCK(so);

    return res;
}


long sendto_pre_checks(struct socket_t *so, 
                       struct sockaddr *to, socklen_t tolen)
{
    long res;
	char dest_namebuf[128];

	// both of dest addr AND its length MUST be provided or omitted
	if((!!to) != (!!tolen))
	{
        return -EINVAL;
	}

    // User has called shutdown() specifying SHUT_RDWR or SHUT_WR
    if(so->flags & SOCKET_FLAG_SHUT_LOCAL)
    {
        return -ENOTCONN;
    }

    // get dest addr
    if(to)
    {
        // use the provided addr
    	if(check_namelen(so, tolen) != 0)
    	{
            return -ENOBUFS;
    	}

        if((res = copy_sockname_from_user(so, dest_namebuf, to)) != 0)
        {
            return res;
        }

    	if(so->domain == AF_INET)
    	{
    	    SOCKET_LOCK(so);
            so->remote_addr.ipv4 = ((struct sockaddr_in *)dest_namebuf)->sin_addr.s_addr;
            so->remote_port = ((struct sockaddr_in *)dest_namebuf)->sin_port;
            SOCKET_UNLOCK(so);
        }
    }
	
	return 0;
}


static long syscall_sendto_internal(int s, void *buf, size_t len, int flags,
                                    struct sockaddr *to, socklen_t tolen)
{
	struct msghdr msg;
	struct iovec aiov;
	struct socket_t *so;
	long res;

    UNUSED(flags);

    if(!buf)
    {
        return -EINVAL;
    }

	if((res = getsock(s, &so)))
	{
		return res;
	}

    if((res = sendto_pre_checks(so, to, tolen)) != 0)
    {
        so->err = res;
		return res;
    }
	
	msg.msg_name = (caddr_t)to;
	msg.msg_namelen = tolen;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
	aiov.iov_base = buf;
	aiov.iov_len = len;

    return do_sendto(so, &msg, 0);
}


/*
 * Handler for syscall sendto().
 */
long syscall_sendto(struct syscall_args *__args)
{
    // syscall args
    struct syscall_args args;
    int s;
    void *buf;
    size_t len;
    int flags;
    struct sockaddr *to;
    socklen_t tolen;
	long res;

    // get the args
    COPY_SYSCALL6_ARGS(args, __args);
    s = (int)(args.args[0]);
    buf = (void *)(args.args[1]);
    len = (size_t)(args.args[2]);
    flags = (int)(args.args[3]);
    to = (struct sockaddr *)(args.args[4]);
    tolen = (socklen_t)(args.args[5]);
    
    res = syscall_sendto_internal(s, buf, len, flags, to, tolen);

	if(res == -EFAULT)
	{
        SYSCALL_EFAULT(buf);
	}
	
	return res;
}


/*
 * Handler for syscall sendmsg().
 */
long syscall_sendmsg(int s, struct msghdr *_msg, int flags)
{
	struct msghdr msg;
	long res;
	struct socket_t *so;

    UNUSED(flags);

    if(!_msg)
    {
        return -EINVAL;
    }
    
	if((res = getsock(s, &so)))
	{
		return res;
	}

    if((res = copy_from_user(&msg, _msg, sizeof(msg))))
    {
        so->err = res;
        return res;
    }
    
    if(!(msg.msg_iov = dup_iovec(_msg->msg_iov, _msg->msg_iovlen)))
    {
        so->err = -ENOMEM;
        return so->err;
    }

    if((res = sendto_pre_checks(so, msg.msg_name, msg.msg_namelen)) != 0)
    {
    	kfree(msg.msg_iov);
        so->err = res;
		return res;
    }

    res = do_sendto(so, &msg, 0);

	kfree(msg.msg_iov);

	if(res == -EFAULT)
	{
        SYSCALL_EFAULT(_msg);
	}

	return res;
}


static long syscall_recvfrom_internal(int s, void *buf, size_t len, int flags,
                                      struct sockaddr *from, 
                                      socklen_t *fromlenaddr)
{
	struct msghdr msg;
	struct iovec aiov;
	long res;
	struct socket_t *so;
	char namebuf[128];
	socklen_t user_namelen;

    if(!buf)
    {
        return -EINVAL;
    }

	if((res = getsock(s, &so)))
	{
		return res;
	}

    // User has called shutdown() specifying SHUT_RDWR or SHUT_RD
    if(so->flags & SOCKET_FLAG_SHUT_REMOTE)
    {
        so->err = -ENOTCONN;
        return so->err;
    }
	
	if(fromlenaddr)
	{
		if((res = copy_from_user(&user_namelen, 
		                         fromlenaddr, sizeof(socklen_t))))
		{
            so->err = res;
			return res;
		}

		if(check_namelen(so, user_namelen) != 0)
		{
            so->err = -ENOBUFS;
		    return so->err;
		}
	}
	
	msg.msg_namelen = sizeof(namebuf);
	msg.msg_name = namebuf;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = buf;
	aiov.iov_len = len;
	msg.msg_control = 0;
	msg.msg_flags = flags;
	
    SOCKET_LOCK(so);
	res = so->proto->sockops->read(so, &msg, flags);

    if(res < 0)
    {
        so->err = res;
    }

    SOCKET_UNLOCK(so);

	if(res >= 0 && from && fromlenaddr)
	{
	    if(copy_sockname_to_user(so, from, namebuf) != 0)
	    {
            SYSCALL_EFAULT(from);
	    }
	    
	    if(copy_to_user(fromlenaddr, &msg.msg_namelen, sizeof(socklen_t)) != 0)
	    {
            SYSCALL_EFAULT(fromlenaddr);
	    }
	}

	return res;
}


/*
 * Handler for syscall recvfrom().
 */
long syscall_recvfrom(struct syscall_args *__args)
{
    // syscall args
    struct syscall_args args;
    int s;
    void *buf;
    size_t len;
    int flags;
    struct sockaddr *from;
    socklen_t *fromlenaddr;
	long res;

    // get the args
    COPY_SYSCALL6_ARGS(args, __args);
    s = (int)(args.args[0]);
    buf = (void *)(args.args[1]);
    len = (size_t)(args.args[2]);
    flags = (int)(args.args[3]);
    from = (struct sockaddr *)(args.args[4]);
    fromlenaddr = (socklen_t *)(args.args[5]);

    res = syscall_recvfrom_internal(s, buf, len, flags, from, fromlenaddr);

	if(res == -EFAULT)
	{
        SYSCALL_EFAULT(buf);
	}
	
	return res;
}


/*
 * Handler for syscall recvmsg().
 */
long syscall_recvmsg(int s, struct msghdr *_msg, int flags)
{
	struct msghdr msg;
	register long res;
	struct socket_t *so;
	char namebuf[128];
	void *from = NULL;
	socklen_t user_namelen = 0;

    if(!_msg)
    {
        return -EINVAL;
    }

	if((res = getsock(s, &so)))
	{
		return res;
	}

    // User has called shutdown() specifying SHUT_RDWR or SHUT_RD
    if(so->flags & SOCKET_FLAG_SHUT_REMOTE)
    {
        so->err = -ENOTCONN;
        return so->err;
    }

	if((res = copy_from_user(&msg, _msg, sizeof(msg))))
	{
        so->err = res;
		return res;
	}

    if(!(msg.msg_iov = dup_iovec(_msg->msg_iov, _msg->msg_iovlen)))
    {
        so->err = -ENOMEM;
        return so->err;
    }
    
	if(msg.msg_namelen)
	{
		if(check_namelen(so, msg.msg_namelen) != 0)
		{
		    so->err = -ENOBUFS;
            return so->err;
		}
		
		user_namelen = msg.msg_namelen;
	}
	
	if(msg.msg_name)
	{
	    from = msg.msg_name;
	}

	msg.msg_namelen = sizeof(namebuf);
	msg.msg_name = namebuf;

    SOCKET_LOCK(so);
    res = so->proto->sockops->read(so, &msg, flags);

    if(res < 0)
    {
        so->err = res;
    }

    SOCKET_UNLOCK(so);

	if(res >= 0 && from && user_namelen)
    {
        if(copy_sockname_to_user(so, from, namebuf) != 0)
        {
            SYSCALL_EFAULT(_msg);
        }

        if(copy_to_user(&_msg->msg_namelen, &msg.msg_namelen, sizeof(socklen_t)) != 0)
        {
            SYSCALL_EFAULT(_msg);
        }
    }

	kfree(msg.msg_iov);

	if(res == -EFAULT)
	{
        SYSCALL_EFAULT(_msg);
	}
	
	return res;
}


static long socket_shutdown(struct socket_t *so, int how)
{
    if(how & SHUT_RDWR)
    {
        so->flags |= SOCKET_FLAG_SHUT_LOCAL;
        so->flags |= SOCKET_FLAG_SHUT_REMOTE;
        socket_clean_queue(&so->inq);
        socket_clean_queue(&so->outq);

        if(so->proto->protocol == IPPROTO_TCP)
        {
            tcp_notify_closing(so);
        }
    }
    else if(how & SHUT_WR)
    {
        so->flags |= SOCKET_FLAG_SHUT_LOCAL;
        socket_clean_queue(&so->outq);

        if(so->proto->protocol == IPPROTO_TCP)
        {
            tcp_notify_closing(so);
        }
    }
    else if(how & SHUT_RD)
    {
        so->flags |= SOCKET_FLAG_SHUT_REMOTE;
        socket_clean_queue(&so->inq);
    }
    else
    {
        return -EINVAL;
    }

    return 0;
}


/*
 * Handler for syscall shutdown().
 */
long syscall_shutdown(int s, int how)
{
    struct socket_t *so;
	long res;

	if((res = getsock(s, &so)))
	{
		return res;
	}

    SOCKET_LOCK(so);
    res = socket_shutdown(so, how);
    SOCKET_UNLOCK(so);

    return res;
}


void socket_close(struct socket_t *so)
{
    if(!so || !so->proto)
    {
        return;
    }

    SOCKET_LOCK(so);
    socket_shutdown(so, SHUT_RDWR);
    so->refs--;

    if(so->pairedsock)
    {
        struct socket_t *so2 = so->pairedsock;

        so->pairedsock = NULL;
        SOCKET_UNLOCK(so);

        SOCKET_LOCK(so2);
        so2->pairedsock = NULL;
        //so2->poll_events |= POLLHUP;
        __sync_or_and_fetch(&so2->poll_events, POLLHUP);
        SOCKET_UNLOCK(so2);
    }
    else
    {
        SOCKET_UNLOCK(so);
    }

    if(so->proto->protocol != IPPROTO_TCP)
    {
        socket_delete(so, PIT_FREQUENCY * 5);
    }
}


/*
 * Handler for syscall setsockopt().
 */
long syscall_setsockopt(int s, int level, int name, void *val, int valsize)
{
    struct socket_t *so;
	long res;

	if((res = getsock(s, &so)))
	{
		return res;
	}

	if(val)
	{
		if(valsize <= 0 || 
		   valsize > 256 /* arbitrary limit, options can't be that long! */)
	    {
	        return -EINVAL;
	    }
	    
	    char copy[valsize];

        if(copy_from_user(copy, val, valsize) != 0)
        {
            SYSCALL_EFAULT(val);
        }

    	res = so->proto->sockops->setsockopt(so, level, name, copy, valsize);

    	if(res == -EFAULT)
    	{
            SYSCALL_EFAULT(val);
    	}
	}
	else
	{
    	res = so->proto->sockops->setsockopt(so, level, name, NULL, 0);
	}
	
	return res;
}


/*
 * Handler for syscall getsockopt().
 */
long syscall_getsockopt(int s, int level, int name, void *aval, int *avalsize)
{
    struct socket_t *so;
	int valsize = 0;
	long res;

	if((res = getsock(s, &so)))
	{
		return res;
	}
	
	if(aval)
	{
		if((res = copy_from_user(&valsize, avalsize, sizeof(valsize))))
		{
			return res;
		}
		
		if(valsize <= 0 || 
		   valsize > 256 /* arbitrary limit, options can't be that long! */)
		{
		    return -EINVAL;
		}

	    char val[valsize];

	    if((res = so->proto->sockops->getsockopt(so, level, name, 
	                                             val, &valsize)) == 0)
    	{
    		if((res = copy_to_user(aval, val, valsize)) == 0)
    		{
    			res = copy_to_user(avalsize, &valsize, sizeof(valsize));
    		}
    	}
	}
	else
	{
	    res = so->proto->sockops->getsockopt(so, level, name, NULL, NULL);
	}

    if(res == -EFAULT)
    {
        SYSCALL_EFAULT(aval);
    }
	
	return res;
}


/*
 * Handler for syscall getsockname().
 */
long syscall_getsockname(int fdes, struct sockaddr *_name, socklen_t *namelen)
{
    struct socket_t *so;
	socklen_t len;
	long res;

    if(!_name || !namelen)
    {
        SYSCALL_EFAULT(_name);
    }

	if((res = getsock(fdes, &so)))
	{
		return res;
	}
	
	if((res = copy_from_user(&len, namelen, sizeof(len))))
	{
		return res;
	}
	
	if(so->domain == AF_INET)
	{
	    struct sockaddr_in sin;

	    sin.sin_family = AF_INET;
	    sin.sin_addr.s_addr = so->local_addr.ipv4;
	    sin.sin_port = so->local_port;
	    
	    if(len < sizeof(struct sockaddr_in))
	    {
	        return -ENOBUFS;
	    }
    	
    	if((res = copy_to_user(_name, &sin, sizeof(struct sockaddr_in))) == 0)
    	{
    	    len = sizeof(struct sockaddr_in);
    		res = copy_to_user(namelen, &len, sizeof(len));
    	}
	}
	else if(so->domain == AF_INET6)
	{
        /*
         * FIXME: We only support IPv4 for now.
         */
        return -EAFNOSUPPORT;
	}
	else if(so->domain == AF_UNIX)
	{
	    if(len < sizeof(struct sockaddr_un))
	    {
	        return -ENOBUFS;
	    }

    	if((res = copy_to_user(_name, &so->local_addr.sun, 
    	                       sizeof(struct sockaddr_un))) == 0)
    	{
    	    len = sizeof(struct sockaddr_un);
    		res = copy_to_user(namelen, &len, sizeof(len));
    	}
	}
	else
	{
	    return -EINVAL;
	}
	
	if(res == -EFAULT)
	{
        SYSCALL_EFAULT(_name);
	}

	return res;
}


/*
 * Handler for syscall getpeername().
 */
long syscall_getpeername(int fdes, struct sockaddr *_name, socklen_t *alen)
{
    struct socket_t *so;
	socklen_t len;
    long res;

	if((res = getsock(fdes, &so)))
	{
		return res;
	}
	
	if((res = copy_from_user(&len, alen, sizeof(len))))
	{
		return res;
	}

    if(so->state != SOCKSTATE_CONNECTED)
    {
        return -ENOTCONN;
    }
    
	if(so->domain == AF_INET)
	{
	    struct sockaddr_in sin;

	    sin.sin_family = AF_INET;
	    sin.sin_addr.s_addr = so->remote_addr.ipv4;
	    sin.sin_port = so->remote_port;
	    
	    if(len < sizeof(struct sockaddr_in))
	    {
	        return -ENOBUFS;
	    }
    	
    	if((res = copy_to_user(_name, &sin, sizeof(struct sockaddr_in))) == 0)
    	{
    	    len = sizeof(struct sockaddr_in);
    		res = copy_to_user(alen, &len, sizeof(len));
    	}
	}
	else if(so->domain == AF_INET6)
	{
        /*
         * FIXME: We only support IPv4 for now.
         */
        return -EAFNOSUPPORT;
	}
	else if(so->domain == AF_UNIX)
	{
	    if(len < sizeof(struct sockaddr_un))
	    {
	        return -ENOBUFS;
	    }

    	if((res = copy_to_user(_name, &so->remote_addr.sun, 
    	                       sizeof(struct sockaddr_un))) == 0)
    	{
    	    len = sizeof(struct sockaddr_un);
    		res = copy_to_user(alen, &len, sizeof(len));
    	}
	}
	else
	{
	    return -EINVAL;
	}

	if(res == -EFAULT)
	{
        SYSCALL_EFAULT(_name);
	}

	return res;
}


/*
 * Handler for syscall listen().
 */
long syscall_listen(int s, int backlog)
{
	struct socket_t *so;
	long res;

    if(backlog < 1)
    {
        return -EINVAL;
    }
    
	if((res = getsock(s, &so)))
	{
		return res;
	}

    if(so->proto->protocol == IPPROTO_UDP || RAW_SOCKET(so))
    {
        return -EINVAL;
    }

    SOCKET_LOCK(so);
    so->state = SOCKSTATE_LISTENING;
    so->max_backlog = backlog;
    SOCKET_UNLOCK(so);
    
    return 0;
}


/*
 * Handler for syscall accept().
 */
long syscall_accept(int fd, struct sockaddr *_name, socklen_t *anamelen)
{
    struct sockaddr *name;
	socklen_t namelen, namelen_tmp;
	long res;
	struct socket_t *so, *newso = NULL;

	if((res = getsock(fd, &so)))
	{
		return res;
	}
    
    if(so->proto->protocol == IPPROTO_UDP || RAW_SOCKET(so))
    {
        return -EINVAL;
    }

    if(so->state != SOCKSTATE_LISTENING)
    {
        return -ENOTCONN;
    }

	if((res = copy_from_user(&namelen_tmp, anamelen, sizeof(namelen_tmp))))
	{
		return res;
	}

    namelen = (so->domain == AF_UNIX) ?
                sizeof(struct sockaddr_un) :
                    (so->domain == AF_INET6) ? 
                        sizeof(struct sockaddr_in6) :
                        sizeof(struct sockaddr_in);

    if(!(name = kmalloc(namelen)))
    {
	    return -ENOBUFS;
    }

    // The caller might have passed storage less than required. To ensure
    // we do not break user applications, we truncate the connected address
    // to their storage.
    // FIXME: This has the potential side effect of the user not receiving a
    //        correct address.
    if(namelen > namelen_tmp)
    {
        namelen = namelen_tmp;
    }

try:

    res = -EINVAL;

    kernel_mutex_lock(&sock_lock);

    for(newso = sock_head.next; newso != NULL; newso = newso->next)
    {
        if(newso->parent == so)
        {
            newso->parent = NULL;

            if(so->domain == AF_UNIX)
            {
                A_memcpy(name, &newso->remote_addr.sun,
                               sizeof(struct sockaddr_un));
            }
            else if(so->domain == AF_INET)
            {
        	    ((struct sockaddr_in *)name)->sin_family = AF_INET;
                ((struct sockaddr_in *)name)->sin_port = 
                                        newso->remote_port;
                ((struct sockaddr_in *)name)->sin_addr.s_addr =
                                        newso->remote_addr.ipv4;
            }

            so->pending_connections--;
            res = 0;
            break;
        }
    }

    kernel_mutex_unlock(&sock_lock);

    if(!newso)
    {
        if(so->flags & SOCKET_FLAG_NONBLOCK)
        {
            kfree(name);
            return -EAGAIN;
        }
        
        block_task(&so->pending_connections, 1);

        if(this_core->cur_task->woke_by_signal)
        {
            kfree(name);
            return -EINTR;
        }

        goto try;
    }

    if((res = sock_createf(0, 0, 0, O_RDWR | O_NOATIME, newso)) < 0)
    {
        kfree(name);
    	return res;
    }

	if(_name)
	{
		/* SHOULD COPY OUT A CHAIN HERE */
		if(copy_to_user(_name, name, namelen) == 0)
		{
			if(copy_to_user(anamelen, &namelen, sizeof (*anamelen)) != 0)
			{
			    res = -EFAULT;
			}
		}
	}
	
	kfree(name);

	if(res == -EFAULT)
	{
        SYSCALL_EFAULT(_name);
	}

	return res;
}


/*
 * Handler for syscall socketpair().
 */
long syscall_socketpair(int domain, int type, int protocol, int *rsv)
{
	struct socket_t *so1, *so2;
	int sv[2];
	long res;
	volatile struct task_t *ct = this_core->cur_task;

    if((res = sock_createf(domain, type, protocol, 
                                    O_RDWR | O_NOATIME, NULL)) < 0)
    {
        return res;
    }

	so1 = ct->ofiles->ofile[res]->node->data;

    if(so1->proto->sockops->connect2 == NULL)
    {
        syscall_close(res);
        return -EPROTONOSUPPORT;
    }

	sv[0] = res;

    if((res = sock_createf(domain, type, protocol, 
                                    O_RDWR | O_NOATIME, NULL)) < 0)
    {
        syscall_close(sv[0]);
        return res;
    }

	sv[1] = res;
	so2 = ct->ofiles->ofile[sv[1]]->node->data;
	
	if(so1->proto != so2->proto)
	{
        syscall_close(sv[0]);
        syscall_close(sv[1]);
        return -EPROTONOSUPPORT;
	}

	if((res = so1->proto->sockops->connect2(so1, so2)))
	{
        syscall_close(sv[0]);
        syscall_close(sv[1]);
		return res;
	}

    sock_connected(so1);
    sock_connected(so2);

	return copy_to_user(rsv, sv, 2 * sizeof (int));
}

