/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: ksymtab.h
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
 *  \file ksymtab.h
 *
 *  Functions to work with the kernel's symbol table.
 */

#ifndef __KERNEL_SYMTAB_H__
#define __KERNEL_SYMTAB_H__

#define malloc      kmalloc
#define free        kfree

#include <mm/mmngr_virtual.h>
#include <sys/hash.h>

extern struct hashtab_t *ksymtab;

/**
 * @brief Initialise ksymtab.
 *
 * Initialise the kernel's symbol table. The symbol table is loaded by the
 * bootloader (currently GRUB) as the second loaded module.
 *
 * @param   data_start  symbol table start address
 * @param   data_end    symbol table end address
 *
 * @return  zero on success, -(errno) on failure.
 */
int ksymtab_init(virtual_addr data_start, virtual_addr data_end);

/**
 * @brief Get symbol value.
 *
 * Search the kernel symbol table for the given symbol \a name and return
 * its value.
 *
 * @param   name    symbol name to look for
 *
 * @return  symbol value on success, NULL if symbol not found.
 */
void *ksym_value(char *name);

#endif      /* __KERNEL_SYMTAB_H__ */
