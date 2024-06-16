/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: bitsperlong.h
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
 *  \file bitsperlong.h
 *
 *  Defines the number of bits per long, depending on the architecture.
 */

#ifndef BITSPERLONG_H
#define BITSPERLONG_H

#if defined __x86_64__
# define BITS_PER_LONG          64
#else
# define BITS_PER_LONG          32
#endif

#define __BITS_PER_LONG         BITS_PER_LONG

#endif      /* BITSPERLONG_H */
