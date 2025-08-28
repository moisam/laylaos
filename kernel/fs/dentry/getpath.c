/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: getpath.c
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
 *  \file getpath.c
 *
 *  This file includes the getpath() function, which is used to get the
 *  full pathname for a given file or directory node.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/pcache.h>
#include <mm/kheap.h>

#define bcopy(a,b,c)    memmove (b,a,c)


long get_dot_dot(struct fs_node_t **dir, struct fs_node_t **dotdot)
{
    KDEBUG("get_dot_dot:\n");
    struct dirent *entry;
    struct cached_page_t *dbuf;
    //struct IO_buffer_s *dbuf;
    struct fs_node_t *tmp;
    size_t dbuf_off;
    long i;
    ino_t ino;
    
    *dotdot = NULL;
    
    if((i = vfs_finddir(*dir, "..", &entry, &dbuf, &dbuf_off)) < 0)
    {
        return i;
    }
    KDEBUG("get_dot_dot: 1\n");
    
    ino = entry->d_ino;
    kfree(entry);
    release_cached_page(dbuf);
    //brelse(dbuf);
    
    // if the inode number for this dir and '..' is the same, the dir is
    // the root dir of this device
    if(ino == (*dir)->inode)
    {
        KDEBUG("get_dot_dot: 1a\n");
        // if the device is mounted, get the mountpoint's inode
        struct mount_info_t *d = get_mount_info2(*dir);
        
        if(d && d->root == *dir && d->mpoint)
        {
            tmp = d->mpoint;
            INC_NODE_REFS(tmp);

            if((i = get_dot_dot(&tmp, dotdot)) < 0)
            {
                release_node(tmp);
                return i;
            }
            
            release_node(*dir);
            *dir = tmp;
            
            return 0;
        }
        
        return -ENOENT;
    }
    else
    {
        KDEBUG("get_dot_dot: 1b\n");
        *dotdot = get_node((*dir)->dev, ino, 0);
        return *dotdot ? 0 : -ENOENT;
    }
}


long getpath(struct fs_node_t *dir, char **__path)
{
    KDEBUG("getpath:\n");
    struct dirent *dp;
    register dev_t dev;
    register ino_t ino;
    register int first;
    register char *bpt;
    dev_t root_dev;
    ino_t root_ino;
    size_t ptsize;
    char *pt, *ept;
    struct fs_node_t *node, *parent;
    struct cached_page_t *dbuf;
    //struct IO_buffer_s *dbuf;
    size_t dbuf_off;
    long res;

    if(!__path)
    {
        return -EINVAL;
    }
    
    *__path = NULL;

    if(!(pt = (char *)kmalloc(ptsize = 1024 - 4)))
    {
        return -ENOMEM;
    }

    ept = pt + ptsize;
    bpt = ept - 1;
    *bpt = '\0';

    /* Save root values, so know when to stop. */

    if(!system_root_node ||
       !(parent = get_node(system_root_node->dev, 
                            system_root_node->inode, GETNODE_FOLLOW_MPOINTS)))
    {
        printk("dentry: failed to get root node\n");
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        res = -ENOENT;
        goto err;
    }
    
    root_dev = parent->dev;
    root_ino = parent->inode;
    release_node(parent);

    // if the requested node refers to sysroot, return '/' instead of 
    // going into the loop as this will end in a mounted path, something
    // like '/initrd/rootfs', which is not what the caller expects
    if((dir->dev == root_dev && dir->inode == root_ino) ||
       (dir->dev == system_root_node->dev && dir->inode == system_root_node->inode))
    {
        char *tmp;

        if((tmp = kmalloc(4)))
        {
            kfree(pt);
            pt = tmp;
            __asm__ __volatile__("":::"memory");
        }

        pt[0] = '/';
        pt[1] = '\0';
        *__path = pt;
        return 0;
    }

    node = dir;
    INC_NODE_REFS(node);

    for(first = 1;; first = 0)
    {
        KDEBUG("getpath: bpt = '%s'\n", bpt);

        /* Save current node values. */
        ino = node->inode;
        dev = node->dev;

        /* Check for reaching root. */
        if(root_dev == dev && root_ino == ino)
        {
            KDEBUG("getpath: finishing\n");
            
            *--bpt = '/';
            /*
             * It's unclear that it's a requirement to copy the
             * path to the beginning of the buffer, but it's always
             * been that way and stuff would probably break.
             */
            (void) bcopy(bpt, pt, ept - bpt);

            // alloc a smaller buffer if the path is small to avoid wasting
            // heap space with unused bytes
            if(ept - bpt <= 256)
            {
                char *tmp;

                if((tmp = kmalloc(ept - bpt)))
                {
                    A_memcpy(tmp, pt, ept - bpt);
                    kfree(pt);
                    pt = tmp;
                    __asm__ __volatile__("":::"memory");
                }
            }

            KDEBUG("getpath: pt = '%s'\n", pt);

            release_node(node);
            *__path = pt;
            return 0;
        }

        if((res = get_dot_dot(&node, &parent)) < 0)
        {
            goto err2;
        }

        KDEBUG("getpath: node @ 0x%x, parent @ 0x%x\n", node, parent);

        if((res = vfs_finddir_by_inode(parent, node,
                                       &dp, &dbuf, &dbuf_off)) < 0)
        {
            goto err2;
        }
        
        release_cached_page(dbuf);
        //brelse(dbuf);

        KDEBUG("getpath: 2 node @ 0x%x, parent @ 0x%x\n", node, parent);

        /*
         * Check for length of the current name, preceding slash,
         * leading slash.
         */

        if((size_t)(bpt - pt) <= strlen(dp->d_name) + (first ? 1 : 2))
        {
            char *tmp;
            size_t len, off;

            off = bpt - pt;
            len = ept - bpt;
      
            if(!(tmp = (char *)krealloc(pt, ptsize *= 2)))
            {
                kfree(dp);
                goto err2;
            }

            pt = tmp;
            bpt = pt + off;
            ept = pt + ptsize;
            (void) bcopy(bpt, ept - len, len);
            bpt = ept - len;
        }

        if(!first)
        {
            *--bpt = '/';
        }
    
        bpt -= strlen(dp->d_name);
        bcopy(dp->d_name, bpt, strlen(dp->d_name));
        
        kfree(dp);

        release_node(node);
        node = parent;
    }

err2:
    release_node(node);
    
err:
    kfree(pt);
    return res;
}

