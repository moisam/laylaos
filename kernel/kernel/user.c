/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: user.c
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
 *  \file user.c
 *
 *  The kernel-user interface for copying data from user space to kernel
 *  space and vice versa.
 */

//#define __DEBUG

#include <string.h>
#include <errno.h>
#define __USE_XOPEN_EXTENDED
#include <signal.h>
#include <kernel/laylaos.h>
#include <kernel/ksignal.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <mm/kheap.h>


/*
 * Userspace address validation.
 */
int valid_addr(struct task_t *ct, virtual_addr addr, virtual_addr addr_end)
{
    // kernel tasks and init task can do whatever
    if(!ct->user || ct->pid == 1)
    {
        return 0;
    }

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


/*
 * Copy to userspace.
 */
int copy_to_user(void *dest, void *src, size_t len)
{
    struct task_t *ct = cur_task;

    if(!len || !src || !dest)
    {
        add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, src ? src : dest);
        return -EFAULT;
    }
    
    virtual_addr addr     = (virtual_addr)dest;
    virtual_addr addr_end = addr + len - 1;
    
    if(valid_addr(ct, addr, addr_end) != 0)     /* invalid address */
    {
        add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, (void *)addr);
        return -EFAULT;
    }
    
    A_memcpy(dest, src, len);

    return 0;
}


/*
 * Copy from userspace.
 */
int copy_from_user(void *dest, void *src, size_t len)
{
    struct task_t *ct = cur_task;

    if(!len || !src || !dest)
    {
        add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, src ? src : dest);
        return -EFAULT;
    }
    
    virtual_addr addr     = (virtual_addr)src;
    virtual_addr addr_end = addr + len - 1;
    
    if(valid_addr(ct, addr, addr_end) != 0)     /* invalid address */
    {
        add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, (void *)addr);
        return -EFAULT;
    }
    
    A_memcpy(dest, src, len);

    return 0;
}


/*
 * Copy a string from userspace.
 */
int copy_str_from_user(char *str, char **res, size_t *reslen)
{
    struct task_t *ct = cur_task;
    char *dest, *s = str;
    int oldsig = ct->woke_by_signal;
    size_t sz;

    if(!str || !res)
    {
        add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, 
                                 str ? (void *)str : (void *)res);
        return -EFAULT;
    }
    
    *res = NULL;
    *reslen = 0;
    ct->woke_by_signal = 0;
    
    while(*s)
    {
        if(ct->woke_by_signal == SIGSEGV)
        {
            goto fault;
        }
        
        s++;
    }

    if(ct->woke_by_signal == SIGSEGV)
    {
        goto fault;
    }
    
    ct->woke_by_signal = oldsig;
    sz = s - str + 1;
    
    if(!(dest = kmalloc(sz)))
    {
        add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, s);
        return -EFAULT;
    }
    
    A_memcpy(dest, str, sz);
    *res = dest;
    *reslen = sz - 1;
    return 0;

fault:
    ct->woke_by_signal = oldsig;
    add_task_segv_signal(ct, SIGSEGV, SEGV_MAPERR, s);
    return -EFAULT;
}

