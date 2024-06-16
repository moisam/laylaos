/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: endian.h
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
 *  \file endian.h
 *
 *  Inlined functions for swapping words and double words from little-endian
 *  to big-endian and vice versa. They are used by the image loading functions
 *  in the library.
 */

#ifndef GUI_ENDIAN_H
#define GUI_ENDIAN_H

//This function will swap the bytes from big-endian to little-endian
//and vice versa, for 32-bit double words.
//This code was adopted with many thanks from Stackoverflow forum:
//http://stackoverflow.com/questions/2182002/convert-big-endian-to-little-
//		endian-in-c-without-using-provided-func
static inline uint32_t swap_dword(uint32_t x)
{
	return ((x >> 24) & 0xff) |      // move byte 3 to byte 0
               ((x << 8) & 0xff0000) |   // move byte 1 to byte 2
               ((x >> 8) & 0xff00) |     // move byte 2 to byte 1
               ((x << 24) & 0xff000000); // byte 0 to byte 3
}

//This function will swap the bytes from big-endian to little-endian
//and vice versa, for 16-bit words.
static inline uint16_t swap_word(uint16_t x)
{
	return ((x >> 8) & 0xff) |      // move byte 1 to byte 0
               ((x << 8) & 0xff00);    // move byte 0 to byte 1
}

#endif      /* GUI_ENDIAN_H */
