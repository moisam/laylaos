/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: pcache.c
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
 *  \file pcache.c
 *
 *  Page cache implementation.
 */

//#define __DEBUG
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <kernel/laylaos.h>
#include <kernel/pcache.h>
#include <kernel/mutex.h>
#include <kernel/dev.h>
#include <kernel/task.h>
#include <mm/kheap.h>
#include <mm/kstack.h>


#define INIT_HASHSZ             256
struct hashtab_t *pcachetab = NULL;
struct kernel_mutex_t pcachetab_lock = { 0, };

#include "pcache_internal.h"


static inline void release_page_memory(struct cached_page_t *pcache)
{
    if(pcache->virt)
    {
        dec_frame_shares(pcache->phys);
        vmmngr_free_page(get_page_entry((void *) pcache->virt));
        vmmngr_flush_tlb_entry(pcache->virt);
    }
    
    kfree(pcache);
}


void free_cached_page(struct pcache_key_t *pkey, struct cached_page_t *pcache)
{
    kernel_mutex_lock(&pcachetab_lock);

    if(get_frame_shares(pcache->phys) > 1)
    {
        pcache->flags &= ~PCACHE_FLAG_BUSY;
        kernel_mutex_unlock(&pcachetab_lock);
        printk("pcache: postponing page removal\n");
        return;
    }

    pcache_remove(pcachetab, pkey);
    kfree(pkey);
    release_page_memory(pcache);
    kernel_mutex_unlock(&pcachetab_lock);
}


void release_cached_page(struct cached_page_t *pcache)
{
    int wanted;

    if(!pcache)
    {
        return;
    }

    wanted = (pcache->flags & PCACHE_FLAG_WANTED);
    
    if(pcache->dev == PROCFS_DEVID)
    {
        return;
    }

    kernel_mutex_lock(&pcachetab_lock);
    pcache->flags &= ~(PCACHE_FLAG_BUSY | PCACHE_FLAG_WANTED);
    dec_frame_shares(pcache->phys);
    kernel_mutex_unlock(&pcachetab_lock);

    if(wanted)
    {
        unblock_tasks(pcache);
    }
}


static inline void mark_as_dirty(struct cached_page_t *pcache)
{
    pt_entry *e;

    if((e = get_page_entry((void *)pcache->virt)))
    {
        PTE_ADD_ATTRIB(e, I86_PTE_DIRTY);
    }
}


static inline void mark_as_clean(struct cached_page_t *pcache)
{
    pt_entry *e;

    if((e = get_page_entry((void *)pcache->virt)))
    {
        PTE_DEL_ATTRIB(e, I86_PTE_DIRTY);
    }
}


