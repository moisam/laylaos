/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
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
#include <kernel/asm.h>
#include <mm/kheap.h>
#include <fs/sockfs.h>
#include <fs/pipefs.h>
#include <fs/dentry.h>

// file node table
//struct fs_node_t node_table[NR_INODE];
//static struct fs_node_t *last_alloc_node = node_table;

struct fs_node_t *node_table[NR_INODE];
//struct fs_node_t first_node = { 0, };
volatile struct kernel_mutex_t list_lock = { 0, };


/*
 * Write out all modified inodes to disk. Called by update().
 */
void sync_nodes(dev_t dev)
{
    struct fs_node_t **node;
    struct fs_node_t **llnode = &node_table[NR_INODE];

    kernel_mutex_lock(&list_lock);
    
    for(node = node_table; node < llnode; node++)
    {
        if(!(*node))
        {
            continue;
        }

        //kernel_mutex_lock(&(*node)->lock);

        if(dev != NODEV && (*node)->dev != dev)
        {
            //kernel_mutex_unlock(&(*node)->lock);
            continue;
        }

        __sync_or_and_fetch(&(*node)->flags, FS_NODE_KEEP_INCORE);
        kernel_mutex_unlock(&list_lock);
        kernel_mutex_lock(&(*node)->lock);

        if((*node)->dev && /* ((*node)->flags & FS_NODE_DIRTY) && */ 
            !IS_PIPE((*node)) && !((*node)->flags & FS_NODE_STALE))
        {
            KDEBUG("sync_nodes: dev 0x%x, n 0x%x\n", (*node)->dev, (*node)->inode);
			write_node((*node));
	    }

        __sync_and_and_fetch(&(*node)->flags, ~FS_NODE_KEEP_INCORE);
        kernel_mutex_unlock(&(*node)->lock);
        kernel_mutex_lock(&list_lock);
	}

    kernel_mutex_unlock(&list_lock);
}


long files_referencing_node(struct fs_node_t *node)
{
    struct file_t *f, *lf;
    long refs = 0;

    for(f = ftab, lf = &ftab[NR_FILE]; f < lf; f++)
    {
        if(f->node == node)
        {
            refs++;
        }
    }

    return refs;
}


static void zero_out_node(struct fs_node_t *node)
{
    node->dev = 0;
    node->inode = 0;
    node->minfo = NULL;

    __lock_xchg_int(&node->refs, 0);
    //node->refs = 0;

    node->mode = 0;
    node->uid = 0;
    node->mtime = 0;
    node->atime = 0;
    node->ctime = 0;
    node->size = 0;
    node->links = 0;
    node->gid = 0;
    node->disk_sectors = 0;
    //node->flags = 0;
    node->ops = NULL;
    node->ptr = NULL;
    //node->next = NULL;
    node->data = NULL;
    node->poll = NULL;
    node->select = NULL;
    node->read = NULL;
    node->write = NULL;
    node->alocks = NULL;

    for(volatile int i = 0; i < 15; i++) node->blocks[i] = 0;

    //__sync_and_and_fetch(&node->flags, ~(FS_NODE_DIRTY /* FS_NODE_STALE */));

    __asm__ __volatile__("":::"memory");
}


static void wait_for_node_update(struct fs_node_t *node)
{
    volatile unsigned int flags = node->flags;

    while(flags & FS_NODE_KEEP_INCORE)
    {
        kernel_mutex_unlock(&list_lock);
        scheduler();
        kernel_mutex_lock(&list_lock);
        flags = node->flags;
    }
}


static void remove_from_list(struct fs_node_t *node)
{
    struct fs_node_t **tmp;
    struct fs_node_t **llnode = &node_table[NR_INODE];

    kernel_mutex_lock(&list_lock);

    for(tmp = node_table; tmp < llnode; tmp++)
    {
        if(*tmp == node)
        {
            //kpanic("\n\n*** freeing node!!!!\n\n");
            wait_for_node_update(*tmp);
            *tmp = NULL;
            kernel_mutex_unlock(&list_lock);

            if(node->refs != 0)
            {
                switch_tty(1);
                printk("\n\n*** dev 0x%x, node 0x%x, refs %d, flags 0x%x, links %d\n\n", node->dev, node->inode, node->refs, node->flags, node->links);
                kpanic("*** invalid node\n");
            }

            kfree(node);
            return;
        }
    }

    kernel_mutex_unlock(&list_lock);

    switch_tty(1);
    printk("\n\n*** dev 0x%x, node 0x%x, refs %d, flags 0x%x, links %d\n", node->dev, node->inode, node->refs, node->flags, node->links);
    printk("*** pipe %d, sock %d\n", IS_PIPE(node), IS_SOCKET(node));
    printk("*** select 0x%lx, poll 0x%lx, read 0x%lx, write 0x%lx\n", node->select, node->poll, node->read, node->write);
    //printk("*** sig %c%c%c%c\n\n", node->sig[0], node->sig[1], node->sig[2], node->sig[3]);
    kpanic("\n\n*** node not found in table!!!!\n\n");

}


