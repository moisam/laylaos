/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: posix_timers.c
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
 *  \file posix_timers.c
 *
 *  The kernel's per-process POSIX timer implementation.
 */

//#define __DEBUG

#define _POSIX_MONOTONIC_CLOCK
#define DEFINE_POSIX_TIMER_INLINES

#include <errno.h>
#include <time.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/clock.h>
#include <kernel/task.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <kernel/ksignal.h>
#include <kernel/user.h>
#include <mm/kheap.h>

#include "../kernel/task_funcs.c"
#include "posix_timers_inlines.h"


/*
 * Get POSIX timer.
 */
struct posix_timer_t *get_posix_timer(pid_t tgid, ktimer_t timerid)
{
    struct posix_timer_t *timer;
    volatile struct task_t *task = get_task_by_tgid(tgid);
    
    if(!task || !task->common)
    {
        printk("kernel: trying to get POSIX timer for a NULL task (pid %d)\n",
            task ? task->pid : -1);

        kpanic("Invalid POSIX timer task");
        //screen_refresh(NULL);
        __asm__ __volatile__("xchg %%bx, %%bx"::);

        return NULL;
    }

    for(timer = task->posix_timers; timer != NULL; timer = timer->next)
    {
        if(timer->timerid == timerid)
        {
            return timer;
        }
    }

    return NULL;
}


/*
 * Handler for syscall timer_settime().
 */
long syscall_timer_settime(ktimer_t timerid, int flags,
                           struct itimerspec *new_value,
                           struct itimerspec *old_value)
{
    long res;
    struct itimerspec newval;
    struct posix_timer_t *timer;
    struct clock_waiter_t *head;
	volatile struct task_t *ct = this_core->cur_task;

    if(!timerid)
    {
        return -EINVAL;
    }
    
    kernel_mutex_lock(&ct->common->mutex);

    if(!(timer = get_posix_timer(tgid(ct), timerid)))
    //if(!(timer = get_posix_timer(ct, timerid)))
    {
        kernel_mutex_unlock(&ct->common->mutex);
        return -EINVAL;
    }
    
    if(old_value)
    {
        if(copy_to_user(old_value, &timer->val, 
                            sizeof(struct itimerspec)) != 0)
        {
            kernel_mutex_unlock(&ct->common->mutex);
            return -EFAULT;
        }
    }
    
    if(new_value)
    {
        if(copy_from_user(&newval, new_value, 
                            sizeof(struct itimerspec)) != 0)
        {
            kernel_mutex_unlock(&ct->common->mutex);
            return -EFAULT;
        }
        
        // remove old timer if it was active
        head = &waiter_head[(timer->clockid == CLOCK_REALTIME) ? 1 : 0];
        timer_unwait(head, tgid(ct), timer->timerid);
        //timer_unwait(head, ct, timer->timerid);
        
        // arm the new timer if needed
        if(newval.it_value.tv_sec || newval.it_value.tv_nsec)
        {
            KDEBUG("syscall_timer_settime: sec %ld\n", newval.it_value.tv_sec);
            KDEBUG("syscall_timer_settime: nsec %ld\n", newval.it_value.tv_nsec);

            res = do_clock_nanosleep(tgid(ct), timer->clockid, flags,
            //res = do_clock_nanosleep(ct, timer->clockid, flags,
                                     &newval.it_value, NULL, timer->timerid);

            KDEBUG("syscall_timer_settime: res %d, id %d\n", res, timer->timerid);

            // time has already passed (otherwise we should get -EINTR)
            if(res == 0 || res == -EINVAL)
            {
                A_memset(&timer->val, 0, sizeof(struct itimerspec));
                kernel_mutex_unlock(&ct->common->mutex);
                return 0;
            }
        }

        A_memcpy(&timer->val, &newval, sizeof(struct itimerspec));
        timer->flags = flags;
        timer->cur_overruns = 0;
        timer->saved_overruns = 0;
        //timer->interval_ticks = timespec_to_ticks(&newval.it_interval);
    }

    kernel_mutex_unlock(&ct->common->mutex);
    
    return 0;
}


long timer_gettime_internal(ktimer_t timerid, struct itimerspec *curr_value, int kernel)
{
    struct itimerspec oldval;
    struct posix_timer_t *timer;
    struct clock_waiter_t *head;
    int64_t remaining_ticks;
	volatile struct task_t *ct = this_core->cur_task;

    if(!timerid)
    {
        return -EINVAL;
    }
    
    kernel_mutex_lock(&ct->common->mutex);

    if(!(timer = get_posix_timer(tgid(ct), timerid)))
    {
        kernel_mutex_unlock(&ct->common->mutex);
        return -EINVAL;
    }

    A_memset(&oldval, 0, sizeof(struct itimerspec));
    head = &waiter_head[(timer->clockid == CLOCK_REALTIME) ? 1 : 0];

    if(get_waiter(head, tgid(ct), timerid, &remaining_ticks, 0))
    {
        ticks_to_timespec(remaining_ticks, &oldval.it_value);
    }
    
    A_memcpy(&oldval.it_interval, &timer->val.it_interval,
                sizeof(struct timespec));

    kernel_mutex_unlock(&ct->common->mutex);

    if(kernel)
    {
        A_memcpy(curr_value, &oldval, sizeof(struct itimerspec));
        return 0;
    }
    else
    {
        return copy_to_user(curr_value, &oldval, sizeof(struct itimerspec));
    }
}


