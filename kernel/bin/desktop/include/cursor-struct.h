/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: cursor-struct.h
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
 *  \file cursor-struct.h
 *
 *  Definition of the cursor id and info structures.
 */

#ifndef CURSOR_STRUCT_H
#define CURSOR_STRUCT_H

// Typedef for cursor ids
typedef uint32_t curid_t;

// Struct to hold mouse cursor info
struct cursor_info_t
{
    int x, y;
    mouse_buttons_t buttons;
    curid_t curid;

#define CURSOR_HIDDEN       0
#define CURSOR_SHOWN        1
    uint32_t flags;
};

#endif      /* CURSOR_STRUCT_H */