/*
 * Release the file node. If it is a pipe, wake up sleepers (if any) and free
 * the pipe's memory page.
 * For dirty nodes, update the node on disk. If the file is empty, truncate 
 * it and free the node struct on disk.
 */
void release_node(struct fs_node_t *node)
{
    if(!node)
    {
        return;
    }

    kernel_mutex_lock(&node->lock);

    if(node->refs == 0)
    {
        kernel_mutex_unlock(&node->lock);
        return;
    }
    
    //node->refs--;
    __sync_fetch_and_sub(&node->refs, 1);

    if(IS_PIPE(node))
    {
        kernel_mutex_unlock(&node->lock);
        selwakeup(&node->select_channel);   // wakeup readers/writers

        if(node->refs == 0)
        {
            pipefs_free_node(node);
            remove_from_list(node);
        }

        return;
    }

    if(IS_SOCKET(node))
    {
        if(!(node->flags & FS_NODE_SOCKET_ONDISK))
        {
            kernel_mutex_unlock(&node->lock);

            if(node->refs == 0)
            {
                remove_from_list(node);
            }

            return;
        }
    }
    
    if(node->refs != 0)
    {
        kernel_mutex_unlock(&node->lock);
        return;
    }


    struct memregion_t *tmp;
    volatile long expected_refs = 0, file_refs = 0, mem_refs = 0, pcache_refs = 0;

    file_refs = files_referencing_node(node);
    expected_refs += file_refs;

    elevated_priority_lock(&task_table_lock);

    for_each_taskptr(t)
    {
        if(!*t || (*t)->mem == NULL)
        {
            continue;
        }

        for(tmp = (*t)->mem->first_region; tmp != NULL; tmp = tmp->next)
        {
            if(tmp->inode == node)
            {
                mem_refs++;
            }
        }
    }

    elevated_priority_unlock(&task_table_lock);
    expected_refs += mem_refs;

    pcache_refs = node_has_cached_pages(node);
    expected_refs += pcache_refs;

    if(expected_refs)
    {
        //__sync_bool_compare_and_swap(&node->refs, 0, expected_refs);
        __lock_xchg_int(&node->refs, expected_refs);
        //node->refs = expected_refs;
        __asm__ __volatile__("":::"memory");
        kernel_mutex_unlock(&node->lock);
        return;
        /*
        switch_tty(1);
        printk("\n\n*** dev 0x%x, node 0x%x, refs %d, flags 0x%x, links %d\n", node->dev, node->inode, node->refs, node->flags, node->links);
        printk("\n\n*** expected_refs %d, file_refs %d, mem_refs %d, pcache_refs %d\n", expected_refs, file_refs, mem_refs, pcache_refs);
        kpanic("*** trying to free a referenced node\n");
        */
    }

    
    __sync_or_and_fetch(&node->flags, FS_NODE_STALE);
    kernel_mutex_unlock(&node->lock);
    disk_updater_disable();

    if(node->links == 0)
    {
        remove_cached_node_pages(node);
        truncate_node(node, 0);
        free_node(node);
    }
    else
    {
        //if(node->flags & FS_NODE_DIRTY)
        {
            KDEBUG("%s: dev 0x%x, ino 0x%x\n", __func__, node->dev, node->inode);
            write_node(node);
        }
    }

    disk_updater_enable();
    remove_from_list(node);
}


/*
 * Check if a node is incore (that is, used by some task).
 */
int node_is_incore(dev_t dev, ino_t n)
{
    struct fs_node_t **node;
    struct fs_node_t **llnode = &node_table[NR_INODE];

    kernel_mutex_lock(&list_lock);
    
    for(node = node_table; node < llnode; node++)
    {
        if((*node) && (*node)->refs && (*node)->inode == n && (*node)->dev == dev)
        {
            kernel_mutex_unlock(&list_lock);
            return 1;
	    }
	}
	
    kernel_mutex_unlock(&list_lock);
	return 0;
}


struct fs_node_t *get_empty_node(void)
{
    struct fs_node_t *node;
    struct fs_node_t **tmp;
    struct fs_node_t **llnode = &node_table[NR_INODE];
    volatile unsigned long long older_than_ticks = TWO_MINUTES;

    if(!(node = kmalloc(sizeof(struct fs_node_t))))
    {
        return NULL;
    }

    A_memset(node, 0, sizeof(struct fs_node_t));
    __lock_xchg_int(&node->refs, 1);
    //node->refs = 1;

try:

    kernel_mutex_lock(&list_lock);

