/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: window-attrib-struct.h
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
 *  \file window-attrib-struct.h
 *
 *  Definition of the client window attribute structure.
 */

#ifndef WINDOW_ATTRIB_STRUCT_DEFINED
#define WINDOW_ATTRIB_STRUCT_DEFINED    1

struct window_attribs_t
{
    int gravity;
    int16_t x, y;
    uint16_t w, h;
    uint32_t flags;
};

#endif      /* WINDOW_ATTRIB_STRUCT_DEFINED */
