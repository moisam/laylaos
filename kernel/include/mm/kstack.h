/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: kstack.h
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
 *  \file kstack.h
 *
 *  Helper functions for allocating and freeing kernel stacks for user tasks.
 */

#ifndef __KSTACK_H__
#define __KSTACK_H__

#include <mm/mmngr_phys.h>
#include <mm/mmngr_virtual.h>

/**
 * @brief Allocate a kernel stack.
 *
 * Get the next valid address for a user kstack.
 * If we've reached the end of our allowed memory, restart from the first
 * address and try to find an earlier address that was alloc'd and free'd.
 *
 * @param   phys    the physical address of the top of the kstack is stored 
 *                    here, that is equal to the actual physical address +
 *                    PAGE_SIZE
 * @param   virt    similar to the above, except the virtual address is 
 *                    stored here
 *
 * @return  zero on success, -1 on failure.
 */
int get_kstack(physical_addr *phys, virtual_addr *virt);

/**
 * @brief Free a kernel stack.
 *
 * Free the memory page used by a user kstack.
 *
 * @param   virt    the virtual address of the top of the kstack, that is, the 
 *                    actual virtual address + PAGE_SIZE
 *
 * @return  nothing.
 */
void free_kstack(virtual_addr virt);

/**
 * @brief Get kernel stack count.
 *
 * Return the number of allocated kernel stacks.
 *
 * @return  kstack count.
 */
size_t get_kstack_count(void);

#endif      /* __KSTACK_H__ */
