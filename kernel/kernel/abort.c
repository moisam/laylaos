/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: abort.c
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
 *  \file abort.c
 *
 *  The kernel's abort function, which prints an error message and then calls
 *  kpanic(), which halts the machine.
 */

#include <unistd.h>
#include <stdlib.h>
#include <kernel/laylaos.h>

/*
 * Kernel abort function.
 */
__attribute__((noreturn)) void kabort(char *caller)
{
    printk("kabort() called by %s\n", caller);
    kpanic("kabort()");
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    __builtin_unreachable();
}

