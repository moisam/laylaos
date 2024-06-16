/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: blk_rw.c
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
 *  \file blk_rw.c
 *
 *  General read and write functions for block memory devices.
 */

#include <errno.h>
#include <kernel/vfs.h>
#include <kernel/user.h>
#include <kernel/dev.h>
#include <kernel/pcache.h>


/*
 * Block device write function.
 */
ssize_t block_write(struct file_t *f, off_t *pos,
                    unsigned char *buf, size_t count, int kernel)
{
    struct mount_info_t *d;
    struct cached_page_t *dbuf;
    struct fs_node_header_t tmpnode;
    unsigned int blockno, off, i, done;
    dev_t dev = f->node->blocks[0];
    char *p;
    
    if(!pos || !buf)
    {
        return -EINVAL;
    }

    if((d = get_mount_info(dev)) == NULL)
    {
        return -EINVAL;
    }

    blockno = *pos / d->block_size;
    off = *pos % d->block_size;
    done = 0;
    tmpnode.dev = dev;
    tmpnode.inode = PCACHE_NOINODE;

    while(count != 0)
    {
        if(!(dbuf = get_cached_page((struct fs_node_t *)&tmpnode, blockno, 0)))
        {
            return done ? (int)done : -EIO;
        }
        
        i = (count < d->block_size) ? count : d->block_size;
        p = (char *)(dbuf->virt + off);
        off = 0;
        blockno++;
        *pos += i;
        done += i;
        count -= i;

        if(kernel)
        {
            A_memcpy(p, buf, i);
        }
        else
        {
            copy_from_user(p, buf, i);
        }

        buf += i;
        release_cached_page(dbuf);
    }

    return done;
}


/*
 * Block device read function.
 */
ssize_t block_read(struct file_t *f, off_t *pos,
                   unsigned char *buf, size_t count, int kernel)
{
    struct mount_info_t *d;
    struct cached_page_t *dbuf;
    struct fs_node_header_t tmpnode;
    unsigned int blockno, off, i, read;
    dev_t dev = f->node->blocks[0];
    char *p;

    if((d = get_mount_info(dev)) == NULL)
    {
        return -EINVAL;
    }

    blockno = *pos / d->block_size;
    off = *pos % d->block_size;
    read = 0;
    tmpnode.dev = dev;
    tmpnode.inode = PCACHE_NOINODE;

    while(count != 0)
    {
        if(!(dbuf = get_cached_page((struct fs_node_t *)&tmpnode, blockno, 0)))
        {
            return read ? (int)read : -EIO;
        }

        i = (count < d->block_size) ? count : d->block_size;
        p = (char *)(dbuf->virt + off);
        off = 0;
        blockno++;
        *pos += i;
        read += i;
        count -= i;

        if(kernel)
        {
            A_memcpy(buf, p, i);
        }
        else
        {
            copy_to_user(buf, p, i);
        }

        buf += i;
        release_cached_page(dbuf);
    }

    return read;
}