struct cached_page_t *get_cached_page(struct fs_node_t *node, 
                                      off_t offset, int flags)
{
    struct cached_page_t *pcache;
    struct mount_info_t *d;
    struct disk_req_t req;
    int maj = MAJOR(node->dev);
    int res;


    struct hashtab_item_t *hitem;
    struct pcache_key_t key, *pkey;

    
    if(node->dev == PROCFS_DEVID)
    {
        return NULL;
    }

    key.dev = node->dev;
    key.ino = node->inode;
    key.offset = offset;


loop:

    KDEBUG("get_cached_page: dev 0x%x, inode 0x%x, offset %lx (task %d - %s)\n", node->dev, node->inode, offset, cur_task ? cur_task->pid : -1, cur_task ? cur_task->command : "null");


    // lock the array so no one adds/removes anything while we search
    kernel_mutex_lock(&pcachetab_lock);

    // first, try to find the page in the page cache
    if((hitem = pcache_lookup(pcachetab, &key)))
    {
        pcache = hitem->val;

        if(pcache->flags & PCACHE_FLAG_BUSY)
        {
            pcache->flags |= PCACHE_FLAG_WANTED;
            kernel_mutex_unlock(&pcachetab_lock);
            //block_task(pcache, 0);
            block_task2(pcache, 500);
            goto loop;
        }

        KDEBUG("get_cached_page: pcache hit\n");
        pcache->flags |= PCACHE_FLAG_BUSY;
        inc_frame_shares(pcache->phys);
        pcache->last_accessed = ticks;
        pcache->pid = cur_task ? cur_task->pid : 0;
        kernel_mutex_unlock(&pcachetab_lock);
        return pcache;
    }

    if(flags & PCACHE_PEEK_ONLY)
    {
        kernel_mutex_unlock(&pcachetab_lock);
        return NULL;
    }

    KDEBUG("get_cached_page: cache miss\n");

    // page not found, allocate a new page cache entry
    if(!(pcache = kmalloc(sizeof(struct cached_page_t))) ||
       !(pkey = kmalloc(sizeof(struct pcache_key_t))))
    {
        kpanic("Cannot allocate page cache entry (1)\n");
    }
    
    A_memset(pcache, 0, sizeof(struct cached_page_t));
    A_memset(pkey, 0, sizeof(struct pcache_key_t));
    pcache->dev = node->dev;
    pcache->ino = node->inode;
    pcache->offset = offset;
    //pcache->refs = 1;
    pcache->flags |= PCACHE_FLAG_BUSY;
    pcache->pid = cur_task ? cur_task->pid : 0;

    pkey->dev = node->dev;
    pkey->ino = node->inode;
    pkey->offset = offset;

    if(!(hitem = pcache_alloc_hitem(pkey, pcache)))
    {
        kpanic("Cannot allocate page cache entry (2)\n");
    }

    pcache_add_hitem(pcachetab, pkey, hitem);
    kernel_mutex_unlock(&pcachetab_lock);

    // get a physical page and map it to kernel virtual space
    while(get_next_addr(&pcache->phys, &pcache->virt, 
                        PTE_FLAGS_PW, REGION_PCACHE) != 0)
    {
        printk("pcache: failed to allocate memory, retrying in 30 secs\n");
        block_task2(pcache, PIT_FREQUENCY * 30);
    }

    if(pcache->virt < PCACHE_MEM_START || pcache->virt >= PCACHE_MEM_END)
    {
        kpanic("pcache: got an invalid pcache address\n");
    }

    inc_frame_shares(pcache->phys);

    KDEBUG("get_cached_page: 6 - phys 0x%lx, virt 0x%lx\n", pcache->phys, pcache->virt);

    if((d = get_mount_info(node->dev)) == NULL)
    {
        free_cached_page(pkey, pcache);
        printk("pcache: reading from unmounted device!\n");
        //for(;;) ;
        return NULL;
    }

    if(d->block_size > PAGE_SIZE)
    {
        kpanic("pcache: reading from device with blk size > PAGE_SIZE!\n");
    }

    // assume this is a special block device, e.g. devfs or devpts
    if(d->block_size == 0)
    {
        //switch_tty(1);
        //printk("get_cached_page: d->dev 0x%x\n", d->dev);
        printk("pcache: accessing device with blk size == 0!\n");
        return NULL;
    }

    if(node->inode == PCACHE_NOINODE)
    {
        req.dev = node->dev;
        req.data = pcache->virt;

        //req.blocksz = d->block_size;
        req.datasz = d->block_size;
        req.fs_blocksz = d->block_size;

        req.blockno = offset;
        req.write = 0;

        if((res = bdev_tab[maj].strategy(&req)) < 0)
        {
            free_cached_page(pkey, pcache);
            return NULL;
        }

        pcache->len = d->block_size;
    }
    else
    {
        size_t block = offset / d->block_size;
        int i, n = PAGE_SIZE / d->block_size;
        int bmap_flag = (flags & PCACHE_AUTO_ALLOC) ? BMAP_FLAG_CREATE :
                                                      BMAP_FLAG_NONE;
        virtual_addr p = pcache->virt;
        size_t disk_block[n];
        int how_many = 1;

        res = 0;

        // Find out the mapping of the logical sectors we need to read
        for(i = 0; i < n; i++)
        {
            disk_block[i] = d->fs->ops->bmap(node, block + i, 
                                             d->block_size, bmap_flag);
            //printk("get_cached_page: [%d] %d\n", i, disk_block[i]);
        }

        // To try and reduce disk access requests (and IRQs and the resultant
        // delays), find out the maximum amount of sectors we can read in 
        // one go. We do this by checking consecutive block numbers, which are
        // going to be laid out consecutively on disk. We read that much
        // sectors, then read the non-consecutive rest in the loop below.
        for(i = 1; i < n; i++)
        {
            if(disk_block[i] == disk_block[i - 1] + 1)
            {
                how_many++;
            }
            else
            {
                break;
            }
        }

        i = 0;

        // Read as much as we can
        if(how_many > 1)
        {
            //printk("get_cached_page: how_many %d\n", how_many);
            req.dev = node->dev;
            req.data = p;
            req.datasz = d->block_size * how_many;
            req.fs_blocksz = d->block_size;
            req.blockno = disk_block[0];
            req.write = 0;

            if(bdev_tab[maj].strategy(&req) < 0)
            {
                free_cached_page(pkey, pcache);
                return NULL;
            }

            n -= how_many;
            i = how_many;
            p += d->block_size * how_many;
            res += d->block_size * how_many;
        }

        // Read the rest of the sectors (or all the sectors if we could
        // not find consecutive sectors above)
        while(n--)
        {
            if(!disk_block[i])
            //if(!(i = d->fs->ops->bmap(node, block, d->block_size, bmap_flag)))
            {
                A_memset((void *)p, 0, d->block_size);
            }
            else
            {
                req.dev = node->dev;
                req.data = p;
                //req.blocksz = d->block_size;
                req.datasz = d->block_size;
                req.fs_blocksz = d->block_size;
                //req.blockno = i;
                req.blockno = disk_block[i];
                req.write = 0;

                if(bdev_tab[maj].strategy(&req) < 0)
                {
                    break;
                }
            }

            p += d->block_size;
            res += d->block_size;
            //block++;
            i++;
        }

        if(res == 0)
        {
            free_cached_page(pkey, pcache);
            return NULL;
        }

        pcache->len = PAGE_SIZE;
    }

    KDEBUG("get_cached_page: 10 - res %d\n", res);

    if(res < PAGE_SIZE)
    {
        A_memset((void *)(pcache->virt + res), 0, PAGE_SIZE - res);
    }

    inc_frame_shares(pcache->phys);
    mark_as_clean(pcache);
    pcache->last_accessed = ticks;

    return pcache;
}


