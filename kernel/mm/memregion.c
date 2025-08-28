/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: memregion.c
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
 *  \file memregion.c
 *
 *  Functions for working with task memory regions.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <mm/mmap.h>
#include <mm/memregion.h>
#include <mm/kheap.h>
#include <mm/kstack.h>
#include <mm/mmngr_virtual.h>
#include <kernel/laylaos.h>
#include <kernel/pcache.h>
#include <kernel/ipc.h>
#include <kernel/dev.h>


struct memregion_t *memregion_freelist_head = NULL;
struct memregion_t *memregion_freelist_tail = NULL;
volatile struct kernel_mutex_t memregion_freelist_mutex;

static long msync_internal(struct memregion_t *memregion, 
                           size_t sz, int flags);


/* 
 * Add a freed region struct to the free list.
 */
static void memregion_add_free(struct memregion_t *memregion)
{
    if(!memregion)
    {
        return;
    }
    
    memregion->next = NULL;
    memregion->prev = NULL;

    if(memregion->refs == 0 && memregion->inode)
    {
        struct fs_node_t *node = memregion->inode;

        memregion->inode = NULL;
        release_node(node);
        memregion->fpos = 0;
        memregion->flen = 0;
    }
    
    kernel_mutex_lock(&memregion_freelist_mutex);

    if(memregion_freelist_tail)
    {
        memregion_freelist_tail->next_free = memregion;
        memregion_freelist_tail            = memregion;
    }
    else
    {
        memregion_freelist_head            = memregion;
        memregion_freelist_tail            = memregion;
        memregion_freelist_tail->next_free = NULL     ;
    }

    kernel_mutex_unlock(&memregion_freelist_mutex);
}


/*
 * Get a free memregion struct for use. If the free list is empty, a new
 * memregion struct is allocated.
 *
 * Returns a pointer to a memregion struct, or NULL if out of memory.
 */
static struct memregion_t *memregion_first_free(void)
{
    struct memregion_t *memregion = NULL;
    
    if(memregion_freelist_head)
    {
        kernel_mutex_lock(&memregion_freelist_mutex);

        memregion = memregion_freelist_head;
        memregion_freelist_head = memregion_freelist_head->next_free;

        /* empty list? */
        if(!memregion_freelist_head)
        {
            memregion_freelist_tail = NULL;
        }

        kernel_mutex_unlock(&memregion_freelist_mutex);
    }
    else
    {
        memregion = kmalloc(sizeof(struct memregion_t));
    }
    
    if(memregion)
    {
        A_memset(memregion, 0, sizeof(struct memregion_t));
        init_kernel_mutex(&memregion->mutex);
    }

    return memregion;
}


/* Helper function */
static inline
void memregion_insert_leftto(struct task_t *task,
                             struct memregion_t *memregion,
                             struct memregion_t *leftto)
{
    memregion->prev = leftto->prev;
    memregion->next = leftto;
    leftto->prev = memregion;
    
    if(memregion->prev)
    {
        memregion->prev->next = memregion;
    }
    else
    {
        task->mem->first_region = memregion;
    }
}


/* Helper function */
static inline
void memregion_insert_rightto(struct task_t *task,
                              struct memregion_t *memregion,
                              struct memregion_t *rightto)
{
    memregion->prev = rightto;
    memregion->next = rightto->next;
    rightto->next = memregion;
    
    if(memregion->next)
    {
        memregion->next->prev = memregion;
    }
    else
    {
        task->mem->last_region = memregion;
    }
}


/*
 * Helper function. The task's mem struct should be locked by the caller.
 *
 * Allocate a new memregion struct with the given address range, protection,
 * type and inode. The new struct is inserted into the task's memregion list,
 * either before 'leftto' or after 'rightto'. Hence, the caller should EITHER
 * pass 'leftto' or 'rightto', but NOT BOTH.
 *
 * Returns:
 *   0 on success, -errno on failure.
 *   The newly allocated memregion is returned in the 'res' field.
 */
static inline
long alloc_and_insert(struct task_t *task, struct fs_node_t *inode,
                      virtual_addr start, virtual_addr end,
                      int prot, int type, int flags,
                      struct memregion_t *leftto, struct memregion_t *rightto,
                      struct memregion_t **res)
{
    struct memregion_t *memregion;
    long err;

    *res = NULL;

    if((err = memregion_alloc(inode, prot, type, flags, &memregion)) != 0)
    {
        return err;
    }
    
    memregion->addr = start;
    memregion->size = (end - start) / PAGE_SIZE;
    //memregion->refs++;
    __sync_fetch_and_add(&memregion->refs, 1);
    memregion->prev = NULL;
    memregion->next = NULL;
    
    if(leftto)
    {
        memregion_insert_leftto(task, memregion, leftto);
    }
    else if(rightto)
    {
        memregion_insert_rightto(task, memregion, rightto);
    }
    
    *res = memregion;
    return 0;
}


