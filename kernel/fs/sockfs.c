/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: sockfs.c
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
 *  \file sockfs.c
 *
 *  This file implements sockfs filesystem functions, which provide access to
 *  the sockfs virtual filesystem.
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/filio.h>
#include <net/route.h>
#include <kernel/net/protocol.h>
#include <kernel/net/packet.h>
#include <kernel/net/tcp.h>
#include <kernel/net/netif.h>
#include <kernel/net/socket.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/dev.h>
#include <kernel/ksignal.h>
#include <kernel/select.h>
#include <kernel/user.h>
#include <kernel/fcntl.h>
#include <mm/kheap.h>
#include <fs/sockfs.h>
#include <fs/tmpfs.h>
#include <fs/dummy.h>


/*
 * Create a new socket node.
 */
struct fs_node_t *sockfs_get_node(void)
{
    struct fs_node_t *node;
    
    // get a free node
    if((node = get_empty_node()) == NULL)
    {
        return NULL;
    }
    
    node->mode = S_IFSOCK | 0666;
    node->flags |= FS_NODE_SOCKET;

    node->select = sockfs_select;
    node->poll = sockfs_poll;

    node->read = sockfs_read;
    node->write = sockfs_write;

    return node;
}


/*
 * Read from a socket.
 */
ssize_t sockfs_read(struct file_t *f, off_t *pos,
                    unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(kernel);

	struct msghdr msg;
	struct iovec aiov;
    struct socket_t *so;

    so = (struct socket_t *)f->node->data;

	msg.msg_namelen = 0;
	msg.msg_name = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = (char *)buf;
	aiov.iov_len = count;
	msg.msg_control = 0;
	msg.msg_flags = 0;
	
	return so->proto->sockops->recvmsg(so, &msg, 0);
}


/*
 * Write to a socket.
 */
ssize_t sockfs_write(struct file_t *f, off_t *pos,
                     unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(kernel);

	struct msghdr msg;
	struct iovec aiov;
	struct socket_t *so;
	char dest_namebuf[128];
	char src_namebuf[128];
	int res;

    so = (struct socket_t *)f->node->data;

    if((res = sendto_pre_checks(so, NULL, 0, src_namebuf, dest_namebuf)) != 0)
    {
		return res;
    }

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
	aiov.iov_base = (char *)buf;
	aiov.iov_len = count;

    return do_sendto(so, &msg, src_namebuf, dest_namebuf, 0, 0);
}


/*
 * General block device control function.
 */
int sockfs_ioctl(struct file_t *f, int cmd, char *data, int kernel)
{
	struct socket_t *so;
	int tmp;

	switch(cmd)
	{
	    case SIOCGIFNAME:
	    case SIOCGIFINDEX:
	    case SIOCGIFFLAGS:
	    case SIOCSIFFLAGS:
	    case SIOCGIFPFLAGS:
	    case SIOCSIFPFLAGS:
	    case SIOCGIFADDR:
	    case SIOCSIFADDR:
	    case SIOCDIFADDR:
	    case SIOCGIFDSTADDR:
	    case SIOCSIFDSTADDR:
	    case SIOCGIFBRDADDR:
	    case SIOCSIFBRDADDR:
	    case SIOCGIFNETMASK:
	    case SIOCSIFNETMASK:
	    case SIOCGIFMETRIC:
	    case SIOCSIFMETRIC:
	    case SIOCGIFMTU:
	    case SIOCSIFMTU:
	    case SIOCGIFHWADDR:
	    case SIOCSIFHWADDR:
	    case SIOCSIFHWBROADCAST:
	    case SIOCGIFMAP:
	    case SIOCSIFMAP:
	    case SIOCADDMULTI:
	    case SIOCDELMULTI:
	    case SIOCGIFTXQLEN:
	    case SIOCSIFTXQLEN:
	    case SIOCSIFNAME:
	    case SIOCGIFCONF:
	        return netif_ioctl(f, cmd, data);
	    
	    case FIONBIO:
            if(!(so = (struct socket_t *)f->node->data))
            {
                return -EINVAL;
            }
            
            if(kernel)
            {
                tmp = *(int *)data;
            }
            else
            {
                COPY_VAL_FROM_USER(&tmp, data);
            }
            
            if(tmp)
            {
                so->flags |= SOCKET_FLAG_NONBLOCK;
            }
            else
            {
                so->flags &= ~SOCKET_FLAG_NONBLOCK;
            }

            return 0;
	}
    
    return -ENOSYS;
}


