/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: node.c
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
 *  \file node.c
 *
 *  This file contains the master file node table, along with different 
 *  functions to read, write, update, and truncate file nodes.
 */

//#define __DEBUG

#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/mutex.h>
#include <kernel/clock.h>
#include <kernel/pcache.h>
#include <kernel/tty.h>
#include <fs/sockfs.h>
#include <fs/pipefs.h>
#include <fs/dentry.h>

// file node table
struct fs_node_t node_table[NR_INODE];
static struct fs_node_t *last_alloc_node = node_table;


/*
 * Write out all modified inodes to disk. Called by update().
 */
void sync_nodes(dev_t dev)
{
    struct fs_node_t *node;
    struct fs_node_t *llnode = &node_table[NR_INODE];
    
    for(node = node_table; node < llnode; node++)
    {
        kernel_mutex_lock(&node->lock);
        kernel_mutex_unlock(&node->lock);

        //KDEBUG("sync_nodes: dev 0x%x, n 0x%x\n", node->dev, node->inode);
        
        if(dev != NODEV && node->dev != dev)
        {
            continue;
        }

        if(node->dev && (node->flags & FS_NODE_DIRTY) && !IS_PIPE(node))
        {
            KDEBUG("sync_nodes: dev 0x%x, n 0x%x\n", node->dev, node->inode);
			write_node(node);
			KDEBUG("sync_nodes: written one\n");
	    }
	}

    KDEBUG("sync_nodes: done\n");
}


/*
 * Release the file node. If it is a pipe, wake up sleepers (if any) and free
 * the pipe's memory page.
 * For dirty nodes, update the node on disk. If the file is empty, truncate 
 * it and free the node struct on disk.
 */
void release_node(struct fs_node_t *node)
{
    if(!node || node->refs == 0)
    {
        return;
    }
    
    node->refs--;

    if(IS_PIPE(node))
    {
        unblock_tasks(&node->sleeping_task);
        
        if(node->refs != 0)
        {
            return;
        }
        
        pipefs_free_node(node);
        return;
    }

    if(IS_SOCKET(node))
    {
        if(node->refs != 0)
        {
            return;
        }
        
        //sockfs_free_node(node);
        return;
    }
    
    if(node->refs != 0)
    {
        return;
    }
    
    //release_cached_pages(node);
    
    while(1)
    {
        if(node->links == 0)
        {
            remove_cached_node_pages(node);
            truncate_node(node, 0);
            free_node(node);
            break;
        }
        
        if(node->flags & FS_NODE_DIRTY)
        {
        	KDEBUG("%s: dev 0x%x, ino 0x%x\n", __func__, node->dev, node->inode);
            write_node(node);   // might sleep
            // if someone changed the node while we slept, try again
        }
        else
        {
            break;
        }
    }

    node->minfo = NULL;
}


/*
 * Check if a node is incore (that is, used by some task).
 */
int node_is_incore(dev_t dev, ino_t n)
{
    KDEBUG("node_is_incore: dev 0x%x, n 0x%x\n", dev, n);

    struct fs_node_t *node;
    struct fs_node_t *llnode = &node_table[NR_INODE];
    
    for(node = node_table; node < llnode; node++)
    {
        KDEBUG("node_is_incore: d 0x%x, n 0x%x\n", node->dev, node->inode);
        //kernel_mutex_lock(&node->lock);
        //kernel_mutex_unlock(&node->lock);

        if(node->refs && node->inode == n && node->dev == dev)
        {
            return 1;
	    }
	}
	
	return 0;
}


struct fs_node_t *get_empty_node(void)
{
    struct fs_node_t *node, *np;
    struct fs_node_t *lnode = last_alloc_node;
    struct fs_node_t *llnode = &node_table[NR_INODE];
    
    while(1)
    {
        // start at the last alloc'd slot and walk towards the end
        node = NULL;
        np = lnode;
        
        do
        {
            // found an unused slot
            if(np->refs == 0)
            {
                node = np;
                break;
            }
            
            // used slot - move on to the next
            if(++np >= llnode)
            {
                np = node_table;
            }
        // loop until we come back to the last alloc'd slot
        } while(np != lnode);
        
        // not found
        if(!node)
        {
            printk("vfs: no free nodes!\n");
            return NULL;
        }
        
        last_alloc_node = node;
        
        // write the old node data out to disk if it is dirty
        while(node->flags & FS_NODE_DIRTY)
        {
            write_node(node);
        }

        // we found it! wait until it is unlocked
        kernel_mutex_lock(&node->lock);
        
        if(node->refs == 0)
        {
            break;
        }

        // someone got the node while we slept on the lock, so try to find
        // another node
        kernel_mutex_unlock(&node->lock);
    }
    