/*
 * Allocate a new memregion struct with the given address range, protection,
 * type, flags and inode. The new struct is inserted into the task's memregion
 * list. If 'remove_overlaps' is non-zero, overlapping memory maps are removed
 * automatically.
 *
 * NOTE: The task's mem struct need not be locked by the caller, as we only get
 *       called by syscall_execve() and ELF loader, as well as when initialising
 *       tasking on system startup.
 *
 * Returns:
 *   0 on success, -errno on failure.
 */
long memregion_alloc_and_attach(struct task_t *task, struct fs_node_t *inode,
                                off_t fpos, off_t flen,
                                virtual_addr start, virtual_addr end,
                                int prot, int type, int flags,
                                int remove_overlaps)
{
    struct memregion_t *memregion = NULL;
    long err;

    if((err = memregion_alloc(inode, prot, type, flags, &memregion)) != 0)
    {
        return err;
    }

    memregion->addr = start;
    memregion->size = (end - start) / PAGE_SIZE;
    ////memregion->refs++;
    memregion->fpos = fpos;
    memregion->flen = flen;
    memregion->prev = NULL;
    memregion->next = NULL;

    return memregion_attach(task, memregion, start, 
                            memregion->size, remove_overlaps);
}


/*
 * Change the protection bits of a memory address range.
 * The target address range could be part of a wider memory region, in which
 * case we split the region into smaller regions and change the protection
 * bits of the desired address range only. If 'detach' is set, the
 * address range is actually detached from the task's memory map instead
 * of changing its protection bits.
 *
 * NOTE: The task's mem struct should be locked by the caller.
 *       This function is called by syscall_unmap() and syscall_mprotect().
 *
 * Returns:
 *   0 on success, -errno on failure.
 */
