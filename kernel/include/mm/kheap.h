/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: kheap.h
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
 *  \file kheap.h
 *
 *  Helper functions for allocating and freeing memory from the kernel heap.
 */

#ifndef __KHEAP_H__
#define __KHEAP_H__

#include <mm/mmngr_virtual.h>


#include <mm/malloc.h>
#include <kernel/task.h>

extern struct kernel_mutex_t kheap_lock;

/**
 * @brief Free dynamic memory.
 *
 * Free a region of memory previously allocated on the heap via a call to
 * kmalloc(), kmrealloc(), or kcalloc().
 *
 * @param   p       ptr to memory to free
 *
 * @return  nothing.
 */
static inline void kfree(void *p)
{
    int old_prio = 0, old_policy = 0;
    elevate_priority(cur_task, &old_prio, &old_policy);

    kernel_mutex_lock(&kheap_lock);
    dlfree(p);
    kernel_mutex_unlock(&kheap_lock);

    restore_priority(cur_task, old_prio, old_policy);
}

/**
 * @brief Allocate dynamic memory.
 *
 * Allocate a region of memory on the kernel heap. It can be freed later by 
 * calling kfree().
 *
 * @param   sz      number of bytes to allocate
 *
 * @return  pointer to allocated memory on success, NULL on failure.
 */
static inline void *kmalloc(size_t sz)
{
    int old_prio = 0, old_policy = 0;
    elevate_priority(cur_task, &old_prio, &old_policy);

    kernel_mutex_lock(&kheap_lock);
    void *res = dlmalloc(sz);
    kernel_mutex_unlock(&kheap_lock);

    restore_priority(cur_task, old_prio, old_policy);

    return res;
}

/**
 * @brief Reallocate dynamic memory.
 *
 * Reallocate a previously allocated region of memory on the kernel heap. 
 * It can be freed later by calling kfree().
 *
 * @param   addr    pointer to region to resize
 * @param   sz      new requested size
 *
 * @return  pointer to reallocated memory on success, NULL on failure.
 */
static inline void *krealloc(void *addr, size_t sz)
{
    int old_prio = 0, old_policy = 0;
    elevate_priority(cur_task, &old_prio, &old_policy);

    kernel_mutex_lock(&kheap_lock);
    void *res = dlrealloc(addr, sz);
    kernel_mutex_unlock(&kheap_lock);

    restore_priority(cur_task, old_prio, old_policy);

    return res;
}


/**
 * @brief Initialise kernel heap.
 *
 * Called during boot to initialize the kernel heap.
 *
 * @return  nothing.
 */
void kheap_init(void);

/**
 * @brief Allocate dynamic memory.
 *
 * Allocate a region of memory on the kernel heap and fill it with zeroes.
 * It can be freed later by calling kfree().
 *
 * @param   m       number of items to allocate
 * @param   n       size of each item
 *
 * @return  pointer to allocated memory on success, NULL on failure.
 */
void *kcalloc(size_t m, size_t n);

/**
 * @brief Resize kernel heap.
 *
 * Change the kernel heap's break address (essentially changing its size).
 *
 * @param   incr    if non-zero, amount to increment current kernel size
 *                    by in bytes
 *
 * @return  the new break address
 */
void *kheap_brk(int incr);

#endif      /* __KHEAP_H__ */
