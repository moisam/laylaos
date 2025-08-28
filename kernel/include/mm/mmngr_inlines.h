/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: mmngr_inlines.h
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
 *  \file mmngr_inlines.h
 *
 *  Inlined functions that are used frequently by the virtual memory manager.
 */

#include <errno.h>
#include <kernel/laylaos.h>
#include "../include/kernel/bits/task-defs.h"


#ifndef __ALIGN_DOWN__
#define __ALIGN_DOWN__

STATIC_INLINE virtual_addr align_down(virtual_addr addr)
{
    return (addr & ~(PAGE_SIZE - 1));
}

#endif

#if 0

/**
 * @brief Find the memory region that contains the given address.
 *
 * This function is called from the page fault handler. The sought address
 * need not be page-aligned, as the function automatically aligns it down to
 * the nearest page boundary. If no suitable region is found, the function
 * tries to find a region one page above the given address, provided this
 * region is mapped as a stack region (type == MEMREGION_TYPE_STACK). This
 * way, the page fault handler can expand the stack downwards if the task
 * touches the page right below the stack's lowest address.
 *
 * NOTES:
 *   - The caller must have locked mem->mutex before calling us.
 *
 * @param   task        pointer to task
 * @param   addr        address to search
 *
 * @return  the memregion containing the given address on success, 
 *          NULL on failure.
 */
STATIC_INLINE struct memregion_t *memregion_containing(struct task_t *task,
                                                       virtual_addr addr)
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

/**
 * @brief Check for memregion overlaps.
 *
 * Check if the given address range from 'start' to 'end-1' overlaps with
 * other memory regions.
 *
 * This is a helper function. The task's memory struct should be locked by
 * the caller.
 *
 * @param   task        pointer to task
 * @param   start       start address
 * @param   end         end address
 *
 * @return  zero if no overlaps, or -EEXIST if there are one or more overlaps.
 */
STATIC_INLINE int memregion_check_overlaps(struct task_t *task,
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


/**
 * @brief Get page entry.
 *
 * Get the page table entry representing the given virtual address.
 *
 * @param   virt    virtual address
 *
 * @return  page table entry.
 */
STATIC_INLINE pt_entry *get_page_entry(void *virt)
{
    extern struct task_t *cur_task;

    pdirectory *page_directory = cur_task ? (pdirectory *)cur_task->pd_virt : 
                                      vmmngr_get_directory_virt();

    return get_page_entry_pd(page_directory, virt);
}

#endif


struct kernel_region_t
{
    int region;
    virtual_addr min, max;
    //volatile virtual_addr *last;
    volatile int lock_count;
    volatile struct kernel_mutex_t *mutex;
};

extern struct kernel_region_t kernel_regions[];

