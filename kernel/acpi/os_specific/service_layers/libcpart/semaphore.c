/*
 * Code in this file is largely based on the semaphore implementation from
 * Sortix. The original code is released under the license below.
 * Other bits are released under the same lincense, copyright (c) 2022-2025
 * Mohammed Isam [mailto:mohammed_isam1984@yahoo.com].
 *
 * Copyright (c) 2014 Jonas 'Sortie' Termansen.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _POSIX_PRIORITY_SCHEDULING
#define _POSIX_MONOTONIC_CLOCK

//#define __USE_XOPEN2K           // for sem_timedwait()

#include <errno.h>
#include <sched.h>
//#include <semaphore.h>
#include <signal.h>
#include <limits.h>
#include <stdint.h>
#include <stdatomic.h>
#include <time.h>
#include <kernel/laylaos.h>
#include <kernel/clock.h>
#include <kernel/syscall.h>
#include <kernel/ksignal.h>
#include <kernel/ksigset.h>

typedef struct
{
    int volatile value;
} acpi_sem_t;

int acpi_sem_init(acpi_sem_t *sem, int pshared, unsigned int value);
int acpi_sem_destroy(acpi_sem_t *sem);
acpi_sem_t *acpi_sem_open(const char *name, int oflag, ...);
int acpi_sem_close(acpi_sem_t *sem);
int acpi_sem_unlink(const char *name);
int acpi_sem_wait(acpi_sem_t *sem);
int acpi_sem_timedwait(acpi_sem_t *__restrict sem,
                       const struct timespec *__restrict abstime);
int acpi_sem_trywait(acpi_sem_t *sem);
int acpi_sem_post(acpi_sem_t *sem);
int acpi_sem_getvalue(acpi_sem_t *__restrict sem, int *__restrict sval);

int __timespec_to_absolute(struct timespec*  ts, const struct timespec*  abstime);

// from pthreads/mutex.c
/* initialize 'ts' with the difference between 'abstime' and the current time
 * according to 'clock'. Returns -1 if abstime already expired, or 0 otherwise.
 */
/* static */ int
__timespec_to_absolute(struct timespec*  ts, const struct timespec*  abstime /* , clockid_t  clock */)
{
    ts->tv_sec  = monotonic_time.tv_sec;
    ts->tv_nsec = monotonic_time.tv_nsec;
    //clock_gettime(clock, ts);

    ts->tv_sec  = abstime->tv_sec - ts->tv_sec;
    ts->tv_nsec = abstime->tv_nsec - ts->tv_nsec;

    if(ts->tv_nsec < 0)
    {
        ts->tv_sec--;
        ts->tv_nsec += 1000000000;
    }

    if((ts->tv_nsec < 0) || (ts->tv_sec < 0))
        return -1;
    
    return 0;
}


/* Initialize semaphore object SEM to VALUE.  If PSHARED then share it
   with other processes.  */
int acpi_sem_init(acpi_sem_t *sem, int pshared, unsigned int value)
{
    if(pshared)
    {
        return -ENOSYS;
    }

    if(!sem || value > (unsigned int)INT_MAX)
    {
        return -EINVAL;
    }

    sem->value = (int)value;

    return 0;
}


/* Free resources associated with semaphore object SEM.  */
int acpi_sem_destroy(acpi_sem_t *sem)
{
    UNUSED(sem);

    return 0;
}


/* Open a named semaphore NAME with open flags OFLAG.  */
acpi_sem_t *acpi_sem_open(const char *name, int oflag, ...)
{
    UNUSED(name);
    UNUSED(oflag);

    return NULL;    // -1;
}


/* Close descriptor for named semaphore SEM.  */
int acpi_sem_close(acpi_sem_t *sem)
{
    UNUSED(sem);

    return -ENOSYS;
}


/* Remove named semaphore NAME.  */
int acpi_sem_unlink(const char *name)
{
    UNUSED(name);

    return -ENOSYS;
}


