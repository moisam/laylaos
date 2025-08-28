/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
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

struct clock_waiter_t waiter_head[2];
time_t startup_time = 0;
struct sys_clock monotonic_time;
struct task_t *sleep_task = NULL;
volatile struct kernel_mutex_t waiter_mutex = { 0, };
volatile int waiter_list_busy = 0;
volatile int waiter_mutex_locks = 0;

volatile struct task_t *softsleep_task = NULL;
struct clock_waiter_t *waiter_table = NULL;
struct clock_waiter_t *last_used_waiter = NULL;

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
    if(!(waiter_table = kmalloc(NWAITERS * sizeof(struct clock_waiter_t))))
    {
        kpanic("Failed to init clock waiter table");
    }

    A_memset(waiter_table, 0, NWAITERS * sizeof(struct clock_waiter_t));
    last_used_waiter = waiter_table;

    (void)start_kernel_task("softsleep", softsleep_task_func, NULL,
                            &softsleep_task, 0);
}


void waiter_free(struct clock_waiter_t *w)
{
    w->used = 0;
}


struct clock_waiter_t *waiter_malloc(void)
{
    struct clock_waiter_t *w, *end;

    end = &waiter_table[NWAITERS];

    if(last_used_waiter >= end)
    {
        last_used_waiter = waiter_table;
    }

    w = last_used_waiter;

retry:

    for( ; w < end; w++)
    {
        if(!w->used)
        {
            w->used = 1;
            last_used_waiter = &w[1];
            return w;
        }
    }

    // if we started searching from the middle, try to search from the 
    // beginning, maybe we'll find an entry that someone has free'd
    if((end == &waiter_table[NWAITERS]) && (last_used_waiter != waiter_table))
    {
        end = last_used_waiter;
        w = waiter_table;
        goto retry;
    }

    /*
    struct clock_waiter_t *w;
    struct clock_waiter_t *lw = &waiter_table[NWAITERS];

    for(w = waiter_table; w < lw; w++)
    {
        if(!w->used)
        {
            w->used = 1;
            return w;
        }
    }
    */

    return NULL;
}


/*
 * Timers soft interrupt function.
 */
