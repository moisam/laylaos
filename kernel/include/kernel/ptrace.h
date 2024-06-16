/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: ptrace.h
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
 *  \file ptrace.h
 *
 *  Functions to implement the kernel's process tracing functionality.
 */

#ifndef __KERNEL_PTRACE_H__
#define __KERNEL_PTRACE_H__

/**
 * @brief Kill task's tracees.
 *
 * Send a SIGKILL signal to tracees if the tracer exits.
 *
 * @param   tracer      pointer to the tracer task
 *
 * @return  nothing.
 */
void ptrace_kill_tracees(struct task_t *tracer);

/**
 * @brief Clear ptrace state.
 *
 * Remove the \a tracee from its tracer's list and reset the tracee's
 * internal ptrace state.
 *
 * @param   tracee      pointer to the tracee task
 *
 * @return  nothing.
 */
void ptrace_clear_state(struct task_t *tracee);

/**
 * @brief Copy ptrace state.
 *
 * Copy \a tracee's internal ptrace state to \a ptracee2.
 *
 * @param   tracee2     pointer to the destination tracee task
 * @param   tracee      pointer to the source tracee task
 *
 * @return  nothing.
 */
void ptrace_copy_state(struct task_t *tracee2, struct task_t *tracee);

/**
 * @brief Signal tracer.
 *
 * If the calling task is being traced, this call notifies the tracer of
 * a ptrace event and then blocks the calling task until the tracer
 * unblocks it.
 *
 * @param   signum      signal number to assign to the tracee's state
 * @param   reason      must be one of the PTRACE_EVENT_* macros defined
 *                        in sys/ptrace.h
 *
 * @return  \a signum.
 */
int ptrace_signal(int signum, int reason);

/**
 * @brief Handler for syscall ptrace().
 *
 * Switch function for ptrace requests and actions.
 *
 * @param   request     action to be performed
 * @param   pid         target task's TID
 * @param   addr        address (depends on \a request)
 * @param   data        data (depends on \a request)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_ptrace(int request, pid_t pid, void *addr, void *data);


#ifndef SI_KERNEL
#define SI_KERNEL  0x80 /* Sent by the kernel from somewhere */
#endif      /* !SI_KERNEL */

#endif      /* __KERNEL_PTRACE_H__ */
