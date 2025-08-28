/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: signal_funcs.c
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
 *  \file signal_funcs.h
 *
 *  Inlined functions for the kernel signal dispatcher use.
 */

#include <kernel/detect-libc.h>
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <kernel/laylaos.h>
#include <kernel/ksigset.h>
#include <kernel/user.h>


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
STATIC_INLINE long user_add_task_signal(volatile struct task_t *t, int signum, int force)
{
    siginfo_t siginfo = {
                         .si_code = SI_USER,
                         .si_pid = this_core->cur_task->pid,
                         .si_uid = this_core->cur_task->uid,
                        };

    return add_task_signal((struct task_t *)t, signum, &siginfo, force);
}


/**
 * @brief Add a SIGSEGV signal to a task.
 *
 * Signal number SIGSEGV is added to the task's pending signals, and the 
 * task is awakened if it is sleeping. This is a shorthand to calling
 * add_task_signal() to deliver SIGSEGV, but it also fills the appropriate
 * values for siginfo.
 *
 * @param   t           task to receive the signal
 * @param   code        code number to add to siginfo
 * @param   addr        the offending address causing this segment
 *                        violation signal
 *
 * @return  zero on success, -(errno) on failure.
 */
STATIC_INLINE long add_task_segv_signal(volatile struct task_t *t, int code, void *addr)
{
    siginfo_t siginfo = {
                         .si_code = code,
                         .si_addr = addr,
                        };

    return add_task_signal((struct task_t *)t, SIGSEGV, &siginfo, 1);
}


/**
 * @brief Add a SIGFPE signal to a task.
 *
 * Signal number SIGFPE is added to the task's pending signals, and the 
 * task is awakened if it is sleeping. This is a shorthand to calling
 * add_task_signal() to deliver SIGFPE, but it also fills the appropriate
 * values for siginfo.
 *
 * @param   t           task to receive the signal
 * @param   code        code number to add to siginfo
 * @param   addr        the offending address causing this segment
 *                        violation signal
 *
 * @return  zero on success, -(errno) on failure.
 */
STATIC_INLINE long add_task_fpe_signal(volatile struct task_t *t, int code, void *addr)
{
    siginfo_t siginfo = {
                         .si_code = code,
                         .si_addr = addr,
                        };

    return add_task_signal((struct task_t *)t, SIGFPE, &siginfo, 1);
}


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
STATIC_INLINE long add_task_timer_signal(volatile struct task_t *t, int signum, ktimer_t timerid)
{
    siginfo_t siginfo = {
                         .si_code = SI_TIMER,
                         .si_value.sival_int = timerid,
                         //.si_value.sival_ptr = ptr,
                        };

    //COPY_TO_USER(ptr, &timerid, sizeof(ktimer_t));
    ksigaddset((sigset_t *)&t->signal_timer, signum);

    return add_task_signal((struct task_t *)t, signum, &siginfo, 1);
}


/**
 * @brief Add a signal to a task from one of its children.
 *
 * Signal number \a signum is added to the task's pending signals, and the 
 * task is awakened if it is sleeping. This is a shorthand to calling
 * add_task_signal() to deliver SIGCHLD, but it also fills the appropriate
 * values for siginfo. Field 'status' contains the task's exit status
 * (and code should be CLD_EXITED), or the signal number that caused the
 * task to change state.
 *
 * @param   t           task to receive the signal
 * @param   code        code number to add to siginfo
 * @param   status      calling task's exit status
 *
 * @return  zero on success, -(errno) on failure.
 */
STATIC_INLINE long add_task_child_signal(volatile struct task_t *t, int code, int status)
{
    if(!t->parent)
    {
        return 0;
    }

    // parent might want to block SIGCHLD and wait for us to change
    // status by calling one of the wait() functions, in which case it
    // will be blocked and we need to wake it up
    if(t->parent->properties & PROPERTY_IN_WAIT)
    {
        unblock_task_no_preempt(t->parent);
        return 0;
    }

    // check if parent cares about us changing status
    if(t->parent->sig)
    {
        struct sigaction *act = &t->parent->sig->signal_actions[SIGCHLD];

        if((act->sa_handler != SIG_IGN) && !(act->sa_flags & SA_NOCLDSTOP))
        {
            siginfo_t siginfo = {
                         .si_code = code,
                         .si_pid = t->pid,
                         .si_uid = t->uid,
                         .si_status = status,
                         .si_utime = t->user_time,
                         .si_stime = t->sys_time,
                        };

            return add_task_signal(t->parent, SIGCHLD, &siginfo, 1);
        }
    }
    
    return 0;
}


/**********************************************************************
 *
 * Inlined functions for internal use by the signal handling facility.
 *
 **********************************************************************/

#ifdef __MUSL__

#define SST_SIZE (_NSIG/8/sizeof(long))

STATIC_INLINE void copy_sigset(sigset_t *dest, sigset_t *src)
{
    unsigned long i = 0, *d = (void *)dest, *s = (void *)src;

    for( ; i < SST_SIZE; i++)
    {
        d[i] = s[i];
    }
}

STATIC_INLINE long copy_sigset_to_user(sigset_t *dest, sigset_t *src)
{
    unsigned long i = 0, *d = (void *)dest, *s = (void *)src;

    for( ; i < SST_SIZE; i++)
    {
        COPY_VAL_TO_USER(&d[i], &s[i]);
    }

    return 0;
}

STATIC_INLINE long copy_sigset_from_user(sigset_t *dest, sigset_t *src)
{
    unsigned long i = 0, *d = (void *)dest, *s = (void *)src;

    for( ; i < SST_SIZE; i++)
    {
        COPY_VAL_FROM_USER(&d[i], &s[i]);
    }

    return 0;
}

#elif defined(__NEWLIB__)

// sigset_t is unsigned long on newlib

STATIC_INLINE void copy_sigset(sigset_t *dest, sigset_t *src)
{
    *dest = *src;
}

STATIC_INLINE long copy_sigset_to_user(sigset_t *dest, sigset_t *src)
{
    COPY_VAL_TO_USER(dest, src);
    return 0;
}

STATIC_INLINE long copy_sigset_from_user(sigset_t *dest, sigset_t *src)
{
    COPY_VAL_FROM_USER(dest, src);
    return 0;
}

#else

// fallbacks for other libc implementations

STATIC_INLINE void copy_sigset(sigset_t *dest, sigset_t *src)
{
    A_memcpy(dest, src, sizeof(sigset_t));
}

STATIC_INLINE long copy_sigset_to_user(sigset_t *dest, sigset_t *src)
{
    return copy_to_user(dest, src, sizeof(sigset_t));
}

STATIC_INLINE long copy_sigset_from_user(sigset_t *dest, sigset_t *src)
{
    return copy_from_user(dest, src, sizeof(sigset_t));
}

#endif
