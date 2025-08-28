/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
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
 * Copy a string from userspace.
 */
long copy_str_from_user(char *str, char **res, size_t *reslen)
{
	volatile struct task_t *ct = this_core->cur_task;
    char *dest, *s = str;
    int oldsig = ct->woke_by_signal;
    size_t sz;

    if(!str || !res)
    {
        add_task_segv_signal(ct, SEGV_MAPERR, 
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
        add_task_segv_signal(ct, SEGV_MAPERR, s);
        return -EFAULT;
    }
    
    A_memcpy(dest, str, sz);
    *res = dest;
    *reslen = sz - 1;
    return 0;

fault:

    ct->woke_by_signal = oldsig;
    add_task_segv_signal(ct, SEGV_MAPERR, s);
    return -EFAULT;
}

