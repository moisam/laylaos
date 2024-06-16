/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: select.h
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
 *  \file select.h
 *
 *  Functions to implement the kernel's I/O selection waiting functionality.
 */

#ifndef __SYS_SELECT_H__
#define	__SYS_SELECT_H__

#include <sys/types.h>
#include "bits/syscall-defs.h"

/**
 * @struct selinfo
 * @brief The selinfo structure.
 *
 * A structure used to maintain information about tasks that wish to be
 * notified when I/O becomes possible.
 */
struct selinfo
{
	int channel;    /**< select wait channel */
};


/***********************
 * Function prototypes
 ***********************/

/**
 * @brief Initialise the select table.
 *
 * Initialize the kernel's system-wide selection table.
 *
 * @return  nothing.
 */
void init_seltab(void);

/**
 * @brief Handler for syscall select().
 *
 * Switch function for I/O selection operations.
 *
 * @param   n           highest file descriptor plus 1
 * @param   readfds     file descriptors to check for possible reading
 * @param   writefds    file descriptors to check for possible writing
 * @param   exceptfds   file descriptors to check for possible exceptions
 * @param   timeout     if not NULL, timeout for the wait
 *
 * @return  zero or positive number on success, -(errno) on failure.
 */
int syscall_select(u_int n, fd_set *readfds, fd_set *writefds,
                   fd_set *exceptfds, struct timeval *timeout);

/**
 * @brief Handler for syscall pselect().
 *
 * Switch function for I/O selection operations.
 *
 * @param   __args      packed syscall arguments (see syscall.h)
 *
 * @return  zero or positive number on success, -(errno) on failure.
 */
int syscall_pselect(struct syscall_args *__args);

/**
 * @brief Record a select request.
 *
 * Called by different device drivers to record a task's wish to be notified
 * of selectable events on the select channel.
 *
 * @param   sip     a select channel struct
 *
 * @return  nothing.
 */
void selrecord(struct selinfo *sip);

/**
 * @brief Wakeup select waiters.
 *
 * Do a wakeup when a selectable event occurs.
 *
 * @param   sip     a select channel struct
 *
 * @return  nothing.
 */
void selwakeup(struct selinfo *sip);

//extern int ffs(int mask);

#endif /* __SYS_SELECT_H__ */