long memregion_change_prot(struct task_t *task,
                           virtual_addr start, virtual_addr end,
                           int prot, int detach)
{
    struct memregion_t *tmp, *memregion = task->mem->first_region;
    //size_t sz = (end - start);
    //size_t pages = sz / PAGE_SIZE;
    int found = 0;
    int flags = 0;
    int break_loop, split_left, split_right;
    //off_t fpos;
    virtual_addr start2, end2;

    // prepare page flags
    if(prot != PROT_NONE)
    {
        flags = I86_PTE_PRESENT |
                ((prot & PROT_WRITE) ? I86_PTE_WRITABLE : 0) |
                (((start < USER_MEM_END) &&
                    (end <= USER_MEM_END)) ? I86_PTE_USER : 0);
    }

    while(memregion)
    {
        start2 = memregion->addr;
        end2 = start2 + (memregion->size * PAGE_SIZE);


        /*
         * The possible layouts could be:
         *
         * (A)
         *                   +---------+
         *             start |         | end    <== target addr
         *          +--------+--+------+
         *  start2  |           | end2          <== current memregion
         *          +-----------+
         *
         * (B)
         *               +------+
         *         start |      | end           <== target addr
         *          +----+------+
         *  start2  |           | end2          <== current memregion
         *          +-----------+
         *
         * (C)
         *          +---------+
         *    start |         | end             <== target addr
         *          +------+--+--------+
         *         start2  |           | end2   <== current memregion
         *                 +-----------+
         *
         * (D)
         *          +------+
         *    start |      | end                <== target addr
         *          +------+----+
         *  start2  |           | end2          <== current memregion
         *          +-----------+
         *
         * (E)
         *          +-----------+
         *    start |           | end           <== target addr
         *          +------+----+
         *  start2  |      | end2               <== current memregion
         *          +------+
         *
         * (F)
         *              +---------+
         *        start |         | end         <== target addr
         *          +---+---------+---+
         *  start2  |                 | end2    <== current memregion
         *          +-----------------+
         *
         * (G)
         *          +-----------------+
         *    start |                 | end     <== target addr
         *          +---+---------+---+
         *      start2  |         | end2        <== current memregion
         *              +---------+
         *
         * (H)
         *          +-----------+
         *    start |           | end           <== target addr
         *          +-----------+
         *   start2 |           | end2          <== current memregion
         *          +-----------+
         *
         * (I)
         *          +-----------+
         *    start |           | end           <== target addr
         *          +----+------+
         *       start2  |      | end2          <== current memregion
         *               +------+
         */

        // no overlap
        if(end <= start2 || start >= end2)
        {
            memregion = memregion->next;
            continue;
        }

        found = 1;
        split_left = 0;
        split_right = 0;

        if(start <= start2)
        {
            if(end == end2)
            {
                // case (H) - perfect match
                // case (I)
                // CHANGE PROT FOR WHOLE REGION AND BREAK LOOP
                break_loop = 1;
            }
            else if(end < end2)
            {
                // case (D)
                // case (C)
                // CHANGE PROT LEFT SIDE (start to end)
                // BREAK RIGHT SIDE (end to end2)
                // BREAK LOOP
                split_right = 1;
                break_loop = 1;
            }
            else    // end > end2
            {
                // case (E)
                // case (G)
                // CHANGE PROT FOR WHOLE REGION
                // CONTINUE LOOPING (end2 to end)
                break_loop = 0;
            }
        }
        else if(start > start2)
        {
            if(end == end2)
            {
                // case (B)
                // CHANGE PROT RIGHT SIDE (start to end)
                // BREAK LEFT SIDE (start2 to start)
                // BREAK LOOP
                split_left = 1;
                break_loop = 1;
            }
            else if(end < end2)
            {
                // case (F)
                // CHANGE PROT MIDDLE (start to end)
                // BREAK LEFT SIDE (start2 to start)
                // BREAK RIGHT SIDE (end to end2)
                // BREAK LOOP
                split_right = 1;
                split_left = 1;
                break_loop = 1;
            }
            else    // end > end2
            {
                // case (A)
                // CHANGE PROT MIDDLE (start to end2)
                // BREAK LEFT SIDE (start2 to start)
                // CONTINUE LOOPING (end2 to end)
                split_left = 1;
                break_loop = 0;
            }
        }
        /*
        else        // start < start2
        {
            if(end == end2)
            {
                // case (I)
                // CHANGE PROT FOR WHOLE REGION AND BREAK LOOP
            }
            else if(end < end2)
            {
                // case (C)
                // CHANGE PROT LEFT SIDE (start2 to end)
                // BREAK RIGHT SIDE (end to end2)
                // BREAK LOOP
            }
            else    // end > end2
            {
                // case (G)
                // CHANGE PROT FOR WHOLE REGION
                // CONTINUE LOOPING (end2 to end)
            }
        }
        */

        // split the left segment
        if(split_left)
        {
            if(alloc_and_insert(task, memregion->inode, start2, start,
                                memregion->prot, memregion->type, 
                                memregion->flags,
                                memregion, NULL, &tmp) != 0)
            {
                return -ENOMEM;
            }

            memregion->addr = start;
            memregion->size -= tmp->size;
            
            // adjust the newly alloc'd region's file pos
            if(memregion->inode)
            {
                tmp->fpos = memregion->fpos;
                tmp->flen = start - start2;
                memregion->fpos += tmp->flen;
            
                if(tmp->flen >= memregion->flen)
                {
                    tmp->flen = memregion->flen;
                    memregion->flen = 0;
                }
                else
                {
                    memregion->flen -= tmp->flen;
                }
            }
        }

        // split the right segment
        if(split_right)
        {
            if(alloc_and_insert(task, memregion->inode, end, end2,
                                memregion->prot, memregion->type, 
                                memregion->flags,
                                NULL, memregion, &tmp) != 0)

            {
                return -ENOMEM;
            }

            memregion->size -= tmp->size;

            // adjust the newly alloc'd region's file pos
            if(memregion->inode)
            {
                tmp->fpos = memregion->fpos + (end - start2);
                tmp->flen = end2 - end;
                memregion->flen -= tmp->flen;

                if(memregion->flen < 0)
                {
                    memregion->flen += tmp->flen;
                    tmp->flen = 0;
                }
            }
        }

        // remove the overlapped segment
        tmp = memregion->next;
        
        if(detach)
        {
            memregion_detach(task, memregion, 1);
        }
        else
        {
            uintptr_t private_flag = 
                        (memregion->flags & MEMREGION_FLAG_PRIVATE) ?
                                                        I86_PTE_PRIVATE : 0;

            memregion->prot = prot;
            vmmngr_change_page_flags(memregion->addr,
                                        (memregion->size * PAGE_SIZE),
                                            flags | private_flag);
        }

        if(break_loop)
        {
            break;
        }

        memregion = tmp;
        start = end2;
    }
    
    return found ? 0 : -EINVAL;
}


/* 
 * Allocate a new memory region struct. We try to get a memregion from
 * the free region list. If the list is empty, we try to allocate a new
 * struct (this is all done by calling memregion_first_free()).
 *
 * NOTES:
 *   - prot and type are as defined in mm/mmap.h.
 *   - falgs are as defined in mm/memregion.h.
 *   - The caller must have locked task->mem->mutex before calling us.
 *
 * Returns:
 *   0 on success, -errno on failure.
 *   The newly allocated memregion is returned in the 'res' field.
 */
long memregion_alloc(struct fs_node_t *inode,
                     int prot, int type, int flags,
                     struct memregion_t **res)
{
    *res = NULL;

    /* called during fork, exec and shmget syscalls */
    if((flags & ~ACCEPTED_MEMREGION_FLAGS))
    {
        return -EINVAL;
    }
    
    if(FLAG_SET(flags, MEMREGION_FLAG_PRIVATE) == 
                    FLAG_SET(flags, MEMREGION_FLAG_SHARED))
    {
        return -EINVAL;
    }
    
    if((type < MEMREGION_TYPE_LOWEST) || (type > MEMREGION_TYPE_HIGHEST))
    {
        return -EINVAL;
    }

    if(prot & ~VALID_PROT)
    {
        return -EINVAL;
    }
    
    struct memregion_t *reg = memregion_first_free();

