/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
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

#include "../kernel/task_funcs.c"


#define INIT_HASHSZ             2048
struct hashtab_t *pcachetab = NULL;
volatile struct kernel_mutex_t pcachetab_lock = { 0, };

#include "pcache_internal.h"

#define FILL_ZEROES(k, s)             \
{                                     \
    uint8_t i, *a = (uint8_t *)k;     \
    for(i = 0; i < s; i++) a[i] = 0;  \
}


static inline void release_page_memory(struct cached_page_t *pcache)
{
    if(pcache->virt)
    {
        if(get_frame_shares(pcache->phys) != 1)
        {
            switch_tty(1);
            printk("pcache: wrong refs on page dev 0x%x, ino 0x%x, flags 0x%x, pid %d, curpid %d\n", pcache->dev, pcache->ino, pcache->flags, pcache->pid, this_core->cur_task ? this_core->cur_task->pid : 0);
            printk("pcache: off %ld, refs %d\n", pcache->offset, get_frame_shares(pcache->phys));
            //printk("pcache: holding task 0x%lx\n", pcache->pid ? get_task_by_id(pcache->pid) : NULL);
            kpanic("pcache: infinite loop\n");
        }

        dec_frame_shares(pcache->phys);
        vmmngr_free_page(get_page_entry((void *) pcache->virt));
        vmmngr_flush_tlb_entry(pcache->virt);
    }

    if(pcache->node)
    {
        struct fs_node_t *node = pcache->node;

        pcache->node = NULL;

        // avoid locking ourselves if we call flush_cached_pages() and 
        // it calls bmap(), which calls pmmngr_alloc_block(), and the
        // latter needs to free memory so it calls us, in which case
        // we are already holding the lock to the node
        if(node->lock.holder && node->lock.holder == this_core->cur_task)
        {
            kernel_mutex_unlock(&node->lock);
            release_node(node);
            kernel_mutex_lock(&node->lock);
        }
        else
        {
            release_node(node);
        }
    }
    
    A_memset(pcache, 0, sizeof(struct cached_page_t));
    kfree(pcache);
    __asm__ __volatile__("":::"memory");
}


