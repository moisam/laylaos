/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: timer.h
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
 *  \file timer.h
 *
 *  Functions and macros to implement the kernel's POSIX timers and interval
 *  timer functionality.
 */

#ifndef __TIMER_H__
#define __TIMER_H__

#include <time.h>
#include <signal.h>
#include <sys/time.h>
//#include <sys/signal.h>
#include <kernel/clock.h>

#include "bits/timert-def.h"


/**
 * @struct itimer_t
 * @brief The itimer_t structure.
 *
 * A structure to represent an interval timer.
 */
struct itimer_t
{
    unsigned long rel_ticks;    /**< relative timer value in ticks */
    unsigned long interval;     /**< relative timer interval in ticks */
    struct task_t *task;        /**< task owning this itimer */
    struct itimer_t *next_real; /* linked list for ITIMER_REAL timers */
};

/**
 * @struct posix_timer_t
 * @brief The posix_timer_t structure.
 *
 * A structure to represent a POSIX timer.
 */
struct posix_timer_t
{
    ktimer_t timerid;       /**< timer id */
    clockid_t clockid;      /**< clock id */
    int flags,              /**< timer flags */
        cur_overruns,       /**< current value of overruns */
        saved_overruns;     /**< saved overrun value */
    //int64_t interval_ticks; /**< interval value in ticks */
    struct sigevent sigev;  /**< signal to deliver on timer expiration */
    struct posix_timer_t *next; /**< next timer in task list */
    struct itimerspec val;      /**< current timer value */
};


//#include <kernel/task.h>


/**
 * @var ticks
 * @brief current ticks.
 *
 * Current number of ticks that elapsed since boot.
 */
extern unsigned long long ticks;

/**
 * @var prev_ticks
 * @brief previous ticks.
 *
 * Previous number of ticks, that is, when the last task time accounting
 * was done.
 */
extern unsigned long long prev_ticks;


/**
 * \def PIT_FREQUENCY
 *
 * Programmable Interval Timer (PIT) frequency per second
 */
#define PIT_FREQUENCY       100

#define NSECS_PER_TICK      (1000000000 / PIT_FREQUENCY)
#define USECS_PER_TICK      (1000000 / PIT_FREQUENCY)
#define MSECS_PER_TICK      (1000 / PIT_FREQUENCY)
#define NSEC_PER_USEC       1000
#define USEC_PER_SEC        1000000
#define NSEC_PER_SEC        1000000000
#define NSEC_PER_MSEC       1000000


static inline void ticks_to_timespec(unsigned long n, struct timespec *t)
{
    //t->tv_nsec = n * ntick;
    t->tv_nsec = n * NSECS_PER_TICK;
    t->tv_sec = n / PIT_FREQUENCY;
    
    while(t->tv_nsec > 1000000000)
    {
        t->tv_nsec -= 1000000000;
        t->tv_sec++;
    }
}


static inline void ticks_to_timeval(unsigned long n, struct timeval *tv)
{
    //tv->tv_usec = n * tick;
    tv->tv_usec = n * USECS_PER_TICK;
    tv->tv_sec = n / PIT_FREQUENCY;
    
    while(tv->tv_usec > 1000000)
    {
        tv->tv_usec -= 1000000;
        tv->tv_sec++;
    }
}


static inline unsigned long timeval_to_ticks(struct timeval *tv)
{
    unsigned long ticks;

    ticks = tv->tv_sec * PIT_FREQUENCY;
    ticks += (tv->tv_usec * PIT_FREQUENCY) / USEC_PER_SEC;

    if(tv->tv_usec && tv->tv_usec % USECS_PER_TICK)
    {
        ticks++;
    }
    
    return ticks;
}


static inline unsigned long timespec_to_ticks(struct timespec *ts)
{
    unsigned long ticks;

    ticks = ts->tv_sec * PIT_FREQUENCY;
    ticks += (ts->tv_nsec * PIT_FREQUENCY) / NSEC_PER_SEC;

    if(ts->tv_nsec && ts->tv_nsec % NSECS_PER_TICK)
    {
        ticks++;
    }
    
    return ticks;
}


/**********************************
 * Functions defined in timer.c
 **********************************/

/**
 * @brief Initialise system clock.
 *
 * Initialize the kernel's internal system clock and register the timer IRQ
 * handler.
 *
 * @return  nothing.
 */
void timer_init(void);

