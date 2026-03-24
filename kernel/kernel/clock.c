/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025, 2026 (c)
 * 
 *    file: clock.c
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
 *  \file clock.c
 *
 *  This file impelements different clock functions (gettime(), settime(),
 *  etc.) and the clock_nanosleep() syscall.
 */

//#define __DEBUG

#define _POSIX_CPUTIME
#define _POSIX_THREAD_CPUTIME
#define _POSIX_MONOTONIC_CLOCK
#define DEFINE_POSIX_TIMER_INLINES

#include <time.h>
#include <errno.h>
#include <string.h>
#include <kernel/rtc.h>
#include <kernel/clock.h>
#include <kernel/task.h>
#include <kernel/timer.h>
#include <kernel/ksignal.h>
#include <kernel/user.h>
#include <kernel/asm.h>
#include <kernel/softint.h>
#include <kernel/mutex.h>
#include <kernel/user.h>
#include <mm/kheap.h>

#include "task_funcs.c"
#include "../syscall/posix_timers_inlines.h"

#define NWAITERS            1024

struct posix_timer_t waiter_head[2];

time_t startup_time = 0;
struct sys_clock monotonic_time;
struct task_t *sleep_task = NULL;
volatile struct kernel_mutex_t waiter_mutex = { 0, };
volatile int waiter_mutex_locks = 0;

volatile struct task_t *softsleep_task = NULL;

void softsleep_task_func(void *unused);


/*
 * Initialise system-wide clock.
 */
void init_clock(void)
{
    memset(&monotonic_time, 0, sizeof(monotonic_time));
    memset(&waiter_head, 0, sizeof(waiter_head));

    systime_t time;
    kget_sys_clock(&time);
    startup_time = systime_to_posix(&time);
}


void init_clock_waiters(void)
{
    (void)start_kernel_task("softsleep", softsleep_task_func, NULL,
                            &softsleep_task, 0);
}


/*
 * Timers soft interrupt function.
 */
void softsleep_task_func(void *unused)
{
	UNUSED(unused);
    volatile struct posix_timer_t *t, *prev, *next;
    volatile int i;

	for(;;)
	{
        elevated_priority_lock_recursive(&waiter_mutex, waiter_mutex_locks);

        for(i = 0; i < 2; i++)
        {
            prev = &waiter_head[i];
            t = prev->next_waiting;

            while(t != NULL && t->delta <= 0)
            {
            	next = t->next_waiting;

            	if(t->timerid)
            	{
            	    prev->next_waiting = next;
               	    timer_notify_expired(t);
               	    timer_reset(t);
            	}
            	else
            	{
                	prev = t;
                	unblock_task_no_preempt(get_task_by_id(t->tgid));
            	}

            	t = next;
            }
        }

        elevated_priority_unlock_recursive(&waiter_mutex, waiter_mutex_locks);

        set_task_waitchan(this_core->cur_task, softsleep_task);
        set_task_state(this_core->cur_task, TASK_WAITING);
        scheduler();
    }
}


/*
 * Handler for syscall clock_getres().
 */
long syscall_clock_getres(clockid_t clock_id, struct timespec *res)
{
    if(!res)
    {
        return 0;
    }

    if((clock_id == CLOCK_REALTIME          ) ||
       (clock_id == CLOCK_MONOTONIC         ) ||
       (clock_id == CLOCK_PROCESS_CPUTIME_ID) ||
       (clock_id == CLOCK_THREAD_CPUTIME_ID ))
    {
        struct timespec tm =
        {
            .tv_sec  = 0,
            .tv_nsec = NSECS_PER_TICK,  // CLOCK_RESOLUTION,
        };

        return copy_to_user(res, &tm, sizeof(struct timespec));
    }

    /* unknown clock */
    return -EINVAL;
}