int sync_cached_page(struct cached_page_t *pcache)
{
    struct disk_req_t req;
    int maj = MAJOR(pcache->dev);
    int res = 0;

    KDEBUG("sync_cached_page: dev 0x%x, inode 0x%x, offset %lx (task %d)\n", pcache->dev, pcache->ino, pcache->offset, pcache->pid);

    if(pcache->ino == PCACHE_NOINODE)
    {
        req.dev = pcache->dev;
        req.data = pcache->virt;

        //req.blocksz = pcache->len; // d->block_size;
        req.datasz = pcache->len;
        req.fs_blocksz = pcache->len;

        req.blockno = pcache->offset;
        req.write = 1;

        if(bdev_tab[maj].strategy(&req) < 0)
        {
            return -EIO;
        }

        res = pcache->len;
    }
    else
    {
        struct mount_info_t *d;
        struct fs_node_t *node;
        size_t block;
        int i, n;
        virtual_addr p;

        if((d = get_mount_info(pcache->dev)) == NULL)
        {
            printk("pcache: writing to unmounted device!\n");
            return -EIO;
        }

        if(!(node = get_node(pcache->dev, pcache->ino, 1)))
        {
            return -EIO;
        }

        block = pcache->offset / d->block_size;
        n = pcache->len /* PAGE_SIZE */ / d->block_size;
        p = pcache->virt;

        size_t disk_block[n];
        int how_many = 1;

        // Find out the mapping of the logical sectors we need to read
        for(i = 0; i < n; i++)
        {
            disk_block[i] = d->fs->ops->bmap(node, block + i, 
                                             d->block_size, BMAP_FLAG_NONE);
            //printk("sync_cached_page: [%d] %d\n", i, disk_block[i]);
        }

        // To try and reduce disk access requests (and IRQs and the resultant
        // delays), find out the maximum amount of sectors we can write in 
        // one go. We do this by checking consecutive block numbers, which are
        // going to be laid out consecutively on disk. We write that much
        // sectors, then write the non-consecutive rest in the loop below.
        for(i = 1; i < n; i++)
        {
            if(disk_block[i] == disk_block[i - 1] + 1)
            {
                how_many++;
            }
            else
            {
                break;
            }
        }

        i = 0;

        // Read as much as we can
        if(how_many > 1)
        {
            //printk("sync_cached_page: how_many %d\n", how_many);
            req.dev = node->dev;
            req.data = p;
            req.datasz = d->block_size * how_many;
            req.fs_blocksz = d->block_size;
            req.blockno = disk_block[0];
            req.write = 1;

            if(bdev_tab[maj].strategy(&req) < 0)
            {
                return 0;
            }

            n -= how_many;
            i = how_many;
            p += d->block_size * how_many;
            res += d->block_size * how_many;
        }

        // Read the rest of the sectors (or all the sectors if we could
        // not find consecutive sectors above)

        while(n--)
        {
            if(disk_block[i])
            //if((i = d->fs->ops->bmap(node, block, d->block_size, BMAP_FLAG_NONE)))
            {
                req.dev = pcache->dev;
                req.data = p;

                //req.blocksz = d->block_size;
                req.datasz = d->block_size;
                req.fs_blocksz = d->block_size;

                //req.blockno = i;
                req.blockno = disk_block[i];
                req.write = 1;

                if(bdev_tab[maj].strategy(&req) < 0)
                {
                    break;
                }
            }

            p += d->block_size;
            res += d->block_size;
            //block++;
            i++;
        }
    }

    return res;
}