    // zero out the node struct and return it
    A_memset(node, 0, sizeof(struct fs_node_t));
    node->refs = 1;
    kernel_mutex_unlock(&node->lock);

    return node;
}


struct fs_node_t *get_node(dev_t dev, ino_t n, int follow_mpoints)
{
    if(!dev || !n)
    {
        return NULL;
    }
    
    struct fs_node_t *node = node_table;
    struct fs_node_t *lnode = &node_table[NR_INODE];
    
    while(node < lnode)
    {
        // not the node we want
        if(node->dev != dev || node->inode != n)
        {
            node++;
            continue;
        }
        
        // we found it! wait until it is unlocked
        kernel_mutex_lock(&node->lock);
        
        // make sure no one changed it while we slept
        if(node->dev != dev || node->inode != n)
        {
            // shit! start again from the top
            kernel_mutex_unlock(&node->lock);
            node = node_table;
            continue;
        }
        
        // is it a mount point?
        if((node->flags & FS_NODE_MOUNTPOINT) && follow_mpoints)
        {
            KDEBUG("get_node - found a mount point at dev 0x%x, node 0x%x\n", node->dev, node->inode);
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            lnode = node->ptr;
            kernel_mutex_unlock(&node->lock);
            node = lnode;
            
            if(!node)
            {
                return NULL;
            }
            
            node->refs++;
        }
        else
        {
            node->refs++;
            kernel_mutex_unlock(&node->lock);
        }
        
        KDEBUG("get_node - dev 0x%x, node 0x%x\n", node->dev, node->inode);
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        return node;
    }
    
    // node not found - get an empty node
    if(!(node = get_empty_node()))
    {
        return NULL;
    }
    
    node->dev = dev;
    node->inode = n;
    KDEBUG("get_node - trying to read node: dev 0x%x, node 0x%x\n", node->dev, node->inode);
    
    // read the node from disk
    if(read_node(node) < 0)
    {
    	KDEBUG("%s: 1\n", __func__);
        release_node(node);
        node = NULL;
    }

    KDEBUG("get_node - got node: dev 0x%x, node 0x%x\n", node ? node->dev : 0, node ? node->inode : 0);

    return node;
}


int read_node(struct fs_node_t *node)
{
    if(!node)
    {
        return -EINVAL;
    }
    
    //struct mount_info_t *dinfo = node_mount_info(node);
    struct mount_info_t *dinfo = get_mount_info(node->dev);
    int res;

    KDEBUG("read_node - dev 0x%x, node 0x%x\n", node->dev, node->inode);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    
    if(!dinfo)
    {
        kpanic("Reading inode from unmounted disk!\n");
        return -EINVAL;
    }

    kernel_mutex_lock(&node->lock);
    node->ops = dinfo->fs->ops;
    
    if(dinfo->fs->ops && dinfo->fs->ops->read_inode)
    {
        if((res = dinfo->fs->ops->read_inode(node)) < 0)
        {
            switch_tty(1);
            printk("read_node - dev 0x%x, node 0x%x\n", node->dev, node->inode);

            kpanic("Failed to read inode from disk!\n");
            kernel_mutex_unlock(&node->lock);
            return res;
        }
    }
    
    kernel_mutex_unlock(&node->lock);
    return 0;
}


int write_node(struct fs_node_t *node)
{
    int res;

    if(!node)
    {
        return -EINVAL;
    }

    if(node->dev == 0)
    {
        node->flags &= ~FS_NODE_DIRTY;
        return -EINVAL;
    }

    KDEBUG("write_node: dev 0x%x, inode 0x%x, links 0x%x\n", node->dev, node->inode, node->links);

    kernel_mutex_lock(&node->lock);
    
    //struct mount_info_t *dinfo = node_mount_info(node);
    struct mount_info_t *dinfo = get_mount_info(node->dev);

    if(dinfo && dinfo->fs->ops && dinfo->fs->ops->write_inode)
    {
        if((res = dinfo->fs->ops->write_inode(node)) < 0)
        {
            kpanic("Failed to write inode to disk!\n");
            kernel_mutex_unlock(&node->lock);
            return res;
        }
    }
    
    node->flags &= ~FS_NODE_DIRTY;
    kernel_mutex_unlock(&node->lock);

    KDEBUG("write_node: done\n");

    return 0;
}


