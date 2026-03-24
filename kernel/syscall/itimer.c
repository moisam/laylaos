/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
 * 
 *    file: itimer.c
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
 *  \file itimer.c
 *
 *  The kernel's interval timer implementation.
 */

#include <errno.h>
#include <sys/time.h>
#include <kernel/mutex.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/syscall.h>
#include <kernel/timer.h>
#include <kernel/ksignal.h>
#include <kernel/softint.h>

#include "../kernel/task_funcs.c"

/*****************************************************************************
 * 
 * NOTE: POSIX says we should use timer_gettime() and timer_settime() in place
 *       of getitimer() and setitimer(), which may be removed in future 
 *       versions of POSIX.
 *
 * See: https://man7.org/linux/man-pages/man2/setitimer.2.html
 * 
 *****************************************************************************/


static void arm_itimer(struct posix_timer_t *timer, struct itimerval *val, 
                       ktimer_t timerid, int signo)
{
    timer->sigev.sigev_notify = SIGEV_SIGNAL;
    timer->sigev.sigev_signo = signo;
    timer->sigev.sigev_value.sival_int = 0;
    timer->clockid = (timerid == ITIMER_REAL_ID) ? CLOCK_REALTIME : CLOCK_MONOTONIC;
    timer->timerid = timerid;
    timer->flags = 0;

    timer->val.it_value.tv_sec = val->it_value.tv_sec;
    timer->val.it_value.tv_nsec = val->it_value.tv_usec * 1000;
    timer->val.it_interval.tv_sec = val->it_interval.tv_sec;
    timer->val.it_interval.tv_nsec = val->it_interval.tv_usec * 1000;
    timer->cur_overruns = 0;
    timer->saved_overruns = 0;

    timer->next = this_core->cur_task->posix_timers;
    this_core->cur_task->posix_timers = timer;
}


static void activate_itimer(struct posix_timer_t *timer)
{
    int res;

    if(timer->val.it_value.tv_sec || timer->val.it_value.tv_nsec)
    {


        timer->tgid = tgid(this_core->cur_task);
        res = do_clock_nanosleep(0, &timer->val.it_value, NULL, timer);

        // time has already passed (otherwise we should get -EINTR)
        if(/* res == 0 || */ res == -EINVAL)
        {
            A_memset(&timer->val, 0, sizeof(struct itimerspec));
        }
    }
}


static int __getitimer(int which, struct itimerval *value)
{
    struct itimerspec oldval;

    oldval.it_value.tv_sec = 0;
    oldval.it_value.tv_nsec = 0;
    oldval.it_interval.tv_sec = 0;
    oldval.it_interval.tv_nsec = 0;

    if(which == ITIMER_VIRTUAL)
    {
        kernel_mutex_lock(&this_core->cur_task->common->mutex);
        ticks_to_timeval(this_core->cur_task->itimer_virt.rel_ticks, &value->it_value);
        ticks_to_timeval(this_core->cur_task->itimer_virt.interval, &value->it_interval);
        kernel_mutex_unlock(&this_core->cur_task->common->mutex);
        return 1;
    }
    else if(which == ITIMER_REAL)
    {
        timer_gettime_internal(ITIMER_REAL_ID, &oldval, 1);
    }
    else if(which == ITIMER_PROF)
    {
        timer_gettime_internal(ITIMER_PROF_ID, &oldval, 1);
    }
    else
    {
        return 0;
    }

    value->it_value.tv_sec = oldval.it_value.tv_sec;
    value->it_value.tv_usec = oldval.it_value.tv_nsec / 1000;
    value->it_interval.tv_sec = oldval.it_interval.tv_sec;
    value->it_interval.tv_usec = oldval.it_interval.tv_nsec / 1000;

    return 1;
}


/*
 * Handler for syscall getitimer().
 */
long syscall_getitimer(int which, struct itimerval *value)
{
    struct itimerval val;
    
    if(!value)
    {
        return -EINVAL;
    }

    if(!__getitimer(which, &val))
    {
        return -EINVAL;
    }
    
    return copy_to_user(value, &val, sizeof(struct itimerval));
}


/*
 * Handler for syscall setitimer().
 */
long syscall_setitimer(int which, struct itimerval *value,
                       struct itimerval *ovalue)
{
    struct itimerval val, oldval;

    if(!__getitimer(which, &oldval))
    {
        return -EINVAL;
    }

    if(value)
    {
        COPY_FROM_USER(&val, value, sizeof(struct itimerval));

        if(val.it_value.tv_usec < 0 || val.it_value.tv_usec > 999999)
        {
            return -EINVAL;
        }

        if(which == ITIMER_VIRTUAL)
        {
            kernel_mutex_lock(&this_core->cur_task->common->mutex);
            this_core->cur_task->itimer_virt.interval = timeval_to_ticks(&val.it_interval);
            this_core->cur_task->itimer_virt.rel_ticks = timeval_to_ticks(&val.it_value);
            kernel_mutex_unlock(&this_core->cur_task->common->mutex);
        }
        else if(which == ITIMER_REAL)
        {
            syscall_timer_delete(ITIMER_REAL_ID);
            kernel_mutex_lock(&this_core->cur_task->common->mutex);
            arm_itimer(&this_core->cur_task->itimer_real, &val, ITIMER_REAL_ID, SIGALRM);
            activate_itimer(&this_core->cur_task->itimer_real);
            kernel_mutex_unlock(&this_core->cur_task->common->mutex);
        }
        else if(which == ITIMER_PROF)
        {
            syscall_timer_delete(ITIMER_PROF_ID);
            kernel_mutex_lock(&this_core->cur_task->common->mutex);
            arm_itimer(&this_core->cur_task->itimer_prof, &val, ITIMER_PROF_ID, SIGPROF);
            activate_itimer(&this_core->cur_task->itimer_prof);
            kernel_mutex_unlock(&this_core->cur_task->common->mutex);
        }
        else
        {
            return -EINVAL;
        }
    }

    if(ovalue)
    {
        COPY_TO_USER(ovalue, &oldval, sizeof(struct itimerval));
    }

    return 0;
}


/*
 * Handler for syscal alarm().
 *
 * Manpage says:
 *    alarm() and setitimer(2) share the same timer; calls to one will 
 *    interfere with use of the other.
 */
long syscall_alarm(unsigned int seconds)
{
    unsigned int oldsecs = this_core->cur_task->itimer_real.val.it_value.tv_sec / PIT_FREQUENCY;
    struct itimerval val;

    val.it_value.tv_sec = seconds;
    val.it_value.tv_usec = 0;
    val.it_interval.tv_sec = 0;
    val.it_interval.tv_usec = 0;

    syscall_timer_delete(ITIMER_REAL_ID);

    // arm the new timer if needed
    kernel_mutex_lock(&this_core->cur_task->common->mutex);
    arm_itimer(&this_core->cur_task->itimer_real, &val, ITIMER_REAL_ID, SIGALRM);
    activate_itimer(&this_core->cur_task->itimer_real);
    kernel_mutex_unlock(&this_core->cur_task->common->mutex);

    return oldsecs;
}