    if(!reg)
    {
        return -ENOBUFS;
    }
    
    //memset(reg, 0, sizeof(struct memregion_t));
    reg->prot = prot;
    reg->type = type;
    reg->flags = flags;
    reg->inode = inode;

    if(inode)
    {
        INC_NODE_REFS(inode);
    }

    *res = reg;
    return 0;
}


/* 
 * Attach a memory region to a task.
 * Called during fork, exec, mmap and shmget syscalls.
 *
 * NOTES:
 *   - The caller should have alloc'd memregion by calling memregion_alloc().
 *   - The size argument should be in PAGE_SIZE multiples, not in bytes.
 *   - The caller must have locked task->mem->mutex before calling us.
 *
 * Returns:
 *   0 on success, -errno on failure.
 */
long memregion_attach(struct task_t *task, struct memregion_t *memregion,
                      virtual_addr attachat, size_t size, int remove_overlaps)
{
    if(!task || !memregion || !attachat)
    {
        return -EINVAL;
    }

    virtual_addr end = attachat + (size * PAGE_SIZE);

    //printk("memregion_attach: s %lx, e %lx\n", attachat, end);

    long overlaps = memregion_check_overlaps(task, attachat, end);
    long res;
    struct memregion_t *tmp;

    // If mmap() is not called with the MAP_FIXED flag, we don't remove
    // overlapping mappings.
    if(overlaps)
    {
        if(!remove_overlaps)
        {
            //printk("memregion_attach: overlaps found\n");
            return -EEXIST;
        }

        if((res = memregion_remove_overlaps(task, attachat, end)) != 0)
        {
            //printk("memregion_attach: cannot remove overlaps\n");
            return res;
        }
    }

    memregion->addr = attachat;
    memregion->size = size;
    //memregion->refs++;
    __sync_fetch_and_add(&memregion->refs, 1);
    
    if(task->mem->first_region == NULL)
    {
        //printk("memregion_attach: inserting first\n");
        task->mem->first_region = memregion;
        task->mem->last_region = memregion;
    }
    else
    {
        for(tmp = task->mem->first_region; tmp != NULL; tmp = tmp->next)
        {
            if(tmp->addr < attachat && tmp->next)
            {
                continue;
            }

            if(tmp->addr < attachat)
            {
                memregion_insert_rightto(task, memregion, tmp);
            }
            else
            {
                memregion_insert_leftto(task, memregion, tmp);
            }
            
            break;
        }
        
        if(tmp == NULL)
        {
            kpanic("cannot add memregion to task (in memregion_attach)\n");
        }

        /*
        for(tmp = task->mem->first_region; tmp != NULL; tmp = tmp->next)
        {
            printk("memregion_attach: tmp->addr %lx\n", tmp->addr);
        }
        */
    }
    
    //screen_refresh(NULL);

    task->image_size += memregion->size;

    return 0;
}


/* 
 * Release an alloc'd memregion struct and release its inode (if != NULL).
 */
void memregion_free(struct memregion_t *memregion)
{
    /* add region to free list */
    memregion_add_free(memregion);
}


/*
 * Helper function. The task's mem struct should be locked by the caller.
 *
 * Remove a memregion struct from the task's memregion list and free 
 * the struct.
 */
static void memregion_detach_from_task(struct task_t *task, 
                                       struct memregion_t *memregion)
{
    if(memregion->prev)
    {
        memregion->prev->next = memregion->next;
    }
    else
    {
        task->mem->first_region = memregion->next;
    }

    if(memregion->next)
    {
        memregion->next->prev = memregion->prev;
    }
    else
    {
        task->mem->last_region = memregion->prev;
    }
}


/*
 * Handler for syscall msync().
 */
long syscall_msync(void *addr, size_t length, int flags)
{
    virtual_addr aligned_size, end;
	volatile struct task_t *ct = this_core->cur_task;
    struct memregion_t *memregion;
    int sync = (flags & MS_SYNC);
    int async = (flags & MS_ASYNC);

    // check addr is aligned
    if(!PAGE_ALIGNED(addr))
    {
        return -EINVAL;
    }
    
    // check the length
    if(length == 0)
    {
        return -EINVAL;
    }
    
    if(sync == async)
    {
        return -EINVAL;
    }

    aligned_size = align_up((virtual_addr)length);
    end = (virtual_addr)addr + aligned_size;

    // check we're not trying to unmap kernel memory
    if(((size_t)addr >= USER_MEM_END) ||
                (end > USER_MEM_END))
    {
        return -EINVAL;
    }

    if((memregion = memregion_containing(ct, (virtual_addr)addr)) == NULL)
    {
        return -ENOMEM;
    }
    
    /*
     * NOTE: This is not entirely accurate. We update the memory region
     *       from the start to the end of the requested area. What we should
     *       be doing is to update only the requested area from addr to
     *       addr + aligned_size.
     *
     * TODO: fix this.
     */
    aligned_size = end - memregion->addr;

    return msync_internal(memregion, aligned_size, flags);
}


