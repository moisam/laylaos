/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
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
 *     task->kstack_phys => the physical address of the top of the kstack 
 *                          is stored here, that is equal to the actual
 *                          physical address + PAGE_SIZE
 *     task->kstack_virt => similar to the above, except here the virtual 
 *                          address is stored
 *
 * Returns 0 on success, -1 on failure.
 */
int get_kstack(volatile struct task_t *task)
{
    physical_addr phys = 0;

    if(!(phys = (physical_addr)pmmngr_alloc_block()))
    {
        return -1;
    }

    task->kstack_virt = PHYS_TO_HIMEM(phys) + PAGE_SIZE;
    task->kstack_phys = phys + PAGE_SIZE;

    kstack_count++;

    return 0;
}


/*
 * Free the memory page used by a user kstack.
 *
 * Input:
 *     task->kstack_virt => the virtual address of the top of the kstack, 
 *                          that is, the actual virtual address + PAGE_SIZE
 *
 * Returns nothing.
 */
void free_kstack(volatile struct task_t *task)
{
    pmmngr_free_block((void *)(task->kstack_phys - PAGE_SIZE));

    task->kstack_virt = 0;
    task->kstack_phys = 0;

    kstack_count--;
}


/*
 * Get kernel stack count.
 */
size_t get_kstack_count(void)
{
    return kstack_count;
}