    for(tmp = node_table; tmp < llnode; tmp++)
    {
        if(!(*tmp))
        {
            *tmp = node;
            kernel_mutex_unlock(&list_lock);
            return node;
        }
    }

    kernel_mutex_unlock(&list_lock);

    if(older_than_ticks != 0)
    {
        //if(older_than_ticks == TWO_MINUTES)
        {
            flush_cached_pages(NODEV);
        }

        remove_unreferenced_cached_pages(NULL);
        remove_old_cached_pages(-1, older_than_ticks);
        older_than_ticks = 0;

        /*
        if(older_than_ticks > ONE_MINUTE)
        {
            older_than_ticks -= ONE_MINUTE;
        }
        else
        {
            older_than_ticks -= 15 * PIT_FREQUENCY;
        }
        */

        goto try;
    }



    switch_tty(1);
    printk("\n");
    for(tmp = node_table; tmp < llnode; tmp++)
    {
        if(*tmp) printk("%x:%x,", (*tmp)->dev, (*tmp)->inode);
        else printk("0:0,");
    }
    print_cache_stats();
    kpanic("*** get_node()\n");



    kfree(node);
    return NULL;
}


struct fs_node_t *get_node(dev_t dev, ino_t n, int flags)
{
    int follow_mpoints = (flags & GETNODE_FOLLOW_MPOINTS);

    if(!dev || !n)
    {
        return NULL;
    }

    struct fs_node_t *res, **node = node_table;
    struct fs_node_t **lnode = &node_table[NR_INODE];

    kernel_mutex_lock(&list_lock);

    while(node < lnode)
    {
        // not the node we want
        if(*node == NULL || (*node)->dev != dev || (*node)->inode != n)
        {
            node++;
            continue;
        }
        
        // we found it! wait until it is unlocked
        //__sync_or_and_fetch(&node->flags, FS_NODE_WANTED);
        kernel_mutex_lock(&(*node)->lock);

        if((*node)->flags & FS_NODE_STALE)
        {
            kernel_mutex_unlock(&(*node)->lock);
            kernel_mutex_unlock(&list_lock);

            if(flags & GETNODE_IGNORE_STALE)
            {
                return NULL;
            }

            scheduler();

            kernel_mutex_lock(&list_lock);
            node = node_table;
            continue;
        }
        
        // make sure no one changed it while we slept
        if((*node)->dev != dev || (*node)->inode != n)
        {
            // shit! start again from the top
            kernel_mutex_unlock(&(*node)->lock);
            node = node_table;
            continue;
        }
        
        // is it a mount point?
        if(((*node)->flags & FS_NODE_MOUNTPOINT) && follow_mpoints)
        {
            res = (*node)->ptr;
            kernel_mutex_unlock(&(*node)->lock);
            
            if(!res)
            {
                kernel_mutex_unlock(&list_lock);
                return NULL;
            }

            //res->refs++;
            __sync_fetch_and_add(&res->refs, 1);
        }
        else
        {
            res = (*node);
            //res->refs++;
            __sync_fetch_and_add(&res->refs, 1);
            kernel_mutex_unlock(&(*node)->lock);
        }
        
        kernel_mutex_unlock(&list_lock);
        return res;
    }

    kernel_mutex_unlock(&list_lock);
    
    // node not found - get an empty node
    if(!(res = get_empty_node()))
    {
        kpanic("\nget_node - 1!!\n");
        return NULL;
    }
    
    kernel_mutex_lock(&res->lock);

    //printk("get_node: 1 lock 0x%lx, me 0x%lx, holder 0x%lx\n", &res->lock, this_core->cur_task, res->lock.holder);

    res->dev = dev;
    res->inode = n;
    // make it stale for now so no one can use it until we read it from disk
    __sync_or_and_fetch(&res->flags, FS_NODE_STALE);
    KDEBUG("get_node - trying to read node: dev 0x%x, node 0x%x\n", node->dev, node->inode);

    // read the node from disk
    if(read_node(res) < 0)
    {
        res->dev = 0;
        res->inode = 0;
        kpanic("get_node - 2!!\n");
        kernel_mutex_unlock(&res->lock);

        kernel_mutex_lock(&list_lock);

        for(node = node_table; node < lnode; node++)
        {
            if(*node == res)
            {
                wait_for_node_update(*node);
                *node = NULL;
                kfree(res);
                break;
            }
        }

        kernel_mutex_unlock(&list_lock);

        return NULL;
    }

    // now it's ready for use
    __sync_and_and_fetch(&res->flags, ~FS_NODE_STALE);
    kernel_mutex_unlock(&res->lock);

    //printk("get_node: 2 lock 0x%lx, me 0x%lx, holder 0x%lx\n", &res->lock, this_core->cur_task, res->lock.holder);

    return res;
}


/*
 * Must be called with node locked.
 */