static void mark_dirty_pages(int maj)
{
    struct cached_page_t *pcache;
    pt_entry *e;
    struct hashtab_item_t *hitem;
    int i;
    
    if(!pcachetab)
    {
        return;
    }

    kernel_mutex_lock(&pcachetab_lock);
    
    for(i = 0; i < pcachetab->count; i++)
    {
        hitem = pcachetab->items[i];
        
        while(hitem)
        {
            pcache = hitem->val;

            if(maj != -1 && (int)MAJOR(pcache->dev) != maj)
            {
                hitem = hitem->next;
                continue;
            }

            if(pcache->flags & PCACHE_FLAG_BUSY)
            {
                hitem = hitem->next;
                continue;
            }

            if(!(e = get_page_entry((void *)pcache->virt)))
            {
                hitem = hitem->next;
                continue;
            }

            if(PTE_DIRTY(*e))
            {
                pcache->flags |= /* PCACHE_FLAG_BUSY | */ PCACHE_FLAG_DIRTY;
                pcache->pid = cur_task ? cur_task->pid : 0;
                PTE_DEL_ATTRIB(e, I86_PTE_DIRTY);
                pcache->last_accessed = ticks;
            }

            hitem = hitem->next;
        }
    }

    kernel_mutex_unlock(&pcachetab_lock);
}


static inline void release_pcache_internal(struct hashtab_item_t *hitem,
                                           struct hashtab_item_t *prev, int i)
{
    struct cached_page_t *pcache = hitem->val;

    if(prev)
    {
        prev->next = hitem->next;
    }
    else
    {
        pcachetab->items[i] = hitem->next;
    }

    kfree(hitem->key);
    kfree(hitem);
    release_page_memory(pcache);
}


static void flush_dirty_pages(int maj)
{
    struct cached_page_t *pcache;
    struct hashtab_item_t *hitem, *prev;
    int res, wanted;
    int i;

loop:

    kernel_mutex_lock(&pcachetab_lock);
    
    for(i = 0; i < pcachetab->count; i++)
    {
        hitem = pcachetab->items[i];
        prev = NULL;
        
        while(hitem)
        {
            pcache = hitem->val;

            if(maj != -1 && (int)MAJOR(pcache->dev) != maj)
            {
                prev = hitem;
                hitem = hitem->next;
                continue;
            }

            if(!(pcache->flags & PCACHE_FLAG_DIRTY) ||
               (pcache->flags & PCACHE_FLAG_BUSY))
            {
                prev = hitem;
                hitem = hitem->next;
                continue;
            }

            pcache->flags |= PCACHE_FLAG_BUSY;
            kernel_mutex_unlock(&pcachetab_lock);
            res = sync_cached_page(pcache);
            kernel_mutex_lock(&pcachetab_lock);

            wanted = (pcache->flags & PCACHE_FLAG_WANTED);
            pcache->flags &= ~(PCACHE_FLAG_BUSY | PCACHE_FLAG_DIRTY);

            if(res < 0)
            {
                if(get_frame_shares(pcache->phys) > 1)
                {
                    mark_as_dirty(pcache);
                    printk("pcache: postponing page removal\n");
                }
                else if(!wanted)
                {
                    release_pcache_internal(hitem, prev, i);
                }
            }
            else
            {
                if(res < pcache->len)
                {
                    mark_as_dirty(pcache);
                }
            }

            kernel_mutex_unlock(&pcachetab_lock);

            if(wanted)
            {
                printk("flush_dirty_pages: waking up sleepers on 0x%lx\n", pcache);
                unblock_tasks(pcache);
            }

            goto loop;
        }
    }

    kernel_mutex_unlock(&pcachetab_lock);
}


