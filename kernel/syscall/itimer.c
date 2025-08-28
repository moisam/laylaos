/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
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

#if 0

struct itimer_t itimer_queue_head = { 0, };
siginfo_t itimer_siginfo = { .si_code = SI_TIMER };
struct task_t *softitimer_task = NULL;
volatile struct kernel_mutex_t itimer_mutex = { 0, };
volatile int itimer_list_busy = 0;

void softitimer_task_func(void *unused);

#define TGLEAD(t)       (t)->threads->thread_group_leader


void init_itimers(void)
{
    (void)start_kernel_task("softitimer", softitimer_task_func, NULL,
                            &softitimer_task, 0);
}

#endif


static void arm_itimer(struct posix_timer_t *timer, struct itimerval *val, ktimer_t timerid, int signo)
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
        res = do_clock_nanosleep(tgid(this_core->cur_task), timer->clockid, 0,
                                     &timer->val.it_value, NULL, timer->timerid);

        // time has already passed (otherwise we should get -EINTR)
        if(res == 0 || res == -EINVAL)
        {
            A_memset(&timer->val, 0, sizeof(struct itimerspec));
        }
    }
}


#if 0

static void insert_real_itimer(volatile struct itimer_t *itimer)
{
    volatile struct itimer_t *prev, *next;
    unsigned long ticks = itimer->rel_ticks;

    /*
     * Store waiting tasks in a delta queue, where every task's delta is the 
     * difference between the task's waiting time and the previous task's
     * waiting time. We walk down the list to find a task who's delta is
     * smaller than the current delta and we insert ourselves before it.
     * We correct the delta value as we walk down the list.
     */
	for(prev = &itimer_queue_head;
	    (next = prev->next_real) != NULL && ticks > next->rel_ticks;
	    prev = next)
	{
	    if(next->rel_ticks > 0)
	    {
			ticks -= next->rel_ticks;
		}
	}
	
	/*
	 * Store the new delta, and fix the next task's delta (if we are not the
	 * last task in queue).
	 */
	itimer->rel_ticks = ticks;
	
	if(next != NULL)
	{
		next->rel_ticks -= ticks;
	}

    /* Fix the pointers */
	prev->next_real = itimer;
	itimer->next_real = next;
}


static void remove_real_itimer(struct itimer_t *itimer)
{
    volatile struct itimer_t *prev, *next;

	for(prev = &itimer_queue_head;
	    (next = prev->next_real) != NULL;
	    prev = next)
	{
		if(next == itimer)
		{
			if(next->next_real && next->rel_ticks > 0)
			{
				next->next_real->rel_ticks += next->rel_ticks;
			}

			prev->next_real = next->next_real;
			next->next_real = NULL;
			break;
		}
	}
}


static inline void reset_itimer(volatile struct itimer_t *itimer)
{
    itimer->rel_ticks = itimer->interval;
}


/*
 * Interval timers soft interrupt function.
 */
void softitimer_task_func(void *unused)
{
	UNUSED(unused);
    volatile struct itimer_t *itimer;

    for(;;)
    {
        elevated_priority_lock(&itimer_mutex);
        itimer_list_busy = 1;

    	while((itimer = itimer_queue_head.next_real) != NULL &&
    	                    itimer->rel_ticks == 0)
    	{
            //printk("softitimer: waking up task %d\n", itimer->task->pid);
            itimer_queue_head.next_real = itimer->next_real;
            itimer->next_real = NULL;
            add_task_signal(itimer->task, SIGALRM, &itimer_siginfo, 1);

            /* reset timer and return to the queue if needed */
            reset_itimer(itimer);

            if(itimer->rel_ticks)
            {
                insert_real_itimer(itimer);
            }
    	}

        itimer_list_busy = 0;
        elevated_priority_unlock(&itimer_mutex);

        block_task(&itimer_queue_head, 0);
	}
}


/*
 * Called by the timer irq handler at every tick.
 */
void dec_itimers(void)
{
    volatile struct itimer_t *itimer;
    int needsoft = 0;

    if(itimer_list_busy)
    {
        return;
    }

    /* decrement system-wide real timers */
	for(itimer = itimer_queue_head.next_real;
	    itimer != NULL;
	    itimer = itimer->next_real)
	{
        if(itimer->rel_ticks != 0)
        {
            if(--itimer->rel_ticks == 0)
            {
                needsoft = 1;
            }
            
            break;
        }
        
        needsoft = 1;
	}

	if(needsoft)
	{
        /*
         * This is the code from unblock_kernel_task(). We inline it
         * here for performance, as this function is called repeatedly
         * from timer_callback().
         */
        if(softitimer_task->state == TASK_WAITING)
        {
            softitimer_task->state = TASK_READY;
    
            remove_from_queue(softitimer_task);
            append_to_ready_queue(softitimer_task);
        }
	}

    /* decrement task's virtual timer, if any */
    if(cur_task->itimer_virt.rel_ticks)
    {
        /* only count ticks when running in user mode */
        if(cur_task->user && !cur_task->user_in_kernel_mode)
        {
            if(--cur_task->itimer_virt.rel_ticks == 0)
            {
                /* reset timer if needed */
                reset_itimer(&cur_task->itimer_virt);
                add_task_signal(cur_task, SIGVTALRM, &itimer_siginfo, 1);
            }
        }
    }

    /* decrement task's profiling timer, if any */
    if(cur_task->itimer_prof.rel_ticks)
    {
        if(--cur_task->itimer_prof.rel_ticks == 0)
        {
            /* reset timer if needed */
            reset_itimer(&cur_task->itimer_prof);
            add_task_signal(cur_task, SIGPROF, &itimer_siginfo, 1);
        }
    }
}