/* Wait for SEM being posted. */
int acpi_sem_wait(acpi_sem_t *sem)
{
    int err;
    struct task_t *ct = (struct task_t *)get_cur_task();
    //struct task_t *ct = cur_task;

    if(!sem)
    {
        return -EINVAL;
    }

    if((err = acpi_sem_trywait(sem)) == 0)
    {
        return 0;
    }
    
    if(err != -EAGAIN)
    {
        return err;
    }

    sigset_t old_set_mask;
    sigset_t old_set_allowed;
    sigset_t all_signals;
    ksigfillset(&all_signals);
    syscall_sigprocmask_internal(ct, SIG_SETMASK, &all_signals, &old_set_mask, 1);
    ksignotset(&old_set_allowed, &old_set_mask);

    while((err = acpi_sem_trywait(sem)) != 0)
    {
        if(err == -EAGAIN && syscall_sigpending_internal(&old_set_allowed, 1))
        {
            err = -EINTR;
        }

        if(err != -EAGAIN)
        {
            syscall_sigprocmask_internal(ct, SIG_SETMASK, &old_set_mask, NULL, 1);
            return err;
        }

        syscall_sched_yield();
    }

    syscall_sigprocmask_internal(ct, SIG_SETMASK, &old_set_mask, NULL, 1);

    return 0;
}


/* Similar to `sem_wait' but wait only until ABSTIME. */
int acpi_sem_timedwait(acpi_sem_t *__restrict sem,
                       const struct timespec *__restrict abstime)
{
    int err;
    struct task_t *ct = (struct task_t *)get_cur_task();
    //struct task_t *ct = cur_task;

    if(!sem)
    {
        return -EINVAL;
    }

    if((err = acpi_sem_trywait(sem)) == 0)
    {
        return 0;
    }
    
    if(err != -EAGAIN)
    {
        return err;
    }

    sigset_t old_set_mask;
    sigset_t old_set_allowed;
    sigset_t all_signals;
    ksigfillset(&all_signals);
    syscall_sigprocmask_internal(ct, SIG_SETMASK, &all_signals, &old_set_mask, 1);
    ksignotset(&old_set_allowed, &old_set_mask);

    while((err = acpi_sem_trywait(sem)) != 0)
    {
        if(err == -EAGAIN)
        {
            struct timespec now;

            if(__timespec_to_absolute(&now, abstime /* , CLOCK_MONOTONIC */) < 0)
            {
                err = -ETIMEDOUT;
            }
            
            /*
            struct timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            if ( timespec_le(*abstime, now) )
                errno = ETIMEDOUT;
            */
        }

        if(err == -EAGAIN && syscall_sigpending_internal(&old_set_allowed, 1))
        {
            err = -EINTR;
        }

        if(err != -EAGAIN)
        {
            syscall_sigprocmask_internal(ct, SIG_SETMASK, &old_set_mask, NULL, 1);
            return err;
        }

        syscall_sched_yield();
    }

    syscall_sigprocmask_internal(ct, SIG_SETMASK, &old_set_mask, NULL, 1);

    return 0;
}


/* Test whether SEM is posted.  */
int acpi_sem_trywait(acpi_sem_t *sem)
{
    if(!sem)
    {
        return -EINVAL;
    }

    int old_value = __atomic_load_n(&sem->value, __ATOMIC_SEQ_CST);

    if(old_value <= 0)
    {
        return -EAGAIN;
    }
    
    int new_value = old_value - 1;

    if(!__atomic_compare_exchange_n(&sem->value, &old_value, new_value, 0,
                                    __ATOMIC_SEQ_CST, __ATOMIC_RELAXED))
    {
        return -EAGAIN;
    }
    
    return 0;
}


/* Post SEM.  */
int acpi_sem_post(acpi_sem_t *sem)
{
    if(!sem)
    {
        return -EINVAL;
    }

    while(1)
    {
        //printk("sem_post: sem 0x%lx\n", sem);
        volatile int old_value = __atomic_load_n(&sem->value, __ATOMIC_SEQ_CST);
        //printk("sem_post: old_value %d\n", old_value);

        if(old_value == INT_MAX)
        {
            return -EOVERFLOW;
        }

        volatile int new_value = old_value + 1;

        if(!__atomic_compare_exchange_n(&sem->value, (void *)&old_value, new_value, 0,
                                        __ATOMIC_SEQ_CST, __ATOMIC_RELAXED))
        {
            continue;
        }

        return 0;
    }
}


/* Get current value of SEM and store it in *SVAL.  */
int acpi_sem_getvalue(acpi_sem_t *__restrict sem, int *__restrict sval)
{
    if(!sem || !sval)
    {
        return -EINVAL;
    }

    *sval = __atomic_load_n(&sem->value, __ATOMIC_SEQ_CST);
    return 0;
}

