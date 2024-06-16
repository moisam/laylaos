/* 
 *    Copyright 2022, 2023, 2024 (c) Mohammed Isam [mohammed_isam1984@yahoo.com].
 *    PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
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
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sockops.h>
#include <netinet/in.h>
#include <kernel/laylaos.h>
#include <kernel/syscall.h>
#include <kernel/user.h>
#include <kernel/softint.h>
#include <kernel/net.h>
#include <kernel/net/socket.h>
#include <kernel/net/protocol.h>
#include <kernel/net/tcp.h>
#include <kernel/net/udp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>
#include <kernel/net/unix.h>
#include <kernel/net/raw.h>
#include <mm/kheap.h>
#include <fs/sockfs.h>

#include "../network/new/iovec.c"

#define RAW_SOCKET(so)      ((so)->proto->sockops == &raw_sockops)

#define SOCK_PROTO(so)      (RAW_SOCKET(so) ? IPPROTO_RAW : \
                                              (so)->proto->protocol)

#define asz(x)      ((x) * sizeof(uintptr_t))

#undef INLINE
#define INLINE      static inline __attribute__((always_inline))

#define AVAILABLE_SPACE(so)  \
    ((so->domain == AF_UNIX) ? 65535 : sendto_available_space(so))

static int argsz[18] =
{
    0, asz(3), asz(3), asz(3), asz(2), asz(3), asz(3),
       asz(3), asz(4), asz(4), asz(4), asz(6), asz(6),
       asz(2), asz(5), asz(5), asz(3), asz(3)
};

static int syscall_sendto_internal(int s, void *buf, size_t len, int flags,
                                   struct sockaddr *to, socklen_t tolen);
static int syscall_recvfrom_internal(int s, void *buf, size_t len, int flags,
                                     struct sockaddr *from, 
                                     socklen_t *fromlenaddr);

// tcp and udp socket ports
struct sockport_t *tcp_ports;
struct sockport_t *udp_ports;
struct kernel_mutex_t sockport_lock = { 0, };

// raw sockets
struct socket_t *raw_socks;
struct kernel_mutex_t sockraw_lock = { 0, };

// unix sockets
struct socket_t *unix_socks;
struct kernel_mutex_t sockunix_lock = { 0, };

// forward declarations
INLINE int copy_sockname_from_user(struct socket_t *so, void *dest, void *src);
INLINE int check_namelen(struct socket_t *so, socklen_t len);


/*
 * Handler for syscall socketcall().
 */
int syscall_socketcall(int call, uintptr_t *args)
{
    //printk("syscall_socketcall: call %d\n", call);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    uintptr_t a[asz(6)];
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
    
    switch(call)
    {
        case SOCK_socket:
            return syscall_socket(a[0], a[1], a[2]);
            
        case SOCK_bind:
            return syscall_bind(a[0], (struct sockaddr *)a[1],
                                (socklen_t)a[2]);
            
        case SOCK_connect:
            return syscall_connect(a[0], (struct sockaddr *)a[1],
                                   (socklen_t)a[2]);
            
        case SOCK_listen:
            return syscall_listen(a[0], a[1]);
            
        case SOCK_accept:
            return syscall_accept(a[0], (struct sockaddr *)a[1],
                                  (socklen_t *)a[2]);
            
        case SOCK_getsockname:
            return syscall_getsockname(a[0], (struct sockaddr *)a[1],
                                       (socklen_t *)a[2]);
            
        case SOCK_getpeername:
            return syscall_getpeername(a[0], (struct sockaddr *)a[1],
                                       (socklen_t *)a[2]);
            
        case SOCK_socketpair:
            return syscall_socketpair(a[0], a[1], a[2], (int *)a[3]);
            //return -ENOSYS;
            
        case SOCK_send:
            return syscall_sendto_internal(a[0], (void *)a[1], (size_t)a[2], 
                                           a[3], (struct sockaddr *)NULL, 0);
            
        case SOCK_recv:
            return syscall_recvfrom_internal(a[0], (caddr_t)a[1], 
                                             (size_t)a[2], a[3],
                                             (struct sockaddr *)NULL, 
                                             (socklen_t *)0);
            
        case SOCK_sendto:
            return syscall_sendto_internal(a[0], (void *)a[1], (size_t)a[2], 
                                           a[3], (struct sockaddr *)a[4], 
                                           (socklen_t)a[5]);
            
        case SOCK_recvfrom:
            return syscall_recvfrom_internal(a[0], (void *)a[1], 
                                             (size_t)a[2], a[3],
                                             (struct sockaddr *)a[4], 
                                             (socklen_t *)a[5]);
            
        case SOCK_shutdown:
            return syscall_shutdown(a[0], a[1]);
            
        case SOCK_setsockopt:
            return syscall_setsockopt(a[0], a[1], a[2], 
                                      (void *)a[3], (socklen_t)a[4]);
            
        case SOCK_getsockopt:
            return syscall_getsockopt(a[0], a[1], a[2], 
                                      (caddr_t)a[3], (int *)a[4]);
            
        case SOCK_sendmsg:
            return syscall_sendmsg(a[0], (struct msghdr *)a[1], a[2]);
            
        case SOCK_recvmsg:
            return syscall_recvmsg(a[0], (struct msghdr *)a[1], a[2]);
    }
    
    return -EINVAL;
}


static void *malloced_copy(void *p, int count)
{
    void *buf = (void *)kmalloc(count);
    
    if(!buf)
    {
        return NULL;
    }
    
    if(copy_from_user(buf, p, count) == 0)
    {
        return buf;
    }
    
    return NULL;
}


