/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: kpanic.c
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
 *  \file kpanic.c
 *
 *  The kernel's panic function, which also prints a stack trace of kernel
 *  call stack that led to this panic.
 */

#include <kernel/laylaos.h>
#include <kernel/asm.h>
#include <kernel/ksymtab.h>
#include <kernel/tty.h>
#include <mm/mmap.h>
#include <mm/memregion.h>
#include <gui/vbe.h>

static char *get_func_name(uintptr_t wanted_addr)
{
    int i;
    struct hashtab_item_t *hitem, *candidate_hitem = NULL;
    uintptr_t candidate_addr = 0;
    
    if(!ksymtab)
    {
        return "??";
    }
    
    for(i = 0; i < ksymtab->count; i++)
    {
        hitem = ksymtab->items[i];
        
        while(hitem)
        {
            uintptr_t addr = (uintptr_t)hitem->val;
            
            if(addr < wanted_addr && addr > candidate_addr)
            {
                candidate_addr = addr;
                candidate_hitem = hitem;
            }

            hitem = hitem->next;
        }
    }
    
    return candidate_hitem ? candidate_hitem->key : "??";
}


void kernel_stack_trace(void)
{
    uintptr_t rbp, retaddr;

	printk("Stack trace:\n");

#ifdef __x86_64__
    __asm__ __volatile__("movq %%rbp, %%rax" : "=a"(rbp) :: );
#else
    __asm__ __volatile__("movl %%ebp, %%eax" : "=a"(rbp) :: );
#endif
	
	while(rbp != 0 && rbp >= KERNEL_MEM_START)
	{
	    retaddr = *(uintptr_t *)(rbp + sizeof(uintptr_t));
	    printk(_XPTR_ ": %s \n", retaddr, get_func_name(retaddr));
	    rbp = *(uintptr_t *)rbp;
	}

    //screen_refresh(NULL);
}


/*
 * Kernel panic function.
 */
void kpanic(const char *s)
{
    // if this is a graphical tty, switch to the system console so we can panic
    //if((ttytab[cur_tty].flags & TTY_FLAG_NO_TEXT))
    {
        switch_tty(1);
    }

    /*
    printk("\nblocks %lu (%lu, %lu, %lu)\n", get_cached_block_count(), get_busy_cached_block_count(), get_wanted_cached_block_count(), get_dirty_cached_block_count());
    printk("cache %lu (%lu)\n", get_cached_page_count(), get_busy_cached_page_count());
    printk("ptables %lu\n", used_pagetable_count());
    printk("kstack %lu\n", get_kstack_count());
    printk("shm %lu\n", get_shm_page_count());
    printk("phys_free %lu\n", pmmngr_get_free_block_count());
    kheap_print();

    size_t text = 0, data = 0;
    size_t anonp = 0, anons = 0;

    for_each_taskptr(t)
    {
        if(*t)
        {
            struct memregion_t *memregion;

            for(memregion = (*t)->mem->first_region;
                memregion != NULL;
                memregion = memregion->next)
            {
                // TODO: should we be counting kernel memory here?
                if(memregion->inode == NULL && 
                   memregion->type != MEMREGION_TYPE_KERNEL)
                {
                    if(memregion->flags & MEMREGION_FLAG_PRIVATE)
                    {
                        anonp += memregion->size;
                    }
                    else
                    {
                        anons += memregion->size;
                    }
                }
                else if(memregion->type == MEMREGION_TYPE_TEXT)
                {
                    text += memregion->size;
                }
                else if(memregion->type == MEMREGION_TYPE_DATA)
                {
                    data += memregion->size;
                }
            }
        }
    }

    printk("text %lu, data %lu, anon_priv %lu, anon_shared %lu\n", text, data, anonp, anons);
    */

	printk("Kernel panic: halting all cores\n");
	halt_other_processors();

	printk("Kernel panic: %s\n", s);
	kernel_stack_trace();
    screen_refresh(NULL);
    __asm__ __volatile__("xchg %%bx, %%bx"::);

loop:

    cli();
    hlt();
    goto loop;
	
	__builtin_unreachable();
}

