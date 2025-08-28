/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: kheap.c
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
 *  \file kheap.c
 *
 *  The kernel heap implementation.
 */

//#define __DEBUG

#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/mutex.h>
#include <mm/kheap.h>
#include <mm/malloc.h>
#include <mm/mmap.h>


volatile struct kernel_mutex_t kheap_lock;

static void *sbrk_top = (void *)KHEAP_START;
static int cur_heap_sz = 0;


/*
 * Initialise kernel heap.
 */
void kheap_init(void)
{
    init_kernel_mutex(&kheap_lock);
    void *test = kmalloc(1);
    kfree(test);
}


void kfree(void *p)
{
    int old_prio = 0, old_policy = 0;
    elevate_priority(this_core->cur_task, &old_prio, &old_policy);

    kernel_mutex_lock(&kheap_lock);
    dlfree(p);
    kernel_mutex_unlock(&kheap_lock);

    restore_priority(this_core->cur_task, old_prio, old_policy);
}


void *kmalloc(size_t sz)
{
    int old_prio = 0, old_policy = 0;
    elevate_priority(this_core->cur_task, &old_prio, &old_policy);

    kernel_mutex_lock(&kheap_lock);
    void *res = dlmalloc(sz);
    kernel_mutex_unlock(&kheap_lock);

    restore_priority(this_core->cur_task, old_prio, old_policy);

    return res;
}


void *krealloc(void *addr, size_t sz)
{
    int old_prio = 0, old_policy = 0;
    elevate_priority(this_core->cur_task, &old_prio, &old_policy);

    kernel_mutex_lock(&kheap_lock);
    void *res = dlrealloc(addr, sz);
    kernel_mutex_unlock(&kheap_lock);

    restore_priority(this_core->cur_task, old_prio, old_policy);

    return res;
}


void *kcalloc(size_t m, size_t n)
{
    size_t sz = m * n;
    
    if(m && n && (m != sz / n))
    {
        // integer overflow
        return NULL;
    }
    
    int old_prio = 0, old_policy = 0;
    elevate_priority(this_core->cur_task, &old_prio, &old_policy);

    kernel_mutex_lock(&kheap_lock);
    void *res = dlmalloc(sz);
    kernel_mutex_unlock(&kheap_lock);

    restore_priority(this_core->cur_task, old_prio, old_policy);

    if(res)
    {
        A_memset((void *)res, 0, sz);
    }
    
    return res;
}


#pragma GCC push_options
#pragma GCC optimize("O0")

void *kheap_brk(int incr)
{
    if(incr > 0)
    {
        virtual_addr old_end_data = KHEAP_START + cur_heap_sz;
        virtual_addr end_data_seg = old_end_data + incr;
        
        // if the new size is not page-aligned, make it so
        end_data_seg = align_up(end_data_seg);
        
        // now alloc memory for the new pages, starting from the current
        // brk (aligned to the nearest lower page size), up to the new
        // brk address.
        volatile virtual_addr i;
        
        for(i = align_down(old_end_data);
            i < end_data_seg; i += PAGE_SIZE)
        {
            pt_entry *pt = get_page_entry((void *)i);
            
            if(!pt)
            {
                kpanic("failed to expand kernel heap!");
                empty_loop();
            }
            
            if(!PTE_PRESENT(*pt))
            {
                if(!vmmngr_alloc_page(pt, I86_PTE_WRITABLE))
                {
                    kpanic("failed to expand kernel heap!");
                    empty_loop();
                }

                vmmngr_flush_tlb_entry(i);
            }
        }

        cur_heap_sz += incr;
        sbrk_top = (void *) (KHEAP_START + cur_heap_sz);
        
        return (void *)old_end_data;
    }
    else if(incr < 0)
    {
        // we don't currently support shrink behavior
        __asm__ __volatile__("xchg %%bx, %%bx":::);
        return (void *) MFAIL;
    }

    return sbrk_top;
}

#pragma GCC pop_options


void kheap_print(void)
{
    printk("cur_heap_sz %d, sbrk_top 0x%lx\n", cur_heap_sz, sbrk_top);
}

