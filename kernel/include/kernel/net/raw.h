/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: raw.h
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
 *  \file raw.h
 *
 *  Functions and macros for handling RAW network packets.
 */

#ifndef RAW_H
#define RAW_H

#define RAWTTL                  255              // RAW time to live

// externs defined in raw.c
extern struct sockops_t raw_sockops;


/*********************************************
 * Functions defined in network/raw.c
 *********************************************/

int raw_input(struct packet_t *p);

#endif      /* RAW_H */