/*
 * Handler for syscall timer_gettime().
 */
long syscall_timer_gettime(ktimer_t timerid, struct itimerspec *curr_value)
{
    return timer_gettime_internal(timerid, curr_value, 0);
}


/*
 * Handler for syscall timer_create().
 */
long syscall_timer_create(clockid_t clockid, struct sigevent *sevp,
                          ktimer_t *timerid)
{
    struct sigevent ev;
    struct posix_timer_t *timer;
	volatile struct task_t *ct = this_core->cur_task;

    /* NOTE: for now, we only support those two */
    if(clockid != CLOCK_REALTIME && clockid != CLOCK_MONOTONIC)
    {
        return -EINVAL;
    }
    
    if(sevp)
    {
        COPY_FROM_USER(&ev, sevp, sizeof(struct sigevent));
        
        /* NOTE: for now, we only support those two */
        if(ev.sigev_notify != SIGEV_SIGNAL && ev.sigev_notify != SIGEV_NONE)
        {
            return -EINVAL;
        }
        
        if(ev.sigev_notify == SIGEV_SIGNAL &&
           (ev.sigev_signo < 1 || ev.sigev_signo >= NSIG))
        {
            return -EINVAL;
        }
    }
    else
    {
        ev.sigev_notify = SIGEV_SIGNAL;
        ev.sigev_signo = SIGALRM;
        ev.sigev_value.sival_int = 0;
    }
    
    if(!(timer = (struct posix_timer_t *)
                    kmalloc(sizeof(struct posix_timer_t))))
    {
        return -ENOMEM;
    }
    
    A_memset(timer, 0, sizeof(struct posix_timer_t));
    A_memcpy(&timer->sigev, &ev, sizeof(struct sigevent));
    timer->clockid = clockid;
    
    kernel_mutex_lock(&ct->common->mutex);
    
    timer->timerid = ++ct->last_timerid;
    timer->next = ct->posix_timers;
    ct->posix_timers = timer;

    kernel_mutex_unlock(&ct->common->mutex);
    
    return copy_to_user(timerid, &timer->timerid, sizeof(ktimer_t));
}


static void free_timer(volatile struct posix_timer_t *timer)
{
    if(timer->timerid != ITIMER_REAL_ID &&
       timer->timerid != ITIMER_PROF_ID &&
       timer->timerid != ITIMER_VIRT_ID)
    {
        kfree((void *)timer);
    }
}


/*
 * Handler for syscall timer_delete().
 */
long syscall_timer_delete(ktimer_t timerid)
{
    struct posix_timer_t *timer, *tmp;
    struct clock_waiter_t *head;
	volatile struct task_t *ct = this_core->cur_task;

    if(!timerid)
    {
        return -EINVAL;
    }

    kernel_mutex_lock(&ct->common->mutex);

    if(!(timer = get_posix_timer(tgid(ct), timerid)))
    {
        kernel_mutex_unlock(&ct->common->mutex);
        return -EINVAL;
    }

    // remove timer if it is armed
    head = &waiter_head[(timer->clockid == CLOCK_REALTIME) ? 1 : 0];
    timer_unwait(head, tgid(ct), timerid);

    if(timer == ct->posix_timers)
    {
        ct->posix_timers = timer->next;
    }
    else
    {
        for(tmp = ct->posix_timers; tmp->next != NULL; tmp = tmp->next)
        {
            if(tmp->next == timer)
            {
                tmp->next = timer->next;
                break;
            }
        }
    }

    free_timer(timer);
    kernel_mutex_unlock(&ct->common->mutex);

    return 0;
}


/*
 * Handler for syscall timer_getoverrun().
 */
long syscall_timer_getoverrun(ktimer_t timerid)
{
    struct posix_timer_t *timer;
    long res;
	volatile struct task_t *ct = this_core->cur_task;

    if(!timerid)
    {
        return -EINVAL;
    }

    kernel_mutex_lock(&ct->common->mutex);

    if(!(timer = get_posix_timer(tgid(ct), timerid)))
    //if(!(timer = get_posix_timer(ct, timerid)))
    {
        kernel_mutex_unlock(&ct->common->mutex);
        return -EINVAL;
    }

    res = timer->saved_overruns ? timer->saved_overruns - 1 : 0;
    timer->saved_overruns = 0;

    kernel_mutex_unlock(&ct->common->mutex);

    KDEBUG("syscall_timer_getoverrun: res %d\n", res);

    return res;
}


/*
 * Disarm POSIX timers.
 */
void disarm_timers(pid_t tgid)
{
    volatile struct posix_timer_t *timer, *next;
    volatile struct clock_waiter_t *head;
    volatile struct task_t *task = get_task_by_tgid(tgid);

    if(!task || !task->common)
    {
        return;
    }

    kernel_mutex_lock(&task->common->mutex);

    timer = task->posix_timers;

    while(timer)
    {
        next = timer->next;
        head = &waiter_head[(timer->clockid == CLOCK_REALTIME) ? 1 : 0];
        timer_unwait(head, tgid, timer->timerid);
        free_timer(timer);
        timer = next;
    }

    task->posix_timers = NULL;
    task->last_timerid = 3;

    kernel_mutex_unlock(&task->common->mutex);
}

