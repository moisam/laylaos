/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: ksignal.h
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
 *  \file ksignal.h
 *
 *  Functions and macros to work with task signals.
 */

#ifndef __KSIGNAL_H__
#define __KSIGNAL_H__

#include <kernel/task.h>


/**
 * \def __W_EXITCODE
 * Create an exit code from the return code \a ret and the signal number \a sig
 */
#define __W_EXITCODE(ret, sig)   ((ret) << 8 | (sig))

/**
 * \def __W_STOPCODE
 * Create a stop code from signal number \a sig
 */
#define __W_STOPCODE(sig)        ((sig) << 8 | 0x7f)

/**
 * \def __W_CONTINUED
 * Task status that is returned if the task has been continued by SIGCONT
 */
#define __W_CONTINUED            0xffff

/**
 * \def __WCOREFLAG
 * Set in the task status if the task has core dumped
 */
#define __WCOREFLAG              0x80

/**
 * \def WEXITSTATUS
 * Extract the exit code from \a status
 */
#undef WEXITSTATUS
#define WEXITSTATUS(status)     (((status) & 0xff00) >> 8)

/**
 * \def WTERMSIG
 * Extract the termination signal number from \a status
 */
#undef WTERMSIG
#define WTERMSIG(status)        ((status) & 0x7f)

/**
 * \def WSTOPSIG
 * Extract the stop signal number from \a status
 */
#undef WSTOPSIG
#define WSTOPSIG(status)        WEXITSTATUS(status)

/**
 * \def WIFEXITED
 * Check if the task has exited normally
 */
#undef WIFEXITED
#define WIFEXITED(status)       (WTERMSIG(status) == 0)

/**
 * \def WIFSIGNALED
 * Check if the task has been terminated by a signal
 */
#undef WIFSIGNALED
#define WIFSIGNALED(status)     \
    (((signed char) (((status) & 0x7f) + 1) >> 1) > 0)

/**
 * \def WIFSTOPPED
 * Check if the task has been stopped by a signal
 */
#undef WIFSTOPPED
#define WIFSTOPPED(status)      (((status) & 0xff) == 0x7f)

/**
 * \def WCOREDUMP
 * Check if the task has core dumped
 */
#undef WCOREDUMP
#define WCOREDUMP(status)       ((status) & __WCOREFLAG)

/**
 * \def WIFCONTINUED
 * Check if the task has been continued by SIGCONT
 */
#undef WIFCONTINUED
#define WIFCONTINUED(status)    ((status) == __W_CONTINUED)


/************************
 * Functions
 ************************/

/**
 * @brief Sigset to unsigned long.
 *
 * Convert the given sigset_t to an unsigned long signal bitmask.
 *
 * @param   set         sigset_t to convert
 *
 * @return  unsigned long signal bitmask.
 */
unsigned long sigset_to_ulong(sigset_t *set);

/**
 * @brief Get ignored task signals.
 *
 * Get the signals ignored by the given task as an unsigned long integer.
 *
 * @param   task        task whose ignored signals to return
 *
 * @return  unsigned long signal bitmask.
 */
unsigned long get_ignored_task_signals(struct task_t *task);

/**
 * @brief Initialise signals.
 *
 * Called during boot time when the tasking service is initialized.
 *
 * @return  nothing.
 */
void init_signals(void);

/**
 * @brief Check and dispatch pending signals.
 *
 * Check if the current task has at least one pending deliverable signal and 
 * call the signal dispatcher if so.
 *
 * @param   r           current CPU registers
 *
 * @return  nothing.
 */
void check_pending_signals(struct regs *r);

/**
 * @brief Handler for syscall sigreturn().
 *
 * Syscall for returning from a user signal handler.
 *
 * @param   user_stack  pointer to the user stack, from which the previous
 *                        task context is restored
 *
 * @return  never returns.
 */
int syscall_sigreturn(uintptr_t user_stack);

/**
 * @brief Save task sigmask.
 *
 * Save the current task signal mask.
 *
 * @return  nothing.
 */
void save_sigmask(void);

/**
 * @brief Restore task sigmask.
 *
 * Restore the signal mask from the saved mask.
 * This is called by syscall_sigreturn() upon return from user signal handler.
 *
 * @return  nothing.
 */
void restore_sigmask(void);

/**
 * @brief Handler for syscall sigaction().
 *
 * Set the task's signal handler for the given signal and return the old
 * signal handler if needed.
 *
 * @param   signum  signal number
 * @param   newact  if not NULL, the new signal handler
 * @param   oldact  if not NULL, the old signal handler is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_sigaction(int signum,
                      struct sigaction* newact, struct sigaction* oldact);

/**
 * @brief Handler for syscall signal().
 *
 * This is not implemented. Set signal handlers using sigaction() instead.
 *
 * @param   signum      unused
 * @param   handler     unused
 * @param   sa_restorer unused
 *
 * @return  -(ENOSYS) always.
 */
int syscall_signal(int signum, void *handler, void *sa_restorer);