static long msync_internal(struct memregion_t *memregion, size_t sz, int flags)
{
    UNUSED(flags);

    size_t write_size, file_end, mem_end;
    off_t file_pos;

    if((memregion->flags & MEMREGION_FLAG_SHARED) &&
       (memregion->flags & MEMREGION_FLAG_USER) &&
        memregion->inode)
    {
        virtual_addr laddr = memregion->addr + sz;
        virtual_addr i = memregion->addr;

        // where the file-backed memmapped region ends
        mem_end = memregion->fpos + memregion->flen;
        
        while(i < laddr)
        {
            pt_entry *page = get_page_entry((void *)i);
      
            if(!page || !PTE_PRESENT(*page) || 
                        !PTE_WRITABLE(*page) || !PTE_DIRTY(*page))
            {
                i += PAGE_SIZE;
                continue;
            }

            // where to write to in the file
            file_pos = memregion->fpos + (i - memregion->addr);
            // where to stop writing in the file
            file_end = file_pos + PAGE_SIZE;
        
            // don't write past the mmaped region in the file
            if(file_end > mem_end)
            {
                write_size = (file_end - mem_end);

                if(write_size >= PAGE_SIZE)
                {
                    //write_size = 0;
                    i += PAGE_SIZE;
                    continue;
                }
                else
                {
                    write_size = PAGE_SIZE - write_size;
                }
            }
            else
            {
                write_size = PAGE_SIZE;
            }

            vfs_write_node(memregion->inode, &file_pos, (unsigned char *)i, 
                           write_size, 0);
            PTE_DEL_ATTRIB(page, I86_PTE_DIRTY);
            vmmngr_flush_tlb_entry(i);

            i += PAGE_SIZE;
        }
    }
    
    return 0;
}


/* 
 * Detach a memory region from a task and add it to the free region list.
 * If the region was mmap-ed from a file, dirty pages are written back to
 * the file. If 'free_pages' is non-zero, the physical memory pages are
 * released.
 *
 * NOTES:
 *   - The caller must have locked task->mem->mutex before calling us.
 *
 * Returns:
 *   0 on success, -errno on failure.
 */
long memregion_detach(struct task_t *task, struct memregion_t *memregion,
                      int free_pages)
{
    if(!task || !memregion)
    {
        return -EINVAL;
    }
    
    size_t sz = memregion->size * PAGE_SIZE;
    long res;

    // don't remove shared memory mappings if this task was vforked, as the
    // parent will essentially be stuffed
    if(!(task->properties & PROPERTY_VFORK))
    {
        if(memregion->type == MEMREGION_TYPE_SHMEM)
        {
            if((res = shmdt_internal(task, memregion, 
                                        (void *)memregion->addr)) < 0)
            {
                return res;
            }
        }
        else
        {
            msync_internal(memregion, sz, MS_SYNC);
        }
    }

    // detach region from task
    memregion_detach_from_task(task, memregion);
    
    // release memory
    if(free_pages)
    {
        vmmngr_free_pages(memregion->addr, sz);
    }

    task->image_size -= memregion->size;
    
    // add region to free list
    //memregion->refs--;
    __sync_fetch_and_sub(&memregion->refs, 1);
    memregion_add_free(memregion);

    return 0;
}


/*
 * Detach (and possibly free pages used by) a user-allocated memory regions.
 * Called during exec(), as well when a task terminates (if all threads
 * are dead). If 'free_pages' is non-zero, the physical memory pages are
 * released.
 */
void memregion_detach_user(struct task_t *task, int free_pages)
{
    struct memregion_t *tmp, *next;
    
    for(tmp = task->mem->first_region; tmp != NULL; )
    {
        next = tmp->next;
        
        if(tmp->type != MEMREGION_TYPE_KERNEL)
        //if(tmp->flags & MEMREGION_FLAG_USER)
        {
            memregion_detach(task, tmp, free_pages);
        }
        
        tmp = next;
    }
}


/*
 * Duplicate task memory map, making a copy of all its memory regions.
 * Called during fork().
 *
 * NOTES:
 *   - The caller must have locked mem->mutex before calling us.
 *
 * Returns:
 *   the memory map copy on success, NULL on failure.
 */
struct task_vm_t *task_mem_dup(struct task_vm_t *mem)
{
    struct task_vm_t *copy;
    struct memregion_t *memregion, *prev = NULL, *tmp = NULL;
    
    if(!mem)
    {
        return NULL;
    }
    
    if(!(copy = kmalloc(sizeof(struct task_vm_t))))
    {
        return NULL;
    }
    
    A_memset(copy, 0, sizeof(struct task_vm_t));
    init_kernel_mutex(&copy->mutex);

