/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: directrw.h
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
 *  \file directrw.h
 *
 *  Inlined functions for fast reading and writing. These are mostly used
 *  internally by the GUI library and are NOT intended for client application
 *  use.
 */

#ifndef DIRECT_RW_H
#define DIRECT_RW_H

#include <errno.h>
#include <sys/syscall.h>

#ifdef __x86_64__

#define RW_SYSCALL(name, res, a, b, c, d)                   \
    res = __NR_##name;                                      \
    __asm__ __volatile__ ("movq %4, %%r10\n"                \
                          "syscall"                         \
                          : "+a" (res)                      \
                          : "D" (a), "S" (b), "d" (c),      \
                            "r" ((uintptr_t)d)              \
                          : "rcx", "r9", "r11", "r10", "memory");

#else       /* !__x86_64__ */

#define RW_SYSCALL(name, res, a, b, c, d)                   \
    __asm__ __volatile__ ("int $0x80"                       \
                            : "=a" (res)                    \
                            : "0" (__NR_##name), "b" (a),   \
                              "c" (b), "d" (c), "D" (d)     \
                            : "memory");

#endif      /* !__x86_64__ */


static inline ssize_t direct_read(int file, void *ptr, size_t len)
{
    ssize_t bytes = 0;
    int res;

    RW_SYSCALL(read, res, file, ptr, len, &bytes);

    if(res < 0)
    {
        __set_errno(-res);
        return -1;
    }

    return bytes;
}


static inline ssize_t direct_write(int file, const void *ptr, size_t len)
{
    ssize_t bytes = 0;
    int res;

    RW_SYSCALL(write, res, file, ptr, len, &bytes);

    if(res < 0)
    {
        __set_errno(-res);
        return -1;
    }

    return bytes;
}

#endif      /* DIRECT_RW_H */