int truncate_node(struct fs_node_t *node, size_t sz)
{
    int res = 0;

    if(!node || node->dev == 0)
    {
        return -EINVAL;
    }

	if(!(S_ISREG(node->mode) || S_ISDIR(node->mode)))
	{
        return -EINVAL;
    }
    
    KDEBUG("truncate_node: dev 0x%x, n 0x%x\n", node->dev, node->inode);
    
    kernel_mutex_lock(&node->lock);
    
    //struct mount_info_t *dinfo = node_mount_info(node);
    struct mount_info_t *dinfo = get_mount_info(node->dev);

    if(dinfo && dinfo->fs->ops && dinfo->fs->ops->bmap)
    {
        size_t i;
        size_t block_size = dinfo->block_size;
        size_t newb = (sz + block_size - 1) / block_size;
        size_t oldb = (node->size + block_size - 1) / block_size;

        if(sz > node->size)      // expanding file
        {
            for(i = oldb; i < newb; i++)
            {
                if(dinfo->fs->ops->bmap(node, i, block_size, 
                                                 BMAP_FLAG_CREATE) == 0)
                {
                    /*
                     * TODO: Some error occured in bmap(). 
                     *       Handle it correctly!
                     */
                    sz = i * block_size;
                    res = -EIO;
                    break;
                }
            }
        }
        else if(sz < node->size) // shrinking file
        {
            // As i is unsigned, we need to check if it reaches zero and bail
            // out, otherwise it will become negative (and therefore larger
            // than what we are comparing it to), and the loop will run forver
            for(i = oldb; i != 0 && i > newb; i--)
            //for(i = oldb - 1; i >= newb; i--)
            {
                KDEBUG("truncate_node: i %d\n", i);

                /*
                 * TODO: Handle bmap() errors correctly. 
                 */
                dinfo->fs->ops->bmap(node, i - 1, block_size, BMAP_FLAG_FREE);
            }
        }
        
        node->size = sz;
    }
    
    time_t t = now();
    node->ctime = t;
    node->mtime = t;
    node->flags |= FS_NODE_DIRTY;
    kernel_mutex_unlock(&node->lock);
    KDEBUG("truncate_node: done\n");
    return res;
}


struct fs_node_t *new_node(dev_t dev)
{
    struct mount_info_t *dinfo = get_mount_info(dev);
    struct fs_node_t *node = get_empty_node();
    struct task_t *ct = cur_task;
    int res = 0;
    
    KDEBUG("new_node: dev = 0x%x, dinfo @ 0x%x\n", dev, dinfo);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    
    if(!node)
    {
        return NULL;
    }

    if(!dinfo)
    {
        node->refs = 0;
        return NULL;
    }

    kernel_mutex_lock(&node->lock);
    node->ops = dinfo->fs->ops;
    node->dev = dev;
    
    if(dinfo->fs->ops && dinfo->fs->ops->alloc_inode)
    {
        KDEBUG("new_node: alloc'ing\n");
        
        if((res = dinfo->fs->ops->alloc_inode(node)) < 0)
        {
            node->dev = 0;

            //if((ttytab[cur_tty].flags & TTY_FLAG_NO_TEXT))
            {
                switch_tty(1);
            }

            printk("new_node: dev = 0x%x\n", dev);
            kpanic("Failed to alloc new inode!\n");
            node->refs = 0;
            kernel_mutex_unlock(&node->lock);
            return NULL;
            //return res;
        }
    }
    
    KDEBUG("new_node: res = %d\n", res);
    node->refs = 1;
    node->links = 1;
    time_t t = now();
    node->ctime = t;
    node->mtime = t;
    node->atime = t;
    node->uid = ct->euid;
    node->gid = ct->egid;
    node->flags |= FS_NODE_DIRTY;
    kernel_mutex_unlock(&node->lock);
    return node;
}


void free_node(struct fs_node_t *node)
{
    if(!node)
    {
        return;
    }

    if(!node->dev)
    {
        A_memset(node, 0, sizeof(struct fs_node_t));
        return;
    }
    
    if(node->refs > 1)
    {
        kpanic("Freeing node with non-zero refs!\n");
    }
    
    if(node->links)
    {
        kpanic("Freeing node with non-zero links!\n");
    }

    //struct mount_info_t *dinfo = node_mount_info(node);
    struct mount_info_t *dinfo = get_mount_info(node->dev);

    if(dinfo && dinfo->fs->ops && dinfo->fs->ops->free_inode)
    {
        if(dinfo->fs->ops->free_inode(node) < 0)
        {
            kpanic("Failed to free inode!\n");
        }
    }

    A_memset(node, 0, sizeof(struct fs_node_t));

    invalidate_dentry(node);
}