    for(memregion = mem->first_region; 
        memregion != NULL; 
        memregion = memregion->next)
    {
        if(!(tmp = kmalloc(sizeof(struct memregion_t))))
        {
bailout:
            // roll back everything
            for(memregion = copy->first_region; memregion != NULL; )
            {
                if(memregion->inode)
                {
                    struct fs_node_t *node = memregion->inode;

                    memregion->inode = NULL;
                    release_node(node);
                }

                tmp = memregion->next;
                kfree(memregion);
                memregion = tmp;
            }
            
            kfree(copy);

            return NULL;
        }

        if(memregion->type == MEMREGION_TYPE_SHMEM)
        {
            if(shmat_internal((struct task_t *)this_core->cur_task, memregion,
                                        (void *)memregion->addr) < 0)
            {
                kfree(tmp);
                goto bailout;
            }
        }
        
        A_memcpy(tmp, memregion, sizeof(struct memregion_t));
        tmp->refs = 1;
        tmp->next = NULL;
        tmp->prev = prev;

        if(tmp->inode)
        {
            INC_NODE_REFS(tmp->inode);
        }
        
        if(memregion == mem->first_region)
        {
            copy->first_region = tmp;
        }
        
        if(prev)
        {
            prev->next = tmp;
        }
        
        prev = tmp;
    }
    
    copy->last_region = tmp;

    copy->__image_size = mem->__image_size;
    copy->__end_data = mem->__end_data;
    copy->__end_stack = mem->__end_stack;
    copy->__base_addr = mem->__base_addr;
    copy->vdso_code_start = mem->vdso_code_start;

    return copy;
}


void task_mem_free(struct task_vm_t *mem)
{
    struct memregion_t *memregion, *next = NULL;
    
    if(!mem)
    {
        return;
    }
    
    kernel_mutex_lock(&(mem->mutex));

    for(memregion = mem->first_region; memregion != NULL; memregion = next)
    {
        next = memregion->next;
        kfree(memregion);
    }
    
    mem->first_region = NULL;

    kernel_mutex_unlock(&(mem->mutex));
}


void memregion_consolidate(struct task_t *task)
{
    struct memregion_t *tmp, *memregion = task->mem->first_region;
    virtual_addr end;

    if(!memregion)
    {
        return;
    }

    /*
    printk("mmap: ********** old memregion map\n");

    for(struct memregion_t *tmp = task->mem->first_region; tmp != NULL; tmp = tmp->next)
    {
        printk("mmap: tmp->addr %lx - 0x%lx\n", tmp->addr, tmp->addr + (tmp->size * PAGE_SIZE));
    }
    */

    while(memregion->next)
    {
        end = memregion->addr + (memregion->size * PAGE_SIZE);
        
        if(end == memregion->next->addr &&
           memregion->type != MEMREGION_TYPE_SHMEM &&
           memregion->inode == memregion->next->inode &&
           memregion->type == memregion->next->type &&
           memregion->prot == memregion->next->prot &&
           memregion->flags == memregion->next->flags)
        {
            if(memregion->inode == NULL ||
               (memregion->fpos + memregion->flen) == memregion->next->fpos)
            {
                //printk("memregion_consolidate: before - fp %lx, fs %lx, mp %lx, ms %lx\n", memregion->fpos, memregion->flen, memregion->addr, memregion->size * PAGE_SIZE);

                if(memregion->inode)
                {
                    memregion->flen += memregion->next->flen;
                }
                /*
                else
                {
                    memregion->flen = 0;
                    memregion->fpos = 0;
                }
                */

                tmp = memregion->next;
                memregion->size += tmp->size;
                memregion->next = tmp->next;

                if(memregion->next)
                {
                    memregion->next->prev = memregion;
                }

                if(tmp == task->mem->last_region)
                {
                    task->mem->last_region = memregion;
                }

                // add region to free list
                //tmp->refs--;
                __sync_fetch_and_sub(&tmp->refs, 1);
                memregion_add_free(tmp);

                //printk("memregion_consolidate: after - fp %lx, fs %lx, mp %lx, ms %lx\n", memregion->fpos, memregion->flen, memregion->addr, memregion->size * PAGE_SIZE);
            }
            else
            {
                memregion = memregion->next;
            }
        }
        else
        {
            memregion = memregion->next;
        }
    }

    /*
    printk("mmap: ********** new memregion map\n");

    for(struct memregion_t *tmp = task->mem->first_region; tmp != NULL; tmp = tmp->next)
    {
        printk("mmap: tmp->addr %lx - 0x%lx\n", tmp->addr, tmp->addr + (tmp->size * PAGE_SIZE));
    }

    screen_refresh(NULL);
    __asm__ __volatile__("xchg %%bx, %%bx"::);
    */
}


/*
 * This is similar to release_cached_page() except it does not decrement
 * the physical frame's share count as we need it to stay as is, but we
 * still need to wakeup any waiters.
 */
static inline void release_and_wakeup_waiters(struct cached_page_t *pcache)
{
    int wanted = (pcache->flags & PCACHE_FLAG_WANTED);
    
    kernel_mutex_lock(&pcachetab_lock);
    __sync_and_and_fetch(&pcache->flags, ~(PCACHE_FLAG_BUSY | PCACHE_FLAG_WANTED));
    kernel_mutex_unlock(&pcachetab_lock);

    if(wanted)
    {
        unblock_tasks(pcache);
    }
}


