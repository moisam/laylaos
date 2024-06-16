/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
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

//#include <mm/mmap.h>
#include "../include/kernel/bits/task-defs.h"

#undef INLINE
#define INLINE      static inline __attribute__((always_inline))

#ifndef __ALIGN_DOWN__
#define __ALIGN_DOWN__

INLINE virtual_addr align_down(virtual_addr addr)
{
    return (addr & ~(PAGE_SIZE - 1));
}

#endif

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
INLINE struct memregion_t *memregion_containing(struct task_t *task,
                                                virtual_addr addr)
{
    register struct memregion_t *memregion;
    virtual_addr start = align_down(addr);
    virtual_addr end = start + PAGE_SIZE - 1;
    
    for(memregion = task->mem->first_region; 
        memregion != NULL; 
        memregion = memregion->next)
    {
        register virtual_addr start2 = memregion->addr;
        register virtual_addr end2 = 
                            start2 + (memregion->size * PAGE_SIZE) - 1;

        // no overlap
        if(end < start2 || start > end2)
        {
            continue;
        }
        
        return memregion;
    }
    
    return NULL;
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
INLINE pt_entry *get_page_entry(void *virt)
{
    extern struct task_t *cur_task;

    pdirectory *page_directory = cur_task ? (pdirectory *)cur_task->pd_virt : 
                                      vmmngr_get_directory_virt();

    return get_page_entry_pd(page_directory, virt);
}


struct kernel_region_t
{
    int region;
    virtual_addr min, max;
    volatile virtual_addr *last;
    struct kernel_mutex_t *mutex;
};

extern struct kernel_region_t kernel_regions[];

static inline
void get_region_bounds(virtual_addr *addr_min, virtual_addr *addr_max,
                       volatile virtual_addr **last_addr,
                       struct kernel_mutex_t **mutex,
                       int region, const char *caller)
{
    struct kernel_region_t *r;
    
    *addr_min = 0;
    *addr_max = 0;
    *last_addr = NULL;
    *mutex = NULL;
    
    for(r = kernel_regions; r->mutex; r++)
    {
        if(r->region == region)
        {
            *addr_min = r->min;
            *addr_max = r->max;
            *last_addr = r->last;
            *mutex = r->mutex;
            return;
        }
    }

    printk("vmm: invalid memory region specified (%s())\n", caller);
    empty_loop();
}

