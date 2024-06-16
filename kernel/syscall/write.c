/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: write.c
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
 *  \file write.c
 *
 *  Functions to write to files.
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
#include <kernel/clock.h>
#include <kernel/fio.h>
#include <fs/sockfs.h>
#include <fs/pipefs.h>


static inline void write_sync(int fd, struct file_t *f, struct fs_node_t *node)
{
    struct mount_info_t *dinfo;

	if((f->flags & O_SYNC))
	{
	    syscall_fsync(fd);
	    return;
	}

	if((f->flags & O_DSYNC))
	{
	    syscall_fdatasync(fd);
	    return;
	}

    if((dinfo = node_mount_info(node)) &&
    //if((dinfo = get_mount_info(node->dev)) &&
       (dinfo->mountflags & MS_SYNCHRONOUS))
	{
	    syscall_fsync(fd);
	}
}


static inline void update_file_node(struct file_t *f)
{
	f->node->mtime = now();
    f->node->flags |= FS_NODE_DIRTY;

	if(!(f->flags & O_APPEND))
	{
	    f->node->ctime = f->node->mtime;
	}
}


static inline int write_internal(struct file_t *f, 
                                 unsigned char *buf, size_t count, 
                                 off_t *offset, ssize_t *copied)
{
    KDEBUG("syscall_write: fd %d, buf 0x%x, count %d\n", fd, buf, count);
    ssize_t res;
    off_t pos;

    // don't bother if count == 0
    if(!count)
    {
        res = 0;
        goto fin;
    }

	pos = *offset;

    /* this call shouldn't modify f->pos */
    res = f->node->write(f, &pos, buf, count, 0);

fin:
    
    if(res >= 0)
    {
        COPY_VAL_TO_USER(copied, &res);

    	*offset = pos;

        /*
         * TODO: should we account for the actual bytes written, instead of 
         *       what the task asked for?
         */
        cur_task->write_count += count;

        res = 0;
    }

    return res;
}


/*
 * Handler for syscall write().
 */
int syscall_write(int fd, unsigned char *buf, size_t count, ssize_t *copied)
{
    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    int sync, res;

    if(fdnode(fd, cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

    if(!buf || !copied)
    {
        return -EINVAL;
    }

	// seek to EOF if the file was opened with O_APPEND
	if((f->flags & O_APPEND) && (node->dev != PROCFS_DEVID))
	{
	    f->pos = node->size;
	}

    res = write_internal(f, buf, count, &f->pos, copied);

    update_file_node(f);
    sync = !!(S_ISBLK(node->mode) | S_ISDIR(node->mode) | S_ISREG(node->mode));
    cur_task->write_calls++;

    if(sync)
	{
        write_sync(fd, f, node);
	}

    return res;
}


/*
 * Handler for syscall pwrite().
 */
int syscall_pwrite(int fd, void *buf, size_t count, off_t _offset, 
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

    res = write_internal(f, (unsigned char *)buf, count, &offset, copied);

    update_file_node(f);
    sync = !!(S_ISBLK(node->mode) | S_ISDIR(node->mode) | S_ISREG(node->mode));
    cur_task->write_calls++;

    if(sync)
	{
        write_sync(fd, f, node);
	}

    return res;
}


/*
 * Handler for syscall writev().
 */
int syscall_writev(int fd, struct iovec *iov, int count, ssize_t *copied)
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

	// seek to EOF if the file was opened with O_APPEND
	if((f->flags & O_APPEND) && (node->dev != PROCFS_DEVID))
	{
	    f->pos = node->size;
	}

    for(i = 0; i < count; i++)
    {
        COPY_VAL_FROM_USER(&iov_base, &iov[i].iov_base);
        COPY_VAL_FROM_USER(&iov_len, &iov[i].iov_len);
        
        if(!iov_base)
        {
            break;
        }

        if((res = write_internal(f, iov_base, iov_len, &f->pos, copied)) < 0)
        {
            //printk("syscall_writev: res %d\n", res);
            return res;
        }
        
        total += *copied;
        
        if(*copied < (ssize_t)iov_len)
        {
            break;
        }
    }
    
    COPY_VAL_TO_USER(copied, &total);

    update_file_node(f);
    sync = !!(S_ISBLK(node->mode) | S_ISDIR(node->mode) | S_ISREG(node->mode));
    cur_task->write_calls++;

    if(sync)
	{
        write_sync(fd, f, node);
	}

    return 0;
}


/*
 * Handler for syscall pwritev().
 */
int syscall_pwritev(int fd, struct iovec *iov, int count,
                    off_t _offset, ssize_t *copied)
{
    int i;
    int res;
    ssize_t total = 0;
    void *iov_base;
    size_t iov_len;

    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    int sync;
    off_t offset = _offset;

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

        if((res = write_internal(f, iov_base, iov_len, &offset, copied)) < 0)
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

    update_file_node(f);
    sync = !!(S_ISBLK(node->mode) | S_ISDIR(node->mode) | S_ISREG(node->mode));
    cur_task->write_calls++;

    if(sync)
	{
        write_sync(fd, f, node);
	}

    return 0;
}

