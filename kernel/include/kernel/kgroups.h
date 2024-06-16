/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: kgroups.h
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
 *  \file kgroups.h
 *
 *  Functions and macros to work with kernel groups. These groups include 
 *  daemon, sys, admin, tty and kmem.
 */

#ifndef __KGROUP_H__
#define __KGROUP_H__

#include <sys/types.h>

/*
 * These are indices into the kgroups array, they ARE NOT the gid's!
 */
#define KGROUP_NOGROUP      0
#define KGROUP_DAEMON       1
#define KGROUP_SYS          2
#define KGROUP_ADMIN        3
#define KGROUP_TTY          4
#define KGROUP_KMEM         5

#define KGROUP_LAST         5


/**
 * @brief Initialise kgroups.
 *
 * Initialise kernel groups by reading /etc/groups. If the file is not 
 * available or any other error occurs, kgroups are initialised to builtin
 * defaults (see kgroups.c for details).
 *
 * @return  nothing.
 */
void kgroups_init(void);

/*
 * Helper function.
 */
gid_t get_kgroup(int i);

#endif      /* __KGROUP_H__ */
