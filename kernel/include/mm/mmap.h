/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: mmap.h
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
 *  \file mmap.h
 *
 *  Functions and macros for working with memory maps.
 */

#ifndef __KERNEL_MMAP_H__
#define __KERNEL_MMAP_H__

#ifndef __USE_MISC
#define __USE_MISC  1
#endif

#include <sys/mman.h>
#include <kernel/syscall.h>
#include <mm/mmngr_virtual.h>

/**
 * \def PAGE_ALIGNED
 *
 * Check if an address is page-aligned
 */
#define PAGE_ALIGNED(a)     !((size_t)(a) & (size_t)(PAGE_SIZE - 1))

/**
 * \def FLAG_SET
 *
 * Check if a bit is set in a flag
 */
#define FLAG_SET(f, a)      (((f) & (a)) == (a))

/**
 * \def VALID_PROT
 *
 * Valid memory protection bits
 */
#define VALID_PROT          (PROT_READ | PROT_WRITE | PROT_EXEC)


#undef INLINE
#define INLINE      static inline __attribute__((always_inline))

INLINE virtual_addr align_up(virtual_addr addr)
{
    if(addr & (PAGE_SIZE - 1))
    {
        addr &= ~(PAGE_SIZE - 1);
        addr += PAGE_SIZE;
    }
    
    return addr;
}


#ifndef __ALIGN_DOWN__
#define __ALIGN_DOWN__

INLINE virtual_addr align_down(virtual_addr addr)
{
    return (addr & ~(PAGE_SIZE - 1));
}

#endif


/**********************************
 * Functions defined in mmap.c
 **********************************/

/**
 * @brief Reserve memory in userspace.
 *
 * Used when mapping userspace memory maps and when attaching shared memory 
 * segments.
 *
 * @param   size        size of memory to reserve
 * @param   min         address to start looking at
 * @param   max         maximum accepted address
 *
 * @return  virtual userspace address on success, zero on failure.
 */
virtual_addr get_user_addr(virtual_addr size, virtual_addr min, virtual_addr max);

/**
 * @brief Handler for syscall mmap().
 *
 * Map files into memory.
 *
 * @param   __args      packed syscall arguments (see syscall.h and 
 *                        sys/mman.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_mmap(struct syscall_args *__args);

/**
 * @brief Handler for syscall munmap().
 *
 * Unmap files from memory.
 *
 * @param   addr        virtual address to unmap
 * @param   length      size of memory mapping
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_munmap(void *addr, size_t length);

/**
 * @brief Handler for syscall mprotect().
 *
 * Set protection on a region of memory.
 *
 * @param   addr        virtual address
 * @param   length      size of memory mapping
 * @param   prot        protection bits (see the PROT_* definitions in 
 *                        sys/mman.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_mprotect(void *addr, size_t length, int prot);

/**
 * @brief Handler for syscall mremap().
 *
 * Remap a virtual memory address.
 *
 * @param   __args      packed syscall arguments (see syscall.h and 
 *                        sys/mman.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_mremap(struct syscall_args *__args);

/**
 * @brief Handler for syscall mincore().
 *
 * Check if pages are resident in memory.
 *
 * @param   addr        virtual address
 * @param   length      length to check
 * @param   vec         must point to an array containing at least
 *                        (length + PAGE_SIZE - 1) / PAGE_SIZE bytes. The 
 *                        least significant bit of each byte will be set if
 *                        the corresponding page is resident in memory
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_mincore(void *addr, size_t length, unsigned char *vec);


/**********************************
 * Functions defined in mlock.c
 **********************************/

/**
 * @brief Handler for syscall mlock().
 *
 * Lock pages into memory, preventing them from being swapped.
 *
 * @param   addr        virtual address
 * @param   length      length to lock
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_mlock(void *addr, size_t length);

/**
 * @brief Handler for syscall mlock2().
 *
 * Lock pages into memory, preventing them from being swapped.
 *
 * @param   addr        virtual address
 * @param   length      length to lock
 * @param   flags       zero or MLOCK_ONFAULT
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_mlock2(void *addr, size_t length, unsigned int flags);

/**
 * @brief Handler for syscall munlock().
 *
 * Unlock pages from memory, allowing them to be swapped.
 *
 * @param   addr        virtual address
 * @param   length      length to unlock
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_munlock(void *addr, size_t length);

/**
 * @brief Handler for syscall mlockall().
 *
 * Lock all mapped pages into memory, preventing them from being swapped.
 *
 * @param   flags       see the MCL_* definitions in sys/mman.h
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_mlockall(int flags);

/**
 * @brief Handler for syscall munlockall().
 *
 * Unlock all mapped pages into memory, allowing them to be swapped.
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_munlockall(void);

#endif      /* __KERNEL_MMAP_H__ */
