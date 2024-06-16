/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: kstack.c
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
 *  \file kstack.c
 *
 *  Helper functions for allocating and freeing kernel stacks for user tasks.
 */

//#define __DEBUG

#include <kernel/laylaos.h>
#include <kernel/mutex.h>
#include <mm/kstack.h>
#include <mm/memregion.h>


volatile size_t kstack_count = 0;


/*
 * Get the next valid address for a user kstack.
 * If we've reached the end of our allowed memory, restart from the first
 * address and try to find an earlier address that was alloc'd and free'd.
 *
 * Output:
 *     phys => the physical address of the top of the kstack is stored here,
 *             that is equal to the actual physical address + PAGE_SIZE
 *     virt => similar to the above, except here the virtual address is stored
 *
 * Returns 0 on success, -1 on failure.
 */
int get_kstack(physical_addr *phys, virtual_addr *virt)
{
    // for safety
    *virt = 0;
    *phys = 0;


    if((*virt = vmmngr_alloc_and_map(PAGE_SIZE * 2, 0,
                                     PTE_FLAGS_PWU, phys, 
                                     REGION_KSTACK)) == 0)
    {
        // nothing found
        return -1;
    }

    vmmngr_change_page_flags(*virt, PAGE_SIZE, 0);
    *virt += (PAGE_SIZE * 2);

    /*
    if(get_next_addr(phys, virt, PTE_FLAGS_PWU, REGION_KSTACK) != 0)
    {
        // nothing found
        return -1;
    }

    *virt += PAGE_SIZE;
    *phys += PAGE_SIZE;
    */

    kstack_count++;

    //KDEBUG("new task kstack @ 0x%x - 0x%x\n", *phys, *virt);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    return 0;
}


/*
 * Free the memory page used by a user kstack.
 *
 * Input:
 *     vaddr => the virtual address of the top of the kstack, that is, the 
 *              actual virtual address + PAGE_SIZE
 *
 * Returns nothing.
 */
void free_kstack(virtual_addr vaddr)
{
    vaddr -= PAGE_SIZE;
    pt_entry *pt = get_page_entry((void *)vaddr);
    vmmngr_free_page(pt);
    vmmngr_flush_tlb_entry(vaddr);


    pt = get_page_entry((void *)(vaddr - PAGE_SIZE));
    vmmngr_free_page(pt);
    vmmngr_flush_tlb_entry(vaddr);


    kstack_count--;
}


/*
 * Get kernel stack count.
 */
size_t get_kstack_count(void)
{
    return kstack_count;
}