void free_cached_page(struct pcache_key_t *pkey, struct cached_page_t *pcache)
{
    kernel_mutex_lock(&pcachetab_lock);

    if(get_frame_shares(pcache->phys) > 1)
    {
        __sync_and_and_fetch(&pcache->flags, ~PCACHE_FLAG_BUSY);
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
    __sync_and_and_fetch(&pcache->flags, ~(PCACHE_FLAG_BUSY | PCACHE_FLAG_WANTED));
    dec_frame_shares(pcache->phys);
    kernel_mutex_unlock(&pcachetab_lock);
    __asm__ __volatile__("":::"memory");

    if(wanted)
    {
        unblock_tasks(pcache);
    }
    /*
    else if(get_frame_shares(pcache->phys) <= 1)
    {
        // This might not be the best idea as it involves re-reading pages
        // if they are needed after they are released. Here we release the 
        // page once the last reference is removed to free memory and ensure
        // changes are flushed to disk
        volatile struct hashtab_item_t *hitem;
        struct pcache_key_t key, *pkey;

        if(pcache->flags & (PCACHE_FLAG_ALWAYS_DIRTY | PCACHE_FLAG_DIRTY))
        {
            __sync_and_and_fetch(&pcache->flags, ~PCACHE_FLAG_DIRTY);
            sync_cached_page(pcache);
        }

        if(!(pcache->flags & (PCACHE_FLAG_BUSY | PCACHE_FLAG_WANTED | PCACHE_FLAG_DIRTY)))
        {
            FILL_ZEROES(&key, sizeof(struct pcache_key_t));
            key.dev = pcache->dev;
            key.ino = pcache->ino;
            key.offset = pcache->offset;

            kernel_mutex_lock(&pcachetab_lock);
            hitem = pcache_lookup(pcachetab, &key);

            if(hitem)
            {
                pkey = hitem->key;
                pcache_remove(pcachetab, pkey);
                kfree(pkey);
                release_page_memory(pcache);
            }

            kernel_mutex_unlock(&pcachetab_lock);
            __asm__ __volatile__("":::"memory");
        }
    }
    */
}


/*
#define MAY_LOCK(lock)                              \
    volatile int unlock = 0;                        \
    if(kernel_mutex_trylock(lock)) {                \
        if(!this_core->cur_task || (lock)->holder != this_core->cur_task) {\
            kernel_mutex_lock(lock);                \
            unlock = 1;                             \
        }                                           \
    } else unlock = 1;

#define MAY_UNLOCK(lock)    \
    if(unlock) kernel_mutex_unlock(lock);
*/


struct cached_page_t *get_cached_page(struct fs_node_t *node, 
                                      off_t offset, int flags)
{
    struct cached_page_t *pcache;
    struct mount_info_t *d;
    struct disk_req_t req;
    struct ustat ubuf;
    int maj = MAJOR(node->dev);
    int res;
    volatile struct hashtab_item_t *hitem;
    struct pcache_key_t key, *pkey;
    volatile int tries = 0;

    if(node->dev == PROCFS_DEVID)
    {
        return NULL;
    }

    //printk("get_cached_page: dev 0x%x, node 0x%x\n", node->dev, node->inode);

    FILL_ZEROES(&key, sizeof(struct pcache_key_t));
    key.dev = node->dev;
    key.ino = node->inode;
    key.offset = offset;

loop:

    // lock the array so no one adds/removes anything while we search
    kernel_mutex_lock(&pcachetab_lock);

    // first, try to find the page in the page cache
    if((hitem = pcache_lookup(pcachetab, &key)))
    {
        pcache = hitem->val;

        if(pcache->flags & PCACHE_FLAG_STALE)
        {
            //__asm__ __volatile__("xchg %%bx, %%bx":::);
            kernel_mutex_unlock(&pcachetab_lock);
            //remove_unreferenced_cached_pages();
            remove_stale_cached_pages();

            if(flags & PCACHE_IGNORE_STALE)
            {
                return NULL;
            }

            if(++tries >= 50)
            {
                switch_tty(1);
                printk("pcache: stale page dev 0x%x, ino 0x%x, flags 0x%x, pid %d, curpid %d\n", key.dev, key.ino, pcache->flags, pcache->pid, this_core->cur_task ? this_core->cur_task->pid : 0);
                printk("pcache: refs %d\n", get_frame_shares(pcache->phys));
                //printk("pcache: holding task 0x%lx\n", pcache->pid ? get_task_by_id(pcache->pid) : NULL);
                kpanic("pcache: infinite loop\n");
            }

            scheduler();

            goto loop;
        }

        if(pcache->flags & PCACHE_FLAG_BUSY)
        {
            __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_WANTED);
            kernel_mutex_unlock(&pcachetab_lock);

            if(++tries >= 500000)
            {
                switch_tty(1);
                printk("pcache: busy page dev 0x%x, ino 0x%x, flags 0x%x, pid %d, curpid %d\n", key.dev, key.ino, pcache->flags, pcache->pid, this_core->cur_task ? this_core->cur_task->pid : 0);
                printk("pcache: refs %d\n", get_frame_shares(pcache->phys));
                //printk("pcache: holding task 0x%lx\n", pcache->pid ? get_task_by_id(pcache->pid) : NULL);
                kpanic("pcache: infinite loop\n");
            }

            //block_task(pcache, 0);
            block_task2(pcache, 300);
            goto loop;
        }

        __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_BUSY);
        inc_frame_shares(pcache->phys);
        pcache->last_accessed = ticks;
        pcache->pid = this_core->cur_task ? this_core->cur_task->pid : 0;
        kernel_mutex_unlock(&pcachetab_lock);
        __asm__ __volatile__("":::"memory");

        return pcache;
    }

    if(flags & PCACHE_PEEK_ONLY)
    {
        kernel_mutex_unlock(&pcachetab_lock);
        return NULL;
    }

    // page not found, allocate a new page cache entry
    if(!(pcache = kmalloc(sizeof(struct cached_page_t))) ||
       !(pkey = kmalloc(sizeof(struct pcache_key_t))))
    {
        kpanic("Cannot allocate page cache entry (1)\n");
    }
    
    //A_memset(pcache, 0, sizeof(struct cached_page_t));
    //A_memset(pkey, 0, sizeof(struct pcache_key_t));
    FILL_ZEROES(pcache, sizeof(struct cached_page_t));
    FILL_ZEROES(pkey, sizeof(struct pcache_key_t));

    // The node passed to us might be a struct fs_node_header_t, which is
    // not a complete node. So get the node struct in all cases to ensure
    // we have a proper node reference.
    if(node->inode != PCACHE_NOINODE)
    {
        pcache->node = node;
        //INC_NODE_REFS(node);
        __sync_fetch_and_add(&node->refs, 1);

        /*
        if(node->links == 0)
        {
            __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_STALE);
        }
        */
    }

    pcache->dev = node->dev;
    pcache->ino = node->inode;
    pcache->offset = offset;
    //pcache->refs = 1;
    __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_BUSY);
    pcache->pid = this_core->cur_task ? this_core->cur_task->pid : 0;

    pkey->dev = node->dev;
    pkey->ino = node->inode;
    pkey->offset = offset;

    if(!(hitem = pcache_alloc_hitem(pkey, pcache)))
    {
        kpanic("Cannot allocate page cache entry (2)\n");
    }

    pcache_add_hitem(pcachetab, pkey, (struct hashtab_item_t *)hitem);
    kernel_mutex_unlock(&pcachetab_lock);

    // get a physical page and map it to kernel virtual space
    while(get_next_addr(&pcache->phys, &pcache->virt, 
                        PTE_FLAGS_PW, REGION_PCACHE) != 0)
    {
        kpanic("pcache: failed to allocate memory, retrying in 10 secs\n");
        block_task2(pcache, PIT_FREQUENCY * 10);
    }

    if(pcache->virt < PCACHE_MEM_START || pcache->virt >= PCACHE_MEM_END)
    {
        kpanic("pcache: got an invalid pcache address\n");
    }

    inc_frame_shares(pcache->phys);

    if((d = get_mount_info(node->dev)) == NULL)
    {
        free_cached_page(pkey, pcache);
        printk("pcache: reading from unmounted device!\n");
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
        kpanic("pcache: accessing device with blk size == 0!\n");
        return NULL;
    }

    // If reading from a node, check the disk has some free blocks before
    // reading the page, in case we needed to alloc blocks later on when we
    // sync the page. We skip the check for readonly filesystems, e.g. iso9660.
    // We don't check if we are reading a direct disk block as there are used
    // and managed by their respective filesystem driver and are typically
    // used to manage metadata.
    if(node->inode != PCACHE_NOINODE &&
       !(d->mountflags & MS_RDONLY) && 
       d->fs->ops->ustat && d->fs->ops->ustat(d, &ubuf) == 0)
    {
        if(ubuf.f_tfree < (PAGE_SIZE / d->block_size))
        {
            free_cached_page(pkey, pcache);
            __asm__ __volatile__("xchg %%bx, %%bx":::);
            printk("pcache: device has no space left (dev 0x%x, free %d)!\n", 
                    node->dev, ubuf.f_tfree);
            return NULL;
        }
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
        //MAY_LOCK(&pcache->node->lock);
        kernel_mutex_lock(&pcache->node->lock);

        //printk("get_cached_page: dev 0x%x, node 0x%x, node @ 0x%lx, bmap @ 0x%lx\n", pcache->node->dev, pcache->node->inode, node, d->fs->ops->bmap);

        // Find out the mapping of the logical sectors we need to read
        for(i = 0; i < n; i++)
        {
            disk_block[i] = d->fs->ops->bmap(pcache->node, block + i, 
                                             d->block_size, bmap_flag);
            //printk("get_cached_page: [%d/%d] %d = %d\n", i, n, block + i, disk_block[i]);
        }

        //printk("get_cached_page: done\n");
        //printk("get_cached_page: 1 lock 0x%lx, me 0x%lx, holder 0x%lx\n", &pcache->node->lock, this_core->cur_task, pcache->node->lock.holder);

        //MAY_UNLOCK(&pcache->node->lock);
        kernel_mutex_unlock(&pcache->node->lock);

        //printk("get_cached_page: 2 lock 0x%lx, me 0x%lx, holder 0x%lx\n", &pcache->node->lock, this_core->cur_task, pcache->node->lock.holder);

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

    if(res < PAGE_SIZE)
    {
        A_memset((void *)(pcache->virt + res), 0, PAGE_SIZE - res);
    }

    inc_frame_shares(pcache->phys);
    //mark_as_clean(pcache);
    pcache->last_accessed = ticks;
    __asm__ __volatile__("":::"memory");

    return pcache;
}


