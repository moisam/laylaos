/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: vdso.c
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
 *  \file vdso.c
 *
 *  The kernel implementation of the virtual shared object (vdso).
 */

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/user.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmap.h>
#include "../../vdso/vdso.h"

virtual_addr vdso_code_start = 0;
virtual_addr vdso_code_end = 0;
virtual_addr vdso_data_addr = 0;

static struct timespec dummy_monotonic;
static time_t dummy_startup_time;

struct timespec *vdso_monotonic = &dummy_monotonic;
time_t *vdso_startup_time = &dummy_startup_time;

/*
 * Initialise kernel-side support for vdso.
 */
int vdso_stub_init(virtual_addr start, virtual_addr end)
{
    virtual_addr sz;

    start &= ~(PAGE_SIZE - 1);
    end = align_up(end);
    sz = end - start;

    // these are the code pages we will map to user tasks
    if(!(vdso_code_start = 
            vmmngr_alloc_and_map(VDSO_STATIC_CODE_SIZE, 0,
                                 PTE_FLAGS_PW, NULL, REGION_KMODULE)))
    {
        printk("  Failed to alloc vdso shared data page\n");
        return -ENOMEM;
    }

    if(sz > VDSO_STATIC_CODE_SIZE)
    {
        printk("  vdso size larger than predefined maximum (0x%x)\n",
                VDSO_STATIC_CODE_SIZE);
        return -ENOMEM;
    }

    vdso_code_end = vdso_code_start + sz;
    A_memcpy((void *)vdso_code_start, (void *)start, sz);

    if(sz < VDSO_STATIC_CODE_SIZE)
    {
        A_memset((void *)vdso_code_end, 0, VDSO_STATIC_CODE_SIZE - sz);
    }

    // this is the shared data page we will map to user tasks
    if(!(vdso_data_addr = 
            vmmngr_alloc_and_map(PAGE_SIZE, 0,
                                 PTE_FLAGS_PW, NULL, REGION_KMODULE)))
    {
        printk("  Failed to alloc vdso shared data page\n");
        return -ENOMEM;
    }

    A_memset((void *)vdso_data_addr, 0, PAGE_SIZE);

    vdso_monotonic = (struct timespec *)(vdso_data_addr + VDSO_OFFSET_CLOCK_GETTIME);
    vdso_startup_time = (time_t *)(vdso_data_addr + VDSO_OFFSET_STARTUP_TIME);
    *vdso_startup_time = startup_time;

    return 0;
}


/*
 * Map the vdso code and data pages to the newly created task.
 */
int map_vdso(virtual_addr *resaddr)
{
    virtual_addr src, dest, mapaddr;
    virtual_addr mapsz = VDSO_STATIC_CODE_SIZE;
    pdirectory *pml4_dest = (pdirectory *)this_core->cur_task->pd_virt;
    pdirectory *pml4_src = (pdirectory *)get_idle_task()->pd_virt;
    volatile pt_entry *esrc, *edest;
    int res;

    *resaddr = 0;

    if(mapsz == 0)
    {
        // no vdso
        return -EINVAL;
    }

    // ensure no one changes the task memory map while we're fiddling with it
    kernel_mutex_lock(&(this_core->cur_task->mem->mutex));
    
    // choose an address to map vdso code (add an extra page for shared data)
    if((mapaddr = get_user_addr(mapsz + PAGE_SIZE, 
                                USER_SHM_START, USER_SHM_END)) == 0)
    {
        kernel_mutex_unlock(&(this_core->cur_task->mem->mutex));
        return -ENOMEM;
    }

    if((res = memregion_alloc_and_attach((struct task_t *)this_core->cur_task, 
                                         NULL, 0, 0,
                                         mapaddr, mapaddr + mapsz + PAGE_SIZE,
                                         PROT_READ | PROT_WRITE,
                                         MEMREGION_TYPE_DATA,
                                         MAP_PRIVATE | MEMREGION_FLAG_USER |
                                                       MEMREGION_FLAG_VDSO,
                                         0)) != 0)
    {
        kernel_mutex_unlock(&(this_core->cur_task->mem->mutex));
        return res;
    }

    kernel_mutex_unlock(&(this_core->cur_task->mem->mutex));
    
    // map code
    for(dest = mapaddr, src = vdso_code_start;
        src < vdso_code_end;
        dest += PAGE_SIZE, src += PAGE_SIZE)
    {
        if(!(esrc = get_page_entry_pd(pml4_src, (void *)src)) ||
           !(edest = get_page_entry_pd(pml4_dest, (void *)dest)))
        {
            return -ENOMEM;
        }
        
        *edest = *esrc;

        PTE_DEL_ATTRIB(edest, I86_PTE_WRITABLE);
        PTE_ADD_ATTRIB(edest, I86_PTE_COW | I86_PTE_PRIVATE | I86_PTE_USER);

        inc_frame_shares(PTE_FRAME(*esrc));
        vmmngr_flush_tlb_entry(dest);
    }

    *resaddr = mapaddr;
    this_core->cur_task->mem->vdso_code_start = mapaddr;

    dest = mapaddr + VDSO_STATIC_CODE_SIZE;

    // map data into the last page
    if(!(esrc = get_page_entry_pd(pml4_src, (void *)vdso_data_addr)) ||
       !(edest = get_page_entry_pd(pml4_dest, (void *)dest)))
    {
        return -ENOMEM;
    }
        
    *edest = *esrc;

    PTE_DEL_ATTRIB(edest, I86_PTE_WRITABLE);
    PTE_ADD_ATTRIB(edest, I86_PTE_COW | I86_PTE_PRIVATE | I86_PTE_USER);

    inc_frame_shares(PTE_FRAME(*esrc));
    vmmngr_flush_tlb_entry(dest);

    return 0;
}

