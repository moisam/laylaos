/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: user.h
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
 *  \file user.h
 *
 *  Functions and macros for copying data between user space and kernel space.
 */

#ifndef __USER_H__
#define __USER_H__

#define __USE_XOPEN_EXTENDED
#include <errno.h>
#include <signal.h>
#include <kernel/laylaos.h>
#include <kernel/ksignal.h>
#include <kernel/smp.h>
#include <mm/mmngr_virtual.h>

STATIC_INLINE long add_task_segv_signal(volatile struct task_t *t, int code, void *addr);

/**
 * @brief Userspace address validation.
 *
 * Called when copying data to and from userspace to validate the source
 * or destination userspace address range to ensure it is part of the calling
 * task's address space and that it falls in the user's, not the kernel's,
 * address space.
 *
 * This function is a huge bottleneck as it is called frequently, every time
 * we are about to copy data to/from user space. At the moment we simply
 * check the given address range to ensure it does not fall in the kernel
 * space, and if an address turns out to be unmapped when we perform the copy,
 * we let the pagefault handler deal with it. Fetching the actual page table
 * entries would take so long it would cripple the system. Checking the
 * address against the task's memory regions is done again in the page fault
 * handler, so doing it here does not seem to add much.
 *
 * NOTE: It would probably make more sense to check against the task's (or
 *       more accurately the thread's) LDT limit. Maybe also check for
 *       read/write access rights, depending on the requested operation.
 *
 * @param   ct          pointer to current task
 * @param   addr        start of address range
 * @param   addr_end    end of address range
 *
 * @return  0 if address range is valid, -(EFAULT) otherwise.
 */
STATIC_INLINE int valid_addr(volatile struct task_t *ct, 
                             virtual_addr addr, virtual_addr addr_end)
{
    // kernel tasks and init task can do whatever
    if(!ct->user || ct->pid == 1)
    {
        return 0;
    }

#if 0

    struct memregion_t *memregion /* = NULL */;
    virtual_addr memregion_end;

try:

    if(!(memregion = memregion_containing(ct, addr)))
    {
        return -EFAULT;
    }
    
    memregion_end = (memregion->addr + (memregion->size * PAGE_SIZE));
    
    if(addr_end >= memregion_end)
    {
        // If the memregion contains the start address but no the end address
        // of the requested address range, it could be because the address
        // range is split across memregions. In this case, we call ourselves
        // recursively, checking the end of the requested address range,
        // until we either find a memregion that contains the last part of the
        // address range, or we return error.

        addr = memregion_end;
        goto try;
    }

#endif

    // simple checks for now
    if(addr >= USER_MEM_END)
    {
        return -EFAULT;
    }

    if(addr_end >= USER_MEM_END)
    {
        return -EFAULT;
    }
    
    return 0;
}


/**
 * @brief Copy to userspace.
 *
 * Copy data from kernel space to userspace. This function calls valid_addr()
 * to validate the destination address \a dest only, as the source address
 * \a src is assumed to be in kernel space.
 *
 * @param   dest    destination address (in user space)
 * @param   src     source address (in kernel space)
 * @param   len     length of data
 *
 * @return  0 if address range is valid, -(EFAULT) otherwise.
 *
 * @see     valid_addr()
 */
STATIC_INLINE long copy_to_user(void *dest, void *src, size_t len)
{
    if(!len || !src || !dest)
    {
        add_task_segv_signal(this_core->cur_task, SEGV_MAPERR, src ? src : dest);
        return -EFAULT;
    }
    
    virtual_addr addr     = (virtual_addr)dest;
    virtual_addr addr_end = addr + len - 1;
    
    if(valid_addr(this_core->cur_task, addr, addr_end) != 0)     /* invalid address */
    {
        add_task_segv_signal(this_core->cur_task, SEGV_MAPERR, (void *)addr);
        return -EFAULT;
    }
    
    A_memcpy(dest, src, len);

    return 0;
}


/**
 * @brief Copy from userspace.
 *
 * Copy data to kernel space from userspace. This function calls valid_addr()
 * to validate the source address \a src only, as the destination address
 * \a dest is assumed to be in kernel space.
 *
 * @param   dest    destination address (in kernel space)
 * @param   src     source address (in user space)
 * @param   len     length of data
 *
 * @return  0 if address range is valid, -(EFAULT) otherwise.
 *
 * @see     valid_addr()
 */
STATIC_INLINE long copy_from_user(void *dest, void *src, size_t len)
{
    if(!len || !src || !dest)
    {
        add_task_segv_signal(this_core->cur_task, SEGV_MAPERR, src ? src : dest);
        return -EFAULT;
    }
    
    virtual_addr addr     = (virtual_addr)src;
    virtual_addr addr_end = addr + len - 1;
    
    if(valid_addr(this_core->cur_task, addr, addr_end) != 0)     /* invalid address */
    {
        add_task_segv_signal(this_core->cur_task, SEGV_MAPERR, (void *)addr);
        return -EFAULT;
    }
    
    A_memcpy(dest, src, len);

    return 0;
}


/**
 * @brief Copy string from userspace.
 *
 * Copy a string to kernel space from userspace. This functions creates a
 * kmalloc'd copy in kernel space. The address of the kernel copy is placed
 * in \a res, and the length of the string is placed in \a reslen.
 *
 * @param   str     pointer to source string (in user space)
 * @param   res     pointer to copied string (in kernel space) is returned here
 * @param   reslen  length of copied string is returned here
 *
 * @return  0 if address range is valid, -(EFAULT) otherwise.
 */
long copy_str_from_user(char *str, char **res, size_t *reslen);


/**************************************
 * Helpful macros for use by syscalls
 **************************************/

#define COPY_FROM_USER(dest, src, sz)               \
    if(copy_from_user(dest, src, sz) != 0) return -EFAULT;

#define COPY_TO_USER(dest, src, sz)                 \
    if(copy_to_user(dest, src, sz) != 0) return -EFAULT;

// Quick and dirty way of copying a value from the kernel to a pointer in
// user space. It only checks the pointer falls within the user space memory
// bounds. The assignment might still result in segmentation fault, which
// will be dealt with in pagefault().
// Use only for direct values like int, char, ..., but not structs or arrays.

#define COPY_VAL_TO_USER(uptr, kptr)                        \
    if((uintptr_t)uptr > USER_MEM_START &&                  \
        ((uintptr_t)uptr + sizeof(*(uptr))) < USER_MEM_END) \
    {                                                       \
            *(uptr) = *(kptr);                              \
    }                                                       \
    else                                                    \
    {                                                       \
        add_task_segv_signal(this_core->cur_task, SEGV_MAPERR, (void *)uptr); \
        return -EFAULT;                                     \
    }

// Similar macro to copy a value from user space to the kernel.

#define COPY_VAL_FROM_USER(kptr, uptr)                      \
    if((uintptr_t)uptr > USER_MEM_START &&                  \
        ((uintptr_t)uptr + sizeof(*(uptr))) < USER_MEM_END) \
    {                                                       \
            *(kptr) = *(uptr);                              \
    }                                                       \
    else                                                    \
    {                                                       \
        add_task_segv_signal(this_core->cur_task, SEGV_MAPERR, (void *)uptr); \
        return -EFAULT;                                     \
    }

#include "signal_funcs.h"

#endif      /* __USER_H__ */