int sync_cached_page(struct cached_page_t *pcache)
{
    struct mount_info_t *d;
    struct disk_req_t req;
    int maj = MAJOR(pcache->dev);
    int res = 0;

    KDEBUG("sync_cached_page: dev 0x%x, inode 0x%x, offset %lx (task %d)\n", pcache->dev, pcache->ino, pcache->offset, pcache->pid);

    /*
    if(pcache->flags & PCACHE_FLAG_STALE)
    {
        return -EIO;
    }
    */

    if((d = get_mount_info(pcache->dev)) == NULL)
    {
        printk("pcache: writing to unmounted device!\n");
        return -EIO;
    }

    // no point in trying to write to a readonly filesystem (e.g. iso9660),
    // but don't return an errno as we don't want to mark the page as stale
    if(d->mountflags & MS_RDONLY)
    {
        return 0;
    }

    if(pcache->ino == PCACHE_NOINODE)
    {
        if(pcache->flags & PCACHE_FLAG_STALE)
        {
            return -EIO;
        }

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
        //struct fs_node_t *node;
        size_t block;
        int i, n;
        virtual_addr p;

        if(pcache->node == NULL /* || pcache->node->links == 0 */)
        {
            return -EIO;
        }

        if(pcache->node->refs == 0)
        {
            switch_tty(1);
            printk("\n\n*** dev 0x%x, n 0x%x, refs %d, flags 0x%x, ptr 0x%lx\n", 
                    pcache->node->dev, pcache->node->inode, pcache->node->refs, 
                    pcache->node->flags, pcache->node);
            kpanic("\n\n*** pcache with 0 node refs!\n\n");
        }
#if 0
        if(!(node = get_node(pcache->dev, pcache->ino, 
                               GETNODE_FOLLOW_MPOINTS /* | GETNODE_IGNORE_STALE */)))
        {
            /*
            printk("sync_cached_page: dev 0x%x, inode 0x%x\n", pcache->dev, pcache->ino);
            printk("sync_cached_page: len 0x%x, flags 0x%x\n", pcache->len, pcache->flags);
            kpanic("\nsync_cached_page: invalid inode\n\n");
            */
            return -EIO;
        }
#endif

        // before we lock the node and call bmap, make sure we are not
        // trying to recursively lock the node
        if(pcache->node->lock.holder && pcache->node->lock.holder == this_core->cur_task)
        {
            return -EAGAIN;
        }

        block = pcache->offset / d->block_size;
        n = pcache->len /* PAGE_SIZE */ / d->block_size;
        p = pcache->virt;

        size_t disk_block[n];
        size_t off = pcache->offset;
        int bmap_flag, how_many = 1;

        //MAY_LOCK(&pcache->node->lock);
        kernel_mutex_lock(&pcache->node->lock);

        // Find out the mapping of the logical sectors we need to read
        for(i = 0; i < n; i++)
        {
            bmap_flag = (off < pcache->node->size) ? BMAP_FLAG_CREATE :
                                                     BMAP_FLAG_NONE;
            disk_block[i] = d->fs->ops->bmap(pcache->node, block + i, 
                                             d->block_size, bmap_flag);
            off += d->block_size;
            //printk("sync_cached_page: [%d] %d\n", i, disk_block[i]);
        }

        //MAY_UNLOCK(&pcache->node->lock);
        kernel_mutex_unlock(&pcache->node->lock);

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
            req.dev = pcache->dev;
            req.data = p;
            req.datasz = d->block_size * how_many;
            req.fs_blocksz = d->block_size;
            req.blockno = disk_block[0];
            req.write = 1;

            if(bdev_tab[maj].strategy(&req) < 0)
            {
                //release_node(node);
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

        //release_node(node);
    }

    return res;
}


static void mark_dirty_pages(int maj)
{
    struct cached_page_t *pcache;
    volatile struct hashtab_item_t *hitem;
    volatile int i;

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

            if(pcache->flags & PCACHE_FLAG_ALWAYS_DIRTY)
            {
                __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_DIRTY);
            }

            hitem = hitem->next;
        }
    }

    kernel_mutex_unlock(&pcachetab_lock);
}