/*
 * Load a memory page from the file node referenced in the given memregion,
 * or zero-out the page is the memregion has no file backing. The function
 * allocates a new physical memory page and sets its protection according
 * to the memregion's prot field.
 * This function is called from the page fault handler. The sought address
 * need not be page-aligned, as the function automatically aligns it down to
 * the nearest page boundary.
 * If the memregion is shared by more than one task (i.e. the tasklist
 * field != NULL), the other tasks' page tables are updated to advertise
 * the newly allocated physical memory page.
 *
 * TODO: there is a possible race condition if one of the other tasks page
 *       faults and allocates physical memory page for the same address
 *       before we advertise our physical memory page.
 *
 * NOTES:
 *   - The caller must have locked mem->mutex before calling us.
 *
 * Returns:
 *   0 on success, -errno on failure.
 */
long memregion_load_page(struct memregion_t *memregion, pdirectory *pd,
                         volatile virtual_addr __addr)
{
    //struct file_t file;
    off_t file_pos;
    //int res = 0;
    size_t read_size = 0, file_end = 0, mem_end = 0;
    struct cached_page_t *pcache = NULL;

    if(!memregion || !pd)
    {
        return -EINVAL;
    }

    pt_entry *e = get_page_entry_pd(pd, (void *)__addr);

    if(!e)
    {
        return -ENOMEM;
    }
    
    virtual_addr addr = align_down(__addr);

    // if no backing file, zero-fill the page
    if(!memregion->inode)
    {
        if(!vmmngr_alloc_page(e, PTE_FLAGS_PWU))
        {
            return -ENOMEM;
        }

        A_memset((void *)addr, 0, PAGE_SIZE);
    }
    else
    {
        // where to read from in the file
        file_pos = memregion->fpos + (addr - memregion->addr);

        // where to stop reading in the file
        file_end = file_pos + PAGE_SIZE;

        // where the file-backed mmapped region ends
        mem_end = memregion->fpos + memregion->flen;
         
        // but don't read past the mmaped region in the file
        if(file_end > mem_end)
        {
            /*
             *        file_pos +-------+ file_end
             *                 |       |
             *                 +-------+
             *  mem_pos +----------+ mem_end
             *          |          |
             *          +----------+
             */
            read_size = (file_end - mem_end);
            
            // reading past 1-page means the memregion was up-aligned and
            // there is no file backing in this range, so make up a zero
            // filled page (this happens, for example, with the ldso pages
            // that were loaded by the kernel's elf loader)
            if(read_size >= PAGE_SIZE)
            {
                //read_size = 0;

                if(!vmmngr_alloc_page(e, PTE_FLAGS_PWU))
                {
                    return -ENOMEM;
                }

                A_memset((void *)addr, 0, PAGE_SIZE);
                goto fin;
            }
            else
            {
                read_size = PAGE_SIZE - read_size;
            }
        }
        else
        {
            read_size = PAGE_SIZE;
        }

        if((pcache = get_cached_page(memregion->inode, file_pos, 0)))
        {
            if((read_size == PAGE_SIZE) ||
               !(memregion->flags & MEMREGION_FLAG_PRIVATE))
            {

                if(!e)
                {
                    kpanic("invalid pointer in memregion_load_page()\n");
                }

                PTE_SET_FRAME(e, pcache->phys);
                PTE_ADD_ATTRIB(e, PTE_FLAGS_PWU);

                if((memregion->prot & PROT_WRITE))
                {
                    __sync_or_and_fetch(&pcache->flags, PCACHE_FLAG_ALWAYS_DIRTY);
                }

                //pcache->flags &= ~PCACHE_FLAG_BUSY;
                release_and_wakeup_waiters(pcache);
                
                goto fin;
            }
            else
            {
                e = get_page_entry_pd(pd, (void *)__addr);
                //dec_frame_shares(pcache->phys);

                if(!vmmngr_alloc_page(e, PTE_FLAGS_PWU))
                {
                    //pcache->flags &= ~PCACHE_FLAG_BUSY;
                    release_cached_page(pcache);
                    return -ENOMEM;
                }
                
                A_memcpy((void *)addr, (void *)pcache->virt, read_size);

                if(read_size != PAGE_SIZE)
                {
                    A_memset((void *)(addr + read_size), 0, 
                             PAGE_SIZE - read_size);
                }

                //pcache->flags &= ~PCACHE_FLAG_BUSY;
                release_cached_page(pcache);
                
                goto fin;
            }
        }

        __asm__ __volatile__("xchg %%bx, %%bx":::);
        return -ENOMEM;
        /*
        if(!vmmngr_alloc_page(e, PTE_FLAGS_PWU))
        {
            return -ENOMEM;
        }
        
        if((res = vfs_read_node(memregion->inode, &file_pos, 
                                (unsigned char *)addr, read_size, 1)) < 0)
        {
            vmmngr_free_page(e);
            vmmngr_flush_tlb_entry(__addr);
            return res;
        }
        
        // if reading past EOF (or the mmaped range), fill the rest of 
        // page with zeroes
        if(res != PAGE_SIZE)
        {
            A_memset((void *)(addr + res), 0, PAGE_SIZE - res);
        }
        */
    }

fin:

    if(!(memregion->prot & PROT_WRITE))
    {
        PTE_DEL_ATTRIB(e, I86_PTE_WRITABLE);
    }
    
    if(memregion->flags & MEMREGION_FLAG_PRIVATE)
    {
        if((memregion->prot & PROT_WRITE))
        {
            PTE_DEL_ATTRIB(e, I86_PTE_WRITABLE);
            PTE_ADD_ATTRIB(e, I86_PTE_COW);
        }

        PTE_ADD_ATTRIB(e, I86_PTE_PRIVATE);
    }

    __asm__ __volatile__("":::"memory");
    vmmngr_flush_tlb_entry(__addr);
    
    return 0;
}


