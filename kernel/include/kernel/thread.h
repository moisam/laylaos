/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: thread.h
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
 *  \file thread.h
 *
 *  Functions and macros to work with threads.
 */

#ifndef __THREADS_H__
#define __THREADS_H__

#include <kernel/task.h>
#include <kernel/ids.h>

#define THREADS_PER_PROCESS         512     /**< max threads per task */

/************************
 * Function prototypes
 ************************/

/**
 * @brief Handler for syscall tgkill().
 *
 * Signal a thread in a thread group.
 *
 * @param   tgid        thread group id
 * @param   tid         thread id
 * @param   sig         signal to deliver
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see: https://man7.org/linux/man-pages/man2/tgkill.2.html
 */
long syscall_tgkill(pid_t tgid, pid_t tid, int sig);

/**
 * @brief Handler for syscall gettid().
 *
 * Get calling task's thread id.
 *
 * @return  thread id.
 */
long syscall_gettid(void);

/**
 * @brief Check if task's threads are dead.
 *
 * Helper function to check if the other threads (other than the calling
 * thread) in this task are dead.
 *
 * @param   task        pointer to task
 *
 * @return  1 if all other threads are zombies, 0 if at least one is alive.
 */
int other_threads_dead(volatile struct task_t *task);

/**
 * @brief Terminate thread group.
 *
 * Helper function to terminate (exit) all threads in the currently running
 * task. Does the actual work of the exit_group syscall, which is called by 
 * the _exit() C library function.
 *
 * @param   code        exit code
 *
 * @return  never returns.
 *
 * @see     __terminate_thread_group()
 */
void terminate_thread_group(int code);

/**
 * @brief Terminate thread group.
 *
 * Does the actual work of terminating the thread group.
 *
 * @return  nothing.
 *
 * @see     terminate_thread_group()
 */
void __terminate_thread_group(void);

/**
 * @brief Reap zombie threads.
 *
 * Look for zombie threads in the given \a task and reap them.
 *
 * @param   task            pointer to task
 *
 * @return  nothing.
 */
void reap_dead_threads(struct task_t *task);

/**
 * @brief Get thread group id.
 *
 * Get the thread group id of the given \a task.
 *
 * @param   task            pointer to task
 *
 * @return  thread group id.
 */
long get_tgid(struct task_t *task);

#endif      /* __THREADS_H__ */