static inline void release_pcache_internal(volatile struct hashtab_item_t *hitem,
                                           volatile struct hashtab_item_t *prev, int i)
{
    struct cached_page_t *pcache = hitem->val;

    /*
    if(get_frame_shares(pcache->phys) > 1)
    {
        //__asm__ __volatile__("xchg %%bx, %%bx":::);
        printk("pcache: trying to release page with %d refs\n", get_frame_shares(pcache->phys));
        return;
    }
    */

    if(prev)
    {
        prev->next = hitem->next;
    }
    else
    {
        pcachetab->items[i] = hitem->next;
    }

    kfree(hitem->key);
    kfree((void *)hitem);
    kernel_mutex_unlock(&pcachetab_lock);
    release_page_memory(pcache);
    kernel_mutex_lock(&pcachetab_lock);
    __asm__ __volatile__("":::"memory");
}


static void flush_dirty_pages(int maj)
{
    struct cached_page_t *pcache;
    volatile struct hashtab_item_t *hitem /* , *prev */;
    int res, wanted;
    volatile int i;

    kernel_mutex_lock(&pcachetab_lock);
    
    for(i = 0; i < pcachetab->count; i++)
    {

loop:

        hitem = pcachetab->items[i];
        //prev = NULL;
        
        while(hitem)
        {
            pcache = hitem->val;

            if(maj != -1 && (int)MAJOR(pcache->dev) != maj)
            {
                //prev = hitem;
                hitem = hitem->next;
                continue;
            }

            if(!(pcache->flags & PCACHE_FLAG_DIRTY) /* ||
               (pcache->flags & PCACHE_FLAG_STALE) */)
            {
                //prev = hitem;
                hitem = hitem->next;
                continue;
            }

            if(pcache->flags & PCACHE_FLAG_BUSY &&
               pcache->pid != this_core->cur_task->pid)
            {
                __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_WANTED);
                kernel_mutex_unlock(&pcachetab_lock);
                block_task2(pcache, 30);
                kernel_mutex_lock(&pcachetab_lock);
                hitem = pcachetab->items[i];
                //prev = NULL;
                continue;
            }

            __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_BUSY);
            __sync_and_and_fetch(&pcache->flags, ~PCACHE_FLAG_DIRTY);
            pcache->pid = -1; //this_core->cur_task->pid;
            pcache->last_accessed = ticks;

            kernel_mutex_unlock(&pcachetab_lock);
            res = sync_cached_page(pcache);
            kernel_mutex_lock(&pcachetab_lock);

            wanted = (pcache->flags & PCACHE_FLAG_WANTED);
            __sync_and_and_fetch(&pcache->flags, 
                    ~(PCACHE_FLAG_BUSY /* | PCACHE_FLAG_DIRTY */ | PCACHE_FLAG_WANTED));

            // If the node is locked, sync_cached_page() returns -EAGAIN so
            // we can flush the page the next round. If we turn the 
            // PCACHE_FLAG_DIRTY flag, we will loop forever as we jump to 
            // the beginning of the loop. If we try to continue the loop from
            // this point, we may walk down a corrup cache as we unlocked the
            // cache mutex while we updated the page. The best thing here is
            // to turn the PCACHE_FLAG_ALWAYS_DIRTY flag and leave the page for
            // now. The worst case scenario is that a few pages, linked to the
            // locked node, will hang around the cache for some time until the
            // node is unlocked and they get flushed.
            if(res == -EAGAIN)
            {
                __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_ALWAYS_DIRTY);
            }
            else if(res < 0)
            {
                __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_STALE);
            }

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