struct memregion_t *memregion_containing(volatile struct task_t *task, virtual_addr addr)
{
    volatile struct memregion_t *memregion;
    virtual_addr start = align_down(addr);
    virtual_addr end = start + PAGE_SIZE - 1;
    
    for(memregion = task->mem->first_region; 
        memregion != NULL; 
        memregion = memregion->next)
    {
        virtual_addr start2 = memregion->addr;
        virtual_addr end2 = start2 + (memregion->size * PAGE_SIZE) - 1;

        // no overlap
        if(end < start2 || start > end2)
        {
            continue;
        }
        
        return (struct memregion_t *)memregion;
    }
    
    return NULL;
}


long memregion_check_overlaps(struct task_t *task,
                              virtual_addr start, virtual_addr end)
{
    volatile struct memregion_t *memregion = task->mem->first_region;
    end--;

    while(memregion)
    {
        virtual_addr start2 = memregion->addr;
        virtual_addr end2 = start2 + (memregion->size * PAGE_SIZE) - 1;
                
        // no overlap
        if(end < start2 || start > end2)
        {
            memregion = memregion->next;
            continue;
        }

        return -EEXIST;
    }

    return 0;
}


/*
 * Get the number of shared memory pages.
 *
 * Returns:
 *   memory usage in pages (not bytes).
 */
size_t memregion_shared_pagecount(volatile struct task_t *task)
{
    struct memregion_t *memregion;
    size_t count = 0;

    if(!task || !task->mem)
    {
        return 0;
    }

    for(memregion = task->mem->first_region;
        memregion != NULL;
        memregion = memregion->next)
    {
        if((memregion->flags & MEMREGION_FLAG_SHARED) && memregion->inode)
        {
            count += memregion->size;
        }
    }
    
    return count;
}


/*
 * Get the number of anonymous memory pages (ones with no file-backing).
 *
 * Returns:
 *   memory usage in pages (not bytes).
 */
size_t memregion_anon_pagecount(volatile struct task_t *task)
{
    struct memregion_t *memregion;
    size_t count = 0;

    if(!task || !task->mem)
    {
        return 0;
    }

    for(memregion = task->mem->first_region;
        memregion != NULL;
        memregion = memregion->next)
    {
        // TODO: should we be counting kernel memory here?
        if(memregion->inode == NULL && 
           memregion->type != MEMREGION_TYPE_KERNEL)
        {
            count += memregion->size;
        }
    }
    
    return count;
}


/*
 * Helper function.
 */
static size_t memregion_pagecount_by_type(volatile struct task_t *task, int type)
{
    struct memregion_t *memregion;
    size_t count = 0;
    
    if(!task || !task->mem)
    {
        return 0;
    }

    for(memregion = task->mem->first_region;
        memregion != NULL;
        memregion = memregion->next)
    {
        if(memregion->type == type)
        {
            count += memregion->size;
        }
    }
    
    return count;
}


/*
 * Get the number of text (code) memory pages.
 *
 * Returns:
 *   memory usage in pages (not bytes).
 */
size_t memregion_text_pagecount(volatile struct task_t *task)
{
    return memregion_pagecount_by_type(task, MEMREGION_TYPE_TEXT);
}


/*
 * Get the number of data memory pages.
 *
 * Returns:
 *   memory usage in pages (not bytes).
 */
size_t memregion_data_pagecount(volatile struct task_t *task)
{
    return memregion_pagecount_by_type(task, MEMREGION_TYPE_DATA);
}


/*
 * Get the number of stack memory pages.
 *
 * Returns:
 *   memory usage in pages (not bytes).
 */
size_t memregion_stack_pagecount(volatile struct task_t *task)
{
    return memregion_pagecount_by_type(task, MEMREGION_TYPE_STACK);
}


/*
 * Get the number of kernel memory pages.
 *
 * Returns:
 *   memory usage in pages (not bytes).
 */
size_t memregion_kernel_pagecount(volatile struct task_t *task)
{
    return memregion_pagecount_by_type(task, MEMREGION_TYPE_KERNEL);
}

