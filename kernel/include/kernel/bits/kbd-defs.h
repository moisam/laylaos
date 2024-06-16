/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: kbd-defs.h
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
 *  \file kbd-defs.h
 *
 *  Keyboard-related structure and macro definitions.
 */

#ifndef KERNEL_KBD_DEFS_H
#define KERNEL_KBD_DEFS_H

#include <stdint.h>

/**
 * @struct keytable_t
 * @brief The keytable_t structure.
 *
 * A structure holding key mapping for a given keyboard layout.
 */
struct keytable_t
{
    char *name;
    uint32_t key[126][8];
};

#endif      /* KERNEL_KBD_DEFS_H */