long do_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
    long res = 0;

    /*
     * NOTE: CLOCK_REALTIME: Its time represents seconds and nanoseconds
     *       since the Epoch.  When its time is changed, timers for a
     *       relative interval are unaffected, but timers for an absolute
     *       point in time are affected.
     */
    if(clock_id == CLOCK_REALTIME)
    {
        tp->tv_sec  = monotonic_time.tv_sec + startup_time;
        tp->tv_nsec = monotonic_time.tv_nsec;
    }
    else if(clock_id == CLOCK_MONOTONIC)
    {
        tp->tv_sec  = monotonic_time.tv_sec;
        tp->tv_nsec = monotonic_time.tv_nsec;
    }
    else if(clock_id == CLOCK_PROCESS_CPUTIME_ID ||
            clock_id == CLOCK_THREAD_CPUTIME_ID)
    {
        time_t t = (this_core->cur_task->user_time + this_core->cur_task->sys_time);

        tp->tv_sec  = t / PIT_FREQUENCY;
        tp->tv_nsec = (t % PIT_FREQUENCY) * 1000000000 /* 1e9 */;
    }
    else
    {
        /* unknown clock */
        res = -EINVAL;
    }
    
    return res;
}


/*
 * Handler for syscall clock_gettime().
 */
long syscall_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
    long res;
    struct timespec tp2;
    
    if(!tp)
    {
        return -EINVAL;
    }
    
    if((res = do_clock_gettime(clock_id, &tp2)) < 0)
    {
        return res;
    }
    
    COPY_VAL_TO_USER(&tp->tv_sec, &tp2.tv_sec);
    COPY_VAL_TO_USER(&tp->tv_nsec, &tp2.tv_nsec);
    return 0;
}


long do_clock_settime(clockid_t clock_id, struct timespec *tp)
{
    if(!tp || !this_core->cur_task)
    {
        return -EINVAL;
    }
    
    if(clock_id == CLOCK_REALTIME)
    {
        if(!suser(this_core->cur_task))
        {
            return -EPERM;
        }

        /* check nanoseconds value as per POSIX */
        if(tp->tv_nsec < 0 || tp->tv_nsec >= 1000000000 /* 1e9 */)
        {
            return -EINVAL;
        }

        time_t old_secs = monotonic_time.tv_sec + startup_time;
        
        startup_time = tp->tv_sec;
        monotonic_time.tv_sec = 0;
        monotonic_time.tv_nsec = tp->tv_nsec;
        
        /* check for any timers that would expire under the new clock value */
        if(old_secs > tp->tv_sec)
        {

            volatile struct posix_timer_t *t;
            int i;

            elevated_priority_lock_recursive(&waiter_mutex, waiter_mutex_locks);

            for(i = 0; i < 2; i++)
            {
                int64_t diff = old_secs - tp->tv_sec;

            	for(t = waiter_head[i].next_waiting; t != NULL; t = t->next_waiting)
        	    {
        	        if(t->delta >= diff)
        	        {
        	            t->delta -= diff;
        	            break;
        	        }

        	        if(t->delta != 0)
        	        {
        	            diff -= t->delta;
        	            t->delta = 0;
        	        }
        	    }
        	}
        	
            elevated_priority_unlock_recursive(&waiter_mutex, waiter_mutex_locks);
        }

        return 0;
    }
    else if(clock_id == CLOCK_MONOTONIC)
    {
        /* fail to set CLOCK_MONOTONIC as per POSIX */
        return -EINVAL;
    }
    else if(clock_id == CLOCK_PROCESS_CPUTIME_ID ||
            clock_id == CLOCK_THREAD_CPUTIME_ID)
    {
        /* Linux doesn't support this. neither would we! */
        return -EPERM;
    }

    /* unknown clock */
    return -EINVAL;
}


/*
 * Handler for syscall clock_settime().
 */
long syscall_clock_settime(clockid_t clock_id, struct timespec *tp)
{
    long res;
    struct timespec tp2;

    if(!tp)
    {
        return -EINVAL;
    }
    
    if((res = copy_from_user(&tp2, tp, sizeof(struct timespec))) != 0)
    {
        return res;
    }
    
    return do_clock_settime(clock_id, &tp2);
}


/*
 * Check expired timers.
 */
void clock_check_waiters(void)
{
	int i, unblock = 0;
    volatile struct posix_timer_t *t;

    if(/* waiter_list_busy || */ softsleep_task == NULL)
    {
        return;
    }

    int old_prio = 0, old_policy = 0;
    elevate_priority(this_core->cur_task, &old_prio, &old_policy);

    if(kernel_mutex_trylock(&waiter_mutex))
    {
        restore_priority(this_core->cur_task, old_prio, old_policy);
        return;
    }

    for(i = 0; i < 2; i++)
    {
    	for(t = waiter_head[i].next_waiting; t != NULL; t = t->next_waiting)
	    {
	        if(--t->delta > 0)
	        {
	            break;
	        }
		
	    	unblock = 1;
		
	        if(t->delta == 0)
	    	{
	    		break;
	    	}
	    }
	}

    kernel_mutex_unlock(&waiter_mutex);
    restore_priority(this_core->cur_task, old_prio, old_policy);

	if(unblock)
	{
        unblock_task_no_preempt(softsleep_task);
	}
}


