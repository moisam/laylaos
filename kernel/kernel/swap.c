/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2025 (c)
 * 
 *    file: swap.c
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
 *  \file swap.c
 *
 *  Swap file implementation (unimplemented yet!).
 *
 *  See: https://man7.org/linux/man-pages/man2/swapon.2.html
 *       https://www.kernel.org/doc/gorman/html/understand/understand014.html
 */

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/swap.h>


/*
 * Handler for syscall swapon().
 */
long syscall_swapon(char *path, int swapflags)
{
    UNUSED(path);
    UNUSED(swapflags);
    
    return -ENOSYS;
}


/*
 * Handler for syscall swapoff().
 */
long syscall_swapoff(char *path)
{
    UNUSED(path);
    
    return -ENOSYS;
}