#endif


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

#if 0
    unsigned long rel_ticks, base_ticks = 0;
    unsigned long interval;
    struct itimer_t *itimer;
    struct task_t *ct = cur_task;

    if(which == ITIMER_REAL)
    {
        itimer = &(ct->itimer_real);

        volatile struct itimer_t *prev, *next;

        elevated_priority_lock(&itimer_mutex);
        itimer_list_busy = 1;

    	for(prev = &itimer_queue_head;
    	    (next = prev->next_real) != NULL && (next != itimer);
    	    prev = next)
    	{
    	    base_ticks += prev->rel_ticks;
    	}
	
        itimer_list_busy = 0;
        elevated_priority_unlock(&itimer_mutex);
    }
    else if(which == ITIMER_VIRTUAL)
    {
        itimer = &(ct->itimer_virt);
    }
    else if(which == ITIMER_PROF)
    {
        itimer = &(ct->itimer_prof);
    }
    else
    {
        return 0;
    }

    rel_ticks = itimer->rel_ticks + base_ticks;
    interval = itimer->interval;

    /* NOTE: convert to microseconds. */
    ticks_to_timeval(rel_ticks, &value->it_value);
    ticks_to_timeval(interval, &value->it_interval);

    return 1;
#endif

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

#if 0
    struct itimerval val, oldval;
    int noset = 0;
    struct task_t *ct = cur_task;

    if(value == NULL)
    {
        noset = 1;
    }
    else
    {
        COPY_FROM_USER(&val, value, sizeof(struct itimerval));
    }

    if(!__getitimer(which, &oldval))
    {
        return -EINVAL;
    }
    
    if(noset)
    {
        goto fin;
    }
    
    if(val.it_value.tv_usec < 0 || val.it_value.tv_usec > 999999)
    {
        return -EINVAL;
    }
    
    /* NOTE: convert to microseconds, then to ticks (jiffies). */
    unsigned long rel_ticks, interval;
    
    rel_ticks = timeval_to_ticks(&val.it_value);
    interval = timeval_to_ticks(&val.it_interval);

    if(which == ITIMER_REAL)
    {
        elevated_priority_lock(&itimer_mutex);
        itimer_list_busy = 1;

        /*
         * remove the old timer, set the new value, then add the timer to
         * the queue if it is not disarmed (because our queue is a delta
         * queue).
         */
        remove_real_itimer(&(ct->itimer_real));

        ct->itimer_real.rel_ticks = rel_ticks;
        ct->itimer_real.interval = interval;
        ct->itimer_real.task = TGLEAD(ct);
        ct->itimer_real.next_real = NULL;

        if(ct->itimer_real.rel_ticks != 0)
        {
            insert_real_itimer(&(ct->itimer_real));
        }

        itimer_list_busy = 0;
        elevated_priority_unlock(&itimer_mutex);
    }
    else if(which == ITIMER_VIRTUAL)
    {
        ct->itimer_virt.rel_ticks = rel_ticks;
        ct->itimer_virt.interval = interval;
    }
    else if(which == ITIMER_PROF)
    {
        ct->itimer_prof.rel_ticks = rel_ticks;
        ct->itimer_prof.interval = interval;
    }
    else
    {
        return -EINVAL;
    }

fin:
    if(ovalue)
    {
        COPY_TO_USER(ovalue, &oldval, sizeof(struct itimerval));
    }

    return 0;
#endif

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


#if 0
    struct task_t *ct = cur_task;
    unsigned long rel_ticks = seconds * PIT_FREQUENCY;
    unsigned int oldsecs = ct->itimer_real.rel_ticks / PIT_FREQUENCY;

    elevated_priority_lock(&itimer_mutex);
    itimer_list_busy = 1;

    /*
     * remove the old timer, set the new value, then add the timer to
     * the queue if it is not disarmed (because our queue is a delta
     * queue).
     */
    remove_real_itimer(&(ct->itimer_real));

    ct->itimer_real.rel_ticks = rel_ticks;
    ct->itimer_real.interval = 0;
    ct->itimer_real.task = TGLEAD(ct);
    ct->itimer_real.next_real = NULL;

    if(ct->itimer_real.rel_ticks != 0)
    {
        insert_real_itimer(&(ct->itimer_real));
    }

    itimer_list_busy = 0;
    elevated_priority_unlock(&itimer_mutex);
#endif

    return oldsecs;
}