static inline void remove_old_cached_pages(int maj)
{
    struct cached_page_t *pcache;
    struct hashtab_item_t *hitem, *prev;
    unsigned long long five_minutes = 5 * 60 * PIT_FREQUENCY;
    unsigned long long older_than = ticks - five_minutes;
    int i;

    // check that 5 mins at least have passed since boot
    if(ticks <= five_minutes)
    {
        return;
    }

    kernel_mutex_lock(&pcachetab_lock);

loop:

    for(i = 0; i < pcachetab->count; i++)
    {
        hitem = pcachetab->items[i];
        prev = NULL;
        
        while(hitem)
        {
            pcache = hitem->val;

            if(maj != -1 && (int)MAJOR(pcache->dev) != maj)
            {
                prev = hitem;
                hitem = hitem->next;
                continue;
            }

            // remove the page if it is old and no one is using it
            if((pcache->last_accessed < older_than) &&
               !(pcache->flags & (PCACHE_FLAG_BUSY | PCACHE_FLAG_WANTED)))
            {
                release_pcache_internal(hitem, prev, i);
                goto loop;
            }

            prev = hitem;
            hitem = hitem->next;
        }
    }

    kernel_mutex_unlock(&pcachetab_lock);
}


void flush_cached_pages(dev_t dev)
{
    int maj;

    if(dev == NODEV)
    {
        maj = -1;
    }
    else
    {
        maj = MAJOR(dev);
    }

    // First, get a list of all the dirty pages and mark them as both
    // dirty and busy, so no one else can claim them.
    // Next, release the table lock and flush dirty pages to disk.
    // We don't need to lock the table the second round as we already
    // marked all dirty pages as busy.

    mark_dirty_pages(maj);
    flush_dirty_pages(maj);
    remove_old_cached_pages(maj);
}


int remove_cached_disk_pages(dev_t dev)
{
    struct cached_page_t *pcache;
    struct hashtab_item_t *hitem, *prev;
    int res = 0;
    int i;

    kernel_mutex_lock(&pcachetab_lock);

loop:

    for(i = 0; i < pcachetab->count; i++)
    {
        hitem = pcachetab->items[i];
        prev = NULL;
        
        while(hitem)
        {
            pcache = hitem->val;

            if(pcache->dev == dev)
            {
                // remove the page if no one is using it
                if(!(pcache->flags & (PCACHE_FLAG_BUSY | PCACHE_FLAG_WANTED)))
                {
                    release_pcache_internal(hitem, prev, i);
                    goto loop;
                }

                res = -EBUSY;
            }

            prev = hitem;
            hitem = hitem->next;
        }
    }

    kernel_mutex_unlock(&pcachetab_lock);
    return res;
}


int remove_cached_node_pages(struct fs_node_t *node)
{
    struct cached_page_t *pcache;
    struct hashtab_item_t *hitem, *prev;
    int res = 0;
    int i;

    if(!node || node->dev == 0 || node->inode == 0)
    {
        return -EINVAL;
    }

    kernel_mutex_lock(&pcachetab_lock);

loop:

    for(i = 0; i < pcachetab->count; i++)
    {
        hitem = pcachetab->items[i];
        prev = NULL;
        
        while(hitem)
        {
            pcache = hitem->val;

            if(pcache->dev == node->dev && pcache->ino == node->inode)
            {
                // remove the page if no one is using it
                if(!(pcache->flags & (PCACHE_FLAG_BUSY | PCACHE_FLAG_WANTED)))
                {
                    release_pcache_internal(hitem, prev, i);
                    goto loop;
                }

                res = -EBUSY;
            }

            prev = hitem;
            hitem = hitem->next;
        }
    }

    kernel_mutex_unlock(&pcachetab_lock);
    return res;
}


/*
 * Get cached page count (pages with backing file nodes).
 */
size_t get_cached_page_count(void)
{
    struct cached_page_t *pcache;
    struct hashtab_item_t *hitem;
    size_t count = 0;
    int i;

    kernel_mutex_lock(&pcachetab_lock);

    for(i = 0; i < pcachetab->count; i++)
    {
        hitem = pcachetab->items[i];
        
        while(hitem)
        {
            pcache = hitem->val;

            if(pcache->ino != PCACHE_NOINODE)
            {
                count++;
            }

            hitem = hitem->next;
        }
    }
    
    kernel_mutex_unlock(&pcachetab_lock);
    return count;
}


/*
 * Get cached disk buffer count (pages with no backing file nodes).
 */
size_t get_cached_block_count(void)
{
    struct cached_page_t *pcache;
    struct hashtab_item_t *hitem;
    size_t count = 0;
    int i;

    kernel_mutex_lock(&pcachetab_lock);

    for(i = 0; i < pcachetab->count; i++)
    {
        hitem = pcachetab->items[i];
        
        while(hitem)
        {
            pcache = hitem->val;

            if(pcache->ino == PCACHE_NOINODE)
            {
                count++;
            }

            hitem = hitem->next;
        }
    }
    
    kernel_mutex_unlock(&pcachetab_lock);
    return count;
}