void remove_old_cached_pages(int maj, unsigned long long older_than_ticks)
{
    struct cached_page_t *pcache;
    volatile struct hashtab_item_t *hitem, *prev;
    unsigned long long older_than = ticks - older_than_ticks;
    volatile int i;

    // First, get a list of all the dirty pages and mark them as both
    // dirty and busy, so no one else can claim them.
    // Next, flush dirty pages to disk.
    mark_dirty_pages(maj);
    flush_dirty_pages(maj);

    // check that the given time have passed since booting
    if(ticks <= older_than_ticks)
    {
        return;
    }

    kernel_mutex_lock(&pcachetab_lock);

    for(i = 0; i < pcachetab->count; i++)
    {

loop:

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

            // remove the page if it is old and no one is using it and it is
            // not dirty (although it should not be dirty for 5 mins as the
            // periodic updater should have flushed it to disk earlier)
            if((pcache->last_accessed < older_than) &&
               !(pcache->flags & (PCACHE_FLAG_BUSY | PCACHE_FLAG_WANTED | PCACHE_FLAG_DIRTY)))
            {
                if(get_frame_shares(pcache->phys) <= 1)
                {
                    release_pcache_internal(hitem, prev, i);
                    goto loop;
                }
            }

            prev = hitem;
            hitem = hitem->next;
        }
    }

    kernel_mutex_unlock(&pcachetab_lock);
}