void softsleep_task_func(void *unused)
{
	UNUSED(unused);
    volatile struct clock_waiter_t *w, *prev, *next;
    struct posix_timer_t *timer;
    volatile int i;
	
	for(;;)
	{
        elevated_priority_lock_recursive(&waiter_mutex, waiter_mutex_locks);
        waiter_list_busy = 1;

        for(i = 0; i < 2; i++)
        {
            prev = &waiter_head[i];
            w = prev->next;
        
            while(w != NULL && w->delta <= 0)
            {
            	next = w->next;

            	if(w->timerid)
            	{
            	    prev->next = (struct clock_waiter_t *)next;

                    if((timer = get_posix_timer(w->pid, w->timerid)))
                    {
                	    timer_notify_expired(w->pid, timer);
                	    timer_reset(w->pid, timer);
                    }

            	    waiter_free((void *)w);
            	}
            	else
            	{
                	prev = w;
                	unblock_task_no_preempt(get_task_by_id(w->pid));
            	}

            	w = /* w-> */ next;
            }
        }

        waiter_list_busy = 0;
        elevated_priority_unlock_recursive(&waiter_mutex, waiter_mutex_locks);

        block_task(waiter_table, 0);
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
            struct clock_waiter_t *w;
            int i;

            elevated_priority_lock_recursive(&waiter_mutex, waiter_mutex_locks);
            waiter_list_busy = 1;

            for(i = 0; i < 2; i++)
            {
                int64_t diff = old_secs - tp->tv_sec;
                
            	for(w = waiter_head[i].next; w != NULL; w = w->next)
        	    {
        	        if(w->delta >= diff)
        	        {
        	            w->delta -= diff;
        	            break;
        	        }
        	        
        	        if(w->delta != 0)
        	        {
        	            diff -= w->delta;
        	            w->delta = 0;
        	        }
        	    }
        	}
        	
            waiter_list_busy = 0;
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
	/* volatile */ int i, unblock = 0;

    /* volatile */ struct clock_waiter_t *w;

    if(waiter_list_busy || softsleep_task == NULL)
    {
        return;
    }

    for(i = 0; i < 2; i++)
    {
    	for(w = waiter_head[i].next; w != NULL; w = w->next)
	    {
	        if(--w->delta > 0)
	        {
	            break;
	        }
		
	    	unblock = 1;
		
	        if(w->delta == 0)
	    	{
	    		break;
	    	}
	    }
	}

	if(unblock)
	{
        unblock_task_no_preempt(softsleep_task);
#if 0
        /*
         * This is the code from unblock_kernel_task(). We inline it
         * here for performance, as this function is called repeatedly
         * from timer_callback().
         */
        if(softsleep_task->state == TASK_WAITING)
        {
            softsleep_task->state = TASK_READY;
    
            remove_from_queue(softsleep_task);
            append_to_ready_queue(softsleep_task);
        }
#endif
	}
}


/*
 * Get clock_waiter_t struct for a task.
 */
struct clock_waiter_t *get_waiter(volatile struct clock_waiter_t *head,
                                  pid_t pid, ktimer_t timerid,
                                  int64_t *remaining_ticks, int unlink)
{
    volatile struct clock_waiter_t *prev, *next;
    int64_t delta = 0;
    
    elevated_priority_lock_recursive(&waiter_mutex, waiter_mutex_locks);
    waiter_list_busy = 1;

	for(prev = head; (next = prev->next) != NULL; prev = next)
	{
	    delta += prev->delta;
	    
		if(next && next->pid == pid && next->timerid == timerid)
		{
			if(remaining_ticks)
			{
			    *remaining_ticks = next->delta + delta;
			}

			if(unlink)
			{
    			if(next->next && next->delta > 0)
    			{
    				next->next->delta += next->delta;
    			}

    			prev->next = next->next;
    			next->next = NULL;
    			next->delta += delta;
			}
			
			break;
		}
	}
	
    waiter_list_busy = 0;
    elevated_priority_unlock_recursive(&waiter_mutex, waiter_mutex_locks);
	
	return (struct clock_waiter_t *)next;
}


long clock_wait(struct clock_waiter_t *head, pid_t pid,
                int64_t delta, ktimer_t timerid)
{
    struct clock_waiter_t *w, *prev, *next;
    //struct task_t *task;
    
    elevated_priority_lock_recursive(&waiter_mutex, waiter_mutex_locks);
    waiter_list_busy = 1;

    if(!(w = waiter_malloc()))
    {
        waiter_list_busy = 0;
        elevated_priority_unlock_recursive(&waiter_mutex, waiter_mutex_locks);

        return delta;
    }
    
    w->delta = 0;
    w->next = NULL;
    w->pid = pid;
    w->timerid = timerid;
    
    /*
     * Store waiting tasks in a delta queue, where every task's delta is the 
     * difference between the task's waiting time and the previous task's
     * waiting time. We walk down the list to find a task who's delta is
     * smaller than the current delta and we insert ourselves before it.
     * We correct the delta value as we walk down the list.
     */
	for(prev = head;
	    (next = prev->next) != NULL && delta > next->delta;
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
	w->delta = delta;
	
	if(next != NULL)
	{
		next->delta -= delta;
	}

    /* Fix the pointers */
	prev->next = w;
	w->next = next;

    waiter_list_busy = 0;
    elevated_priority_unlock_recursive(&waiter_mutex, waiter_mutex_locks);

	/* return if this is a call from timer_settime() */
	if(timerid)
	{
        return w->delta;
	}
	
	/* Block until time expires or we are woken by a signal */
	block_task(head, 1);
	
	/*
	 * Remove us from the queue. If we were woken by a signal, this call will
	 * also store the remaining time in struct w's delta field, which we return
	 * to the caller.
	 */
    (void)get_waiter(head, w->pid, w->timerid, NULL, 1);
	
	/*
	task = get_task_by_id(pid);
    delta = (task && task->woke_by_signal) ? w->delta : 0;
    */
    delta = (w->delta > 0) ? w->delta : 0;

    waiter_free(w);

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
long do_clock_nanosleep(pid_t pid, clockid_t clock_id, int flags, 
                        struct timespec *__rqtp, struct timespec *__rmtp,
                        ktimer_t timerid)
{
    struct timespec rqtp;
    struct timespec rmtp;
    struct clock_waiter_t *head;
    unsigned long long nticks;

    /* NOTE: Linux supports CLOCK_PROCESS_CPUTIME_ID in this function */
    if(clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC)
    {
        /* fail as per POSIX (POSIX says to fail for thread clock only, 
         * other clocks are not specified).
         */
        return -EINVAL;
    }

    if(!__rqtp)
    {
        return -EINVAL;
    }
    
    A_memcpy(&rqtp, __rqtp, sizeof(struct timespec));
    
    /* check nanoseconds value as per POSIX */
    if(rqtp.tv_nsec < 0 || rqtp.tv_nsec >= 1000000000 /* 1e9 */)
    {
        return -EINVAL;
    }

    if(rqtp.tv_sec < 0)
    {
        return -EINVAL;
    }
    
    
    time_t clock_secs = monotonic_time.tv_sec;
    uint64_t clock_nsecs = monotonic_time.tv_nsec;
    time_t my_secs = rqtp.tv_sec;
    uint64_t res_nticks, my_nsecs = rqtp.tv_nsec;
    
    if(clock_id == CLOCK_REALTIME)
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
    }

    if(flags & TIMER_ABSTIME)
    {
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
    head = &waiter_head[(clock_id == CLOCK_REALTIME) ? 1 : 0];
    
    KDEBUG("do_clock_nanosleep: secs %ld\n", my_secs);
    KDEBUG("do_clock_nanosleep: nsecs %ld\n", my_nsecs);
    KDEBUG("do_clock_nanosleep: nticks %ld\n", nticks);
    KDEBUG("do_clock_nanosleep: id %d\n", timerid);

    if(nticks && (res_nticks = clock_wait(head, pid, nticks, timerid)) != 0)
    {
    	volatile struct task_t *task = get_task_by_id(pid);

        if(task && task->woke_by_signal)
        {
            if(__rmtp)
            {
                ticks_to_timespec(res_nticks, &rmtp);
                A_memcpy(__rmtp, &rmtp, sizeof(struct timespec));
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

    if(__rqtp)
    {
        COPY_FROM_USER(&rqtmp, __rqtp, sizeof(struct timespec));
        rqptr = &rqtmp;
    }

    res = do_clock_nanosleep(this_core->cur_task->pid, clock_id, flags, rqptr, &rmtmp, 0);

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