long read_node(struct fs_node_t *node)
{
    if(!node)
    {
        return -EINVAL;
    }
    
    //struct mount_info_t *dinfo = node_mount_info(node);
    struct mount_info_t *dinfo = get_mount_info(node->dev);
    long res;

    if(!dinfo)
    {
        kpanic("Reading inode from unmounted disk!\n");
        return -EINVAL;
    }

    node->ops = dinfo->fs->ops;
    
    if(dinfo->fs->ops && dinfo->fs->ops->read_inode)
    {
        if((res = dinfo->fs->ops->read_inode(node)) < 0)
        {
            //switch_tty(1);
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            printk("read_node - dev 0x%x, node 0x%x, res %d\n", node->dev, node->inode, res);

            //kpanic("Failed to read inode from disk!\n");
            return res;
        }
    }
    
    return 0;
}


/*
 * Must be called with node locked.
 */
long write_node(struct fs_node_t *node)
{
    long res;

    if(!node)
    {
        return -EINVAL;
    }

    if(node->dev == 0)
    {
        __sync_and_and_fetch(&(node->flags), ~FS_NODE_DIRTY);
        //node->flags &= ~FS_NODE_DIRTY;
        return -EINVAL;
    }

    KDEBUG("write_node: dev 0x%x, inode 0x%x, links 0x%x\n", node->dev, node->inode, node->links);
    
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

    __sync_and_and_fetch(&(node->flags), ~FS_NODE_DIRTY);
    //node->flags &= ~FS_NODE_DIRTY;

    return 0;
}


/*
 * Must be called with node locked.
 */
long truncate_node(struct fs_node_t *node, size_t sz)
{
    long res = 0;

    if(!node || node->dev == 0)
    {
        return -EINVAL;
    }

	if(!(S_ISREG(node->mode) || S_ISDIR(node->mode)))
	{
        return -EINVAL;
    }
    
    KDEBUG("truncate_node: dev 0x%x, n 0x%x\n", node->dev, node->inode);

    //remove_cached_node_pages(node);

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

    __sync_or_and_fetch(&(node->flags), FS_NODE_DIRTY);
    //node->flags |= FS_NODE_DIRTY;

    return res;
}


struct fs_node_t *new_node(dev_t dev)
{
    struct mount_info_t *dinfo = get_mount_info(dev);
    struct fs_node_t *node = get_empty_node();
    long res = 0;

    KDEBUG("new_node: dev = 0x%x, dinfo @ 0x%x\n", dev, dinfo);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    if(!node)
    {
        return NULL;
    }

    if(!dinfo)
    {
        __lock_xchg_int(&node->refs, 0);
        //node->refs = 0;
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

            switch_tty(1);
            printk("new_node: dev = 0x%x\n", dev);
            kpanic("Failed to alloc new inode!\n");
            __lock_xchg_int(&node->refs, 0);
            //node->refs = 0;

            kernel_mutex_unlock(&node->lock);
            return NULL;
            //return res;
        }
    }

    KDEBUG("new_node: res = %d\n", res);
    __lock_xchg_int(&node->refs, 1);
    //node->refs = 1;
    node->links = 1;
    time_t t = now();
    node->ctime = t;
    node->mtime = t;
    node->atime = t;
    node->uid = this_core->cur_task->euid;
    node->gid = this_core->cur_task->egid;

    __sync_or_and_fetch(&(node->flags), FS_NODE_DIRTY);
    //node->flags |= FS_NODE_DIRTY;

    kernel_mutex_unlock(&node->lock);
    return node;
}


void free_node(struct fs_node_t *node)
{
    //long res;

    if(!node)
    {
        return;
    }

    if(!node->dev)
    {
        //A_memset(node, 0, sizeof(struct fs_node_t));
        zero_out_node(node);
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

    kernel_mutex_lock(&node->lock);

    if(dinfo && dinfo->fs->ops)
    {
        node->uid = 0;
        node->gid = 0;
        node->mode = 0;
        node->mtime = 0;
        node->atime = 0;
        node->ctime = 0;
        node->disk_sectors = 0;

        // Some filesystems (e.g. tmpfs) free the inode struct on disk when
        // we call its free_inode() function. This is why we write the zeroed
        // out node first, then call free_inode() afterwards.
        /*
        if(dinfo->fs->ops->write_inode)
        {
            if((res = dinfo->fs->ops->write_inode(node)) < 0)
            {
                printk("free_node: failed to write inode to disk!\n");
            }
        }
        */

        if(dinfo->fs->ops->free_inode)
        {
            if(dinfo->fs->ops->free_inode(node) < 0)
            {
                printk("free_node: failed to free inode!\n");
            }
        }
    }

    invalidate_dentry(node);
    //A_memset(node, 0, sizeof(struct fs_node_t));
    zero_out_node(node);
    kernel_mutex_unlock(&node->lock);

    // no need to unlock as memset has cleared the node struct
}