INLINE int getsock(int fd, struct socket_t **so)
{
    struct file_t *fp;
    struct task_t *ct = cur_task;
    
    *so = NULL;
    
    if(fd >= (int)NR_OPEN || !ct->ofiles || 
       (fp = ct->ofiles->ofile[fd]) == NULL)
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


/*
 * Helper function to create a new socket.
 */
int sock_create(int domain, int type, int protocol, struct socket_t **res)
{
    struct proto_t *proto;
    struct socket_t *so;
    int err;

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

    KDEBUG("sock_create: 4 - domain %d, type %d\n", domain, type);

    if((err = proto->sockops->socket(domain, type, &so)) != 0)
    {
        return err;
    }

    so->type = type;
    so->domain = domain;
    so->proto = proto;

    so->pid = cur_task->pid;
    so->uid = cur_task->euid;
    so->gid = cur_task->egid;
    so->ttl = -1;   // use route default
    
    so->inq.max = SOCKET_DEFAULT_QUEUE_SIZE;
    so->outq.max = SOCKET_DEFAULT_QUEUE_SIZE;
    
    so->wakeup = NULL;
	
	*res = so;
	return 0;
}


static int sock_createf(int domain, int type, int protocol,
                        unsigned int flags, struct socket_t *so)
{
    int res, fd;
    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    struct task_t *ct = cur_task;
    
    if(!ct || !ct->ofiles)
    {
        return -EINVAL;
    }

#if 0    
    /*
     * We only support IPv4 for now.
     */
    if(domain == AF_INET6)
    {
        return -EAFNOSUPPORT;
    }
#endif

	if((res = falloc(&fd, &f)) != 0)
	{
	    return res;
	}

    if(!(node = sockfs_get_node()))
    {
    	ct->ofiles->ofile[fd] = NULL;
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
        cloexec_set(ct, fd);
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
    
    KDEBUG("sock_createf: fd %d\n", fd);
	
	return fd;

err:

    ct->ofiles->ofile[fd] = NULL;
    f->refs = 0;
    release_node(node);
    return res;
}


/*
 * Handler for syscall socket().
 */
int syscall_socket(int domain, int type, int protocol)
{
    KDEBUG("%s:\n", __func__);
    
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


struct sockport_t *get_sockport(uint16_t proto, uint16_t port)
{
    struct sockport_t *tmp;
    
    switch(proto)
    {
        case IPPROTO_UDP: tmp = udp_ports; break;
        case IPPROTO_TCP: tmp = tcp_ports; break;
        //case IPPROTO_RAW: tmp = raw_ports; break;
        default: return NULL;
    }

    for( ; tmp != NULL; tmp = tmp->next)
    {
        if(tmp->number == port)
        {
            return tmp;
        }
    }
    
    return NULL;
}


int is_port_free(int domain, uint16_t proto, uint16_t port, struct sockaddr *addr)
{
    struct sockport_t *sp = get_sockport(proto, port);

    if(proto == IPPROTO_RAW)
    {
        return 1;
    }
    
    if(domain == AF_INET6)
    {
        struct in6_addr tmp, *local;
        struct socket_t *so;
        int used = 0;

        IPV6_COPY(tmp.s6_addr, addr ? 
                    ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr :
                        IPv6_ANY);
        
        if(memcmp(tmp.s6_addr, IPv6_ANY, 16) == 0)
        {
            return !sp;
        }
        
        if(!sp)
        {
            return 1;
        }
        
        for(so = sp->sockets; so != NULL; so = so->next)
        {
            if(so->domain == AF_INET6)
            {
                local = (struct in6_addr *)&so->local_addr;
                
                if(ipv6_is_unspecified(local->s6_addr) ||
                   memcmp(local->s6_addr, tmp.s6_addr, 16) == 0)
                {
                    used = 1;
                    break;
                }
            }
        }

        return !used;
    }

    if(domain == AF_INET)
    {
        /*
         * TODO: implement NAT
         */
        
        /*
        if(nat_port_in_use(proto, port))
        {
            return 0;
        }
        */
        
        struct in_addr tmp, *local;
        struct socket_t *so;
        int used = 0;

        tmp.s_addr = addr ? 
                        ((struct sockaddr_in *)addr)->sin_addr.s_addr :
                            INADDR_ANY;
        
        if(tmp.s_addr == INADDR_ANY)
        {
            return !sp;
        }
        
        if(!sp)
        {
            return 1;
        }
        
        for(so = sp->sockets; so != NULL; so = so->next)
        {
            if(so->domain == AF_INET)
            {
                local = (struct in_addr *)&so->local_addr;
                
                if(local->s_addr == INADDR_ANY ||
                   local->s_addr == tmp.s_addr)
                {
                    used = 1;
                    break;
                }
            }
        }

        return !used;
    }
    
    return 1;
}


static uint16_t socket_high_port(int domain, uint16_t proto)
{
    uint16_t port;
    
    if(proto != IPPROTO_UDP && proto != IPPROTO_TCP && proto != IPPROTO_RAW)
    {
        return 0;
    }
    
    while(1)
    {
        uint32_t r = genrand_int32();
        
        port = (uint16_t)(r & 0xffff);
        port = htons((uint16_t)((port % (0xffff - 0x400)) + 0x400));
        
        if(is_port_free(domain, proto, port, NULL))
        {
    	    KDEBUG("socket_high_port: got port %u\n", port);
            return port;
        }
    }
}


static int add_unix_or_raw(struct socket_t **socks, 
                           struct kernel_mutex_t *lock, 
                           struct socket_t *so)
{
    struct socket_t *prev;

    so->next = NULL;

    kernel_mutex_lock(lock);

    if(*socks == NULL)
    {
        *socks = so;
    }
    else
    {
        // check if socket is already there
        for(prev = *socks; prev != NULL; prev = prev->next)
        {
            // update state and return
            if(prev == so)
            {
                so->state |= SOCKET_STATE_BOUND;
                kernel_mutex_unlock(lock);
                return 0;
            }
        }

        // add socket to list end
        for(prev = *socks; prev->next != NULL; prev = prev->next)
        {
            ;
        }

        prev->next = so;
    }

    //so->next = unix_socks;
    //unix_socks = so;
    so->state |= SOCKET_STATE_BOUND;
    kernel_mutex_unlock(lock);

    return 0;
}


int socket_add(struct socket_t *so)
{
    struct socket_t *prev;
    struct sockport_t *sp;

    // add a unix socket
    if(so->domain == AF_UNIX)
    {
        return add_unix_or_raw(&unix_socks, &sockunix_lock, so);
    }

    if(RAW_SOCKET(so))
    {
        return add_unix_or_raw(&raw_socks, &sockraw_lock, so);
    }

    // add a tcp or udp socket
    if(so->proto->protocol != IPPROTO_UDP && 
       so->proto->protocol != IPPROTO_TCP)
    {
        return -EINVAL;
    }
    
    kernel_mutex_lock(&sockport_lock);

    sp = get_sockport(so->proto->protocol, so->local_port);
    //sp = get_sockport(SOCK_PROTO(so), so->local_port);
    
    if(!sp)
    {
        if(!(sp = kmalloc(sizeof(struct sockport_t))))
        {
            kernel_mutex_unlock(&sockport_lock);
            return -ENOMEM;
        }
        
        ////memset(sp, 0, sizeof(struct sockport_t));
        sp->proto = so->proto;
        sp->number = so->local_port;
        sp->sockets = NULL;
        sp->next = NULL;
        
        /* if(RAW_SOCKET(so))
        {
            sp->next = raw_ports;
            raw_ports = sp;
        }
        else */ if(so->proto->protocol == IPPROTO_UDP)
        {
            sp->next = udp_ports;
            udp_ports = sp;
        }
        else if(so->proto->protocol == IPPROTO_TCP)
        {
            sp->next = tcp_ports;
            tcp_ports = sp;
        }
    }

    // check if socket is already there
    for(prev = sp->sockets; prev != NULL; prev = prev->next)
    {
        // update state and return
        if(prev == so)
        {
            so->state |= SOCKET_STATE_BOUND;
            kernel_mutex_unlock(&sockport_lock);
            return 0;
        }
    }
    
    so->next = sp->sockets;
    sp->sockets = so;
    so->state |= SOCKET_STATE_BOUND;

    kernel_mutex_unlock(&sockport_lock);

    return 0;
}


static void sockport_delete(struct sockport_t **list, struct sockport_t *sp)
{
    struct sockport_t *prev, *cur;

    for(prev = NULL, cur = *list; cur != NULL; prev = cur, cur = cur->next)
    {
        if(cur == sp)
        {
            if(prev)
            {
                prev->next = cur->next;
            }
            else
            {
                *list = cur->next;
            }
            
            break;
        }
    }
}


void socket_clean_queues(struct socket_t *so)
{
    struct packet_t *p1, *p2;
    
    IFQ_DEQUEUE(&so->inq, p1);
    IFQ_DEQUEUE(&so->outq, p2);
    
    while(p1 || p2)
    {
        if(p1)
        {
            packet_free(p1);
            IFQ_DEQUEUE(&so->inq, p1);
        }

        if(p2)
        {
            packet_free(p2);
            IFQ_DEQUEUE(&so->outq, p2);
        }
    }
    
    socket_tcp_cleanup(so);
}


static int delete_unix_or_raw(struct socket_t **socks, 
                              struct kernel_mutex_t *lock, 
                              struct socket_t *so)
{
    struct socket_t *prev, *cur;

    kernel_mutex_lock(lock);

    for(prev = NULL, cur = *socks; cur != NULL; prev = cur, cur = cur->next)
    {
        if(cur == so)
        {
            if(prev)
            {
                prev->next = cur->next;
            }
            else
            {
                *socks = cur->next;
            }

            break;
        }
    }

    so->state = SOCKET_STATE_CLOSED;

    if(so->pairedsock)
    {
        so->pairedsock->pairedsock = NULL;
        so->pairedsock = NULL;
    }

    kernel_mutex_unlock(lock);

    socket_clean_queues(so);
    kfree(so);

    return 0;
}


int socket_delete(struct socket_t *so)
{
    struct sockport_t *sp;
    struct socket_t *prev, *cur;

    // delete a unix socket
    if(so->domain == AF_UNIX)
    {
        KDEBUG("socket_delete: UNIX socket\n");
        return delete_unix_or_raw(&unix_socks, &sockunix_lock, so);
    }

    // delete a raw socket
    if(RAW_SOCKET(so))
    {
        KDEBUG("socket_delete: RAW socket\n");
        return delete_unix_or_raw(&raw_socks, &sockraw_lock, so);
    }

    // delete a tcp or udp socket
    
    KDEBUG("socket_delete: removing socket from port %d\n", ntohs(so->local_port));

    kernel_mutex_lock(&sockport_lock);

    if(!(sp = get_sockport(so->proto->protocol, so->local_port)))
    //if(!(sp = get_sockport(SOCK_PROTO(so), so->local_port)))
    {
        KDEBUG("socket_delete: cannot find socket\n");
        kernel_mutex_unlock(&sockport_lock);
        return -EINVAL;
    }

    // remove socket from sockport socket list
    for(prev = NULL, cur = sp->sockets; cur != NULL; prev = cur, cur = cur->next)
    {
        if(cur == so)
        {
            if(prev)
            {
                prev->next = cur->next;
            }
            else
            {
                sp->sockets = cur->next;
            }
            
            break;
        }
    }
    
    // remove the sockport if it has no associated sockets
    if(sp->sockets == NULL)
    {
        KDEBUG("socket_delete: removing sockport\n");
        /* if(RAW_SOCKET(sp))
        {
            sockport_delete(&raw_ports, sp);
        }
        else */ if(so->proto->protocol == IPPROTO_UDP)
        {
            sockport_delete(&udp_ports, sp);
        }
        else if(so->proto->protocol == IPPROTO_TCP)
        {
            sockport_delete(&tcp_ports, sp);
        }
        
        kfree(sp);
    }
    
    KDEBUG("socket_delete: closing socket\n");
    socket_tcp_delete(so);
    so->state = SOCKET_STATE_CLOSED;
    kernel_mutex_unlock(&sockport_lock);
    
    socket_clean_queues(so);
    kfree(so);

    return 0;
}


int socket_update_state(struct socket_t *so, uint16_t more_states, 
                        uint16_t less_states, uint16_t tcp_state)
{
    struct sockport_t *sp;

    if(more_states & SOCKET_STATE_BOUND)
    {
        return socket_add(so);
    }

    if(less_states & SOCKET_STATE_BOUND)
    {
        int res;

        if((res = socket_delete(so)) < 0)
        {
            return res;
        }
        
        return 1;
    }

    // update the state of a unix socket
    if(so->domain == AF_UNIX || RAW_SOCKET(so))
    {
        kernel_mutex_lock(&sockunix_lock);
        so->state |= more_states;
        so->state = (uint16_t)(so->state & (~less_states));
        kernel_mutex_unlock(&sockunix_lock);
        
        return 0;
    }

    // update the state of a tcp, udp or raw socket
    kernel_mutex_lock(&sockport_lock);

    if(!(sp = get_sockport(so->proto->protocol, so->local_port)))
    //if(!(sp = get_sockport(SOCK_PROTO(so), so->local_port)))
    {
        kernel_mutex_unlock(&sockport_lock);
        return -EINVAL;
    }
    
    so->state |= more_states;
    so->state = (uint16_t)(so->state & (~less_states));
    
    if(tcp_state)
    {
        so->state &= 0x00ff;
        so->state |= tcp_state;
    }

    kernel_mutex_unlock(&sockport_lock);
    
    return 0;
}


/*
 * Handler for syscall bind().
 */
int syscall_bind(int s, struct sockaddr *_name, socklen_t namelen)
{
    KDEBUG("%s:\n", __func__);

	struct socket_t *so;
	int res;
	uint16_t port;
	struct sockaddr *name;
    struct sockaddr_in *sin;
    struct sockaddr_in6 *sin6;

    if(!_name || !namelen)
    {
        //return -EFAULT;
        SYSCALL_EFAULT(_name);
    }
    
	if((res = getsock(s, &so)))
	{
		return res;
	}
	
	if(!(name = (struct sockaddr *)malloced_copy(_name, namelen)))
	{
		return -ENOBUFS;
	}



    // we will end up using ONLY ONE of these two below
    sin = (struct sockaddr_in *)name;
    sin6 = (struct sockaddr_in6 *)name;
	
	// validate local address
	if(so->domain == AF_INET)       // IPv4
	{
        if(namelen < sizeof(struct sockaddr_in))
        {
        	kfree(name);
            //return -EFAULT;
            SYSCALL_EFAULT(_name);
        }

        if(sin->sin_family != AF_INET && sin->sin_family != AF_UNSPEC)
        {
        	kfree(name);
            return -EAFNOSUPPORT;
        }
	    
	    if(sin->sin_addr.s_addr != INADDR_ANY)
	    {
	        if(!ipv4_link_find(&sin->sin_addr))
	        {
            	kfree(name);
            	return -EINVAL;
	        }
	    }
	    
	    KDEBUG("syscall_bind: addr ");
	    KDEBUG_IPV4_ADDR(ntohl(sin->sin_addr.s_addr));
	    KDEBUG(", port %u\n", sin->sin_port);
	    
	    port = sin->sin_port;
	}
	else if(so->domain == AF_INET6)     // IPv6
	{
        if(namelen < sizeof(struct sockaddr_in6))
        {
        	kfree(name);
            //return -EFAULT;
            SYSCALL_EFAULT(_name);
        }

        if(sin6->sin6_family != AF_INET6 && sin6->sin6_family != AF_UNSPEC)
        {
        	kfree(name);
            return -EAFNOSUPPORT;
        }
        
        if(!ipv6_is_unspecified(sin6->sin6_addr.s6_addr))
        {
            if(!ipv6_link_get(&sin6->sin6_addr))
            {
            	kfree(name);
            	return -EINVAL;
            }
        }

	    port = sin6->sin6_port;
	}
	else if(so->domain == AF_UNIX)      // UNIX
	{
        res = socket_unix_bind(so, name, namelen);
        kfree(name);
        
        if(res == -EFAULT)
        {
            SYSCALL_EFAULT(_name);
        }

    	KDEBUG("syscall_bind: UNIX done - bound to %s\n", so->local_addr.sun.sun_path);
	    
	    if(res == 0)
	    {
    	    return socket_update_state(so, SOCKET_STATE_BOUND, 0, 0);
    	}
    	else
    	{
    	    return res;
    	}
	}
	else    // neither IPv4 nor IPv6
	{
    	kfree(name);
    	return -EINVAL;
	}
	
	// if port == 0, choose a random high port, unless if it is a raw socket
	if(port == 0 && !RAW_SOCKET(so))
	{
	    //if((port = socket_high_port(so->domain, so->proto->protocol)) == 0)
	    if((port = socket_high_port(so->domain, SOCK_PROTO(so))) == 0)
	    {
        	kfree(name);
        	return -EINVAL;
	    }
	}

    //if(!is_port_free(so->domain, so->proto->protocol, port, name))
    if(!is_port_free(so->domain, SOCK_PROTO(so), port, name))
    {
        kfree(name);
        return -EADDRINUSE;
    }
    
    KDEBUG("syscall_bind: so->local_port %u\n", so->local_port);

    so->local_port = port;

	if(so->domain == AF_INET)       // IPv4
	{
	    so->local_addr.ipv4.s_addr = sin->sin_addr.s_addr;
	}
	else     // IPv6
	{
	    IPV6_COPY(so->local_addr.ipv6.s6_addr, sin6->sin6_addr.s6_addr);
	}

    KDEBUG("syscall_bind: so->local_port %u\n", so->local_port);

	kfree(name);
	
	return socket_update_state(so, SOCKET_STATE_BOUND, 0, 0);
}


/*
 * Handler for syscall connect().
 */
int syscall_connect(int fd, struct sockaddr *_name, socklen_t namelen)
{
    KDEBUG("%s:\n", __func__);

	struct socket_t *so;
	int res;
	uint16_t port;
	struct sockaddr *name;

    if(!_name || !namelen)
    {
        //return -EFAULT;
        SYSCALL_EFAULT(_name);
    }

	if((res = getsock(fd, &so)))
	{
		return res;
	}

	if((so->flags & SOCKET_FLAG_NONBLOCK) && (so->state & SOCKET_STATE_CONNECTING))
	{
		return -EALREADY;
	}

	if(so->state & SOCKET_STATE_CONNECTED)
	{
		return -EISCONN;
	}

	if(!(name = (struct sockaddr *)malloced_copy(_name, namelen)))
	{
		return -ENOBUFS;
	}

	// validate local address
	if(so->domain == AF_INET)       // IPv4
	{
	    struct in_addr local;
	    struct sockaddr_in *sin;

        if(namelen < sizeof(struct sockaddr_in))
        {
        	kfree(name);
            //return -EFAULT;
            SYSCALL_EFAULT(_name);
        }

        sin = (struct sockaddr_in *)name;

        if(sin->sin_family != AF_INET && sin->sin_family != AF_UNSPEC)
        {
        	kfree(name);
            return -EAFNOSUPPORT;
        }
        
        so->remote_addr.ipv4.s_addr = sin->sin_addr.s_addr;

        if(ipv4_source_find(&local, &sin->sin_addr) == 0)
        {
            sock_get_ifp(so);
            so->local_addr.ipv4.s_addr = local.s_addr;
        }
        else
        {
        	kfree(name);
            return -EHOSTUNREACH;
        }
	    
	    port = sin->sin_port;
	}
	else if(so->domain == AF_INET6)     // IPv6
	{
	    struct in6_addr local;
	    struct sockaddr_in6 *sin6;

        if(namelen < sizeof(struct sockaddr_in6))
        {
        	kfree(name);
            //return -EFAULT;
            SYSCALL_EFAULT(_name);
        }

        sin6 = (struct sockaddr_in6 *)name;

        if(sin6->sin6_family != AF_INET6 && sin6->sin6_family != AF_UNSPEC)
        {
        	kfree(name);
            return -EAFNOSUPPORT;
        }
        
        IPV6_COPY(so->remote_addr.ipv6.s6_addr, sin6->sin6_addr.s6_addr);

        if(ipv6_source_find(&local, &sin6->sin6_addr) == 0)
        {
            sock_get_ifp(so);
            IPV6_COPY(so->local_addr.ipv6.s6_addr, local.s6_addr);
        }
        else
        {
        	kfree(name);
            return -EHOSTUNREACH;
        }

	    port = sin6->sin6_port;
	}
	else if(so->domain == AF_UNIX)      // UNIX
	{
        res = socket_unix_connect(so, name, namelen);

        KDEBUG("syscall_connect: UNIX name %s\n", name);

        kfree(name);
        
        if(res == -EFAULT)
        {
            SYSCALL_EFAULT(_name);
        }

    	KDEBUG("syscall_connect: UNIX done - res %d, connected to %s\n", res, so->remote_addr.sun.sun_path);
	    
	    if(res == 0)
	    {
        	socket_update_state(so, SOCKET_STATE_BOUND, 0, 0);
	        socket_update_state(so, SOCKET_STATE_CONNECTED, 0, 0);
    	}

    	return res;
	}
	else    // neither IPv4 nor IPv6
	{
    	kfree(name);
    	return -EINVAL;
	}
	
	so->remote_port = port;

	// if port == 0, choose a random high port, unless if it is a raw socket
	if(so->local_port == 0 && !RAW_SOCKET(so))
	{
	    //if((so->local_port = socket_high_port(so->domain, so->proto->protocol)) == 0)
	    if((so->local_port = socket_high_port(so->domain, SOCK_PROTO(so))) == 0)
	    {
        	kfree(name);
        	return -EINVAL;
	    }
	}
	
	kfree(name);
	socket_update_state(so, SOCKET_STATE_BOUND, 0, 0);
	
	if(so->proto->protocol == IPPROTO_UDP || RAW_SOCKET(so))
	{
    	socket_update_state(so, SOCKET_STATE_CONNECTED, 0, 0);
    	return 0;
	}

	if(so->proto->protocol == IPPROTO_TCP)
	{
	    return tcp_init_connection(so);
	    
	    /*
	    if(tcp_init_connection(so) == 0)
	    {
        	socket_update_state(so, SOCKET_STATE_CONNECTED | 
        	                        SOCKET_STATE_TCP_SYN_SENT,
        	                        SOCKET_STATE_CLOSED, 0);
        	return 0;
	    }

    	return -EHOSTUNREACH;
    	*/
	}
	
	return -EINVAL;
}


int socket_check(struct socket_t *so)
{
    struct sockport_t *sp;
    struct socket_t *tmp;

    // unix sockets
	if(so->domain == AF_UNIX)
	{
        kernel_mutex_lock(&sockunix_lock);

        for(tmp = unix_socks; tmp != NULL; tmp = tmp->next)
        {
            if(tmp == so)
            {
                break;
                //kernel_mutex_unlock(&sockunix_lock);
                //return 0;
            }
        }

        kernel_mutex_unlock(&sockunix_lock);
        return tmp ? 0 : -EINVAL;
	}

	if(RAW_SOCKET(so))
	{
	    return 0;

        /*
        kernel_mutex_lock(&sockraw_lock);

        for(tmp = raw_socks; tmp != NULL; tmp = tmp->next)
        {
            if(tmp == so)
            {
                break;
            }
        }

        kernel_mutex_unlock(&sockraw_lock);
        return tmp ? 0 : -EINVAL;
        */
	}
    
    // tcp & udp sockets
    kernel_mutex_lock(&sockport_lock);

    KDEBUG("socket_check: domain %d, type %d, proto %d, port %d\n", so->domain, so->type, so->proto->protocol, ntohs(so->local_port));

    if(!(sp = get_sockport(so->proto->protocol, so->local_port)))
    //if(!(sp = get_sockport(SOCK_PROTO(so), so->local_port)))
    {
        kernel_mutex_unlock(&sockport_lock);
        return -EINVAL;
    }

    for(tmp = sp->sockets; tmp != NULL; tmp = tmp->next)
    {
        if(tmp == so)
        {
            kernel_mutex_unlock(&sockport_lock);
            return 0;
        }
    }
    
    kernel_mutex_unlock(&sockport_lock);
    return -EINVAL;
}


/*
 * Handler for syscall listen().
 */
int syscall_listen(int s, int backlog)
{
    KDEBUG("%s:\n", __func__);

	struct socket_t *so;
	int res;

	if((res = getsock(s, &so)))
	{
		return res;
	}



    if(backlog < 1)
    {
        return -EINVAL;
    }
    
    // should be on one of the protocol's lists
    if(socket_check(so) < 0)
    {
        return -EINVAL;
    }
    
    if(so->proto->protocol == IPPROTO_UDP || RAW_SOCKET(so))
    {
        return -EINVAL;
    }
    
    if(!(so->state & SOCKET_STATE_BOUND))
    {
        return -EISCONN;
    }

	if(so->domain == AF_UNIX)      // UNIX
	{
    	socket_update_state(so, SOCKET_STATE_LISTENING, 0, 0);
	}
    else if(so->proto->protocol == IPPROTO_TCP)
    {
    	socket_update_state(so, SOCKET_STATE_TCP_SYN_SENT, 0, 
    	                        SOCKET_STATE_TCP_LISTEN);
    }
    
    so->max_backlog = backlog;
    
    return 0;
}


/*
 * Handler for syscall accept().
 */
int syscall_accept(int fd, struct sockaddr *_name, socklen_t *anamelen)
{
    KDEBUG("%s:\n", __func__);
	//__asm__ __volatile__("xchg %%bx, %%bx"::);

    struct sockaddr *name;
	socklen_t namelen;
	int res;
	struct socket_t *so, *newso = NULL;

	if((res = getsock(fd, &so)))
	{
		return res;
	}

    if(!(so->state & SOCKET_STATE_BOUND))
    {
        return -ENOTCONN;
    }
    
    if(so->proto->protocol == IPPROTO_UDP || RAW_SOCKET(so))
    {
        return -EINVAL;
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

try:

    res = -EINVAL;

    if(so->state & SOCKET_STATE_LISTENING)
    {
        kernel_mutex_lock(&sockunix_lock);

        for(newso = unix_socks; newso != NULL; newso = newso->next)
        {
            if(newso->parent == so)
            {
                newso->parent = NULL;

                A_memcpy(name, &newso->remote_addr.sun,
                               sizeof(struct sockaddr_un));

                so->pending_connections--;
                res = 0;
                break;
            }
        }

        kernel_mutex_unlock(&sockunix_lock);
    }
    else if((so->state & SOCKET_STATE_TCP) == SOCKET_STATE_TCP_LISTEN)
    {
        struct sockport_t *sp;
        
        kernel_mutex_lock(&sockport_lock);

        if(!(sp = get_sockport(IPPROTO_TCP, so->local_port)))
        {
            kernel_mutex_unlock(&sockport_lock);
            kfree(name);
            return -EAGAIN;
        }

        for(newso = sp->sockets; newso != NULL; newso = newso->next)
        {
            if(newso->parent == so &&
                    (newso->state & SOCKET_STATE_TCP) == 
                                        SOCKET_STATE_TCP_ESTABLISHED)
            {
                newso->parent = NULL;
                
                if(so->domain == AF_INET6)
                {
                    ((struct sockaddr_in6 *)name)->sin6_port = 
                                        newso->remote_port;
                    IPV6_COPY(((struct sockaddr_in6 *)name)->sin6_addr.s6_addr,
                                        &newso->remote_addr.ipv6);
                }
                else
                {
                    ((struct sockaddr_in *)name)->sin_port = 
                                        newso->remote_port;
                    ((struct sockaddr_in *)name)->sin_addr.s_addr =
                                        newso->remote_addr.ipv4.s_addr;
                }
                
                so->pending_connections--;
                res = 0;
                break;
            }
        }

        kernel_mutex_unlock(&sockport_lock);
    }
    else
    {
        kfree(name);
        return -EINVAL;
    }

    if(!newso)
    {
        if(so->flags & SOCKET_FLAG_NONBLOCK)
        {
            kfree(name);
            return -EAGAIN;
        }
        
        block_task(&so->pending_connections, 1);

        if(cur_task->woke_by_signal)
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
 * Handler for syscall getsockname().
 */
int syscall_getsockname(int fdes, struct sockaddr *_name, socklen_t *namelen)
{
    KDEBUG("%s:\n", __func__);

    //struct sockaddr *name;
    struct socket_t *so;
	socklen_t len;
	int res;

    if(!_name || !namelen)
    {
        //return -EFAULT;
        SYSCALL_EFAULT(_name);
    }

	if((res = getsock(fdes, &so)))
	{
		return res;
	}

    /*
    if(!(so->state & SS_BOUND) && !(so->state & SS_CONNECTED))
    {
        return -EINVAL;
    }
    */
	
	if((res = copy_from_user(&len, namelen, sizeof(len))))
	{
		return res;
	}
	
	if(so->domain == AF_INET)
	{
	    struct sockaddr_in sin;
	    
	    sin.sin_addr.s_addr = so->local_addr.ipv4.s_addr;
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
	    struct sockaddr_in6 sin;

	    if(len < sizeof(struct sockaddr_in6))
	    {
	        return -ENOBUFS;
	    }
	    
	    IPV6_COPY(sin.sin6_addr.s6_addr, so->local_addr.ipv6.s6_addr);
	    sin.sin6_port = so->local_port;
    	
    	if((res = copy_to_user(_name, &sin, sizeof(struct sockaddr_in6))) == 0)
    	{
    	    len = sizeof(struct sockaddr_in6);
    		res = copy_to_user(namelen, &len, sizeof(len));
    	}
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
int syscall_getpeername(int fdes, struct sockaddr *_name, socklen_t *alen)
{
    KDEBUG("%s:\n", __func__);

    //struct sockaddr *name;
    struct socket_t *so;
	socklen_t len;
    int res;

	if((res = getsock(fdes, &so)))
	{
		return res;
	}

    /*
    if(!(so->state & SS_BOUND) && !(so->state & SS_CONNECTED))
    {
        return -EINVAL;
    }
    */
	
	if((res = copy_from_user(&len, alen, sizeof(len))))
	{
		return res;
	}

    if((so->state & SOCKET_STATE_CONNECTED) == 0)
    {
        return -ENOTCONN;
    }
    
	if(so->domain == AF_INET)
	{
	    struct sockaddr_in sin;
	    
	    sin.sin_addr.s_addr = so->remote_addr.ipv4.s_addr;
	    sin.sin_port = so->remote_port;
	    
	    if(len < sizeof(struct sockaddr_in))
	    {
	        return -ENOBUFS;
	    }
    	
    	if((res = copy_to_user(_name, &sin, sizeof(struct sockaddr_in))) == 0)
    	{
    		res = copy_to_user(alen, &len, sizeof(len));
    	}
	}
	else if(so->domain == AF_INET6)
	{
	    struct sockaddr_in6 sin;

	    if(len < sizeof(struct sockaddr_in6))
	    {
	        return -ENOBUFS;
	    }
	    
	    IPV6_COPY(sin.sin6_addr.s6_addr, so->remote_addr.ipv6.s6_addr);
	    sin.sin6_port = so->remote_port;
    	
    	if((res = copy_to_user(_name, &sin, sizeof(struct sockaddr_in6))) == 0)
    	{
    		res = copy_to_user(alen, &len, sizeof(len));
    	}
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
 * Handler for syscall socketpair().
 */
int syscall_socketpair(int domain, int type, int protocol, int *rsv)
{
	struct socket_t *so1, *so2;
	int sv[2];
	int res;
    struct task_t *ct = cur_task;
    
    

    return -ENOSYS;

    

    if((res = sock_createf(domain, type, protocol, 
                                    O_RDWR | O_NOATIME, NULL)) < 0)
    {
        return res;
    }

	sv[0] = res;

    if((res = sock_createf(domain, type, protocol, 
                                    O_RDWR | O_NOATIME, NULL)) < 0)
    {
        syscall_close(sv[0]);
        return res;
    }

	sv[1] = res;
	so1 = ct->ofiles->ofile[sv[0]]->node->data;
	so2 = ct->ofiles->ofile[sv[1]]->node->data;
	
	if(so1->proto != so2->proto || so1->proto->sockops->connect2 == NULL)
	{
        syscall_close(sv[0]);
        syscall_close(sv[1]);
        return -EPROTONOSUPPORT;
	}

	//softint_block(SOFTINT_NET);

	if((res = so1->proto->sockops->connect2(so1, so2)))
	{
        syscall_close(sv[0]);
        syscall_close(sv[1]);
    	//softint_unblock(SOFTINT_NET);
		return res;
	}

	//softint_unblock(SOFTINT_NET);
	return copy_to_user(rsv, sv, 2 * sizeof (int));
}


int sendto_get_ipv4_src(struct socket_t *so, 
                        struct sockaddr_in *dest,
                        struct sockaddr_in *res)
{
    struct in_addr dest4;
    //struct in_addr *src = (struct in_addr *)res;
    
    if(dest->sin_family != AF_INET)
    {
        return -EAFNOSUPPORT;
    }
    
    dest4.s_addr = dest->sin_addr.s_addr;
    
    A_memset(res, 0, sizeof(struct sockaddr_in));

    // Check if socket is connected: destination address MUST match the
    // current connected endpoint
    if(so->state & SOCKET_STATE_CONNECTED)
    {
        res->sin_addr.s_addr = so->local_addr.ipv4.s_addr;
        
        if(so->remote_addr.ipv4.s_addr != dest4.s_addr)
        {
            return -EADDRNOTAVAIL;
        }
    }
    else
    {
        if(ipv4_source_find(&res->sin_addr, &dest4) != 0)
        {
            return -EHOSTUNREACH;
        }
    }
    
    if(res->sin_addr.s_addr != INADDR_ANY)
    {
        so->local_addr.ipv4.s_addr = res->sin_addr.s_addr;
    }
    
    return 0;
}


static int sendto_get_ipv6_src(struct socket_t *so, 
                               struct sockaddr_in6 *dest,
                               struct sockaddr_in6 *res)
{
    struct in6_addr dest6;
    //struct in6_addr *src = (struct in6_addr *)res;

    if(dest->sin6_family != AF_INET6)
    {
        return -EAFNOSUPPORT;
    }

    IPV6_COPY(dest6.s6_addr, dest->sin6_addr.s6_addr);
    A_memset(res, 0, sizeof(struct sockaddr_in6));

    // Check if socket is connected: destination address MUST match the
    // current connected endpoint
    if(so->state & SOCKET_STATE_CONNECTED)
    {
        IPV6_COPY(res->sin6_addr.s6_addr, so->local_addr.ipv6.s6_addr);
        
        if(memcmp(so->remote_addr.ipv6.s6_addr, dest6.s6_addr, 16))
        {
            return -EADDRNOTAVAIL;
        }
    }
    else
    {
        if(ipv6_source_find(&res->sin6_addr, &dest6) != 0)
        {
            return -EHOSTUNREACH;
        }

        if(!ipv6_is_unspecified(res->sin6_addr.s6_addr))
        {
            IPV6_COPY(so->local_addr.ipv6.s6_addr, res->sin6_addr.s6_addr);
        }
    }
    
    return 0;
}


int sendto_pre_checks(struct socket_t *so, 
                      struct sockaddr *to, socklen_t tolen,
                      char *src_namebuf, char *dest_namebuf)
{
    int res;
    
	if(socket_check(so) != 0)
	{
        return -EINVAL;
	}
	
	// both of dest addr AND its length MUST be provided or omitted
	if((!!to) != (!!tolen))
	{
        return -EINVAL;
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
    }
    else
    {
    	if(!(so->state & SOCKET_STATE_CONNECTED))
    	{
            return -ENOTCONN;
    	}

        // use the socket's remote addr
    	if(so->domain == AF_INET)
    	{
    	    struct sockaddr_in tmp = { 0, };
    	    
    	    tmp.sin_family = AF_INET;
    	    tmp.sin_port = so->remote_port;
    	    tmp.sin_addr.s_addr = so->remote_addr.ipv4.s_addr;
    	    A_memcpy(dest_namebuf, &tmp, sizeof(tmp));
    	}
    	else if(so->domain == AF_INET6)
    	{
    	    struct sockaddr_in6 tmp = { 0, };
    	    
    	    tmp.sin6_family = AF_INET6;
    	    tmp.sin6_port = so->remote_port;
    	    IPV6_COPY(tmp.sin6_addr.s6_addr, so->remote_addr.ipv6.s6_addr);
    	    A_memcpy(dest_namebuf, &tmp, sizeof(tmp));
    	}
    	else if(so->domain == AF_UNIX)
    	{
    	    A_memcpy(dest_namebuf, &so->remote_addr.sun, sizeof(struct sockaddr_un));
    	}
    	else
    	{
    	    KDEBUG("syscall_sendto_internal: unknown addr family 1\n");
    	    return -EINVAL;
    	}
    }
    
    // get src addr
	if(so->domain == AF_INET)
	{
	    if((res = sendto_get_ipv4_src(so, (struct sockaddr_in *)dest_namebuf,
	                                      (struct sockaddr_in *)src_namebuf)) != 0)
	    {
	        return res;
	    }
	}
	else if(so->domain == AF_INET6)
	{
	    if((res = sendto_get_ipv6_src(so, (struct sockaddr_in6 *)dest_namebuf,
	                                      (struct sockaddr_in6 *)src_namebuf)) != 0)
	    {
	        if(so->ifp &&
	           ipv6_is_multicast(((struct sockaddr_in6 *)
	                                    dest_namebuf)->sin6_addr.s6_addr))
	        {
	            struct ipv6_link_t *link;
	            
	            if(!(link = ipv6_linklocal_get(so->ifp)))
	            {
	                return -EHOSTUNREACH;
	            }
	            
	            IPV6_COPY(((struct sockaddr_in6 *)
	                        src_namebuf)->sin6_addr.s6_addr, 
	                                link->addr.s6_addr);
	        }
	        else
	        {
	            return res;
	        }
	    }
	}
    else if(so->domain == AF_UNIX)
    {
    	A_memcpy(src_namebuf, &so->local_addr.sun, sizeof(struct sockaddr_un));
    	return 0;
    }
	else
	{
	    KDEBUG("syscall_sendto_internal: unknown addr family 2\n");
	    return -EINVAL;
	}

	// set local port if needed
	if(!(so->state & SOCKET_STATE_BOUND))
	{
	    //if((so->local_port = socket_high_port(so->domain, so->proto->protocol)) == 0)
	    if(!RAW_SOCKET(so) &&
	       (so->local_port = socket_high_port(so->domain, SOCK_PROTO(so))) == 0)
	    {
        	return -EINVAL;
	    }
	    
	    socket_update_state(so, SOCKET_STATE_BOUND, 0, 0);
	}

    // set remote port if needed
    if(!(so->state & SOCKET_STATE_CONNECTED))
	{
    	if(so->domain == AF_INET)
    	{
    	    so->remote_port = ((struct sockaddr_in *)dest_namebuf)->sin_port;
    	}
    	else if(so->domain == AF_INET6)
    	{
    	    so->remote_port = ((struct sockaddr_in6 *)dest_namebuf)->sin6_port;
    	}
	}
	
	return 0;
}


INLINE int sendto_hdr_offset(struct socket_t *so)
{
    if(RAW_SOCKET(so))
    {
        return 0;
    }
    else if(so->proto->protocol == IPPROTO_TCP)
    {
        //return (tcp_opt_size((struct socket_tcp_t *)so, 0) + TCP_HLEN);
        return TCP_HLEN;
    }
    else if(so->proto->protocol == IPPROTO_UDP)
    {
        return UDP_HLEN;
    }
    else
    {
        return 0;
    }
}


static int sendto_available_space(struct socket_t *so)
{
    int mss;
    
    if(so->domain == AF_UNIX)
    {
        mss = 65535;
    }
    //else if(so->proto->protocol == IPPROTO_TCP)
    else if(SOCK_PROTO(so) == IPPROTO_TCP)
    {
        mss = (((struct socket_tcp_t *)so)->smss) ?
                ((struct socket_tcp_t *)so)->smss + TCP_HLEN :
                (int)socket_get_mss(so);
    }
    else
    {
        mss = socket_get_mss(so);
    }
    
    mss -= sendto_hdr_offset(so);
    
    return mss;
}


static int do_sendto_one(struct socket_t *so, struct msghdr *msg,
                         void *src, void *dest, int flags, int kernel)
{
    struct netif_t *ifp = NULL;
    struct packet_t *p;
    int hoff = sendto_hdr_offset(so);
    int res, size;
    
    UNUSED(flags);

    if(so->domain == AF_INET6)
    {
        struct sockaddr_in6 *a = (struct sockaddr_in6 *)dest;
        
        if(ipv6_is_multicast(a->sin6_addr.s6_addr))
        {
            struct ipv6_link_t *link;
            
            a = (struct sockaddr_in6 *)src;
            
            if((link = ipv6_link_get(&a->sin6_addr)))
            {
                ifp = link->ifp;
            }
        }
        else
        {
            ifp = ipv6_source_ifp_find(&a->sin6_addr);
        }
    }
    else if(so->domain == AF_INET)
    {
        struct sockaddr_in *a = (struct sockaddr_in *)dest;
        
        ifp = ipv4_source_ifp_find(&a->sin_addr);

        KDEBUG("do_sendto_one: ifp %lx\n", ifp);
    }
    
    if(!ifp && so->domain != AF_UNIX)
    {
        return -EHOSTUNREACH;
    }

    KDEBUG("do_sendto_one: -- \n");

    if((size = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }

    KDEBUG("do_sendto_one: size %d\n", size);

    if(!(p = packet_alloc(size + hoff, PACKET_TRANSPORT)))
    {
        return -ENOMEM;
    }

    p->sock = so;
    p->ifp = ifp;
    p->transport_hdr = p->data;
    p->frag = IP_DF;
    
    packet_add_header(p, -hoff);

    /*
    if(so->proto->protocol == IPPROTO_TCP)
    {
        p->transport_flags_saved = ((struct socket_tcp_t *)so)->ts_ok;
    }
    */
    
    if(so->domain == AF_INET6)
    {
        IPV6_COPY(p->remote_addr.ipv6.s6_addr,
                      ((struct sockaddr_in6 *)dest)->sin6_addr.s6_addr);
        p->remote_port = ((struct sockaddr_in6 *)dest)->sin6_port;
    }
    else if(so->domain == AF_INET)
    {
        p->remote_addr.ipv4.s_addr = 
                      ((struct sockaddr_in *)dest)->sin_addr.s_addr;
        p->remote_port = ((struct sockaddr_in *)dest)->sin_port;
    }
    
    if((res = read_iovec(msg->msg_iov, msg->msg_iovlen, p->data, 
                         p->count, kernel)) < 0)
    {
        KDEBUG("do_sendto_one: res %d\n", res);
        packet_free(p);
        return res;
    }

    KDEBUG("do_sendto_one: pushing to proto %u\n", so->proto->protocol);

    if((res = so->proto->push(p)) > 0)
    {
        KDEBUG("do_sendto_one: success - res %d\n", res);
        return p->count;
    }

    KDEBUG("do_sendto_one: failure - res %d\n", res);

    //packet_free(p);
    return res;
}


static int do_sendto_fragments(struct socket_t *so, struct msghdr *msg, 
                               void *src, void *dest, int flags, int kernel)
{
    int space = AVAILABLE_SPACE(so);
    int static_hoff = sendto_hdr_offset(so);
    int hoff = sendto_hdr_offset(so);
    int size, res;
    int written = 0;
    struct packet_t *p;
    
    if(space < 0)
    {
        return -EPROTONOSUPPORT;
    }

    KDEBUG("do_sendto_fragments: -- \n");

    if((size = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }
    
    if(space > size)
    {
        return do_sendto_one(so, msg, src, dest, flags, kernel);
    }
    
    // Can't fragment IPv6
    if(so->domain == AF_INET6)
    {
        // XXX: send upto 'space', not 'size' bytes
        return do_sendto_one(so, msg, src, dest, flags, kernel);
    }
    
    while(written < size)
    {
        // Always allocate the max space available: space + offset
        if(size < space)
        {
            space = size;
        }
        
        // update space for last fragment
        if(space > size - written)
        {
            space = size - written;
        }
        
        if(!(p = packet_alloc(space + hoff, PACKET_TRANSPORT)))
        {
            return -ENOMEM;
        }

        p->sock = so;
        p->ifp = so->ifp;
        p->transport_hdr = p->data;

        if(so->domain == AF_INET6)
        {
            IPV6_COPY(p->remote_addr.ipv6.s6_addr,
                      ((struct sockaddr_in6 *)dest)->sin6_addr.s6_addr);
            p->remote_port = ((struct sockaddr_in6 *)dest)->sin6_port;
        }
        else
        {
            p->remote_addr.ipv4.s_addr = 
                      ((struct sockaddr_in *)dest)->sin_addr.s_addr;
            p->remote_port = ((struct sockaddr_in *)dest)->sin_port;
        }
        
        if(written == 0)
        {
            // First fragment: no payload written yet!
            KDEBUG("FRAG: first fragmented frame %p | len = %u offset = 0\n", p, p->count);
            // transport header length field contains total length + header length
            p->frag = IP_MF;
            packet_add_header(p, -hoff);
            space += hoff;
            hoff = 0;
        }
        else
        {
            // Next fragment
            // no transport header in fragmented IP
            // set offset in octets
            p->frag = (uint16_t)((written + static_hoff) >> 3);
            
            if(written + space < size)
            {
                KDEBUG("FRAG: intermediate fragmented frame %p | len = %u offset = %u\n", p, p->count - static_hoff, htons(p->frag));
                p->frag |= IP_MF;
            }
            else
            {
                KDEBUG("FRAG: last fragmented frame %p | len = %u offset = %u\n", p, p->count - static_hoff, htons(p->frag));
                p->frag &= IP_OFFMASK;
            }
        }
        
        if((res = read_iovec(msg->msg_iov, msg->msg_iovlen, p->data, 
                             p->count, kernel)) < 0)
        {
            packet_free(p);
            return res;
        }
        
        /*
        if(so->proto->protocol == IPPROTO_TCP)
        {
            p->transport_flags_saved = ((struct socket_tcp_t *)so)->ts_ok;
        }
        */
        
        if((res = so->proto->push(p)) > 0)
        {
            if(written == 0)
            {
                // first packet
                written += static_hoff;
            }
            
            written += p->count;
        }
        else
        {
            //packet_free(p);
            return res;
        }
    }
    
    return written;
}


int do_sendto(struct socket_t *so, struct msghdr *msg, 
              void *src, void *dest, int flags, int kernel)
{
    int space = AVAILABLE_SPACE(so);
    int size;
    int written = 0;
    
    KDEBUG("do_sendto: space %d\n", space);
    
    if(space < 0)
    {
        return -EPROTONOSUPPORT;
    }

    KDEBUG("do_sendto: -- \n");

    if((size = get_iovec_size(msg->msg_iov, msg->msg_iovlen)) == 0)
    {
        return -EINVAL;
    }

    KDEBUG("do_sendto: size %d\n", size);
    
    if((so->proto->protocol == IPPROTO_UDP ||
        RAW_SOCKET(so)) && size > space)
    {
        return do_sendto_fragments(so, msg, src, dest, flags, kernel);
    }
    
    while(written < size)
    {
        KDEBUG("do_sendto: written %d, size %d\n", written, size);

        int w, plen = size - written;
        
        if(plen > space)
        {
            plen = space;
        }
        
        if((w = do_sendto_one(so, msg, src, dest, flags, kernel)) <= 0)
        {
            // if we have written anything, return the byte count, otherwise
            // return the error code
            if(written == 0)
            {
                written = w;
            }

            break;
        }
        
        written += w;
        
        if(so->proto->protocol == IPPROTO_UDP)
        {
            // Break after the first datagram sent with at most MTU bytes
            break;
        }
    }

    KDEBUG("do_sendto: done - written %d\n", written);
    
    return written;
}


static int syscall_sendto_internal(int s, void *buf, size_t len, int flags,
                                   struct sockaddr *to, socklen_t tolen)
{
    KDEBUG("%s:\n", __func__);

	struct msghdr msg;
	struct iovec aiov;
	struct socket_t *so;
	int res;
	char dest_namebuf[128];
	char src_namebuf[128];

    if(!buf)
    {
        KDEBUG("syscall_sendto_internal: invalid buf\n");
        return -EINVAL;
    }

	if((res = getsock(s, &so)))
	{
        KDEBUG("syscall_sendto_internal: invalid sock\n");
		return res;
	}

    if((res = sendto_pre_checks(so, to, tolen, src_namebuf, dest_namebuf)) != 0)
    {
        KDEBUG("syscall_sendto_internal: failed prechecks (res %d)\n", res);
		return res;
    }
	
	msg.msg_name = (caddr_t)to;
	msg.msg_namelen = tolen;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
	aiov.iov_base = buf;
	aiov.iov_len = len;

    return do_sendto(so, &msg, src_namebuf, dest_namebuf, flags, 0);
}


/*
 * Handler for syscall sendto().
 */
int syscall_sendto(struct syscall_args *__args)
{
    KDEBUG("%s:\n", __func__);

    // syscall args
    struct syscall_args args;
    int s;
    void *buf;
    size_t len;
    int flags;
    struct sockaddr *to;
    socklen_t tolen;
	int res;
    
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
int syscall_sendmsg(int s, struct msghdr *_msg, int flags)
{
    KDEBUG("%s:\n", __func__);

	struct msghdr msg;
	int res;
	struct socket_t *so;
	char dest_namebuf[128];
	char src_namebuf[128];

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
        return res;
    }
    
    if(!(msg.msg_iov = dup_iovec(_msg->msg_iov, _msg->msg_iovlen)))
    {
        return -ENOMEM;
    }

    if((res = sendto_pre_checks(so, msg.msg_name, msg.msg_namelen, 
                                src_namebuf, dest_namebuf)) != 0)
    {
    	kfree(msg.msg_iov);
		return res;
    }

    res = do_sendto(so, &msg, src_namebuf, dest_namebuf, flags, 0);

	kfree(msg.msg_iov);

	if(res == -EFAULT)
	{
        SYSCALL_EFAULT(_msg);
	}

	return res;
}


INLINE int check_namelen(struct socket_t *so, socklen_t len)
{
	if(so->domain == AF_INET)
	{
	    if(len < sizeof(struct sockaddr_in))
	    {
	        return -ENOBUFS;
	    }
	}
	else if(so->domain == AF_INET6)
	{
	    if(len < sizeof(struct sockaddr_in6))
	    {
	        return -ENOBUFS;
	    }
	}
	else if(so->domain == AF_UNIX)
	{
	    if(len < sizeof(struct sockaddr_un))
	    {
	        return -ENOBUFS;
	    }
	}
	
	return 0;
}


INLINE int copy_sockname_to_user(struct socket_t *so, void *dest, void *src)
{
	if(so->domain == AF_INET)
	{
	    return copy_to_user(dest, src, sizeof(struct sockaddr_in));
	}
	else if(so->domain == AF_INET6)
	{
	    return copy_to_user(dest, src, sizeof(struct sockaddr_in6));
	}
	
	return 0;
}


INLINE int copy_sockname_from_user(struct socket_t *so, void *dest, void *src)
{
	if(so->domain == AF_INET)
	{
	    return copy_from_user(dest, src, sizeof(struct sockaddr_in));
	}
	else if(so->domain == AF_INET6)
	{
	    return copy_from_user(dest, src, sizeof(struct sockaddr_in6));
	}
	
	return 0;
}


void packet_copy_remoteaddr(struct socket_t *so, 
                            struct packet_t *p, struct msghdr *msg)
{
    if(msg->msg_name == NULL)
    {
        return;
    }
    
   	if(so->domain == AF_INET)
   	{
   	    struct sockaddr_in sin = { 0, };

   	    sin.sin_family = AF_INET;
   	    sin.sin_addr.s_addr = p->remote_addr.ipv4.s_addr;
   	    sin.sin_port = p->remote_port;
   	    A_memcpy(msg->msg_name, &sin, sizeof(sin));
   	    msg->msg_namelen = sizeof(sin);
   	}
   	else if(so->domain == AF_INET6)
   	{
   	    struct sockaddr_in6 sin6 = { 0, };

   	    IPV6_COPY(sin6.sin6_addr.s6_addr, p->remote_addr.ipv6.s6_addr);
   	    sin6.sin6_family = AF_INET6;
   	    sin6.sin6_port = p->remote_port;
   	    A_memcpy(msg->msg_name, &sin6, sizeof(sin6));
   	    msg->msg_namelen = sizeof(sin6);
   	}
}


static int syscall_recvfrom_internal(int s, void *buf, size_t len, int flags,
                                     struct sockaddr *from, 
                                     socklen_t *fromlenaddr)
{
    KDEBUG("%s:\n", __func__);

	struct msghdr msg;
	struct iovec aiov;
	int res;
	struct socket_t *so;
	char namebuf[64];
	socklen_t user_namelen;

    if(!buf)
    {
        return -EINVAL;
    }

	if((res = getsock(s, &so)))
	{
		return res;
	}
	
	if(socket_check(so) != 0)
	{
        return -EINVAL;
	}
	
	if(!(so->state & SOCKET_STATE_BOUND))
	{
	    printk("syscall_recvfrom_internal: sock not connected\n");
        return -EADDRNOTAVAIL;
	}

	if(fromlenaddr)
	{
		if((res = copy_from_user(&user_namelen, 
		                         fromlenaddr, sizeof(socklen_t))))
		{
			return res;
		}

		if(check_namelen(so, user_namelen) != 0)
		{
		    return -ENOBUFS;
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
	
	res = so->proto->sockops->recvmsg(so, &msg, flags);

    KDEBUG("%s: len %u\n", __func__, len);

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
int syscall_recvfrom(struct syscall_args *__args)
{
    KDEBUG("%s:\n", __func__);

    // syscall args
    struct syscall_args args;
    int s;
    void *buf;
    size_t len;
    int flags;
    struct sockaddr *from;
    socklen_t *fromlenaddr;
	int res;
    
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
int syscall_recvmsg(int s, struct msghdr *_msg, int flags)
{
    KDEBUG("%s:\n", __func__);

	struct msghdr msg;
	register int res;
	struct socket_t *so;
	char namebuf[64];
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

	if(socket_check(so) != 0)
	{
        return -EINVAL;
	}
	
	if(!(so->state & SOCKET_STATE_BOUND))
	{
	    printk("syscall_recvmsg: sock not connected\n");
        return -EADDRNOTAVAIL;
	}

	if((res = copy_from_user(&msg, _msg, sizeof(msg))))
	{
		return res;
	}

    if(!(msg.msg_iov = dup_iovec(_msg->msg_iov, _msg->msg_iovlen)))
    {
        return -ENOMEM;
    }
    
	if(msg.msg_namelen)
	{
		if(check_namelen(so, msg.msg_namelen) != 0)
		{
		    return -ENOBUFS;
		}
		
		user_namelen = msg.msg_namelen;
	}
	
	if(msg.msg_name)
	{
	    from = msg.msg_name;
	}

	msg.msg_namelen = sizeof(namebuf);
	msg.msg_name = namebuf;


    KDEBUG("syscall_recvmsg: -- \n");
    KDEBUG("1 size %u\n", get_iovec_size(_msg->msg_iov, _msg->msg_iovlen));

	if((res = so->proto->sockops->recvmsg(so, &msg, flags)) >= 0)
	{
    	if(from && user_namelen)
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
	}

    KDEBUG("syscall_recvmsg: -- \n");
    KDEBUG("2 size %u\n", get_iovec_size(_msg->msg_iov, _msg->msg_iovlen));
	
	kfree(msg.msg_iov);

	if(res == -EFAULT)
	{
        SYSCALL_EFAULT(_msg);
	}
	
	return res;
}


static int socket_shutdown(struct socket_t *so, int how)
{
    // socket is already closed
    if(so->state & SOCKET_STATE_CLOSED)
    {
        return -EINVAL;
    }
    
    // unbound socket, remove immediately
    if(!(so->state & SOCKET_STATE_BOUND))
    {
        socket_clean_queues(so);
        kfree(so);
        return 1;
    }
    
    if(so->domain == AF_UNIX ||
       so->proto->protocol == IPPROTO_UDP ||
       RAW_SOCKET(so))
    {
        if(how & SHUT_RDWR)
        {
        	socket_update_state(so, SOCKET_STATE_CLOSED, 
        	                        SOCKET_STATE_CLOSING | 
        	                        SOCKET_STATE_BOUND | 
        	                        SOCKET_STATE_CONNECTED, 0);
            return 1;
        }
        else if(how & SHUT_RD)
        {
        	socket_update_state(so, 0, SOCKET_STATE_BOUND, 0);
            return 1;
        }

        return 0;
    }
    else if(so->proto->protocol == IPPROTO_TCP)
    {
        if(how & SHUT_RDWR)
        {
        	socket_update_state(so, SOCKET_STATE_SHUT_LOCAL | 
        	                        SOCKET_STATE_SHUT_REMOTE, 0, 0);
            tcp_notify_closing(so);
        }
        else if(how & SHUT_WR)
        {
        	socket_update_state(so, SOCKET_STATE_SHUT_LOCAL, 0, 0);
            tcp_notify_closing(so);
        }
        else if(how & SHUT_RD)
        {
        	socket_update_state(so, SOCKET_STATE_SHUT_REMOTE, 0, 0);
        }

        return 0;
    }

    return -EINVAL;
}


/*
 * Handler for syscall shutdown().
 */
int syscall_shutdown(int s, int how)
{
    KDEBUG("%s:\n", __func__);

    struct socket_t *so;
	int res;

#if 0

	if((res = getsock(s, &so)))
	{
		return res;
	}

#endif


    struct file_t *fp;
    struct task_t *ct = cur_task;
    
    if(s < 0 || s >= (int)NR_OPEN || !ct->ofiles || 
       (fp = ct->ofiles->ofile[s]) == NULL)
    {
        return -EBADF;
    }
    
    if(!fp->node || !IS_SOCKET(fp->node))
    {
        return -ENOTSOCK;
    }
    
    so = (struct socket_t *)fp->node->data;

    if((res = socket_shutdown(so, how)) == 1)
    {
        fp->node->data = 0;
        fp->node->links = 0;
        return 0;
    }
    
    return res;
}


/*
 * Handler for syscall setsockopt().
 */
int syscall_setsockopt(int s, int level, int name, void *val, int valsize)
{
    KDEBUG("%s:\n", __func__);

    struct socket_t *so;
	//void *opt = NULL;
	int res;

	if((res = getsock(s, &so)))
	{
		return res;
	}

	if(val)
	{
	    if(valsize <= 0)
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
int syscall_getsockopt(int s, int level, int name, void *aval, int *avalsize)
{
    KDEBUG("%s:\n", __func__);

    struct socket_t *so;
	//void *val = NULL;
	int valsize = 0, res;

    KDEBUG("syscall_getsockopt: level %d, name %d\n", level, name);

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


void socket_close(struct socket_t *so)
{
    if(!so || !so->proto)
    {
        return;
    }
    
    /*
    if(so->proto->protocol == IPPROTO_TCP)
    {
        if(tcp_check_listen_close(so) == 0)
        {
            return;
        }
    }
    */
    
    socket_shutdown(so, SHUT_RDWR);
}


int socket_clone(struct socket_t *so, struct socket_t **res)
{
    struct socket_t *clone;
    int i;
    
    *res = NULL;

    if((i = sock_create(so->domain, so->type, so->proto->protocol, &clone)) != 0)
	{
	    return i;
	}
	
	clone->local_port = so->local_port;
	clone->remote_port = so->remote_port;
	clone->state = so->state;
	
	if(so->domain == AF_INET)
	{
	    clone->local_addr.ipv4.s_addr = so->local_addr.ipv4.s_addr;
	    clone->remote_addr.ipv4.s_addr = so->remote_addr.ipv4.s_addr;
	}
	else if(so->domain == AF_INET6)
	{
	    IPV6_COPY(clone->local_addr.ipv6.s6_addr,
	              so->local_addr.ipv6.s6_addr);
	    IPV6_COPY(clone->remote_addr.ipv6.s6_addr,
	              so->remote_addr.ipv6.s6_addr);
	}
	
	*res = clone;

	return 0;
}


int socket_error(struct packet_t *p, uint8_t proto)
{
    struct sockport_t *sp = NULL;
    struct socket_t *so;
    int res = -EPROTONOSUPPORT;
    uint16_t destp = 0;

    kernel_mutex_lock(&sockport_lock);
    
    switch(proto)
    {
        case IPPROTO_UDP:
            {
                struct udp_hdr_t *h = (struct udp_hdr_t *)p->transport_hdr;
                sp = get_sockport(proto, h->srcp);
                destp = h->destp;
                break;
            }
        
        case IPPROTO_TCP:
            {
                struct tcp_hdr_t *h = (struct tcp_hdr_t *)p->transport_hdr;
                sp = get_sockport(proto, h->srcp);
                destp = h->destp;
                break;
            }
        
        default:
            // unknown protocol
            break;
    }
    
    if(sp)
    {
        res = 0;
        
        for(so = sp->sockets; so != NULL; so = so->next)
        {
            if(destp == so->remote_port)
            {
                if(so->wakeup)
                {
                    so->state |= SOCKET_STATE_SHUT_REMOTE;
                    so->wakeup(so, SOCKET_EV_ERR);
                }
                
                break;
            }
        }
    }

    kernel_mutex_unlock(&sockport_lock);
    
    packet_free(p);
    return res;
}


void socket_wakeup(struct socket_t *so, uint16_t ev)
{
    KDEBUG("sock: received a wakeup event (0x%x)\n", ev);

    if(ev & SOCKET_EV_RD)
    {
        unblock_tasks(&so->recvsel);
    }

    if(ev & SOCKET_EV_WR)
    {
        unblock_tasks(&so->sendsel);
    }
    
    // for tcp sockets
    if(ev & SOCKET_EV_CONN)
    {
        unblock_tasks(so);
    }

    // for tcp sockets
    if(ev & SOCKET_EV_CLOSE)
    {
        unblock_tasks(&so->recvsel);
    }
    
    // wakeup everyone in case of error - TODO: do we actually need this?
    if(ev & SOCKET_EV_ERR)
    {
        unblock_tasks(so);
        unblock_tasks(&so->sendsel);
        unblock_tasks(&so->recvsel);
    }
}


struct netif_t *sock_get_ifp(struct socket_t *so)
{
    if(so->domain == AF_INET6)
    {
        so->ifp = ipv6_source_ifp_find(&so->remote_addr.ipv6);
    }
    else if(so->domain == AF_INET)
    {
        so->ifp = ipv4_source_ifp_find(&so->remote_addr.ipv4);
    }
    
    return so->ifp;
}


uint32_t socket_get_mss(struct socket_t *so)
{
    uint32_t mss;

    if(!so)
    {
        return 1280;
    }
    
    if(!so->ifp)
    {
        sock_get_ifp(so);
    }
    
    mss = so->ifp ? so->ifp->mtu : 1280;
    mss -= (so->domain == AF_INET6) ? IPv6_HLEN : IPv4_HLEN;
    
    return mss;
}