/*
 * Get clock_waiter_t struct for a task.
 */
void get_waiter(volatile struct posix_timer_t *head,
                volatile struct posix_timer_t *timer,
                int64_t *remaining_ticks, int unlink)
{
    volatile struct posix_timer_t *prev, *next;
    int64_t delta = 0;
    
    elevated_priority_lock_recursive(&waiter_mutex, waiter_mutex_locks);

	for(prev = head; (next = prev->next_waiting) != NULL; prev = next)
	{
	    delta += prev->delta;
	    
		if(next == timer)
		{
			if(remaining_ticks)
			{
			    *remaining_ticks = next->delta + delta;
			}

			if(unlink)
			{
    			if(next->next_waiting && next->delta > 0)
    			{
    				next->next_waiting->delta += next->delta;
    			}

    			prev->next_waiting = next->next_waiting;
    			next->next_waiting = NULL;
    			next->delta += delta;
			}
			
			break;
		}
	}

    elevated_priority_unlock_recursive(&waiter_mutex, waiter_mutex_locks);
}


long __clock_wait(struct posix_timer_t *head,
                  volatile struct posix_timer_t *timer, int64_t delta)
{
    volatile struct posix_timer_t *prev, *next;
    int i;

    elevated_priority_lock_recursive(&waiter_mutex, waiter_mutex_locks);

    /*
     * Store waiting tasks in a delta queue, where every task's delta is the 
     * difference between the task's waiting time and the previous task's
     * waiting time. We walk down the list to find a task who's delta is
     * smaller than the current delta and we insert ourselves before it.
     * We correct the delta value as we walk down the list.
     */
	for(prev = head;
	    (next = prev->next_waiting) != NULL && delta > next->delta;
	    prev = next)
	{
	    if(next->delta > 0)
	    {
			delta -= next->delta;
		}
	}
	
	/*
	 * Store the new delta, and fix the next task's delta (if we are not the
	 * last task in queue).
	 */
	timer->delta = delta;

	if(next != NULL)
	{
		next->delta -= delta;
	}

    /* Fix the pointers */
	prev->next_waiting = timer;
	timer->next_waiting = next;

    elevated_priority_unlock_recursive(&waiter_mutex, waiter_mutex_locks);

	/* Return if this is a call from timer_settime() */
	if(timer->timerid)
	{
        return timer->delta;
	}

    i = get_task_properties(this_core->cur_task);

    if((i & PROPERTY_SELECT_EVENT) == PROPERTY_SELECT_EVENT)
    {
        goto skip;
    }

    i = get_task_waking_signal(this_core->cur_task);

    if(i != 0)
    {
        goto skip;
    }

	/* Block until time expires or we are woken by a signal */
	//block_task(head, 1);

    if(!get_task_waitchan(this_core->cur_task))
    {
        set_task_waitchan(this_core->cur_task, head);
    }

    set_task_state(this_core->cur_task, TASK_SLEEPING);
    scheduler();

skip:

	/*
	 * Remove us from the queue. If we were woken by a signal, this call will
	 * also store the remaining time in struct w's delta field, which we return
	 * to the caller.
	 */
    (void)get_waiter(head, timer, NULL, 1);

    delta = (timer->delta > 0) ? timer->delta : 0;

    return delta;
}


/*
 * Nanosleep on a clock.
 *
 * NOTE: Linux man pages say:
 *       clock_nanosleep() is never restarted after being interrupted by a
 *         signal handler, regardless of the use of the sigaction(2)
 *         SA_RESTART flag.
 */
