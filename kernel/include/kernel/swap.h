/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: swap.h
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
 *  \file swap.h
 *
 *  Functions and macros to work with swap files.
 */

#ifndef __SWAP_H__
#define __SWAP_H__

/**
 * \def MAX_SWAPFILES
 *
 * Maximum number of supported swap files
 */
#define MAX_SWAPFILES       32


/*******************************
 * Functions defined in swap.c
 *******************************/

/**
 * @brief Handler for syscall swapon().
 *
 * Start swapping to a file/device.
 *
 * @param   path        pathname of swap device/file
 * @param   swapflags   flags (see sys/swap.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_swapon(char *path, int swapflags);

/**
 * @brief Handler for syscall swapoff().
 *
 * Stop swapping to a file/device.
 *
 * @param   path        pathname of swap device/file
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_swapoff(char *path);

#endif      /* __SWAP_H__ */