/*
 * Perform a select operation on a socket.
 */
int sockfs_select(struct file_t *f, int which)
{
    KDEBUG("sockfs_select:\n");

	struct socket_t *so;
	int res;

    if(/* !f || !f->node || */ !f->node->data)
    {
        return -EINVAL;
    }

    so = (struct socket_t *)f->node->data;
    
    if(!so->proto)
    {
        return 0;
    }

	switch(which)
	{
    	case FREAD:
            kernel_mutex_lock(&so->inq.lock);
            res = (so->inq.head != NULL);
            kernel_mutex_unlock(&so->inq.lock);

            if(res)
            {
                return 1;
            }
    		
    		selrecord(&so->recvsel);
    		break;

    	case FWRITE:
        	if((socket_check(so) == 0) &&
        	   (so->state & SOCKET_STATE_CONNECTED))
        	{
                return 1;
        	}

    		selrecord(&so->sendsel);
    		break;

    	case 0:
        	KDEBUG("sockfs_select: so->state 0x%x\n", so->state);

            if(!(so->state & SOCKET_STATE_CONNECTED))
            {
                return 1;
            }

            if(so->state & (SOCKET_STATE_SHUT_LOCAL | 
                            SOCKET_STATE_SHUT_REMOTE |
                            SOCKET_STATE_CLOSING | 
                            SOCKET_STATE_CLOSED))
            {
                return 1;
            }

    		//selrecord(&so->recvsel);
    		break;
	}

	return 0;
}


/*
 * Perform a poll operation on a socket.
 */
int sockfs_poll(struct file_t *f, struct pollfd *pfd)
{
    int res = 0;

	struct socket_t *so;

    if(!f || !f->node || !f->node->data)
    {
        pfd->revents |= POLLNVAL;
        return 0;
    }

    so = (struct socket_t *)f->node->data;
    
    if(!so->proto)
    {
        pfd->revents |= POLLNVAL;
        return 0;
    }

    if(pfd->events & POLLIN)
    {
        int res2;

        kernel_mutex_lock(&so->inq.lock);
        res2 = (so->inq.head != NULL);
        kernel_mutex_unlock(&so->inq.lock);

        if(res2)
        {
            KDEBUG("sockfs_poll: POLLIN yes\n");
            pfd->revents |= POLLIN;
            res = 1;
        }
        else
        {
            KDEBUG("sockfs_poll: POLLIN no\n");
            selrecord(&so->recvsel);
        }
    }

    if(pfd->events & POLLOUT)
    {
        if((socket_check(so) == 0) &&
           (so->state & SOCKET_STATE_CONNECTED))
        {
            KDEBUG("sockfs_poll: POLLOUT yes\n");
            pfd->revents |= POLLOUT;
            res = 1;
        }
        else
        {
            KDEBUG("sockfs_poll: POLLOUT no\n");
    		selrecord(&so->sendsel);
        }
    }
    
    if(!(so->state & SOCKET_STATE_CONNECTED))
    {
        KDEBUG("sockfs_poll: POLLHUP\n");
        pfd->revents |= POLLHUP;
        res = 1;
    }

    if(so->state & (SOCKET_STATE_SHUT_LOCAL | SOCKET_STATE_SHUT_REMOTE |
                    SOCKET_STATE_CLOSING | SOCKET_STATE_CLOSED))
    {
        KDEBUG("sockfs_poll: POLLERR\n");
        pfd->revents |= POLLERR;
        res = 1;
    }
    
    return res;
}

