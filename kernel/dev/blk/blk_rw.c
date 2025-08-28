/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
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
#include <mm/kheap.h>


/*
 * Block device write function.
 */
ssize_t block_write(struct file_t *f, off_t *pos,
                    unsigned char *buf, size_t count, int kernel)
{
    //struct mount_info_t *d;
    //struct cached_page_t *dbuf;
    //struct fs_node_header_t tmpnode;
    struct disk_req_t req;
    dev_t dev = f->node->blocks[0];
    size_t blockno, off, i, done;
    long res;
    int blocksz = 0;
    int maj = MAJOR(dev);
    char *p, *tmpbuf;

    if(!pos || !buf)
    {
        return -EINVAL;
    }

    // make sure we have a strategy function
    if(!bdev_tab[maj].strategy)
    {
        return -EINVAL;
    }

    // get the device's block size (bytes per sector)
    if(!bdev_tab[maj].ioctl ||
       bdev_tab[maj].ioctl(dev, BLKSSZGET, (char *)&blocksz, 1) < 0)
    {
        return -EINVAL;
    }

    // get a temporary buffer
    if(!(tmpbuf = kmalloc(blocksz)))
    {
        return -EAGAIN;
    }

    blockno = *pos / blocksz;
    off = *pos % blocksz;
    done = 0;

    req.dev = dev;
    req.data = (virtual_addr)tmpbuf;
    req.datasz = blocksz;
    req.fs_blocksz = blocksz;

    while(count != 0)
    {
        // read the block from device, copy the new data, then write the
        // block back to the device
        req.blockno = blockno;
        req.write = 0;

        if((res = bdev_tab[maj].strategy(&req)) < 0)
        {
            kfree(tmpbuf);
            return done ? (ssize_t)done : res;
        }
        
        i = (count < (size_t)blocksz) ? count : (size_t)blocksz;
        p = (char *)(tmpbuf + off);

        if(kernel)
        {
            A_memcpy(p, buf, i);
        }
        else
        {
            copy_from_user(p, buf, i);
        }

        req.write = 1;

        if((res = bdev_tab[maj].strategy(&req)) < 0)
        {
            kfree(tmpbuf);
            return done ? (ssize_t)done : res;
        }

        off = 0;
        blockno++;
        *pos += i;
        done += i;
        count -= i;
        buf += i;
    }

    kfree(tmpbuf);

#if 0
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
        dbuf->flags |= PCACHE_FLAG_DIRTY;
        release_cached_page(dbuf);
    }
#endif

    return done;
}


/*
 * Block device read function.
 */
ssize_t block_read(struct file_t *f, off_t *pos,
                   unsigned char *buf, size_t count, int kernel)
{
    //struct mount_info_t *d;
    //struct cached_page_t *dbuf;
    //struct fs_node_header_t tmpnode;
    struct disk_req_t req;
    size_t blockno, off, i, done;
    dev_t dev = f->node->blocks[0];
    long res;
    int blocksz = 0;
    int maj = MAJOR(dev);
    char *p, *tmpbuf;

    // make sure we have a strategy function
    if(!bdev_tab[maj].strategy)
    {
        return -EINVAL;
    }

    // get the device's block size (bytes per sector)
    if(!bdev_tab[maj].ioctl ||
       bdev_tab[maj].ioctl(dev, BLKSSZGET, (char *)&blocksz, 1) < 0)
    {
        return -EINVAL;
    }

    // get a temporary buffer
    if(!(tmpbuf = kmalloc(blocksz)))
    {
        return -EAGAIN;
    }

    blockno = *pos / blocksz;
    off = *pos % blocksz;
    done = 0;

    req.dev = dev;
    req.data = (virtual_addr)tmpbuf;
    req.datasz = blocksz;
    req.fs_blocksz = blocksz;
    req.write = 0;

    while(count != 0)
    {
        req.blockno = blockno;

        if((res = bdev_tab[maj].strategy(&req)) < 0)
        {
            kfree(tmpbuf);
            return done ? (ssize_t)done : res;
        }
        
        i = (count < (size_t)blocksz) ? count : (size_t)blocksz;
        p = (char *)(tmpbuf + off);

        if(kernel)
        {
            A_memcpy(buf, p, i);
        }
        else
        {
            copy_to_user(buf, p, i);
        }

        off = 0;
        blockno++;
        *pos += i;
        done += i;
        count -= i;
        buf += i;
    }

    kfree(tmpbuf);

#if 0
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
#endif

    return done;
}