/**
 * @brief Handler for syscall sigpending().
 *
 * Check for pending signals.
 *
 * @param   set     the signal mask of the pending signals is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_sigpending(sigset_t* set);

/**
 * @brief Handler for syscall sigtimedwait().
 *
 * Wait for queued signals.
 *
 * @param   set     the signal mask of the signals to wait for
 * @param   info    info about signal is returned here (if non-NULL)
 * @param   ts      timeout (if non-NULL)
 *
 * @return  signal number on success, -(errno) on failure.
 */
int syscall_sigtimedwait(sigset_t *set, siginfo_t *info,
                         struct timespec *ts);

/**
 * @brief Handler for syscall sigprocmask().
 *
 * Set the task's signal mask and return the old mask if needed.
 *
 * @param   how         one of: SIG_BLOCK, SIG_UNBLOCK, or SIG_SETMASK
 * @param   userset     if not NULL, the new signal mask
 * @param   oldset      if not NULL, the signal mask is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_sigprocmask(int how, sigset_t *userset, sigset_t *oldset);

/**
 * @brief Handler for syscall sigsuspend().
 *
 * Suspend task signals.
 *
 * @param   set     the signal mask of the signals to suspend
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_sigsuspend(sigset_t *set);

/**
 * @brief Core dump.
 *
 * Dump the task's core before it exits. Currently not implemented.
 *
 * @return  nothing.
 */
void dump_core(void);

/**
 * @brief Handler for syscall signalstack().
 *
 * Set the task's signal stack.
 *
 * @param   ss      if not NULL, the new signal stack
 * @param   old_ss  if not NULL, the old signal stack is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_signaltstack(stack_t *ss, stack_t *old_ss);

/**
 * @brief Add a signal to a task.
 *
 * Signal number \a signum is added to the task's pending signals, and the 
 * task is awakened if it is sleeping.
 *
 * @param   t           task to receive the signal
 * @param   signum      signal number
 * @param   siginfo     additional signal info
 * @param   force       if non-zero, the signal is delivered even if the task
 *                        is not owned by the user
 *
 * @return  zero on success, -(errno) on failure.
 */
int add_task_signal(struct task_t *t, int signum, 
                    siginfo_t *siginfo, int force);

/**
 * @brief Add a signal to a task from a user task.
 *
 * Signal number \a signum is added to the task's pending signals, and the 
 * task is awakened if it is sleeping. This is a shorthand to calling
 * add_task_signal() with a siginfo containing SI_USER and the calling
 * task's uid and pid.
 *
 * @param   t           task to receive the signal
 * @param   signum      signal number
 * @param   force       if non-zero, the signal is delivered even if the task
 *                        is not owned by the user
 *
 * @return  zero on success, -(errno) on failure.
 */
int user_add_task_signal(struct task_t *t, int signum, int force);

/**
 * @brief Add a signal to a task from one of its children.
 *
 * Signal number \a signum is added to the task's pending signals, and the 
 * task is awakened if it is sleeping. This is a shorthand to calling
 * add_task_signal() to deliver SIGCHLD, but it also fills the appropriate
 * values for siginfo.
 *
 * @param   t           task to receive the signal
 * @param   code        code number to add to siginfo
 * @param   status      calling task's exit status
 *
 * @return  zero on success, -(errno) on failure.
 */
int add_task_child_signal(struct task_t *t, int code, int status);

/**
 * @brief Add a timer signal to a task.
 *
 * Signal number \a signum is added to the task's pending signals, and the 
 * task is awakened if it is sleeping. This is a shorthand to calling
 * add_task_signal() with a siginfo containing SI_TIMER and the given timeid.
 *
 * @param   t           task to receive the signal
 * @param   signum      signal number
 * @param   timerid     timer id
 *
 * @return  zero on success, -(errno) on failure.
 */
int add_task_timer_signal(struct task_t *t, int signum, ktimer_t timerid);

/**
 * @brief Add a SIGSEGV signal to a task.
 *
 * Signal number \a signum is added to the task's pending signals, and the 
 * task is awakened if it is sleeping. This is a shorthand to calling
 * add_task_signal() to deliver SIGSEGV, but it also fills the appropriate
 * values for siginfo.
 *
 * @param   t           task to receive the signal
 * @param   signum      signal number
 * @param   code        code number to add to siginfo
 * @param   addr        the offending address causing this segment
 *                        violation signal
 *
 * @return  zero on success, -(errno) on failure.
 */
int add_task_segv_signal(struct task_t *t, int signum, int code, void *addr);


/***************************
 * Helper functions
 ***************************/

int syscall_sigpending_internal(sigset_t* set, int kernel);

int syscall_sigprocmask_internal(struct task_t *ct, int how,
                                 sigset_t* userset,
                                 sigset_t* oldset, int kernel);

/*
 * Helper function defined in syscall_dispatcher.S
 */
extern void do_user_sighandler(uintptr_t stack, void (*handler)());

#endif      /* __KSIGNAL_H__ */
