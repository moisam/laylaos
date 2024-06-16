/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: reboot.h
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
 *  \file reboot.h
 *
 *  Functions and macros to reboot the machine.
 */

#ifndef __KERNEL_REBOOT_H__
#define __KERNEL_REBOOT_H__


#define KERNEL_REBOOT_HALT              (1 << 0)  /**< halt the machine */
#define KERNEL_REBOOT_POWEROFF          (1 << 1)  /**< poweroff the machine */
#define KERNEL_REBOOT_RESTART           (1 << 2)  /**< restart the machine */
#define KERNEL_REBOOT_SUSPEND           (1 << 3)  /**< suspend the machine */


/**
 * @brief Handler for syscall reboot().
 *
 * To reboot/shutdown the system:
 *    - a task does a syscall_reboot() call
 *    - this sends a signal to the init task: SIGINT (for shutdown) or
 *      SIGHUP (for reboot)
 *    - init brings down the system safely (sync files, kill other tasks, etc.)
 *    - init exits
 *    - init's exit code is passed to handle_init_exit(), which performs
 *      the actual shutdown/reboot
 *
 * See: https://man7.org/linux/man-pages/man2/reboot.2.html
 *
 * @param   cmd     one of: \ref KERNEL_REBOOT_HALT, 
 *                          \ref KERNEL_REBOOT_POWEROFF, 
 *                          \ref KERNEL_REBOOT_RESTART, 
 *                          \ref KERNEL_REBOOT_SUSPEND
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_reboot(int cmd);

/**
 * @brief Handler for init exit.
 *
 * This function is called when init (task with pid 1) exits.
 *
 * @param   code    init exit status, which determines whether the system
 *                    reboots or halts (see \ref reboot.c for details)
 *
 * @return  never returns.
 */
void handle_init_exit(int code);

#endif      /* __KERNEL_REBOOT_H__ */