void remove_stale_cached_pages(void)
{
    struct cached_page_t *pcache;
    volatile struct hashtab_item_t *hitem, *prev;
    volatile int i;

    kernel_mutex_lock(&pcachetab_lock);

    for(i = 0; i < pcachetab->count; i++)
    {

loop:

        hitem = pcachetab->items[i];
        prev = NULL;
        
        while(hitem)
        {
            pcache = hitem->val;

            if((pcache->flags & PCACHE_FLAG_STALE) &&
               !(pcache->flags & (PCACHE_FLAG_BUSY | PCACHE_FLAG_WANTED)))
            {
                //__asm__ __volatile__("xchg %%bx, %%bx":::);
                if(get_frame_shares(pcache->phys) <= 1)
                {
                    release_pcache_internal(hitem, prev, i);
                    goto loop;
                }
            }

            prev = hitem;
            hitem = hitem->next;
        }
    }

    kernel_mutex_unlock(&pcachetab_lock);
}


void remove_unreferenced_cached_pages(struct fs_node_t *node)
{
    struct cached_page_t *pcache;
    volatile struct hashtab_item_t *hitem, *prev;
    volatile int i;

    mark_dirty_pages(-1);
    flush_dirty_pages(-1);

    kernel_mutex_lock(&pcachetab_lock);

    for(i = 0; i < pcachetab->count; i++)
    {

loop:

        hitem = pcachetab->items[i];
        prev = NULL;
        
        while(hitem)
        {
            pcache = hitem->val;

            if(node == NULL ||
               (pcache->dev == node->dev && pcache->ino == node->inode))
            {
                // remove the page if no one is using it and it is not dirty
                if(!(pcache->flags & (PCACHE_FLAG_BUSY | PCACHE_FLAG_WANTED | PCACHE_FLAG_DIRTY)))
                {
                    if(get_frame_shares(pcache->phys) <= 1)
                    {
                        release_pcache_internal(hitem, prev, i);
                        goto loop;
                    }
                }
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

    remove_old_cached_pages(maj, TWO_MINUTES);
    remove_stale_cached_pages();
}


int remove_cached_disk_pages(dev_t dev)
{
    struct cached_page_t *pcache;
    volatile struct hashtab_item_t *hitem, *prev;
    int res = 0;
    volatile int i;

    kernel_mutex_lock(&pcachetab_lock);

    for(i = 0; i < pcachetab->count; i++)
    {

loop:

        hitem = pcachetab->items[i];
        prev = NULL;
        
        while(hitem)
        {
            pcache = hitem->val;

            if(pcache->dev == dev)
            {
                __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_STALE);

                // remove the page if no one is using it
                if(!(pcache->flags & (PCACHE_FLAG_BUSY | PCACHE_FLAG_WANTED)))
                {
                    if(get_frame_shares(pcache->phys) <= 1)
                    {
                        release_pcache_internal(hitem, prev, i);
                        goto loop;
                    }
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
    volatile struct hashtab_item_t *hitem, *prev;
    volatile int i;
    int res = 0;

    if(!node || node->dev == 0 || node->inode == 0)
    {
        return -EINVAL;
    }

    kernel_mutex_lock(&pcachetab_lock);

    for(i = 0; i < pcachetab->count; i++)
    {

loop:

        hitem = pcachetab->items[i];
        prev = NULL;
        
        while(hitem)
        {
            pcache = hitem->val;

            if(pcache->dev == node->dev && pcache->ino == node->inode)
            {
                __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_STALE);

                // As the node is getting deleted, we need to remove all its
                // cached pages. If a page is being used, we have to wait until
                // it is released then we remove it. If no one is using it,
                // remove the page immediately
                if(!(pcache->flags & (PCACHE_FLAG_BUSY | PCACHE_FLAG_WANTED)))
                {
                    if(get_frame_shares(pcache->phys) <= 1)
                    {
                        release_pcache_internal(hitem, prev, i);
                        goto loop;
                    }
                    //else printk("cannot remove page: 0x%x:0x%x: shares %d, flags 0x%x (1)\n", pcache->dev, pcache->ino, get_frame_shares(pcache->phys), pcache->flags);
                }
                //else printk("cannot remove page: 0x%x:0x%x: shares %d, flags 0x%x (2)\n", pcache->dev, pcache->ino, get_frame_shares(pcache->phys), pcache->flags);

                res = -EBUSY;
            }

            prev = hitem;
            hitem = hitem->next;
        }
    }

    kernel_mutex_unlock(&pcachetab_lock);
    return res;
    //return 0;
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
 * Similar to the above, but returns only busy pages.
 */
size_t get_busy_cached_page_count(void)
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

            if((pcache->ino != PCACHE_NOINODE) &&
               (pcache->flags & PCACHE_FLAG_BUSY))
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


/*
 * Similar to the above, but returns only busy pages.
 */
size_t get_busy_cached_block_count(void)
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

            if((pcache->ino == PCACHE_NOINODE) &&
               (pcache->flags & PCACHE_FLAG_BUSY))
            {
                count++;
            }

            hitem = hitem->next;
        }
    }
    
    kernel_mutex_unlock(&pcachetab_lock);
    return count;
}


size_t get_wanted_cached_block_count(void)
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

            if((pcache->flags & PCACHE_FLAG_WANTED))
            {
                count++;
            }

            hitem = hitem->next;
        }
    }
    
    kernel_mutex_unlock(&pcachetab_lock);
    return count;
}