/**
 * @brief Switch timer callback.
 *
 * After tasking is initialized, this function is called to switch to the timer
 * callback that can handle task switching.
 *
 * @return  nothing.
 */
void switch_timer(void);


/**********************************
 * Functions defined in itimer.c
 **********************************/

/**
 * @brief Init itimers.
 *
 * Initialise internal timers. Called once during boot.
 *
 * @return  nothing.
 */
void init_itimers(void);

/**
 * @brief Decrement itimers.
 *
 * Called by the timer irq handler at every tick.
 * If there are any expired timers, the function schedules a \a SOFTINT_ITIMER
 * soft interrupt.
 *
 * @return  nothing.
 *
 * @see     softint_schedule()
 */
void dec_itimers(void);

/**
 * @brief Handler for syscall getitimer().
 *
 * Get the value of an interval timer.
 *
 * @param   which   one of: ITIMER_REAL, ITIMER_VIRTUAL, or ITIMER_PROF
 * @param   value   the timer value is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_getitimer(int which, struct itimerval *value);

/**
 * @brief Handler for syscall setitimer().
 *
 * Set the value of an interval timer.
 *
 * @param   which   one of: ITIMER_REAL, ITIMER_VIRTUAL, or ITIMER_PROF
 * @param   value   if not NULL, new timer value
 * @param   ovalue  if not NULL, old timer value is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_setitimer(int which, struct itimerval *value, 
                                 struct itimerval *ovalue);

/**
 * @brief Handler for syscall alarm().
 *
 * Set the value of an interval timer.
 *
 * @param   seconds set the alarm to expire after \a seconds
 *
 * @return  remaining seconds in old alarm if there is one, otherwise zero.
 */
int syscall_alarm(unsigned int seconds);


/***************************************
 * Functions defined in posix_timers.c
 ***************************************/

/**
 * @brief Get POSIX timer.
 *
 * Search the given \a task's timer list to find the timer with id \a timerid.
 *
 * @param   tgid        task group id
 * @param   timerid     timer id (positive integer)
 *
 * @return  POSIX timer on success, NULL on failure.
 */
struct posix_timer_t *get_posix_timer(pid_t tgid, ktimer_t timerid);

/**
 * @brief Handler for syscall timer_settime().
 *
 * Set the value of a POSIX timer. It internally calls do_clock_nanosleep().
 *
 * @param   timerid     timer id (positive integer)
 * @param   flags       zero or TIMER_ABSTIME
 * @param   new_value   if not NULL, new timer value
 * @param   old_value   if not NULL, old timer value is returned here
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see     do_clock_nanosleep()
 */
int syscall_timer_settime(ktimer_t timerid, int flags,
                          struct itimerspec *new_value,
                          struct itimerspec *old_value);

/**
 * @brief Handler for syscall timer_gettime().
 *
 * Get the value of a POSIX timer.
 *
 * @param   timerid     timer id (positive integer)
 * @param   curr_value  current timer value is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_timer_gettime(ktimer_t timerid, struct itimerspec *curr_value);

/**
 * @brief Handler for syscall timer_create().
 *
 * Create a new POSIX timer.
 *
 * @param   clockid     clock id (currently only CLOCK_REALTIME and 
 *                        CLOCK_MONOTONIC are supported)
 * @param   sevp        if not NULL, signal to deliver on timer expiration,
 *                        if NULL, SIGALRM is delivered
 * @param   timerid     new timer id (positive integer) is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_timer_create(clockid_t clockid, struct sigevent *sevp,
                         ktimer_t *timerid);

/**
 * @brief Handler for syscall timer_delete().
 *
 * Remove a POSIX timer.
 *
 * @param   timerid     timer id (positive integer)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_timer_delete(ktimer_t timerid);

/**
 * @brief Handler for syscall timer_getoverrun().
 *
 * Get POSIX timer overrun count.
 *
 * @param   timerid     timer id (positive integer)
 *
 * @return  zero or a positive count on success, -(errno) on failure.
 */
int syscall_timer_getoverrun(ktimer_t timerid);

/**
 * @brief Disarm POSIX timers.
 *
 * Disarm and remove all POSIX timers set by the given \a task.
 * Called during execve() and on task termination.
 *
 * @param   tgid        task group id
 *
 * @return  nothing.
 */
void disarm_timers(pid_t tgid);

#endif      /* __TIMER_H__ */
