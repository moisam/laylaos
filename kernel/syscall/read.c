/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: read.c
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
 *  \file read.c
 *
 *  Functions to read from files.
 */

//#define __DEBUG

#define __USE_XOPEN_EXTENDED
#include <errno.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/dev.h>
#include <kernel/syscall.h>
#include <kernel/fio.h>
#include <fs/sockfs.h>
#include <fs/pipefs.h>


#define UPDATE_ATIME(f, node)           \
	if(!(f->flags & O_NOATIME))         \
	    update_atime(node);


static inline int read_internal(struct file_t *f, 
                                unsigned char *buf, size_t count, 
                                off_t *offset, ssize_t *copied)
{
    ssize_t res;
    off_t pos = *offset;
    
    // don't bother if count == 0
    if(!count)
    {
        res = 0;
        goto fin;
    }

    /* this call shouldn't modify f->pos */
    res = f->node->read(f, &pos, buf, count, 0);

fin:
    
    if(res >= 0)
    {
        COPY_VAL_TO_USER(copied, &res);

    	*offset = pos;

        /*
         * TODO: should we account for the actual bytes read, instead of what
         *       the task asked for?
         */
        cur_task->read_count += count;

        res = 0;
    }

    return res;
}


/*
 * Handler for syscall read().
 */
int syscall_read(int fd, unsigned char *buf, size_t count, ssize_t *copied)
{
    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    int sync, res;

    //printk("syscall_read: buf 0x%lx, count %u\n", buf, count);

    if(fdnode(fd, cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

    if(!buf || !copied)
    {
        return -EINVAL;
    }

    res = read_internal(f, buf, count, &(f->pos), copied);

    sync = !!(S_ISBLK(node->mode) | S_ISDIR(node->mode) | S_ISREG(node->mode));
    cur_task->read_calls++;

    if(sync)
    {
        UPDATE_ATIME(f, node);
    }

    return res;
}


/*
 * Handler for syscall pread().
 */
int syscall_pread(int fd, void *buf, size_t count, off_t _offset, 
                  ssize_t *copied)
{
    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    int sync, res;
    off_t offset = _offset;

    if(fdnode(fd, cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

    if(!buf || !copied)
    {
        return -EINVAL;
    }

    res = read_internal(f, (unsigned char *)buf, count, &offset, copied);

    sync = !!(S_ISBLK(node->mode) | S_ISDIR(node->mode) | S_ISREG(node->mode));
    cur_task->read_calls++;

    if(sync)
    {
        UPDATE_ATIME(f, node);
    }

    return res;
}


/*
 * Handler for syscall readv().
 */
int syscall_readv(int fd, struct iovec *iov, int count, ssize_t *copied)
{
    int i;
    int res;
    ssize_t total = 0;
    void *iov_base;
    size_t iov_len;

    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    int sync;

    if(fdnode(fd, cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

    if(!copied)
    {
        return -EINVAL;
    }

    for(i = 0; i < count; i++)
    {
        COPY_VAL_FROM_USER(&iov_base, &iov[i].iov_base);
        COPY_VAL_FROM_USER(&iov_len, &iov[i].iov_len);

        //printk("syscall_readv: iov_base 0x%lx, iov_len %u\n", iov_base, iov_len);

        if(!iov_base)
        {
            break;
        }

        if((res = read_internal(f, iov_base, iov_len, &(f->pos), copied)) < 0)
        {
            return res;
        }
        
        total += *copied;
        
        if(*copied < (ssize_t)iov_len)
        {
            break;
        }
    }

    COPY_VAL_TO_USER(copied, &total);

    sync = !!(S_ISBLK(node->mode) | S_ISDIR(node->mode) | S_ISREG(node->mode));
    cur_task->read_calls++;

    if(sync)
    {
        UPDATE_ATIME(f, node);
    }

    return 0;
}


/*
 * Handler for syscall preadv().
 */
int syscall_preadv(int fd, struct iovec *iov, int count,
                   off_t _offset, ssize_t *copied)
{
    int i;
    int res;
    ssize_t total = 0;
    void *iov_base;
    size_t iov_len;
    off_t offset = _offset;

    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    int sync;

    if(fdnode(fd, cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

    if(!copied)
    {
        return -EINVAL;
    }

    for(i = 0; i < count; i++)
    {
        COPY_VAL_FROM_USER(&iov_base, &iov[i].iov_base);
        COPY_VAL_FROM_USER(&iov_len, &iov[i].iov_len);

        if(!iov_base)
        {
            break;
        }

        if((res = read_internal(f, iov_base, iov_len, &offset, copied)) < 0)
        {
            return res;
        }
        
        total += *copied;
        
        if(*copied < (ssize_t)iov_len)
        {
            break;
        }
    }

    COPY_VAL_TO_USER(copied, &total);

    sync = !!(S_ISBLK(node->mode) | S_ISDIR(node->mode) | S_ISREG(node->mode));
    cur_task->read_calls++;

    if(sync)
    {
        UPDATE_ATIME(f, node);
    }

    return 0;
}