size_t get_dirty_cached_block_count(void)
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

            if((pcache->flags & PCACHE_FLAG_DIRTY))
            {
                count++;
            }

            hitem = hitem->next;
        }
    }
    
    kernel_mutex_unlock(&pcachetab_lock);
    return count;
}


void print_cache_stats(void)
{
    struct cached_page_t *pcache;
    struct hashtab_item_t *hitem;
    size_t total = 0, busy = 0, unref = 0, dirty = 0, wanted = 0;
    int i;

    kernel_mutex_lock(&pcachetab_lock);

    for(i = 0; i < pcachetab->count; i++)
    {
        hitem = pcachetab->items[i];
        
        while(hitem)
        {
            pcache = hitem->val;

            if((pcache->flags & PCACHE_FLAG_DIRTY))
            {
                dirty++;
            }

            if((pcache->flags & PCACHE_FLAG_BUSY))
            {
                busy++;
            }

            if((pcache->flags & PCACHE_FLAG_WANTED))
            {
                wanted++;
            }

            if(get_frame_shares(pcache->phys) <= 1)
            {
                unref++;
            }

            total++;
            hitem = hitem->next;
        }
    }
    
    kernel_mutex_unlock(&pcachetab_lock);

    printk("\ntotal %ld, dirty %ld, busy %ld, unref %ld, wanted %ld\n", total, dirty, busy, unref, wanted);
}


long node_has_cached_pages(struct fs_node_t *node)
{
    struct cached_page_t *pcache;
    struct hashtab_item_t *hitem;
    long i, refs = 0;

    if(!node)
    {
        return 0;
    }

    kernel_mutex_lock(&pcachetab_lock);

    for(i = 0; i < pcachetab->count; i++)
    {
        hitem = pcachetab->items[i];
        
        while(hitem)
        {
            pcache = hitem->val;

            if(pcache->dev == node->dev && pcache->ino == node->inode)
            {
                //kernel_mutex_unlock(&pcachetab_lock);
                //return 1;
                refs++;
            }

            hitem = hitem->next;
        }
    }
    
    kernel_mutex_unlock(&pcachetab_lock);
    return refs;
}