long do_clock_nanosleep(int flags, 
                        volatile struct timespec *rqtp, 
                        volatile struct timespec *rmtp,
                        volatile struct posix_timer_t *timer)
{
    struct posix_timer_t *head;
    unsigned long long nticks;

    /* NOTE: Linux supports CLOCK_PROCESS_CPUTIME_ID in this function */
    if(timer->clockid != CLOCK_REALTIME && timer->clockid != CLOCK_MONOTONIC)
    {
        /* fail as per POSIX (POSIX says to fail for thread clock only, 
         * other clocks are not specified).
         */
        return -EINVAL;
    }

    if(!rqtp)
    {
        return -EINVAL;
    }

    /* check nanoseconds value as per POSIX */
    if(rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000 /* 1e9 */)
    {
        return -EINVAL;
    }

    if(rqtp->tv_sec < 0)
    {
        return -EINVAL;
    }
    
    
    time_t clock_secs = monotonic_time.tv_sec;
    uint64_t clock_nsecs = monotonic_time.tv_nsec;
    time_t my_secs = rqtp->tv_sec;
    uint64_t res_nticks, my_nsecs = rqtp->tv_nsec;

    if(timer->clockid == CLOCK_REALTIME)
    {
        clock_secs += startup_time;
    }

    if(flags & TIMER_ABSTIME)
    {
        if(my_secs <= clock_secs && my_nsecs <= clock_nsecs)
        {
            return 0;
        }
        
        my_secs -= clock_secs;

        if(my_nsecs <= monotonic_time.tv_nsec)
        {
            return 0;
        }
        
        my_nsecs -= monotonic_time.tv_nsec;
    }
    
    if(my_nsecs && my_nsecs % NSECS_PER_TICK)
    {
        my_nsecs += NSECS_PER_TICK;
    }
    
    nticks = (my_secs * PIT_FREQUENCY) + (my_nsecs / NSECS_PER_TICK);
    head = &waiter_head[(timer->clockid == CLOCK_REALTIME) ? 1 : 0];

    KDEBUG("do_clock_nanosleep: secs %ld\n", my_secs);
    KDEBUG("do_clock_nanosleep: nsecs %ld\n", my_nsecs);
    KDEBUG("do_clock_nanosleep: nticks %ld\n", nticks);
    KDEBUG("do_clock_nanosleep: id %d\n", timerid);
    //printk("ns1 (%d, %ld, %ld, %ld) ", this_core->cur_task->pid, my_secs, my_nsecs, nticks);

    if(nticks && (res_nticks = __clock_wait(head, timer, nticks)) != 0)
    {
    	volatile struct task_t *task = get_task_by_id(timer->tgid);

        if(task && get_task_waking_signal(task))
        //if(task && task->woke_by_signal)
        {
            //printk("ns2 %d, %lu, %lu ", this_core->cur_task->pid, res_nticks, nticks);

            if(rmtp)
            {
                ticks_to_timespec(res_nticks, (struct timespec *)rmtp);
            }

            return -EINTR;
        }
    }

    return 0;
}


/*
 * Handler for syscall clock_nanosleep().
 */
long syscall_clock_nanosleep(clockid_t clock_id, int flags, 
                             struct timespec *__rqtp, struct timespec *__rmtp)
{
    struct timespec rqtmp, rmtmp, *rqptr = NULL;
    long res;

    set_task_waking_signal(this_core->cur_task, 0);
    __sync_and_and_fetch(&this_core->cur_task->properties, ~PROPERTY_SELECT_EVENT);

    if(__rqtp)
    {
        COPY_FROM_USER(&rqtmp, __rqtp, sizeof(struct timespec));
        rqptr = &rqtmp;
    }

    struct posix_timer_t timer;

    timer.timerid = 0;
    timer.tgid = this_core->cur_task->pid;
    timer.clockid = clock_id;

    res = do_clock_nanosleep(flags, rqptr, &rmtmp, &timer);

    if(res == -EINTR && __rmtp)
    {
        COPY_TO_USER(__rmtp, &rmtmp, sizeof(struct timespec));
    }
    
    return res;
}


/*
 * Handler for syscall nanosleep().
 */
long syscall_nanosleep(struct timespec *__rqtp, struct timespec *__rmtp)
{
    return syscall_clock_nanosleep(CLOCK_REALTIME, 0, __rqtp, __rmtp);
}


/*
 * Get startup time.
 */
time_t get_startup_time(void)
{
    return startup_time;
}


/*
 * Get current time in microseconds.
 *
 * TODO: This function doesn't calculate time properly.
 */
void microtime(struct timeval *tvp)
{
	tvp->tv_sec = startup_time + monotonic_time.tv_sec;
	tvp->tv_usec = monotonic_time.tv_nsec / 1000;
}

