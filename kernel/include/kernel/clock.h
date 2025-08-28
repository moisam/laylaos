/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: clock.h
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
 *  \file clock.h
 *
 *  Functions and structure defines for working with the clock.
 */

#ifndef __KERNEL_CLOCK__
#define __KERNEL_CLOCK__

/*
 * We have to first include <sys/types.h> to define things like time_t, then
 * we can define the now() macro, which requires us to define both
 * startup_time and monotonic_time, and only then we can safely #include 
 * <mutex.h> and others.
 */

#include <sys/types.h>
#include "bits/timert-def.h"


/**
 * @struct clock_waiter_t
 * @brief The clock_waiter_t structure.
 *
 * A structure to represent a single clock waiter (task that is waiting on
 * one of the kernel clocks).
 */
struct clock_waiter_t
{
    int64_t delta;                  /**< time delta in ticks */
    pid_t pid;                      /**< waiter task */
    ktimer_t timerid;               /**< timer id */
    struct clock_waiter_t *next;    /**< next waiter task in list */
    int used;
};


/**
 * @struct sys_clock
 * @brief The sys_clock structure.
 *
 * A structure to represent internal system time.
 */
struct sys_clock
{
    time_t tv_sec;                  /**< seconds */
    uint64_t tv_nsec;               /**< nanoseconds */
};


/**
 * @var monotonic_time
 * @brief monotonic time.
 *
 * System monotonic time (counting from \ref startup_time).
 */
extern struct sys_clock monotonic_time;


/**
 * @var startup_time
 * @brief startup time.
 *
 * System startup time (in seconds since 1 Jan 1970).
 */
extern time_t startup_time;


/**
 * \def now
 *
 * Get current time in seconds
 */
#define now()       (startup_time + monotonic_time.tv_sec)

#include <kernel/mutex.h>


/**
 * @var waiter_head
 * @brief heads of clock waiter queues.
 *
 * The kernel keeps separate queues for tasks with timers waiting on different
 * clocks. Currently there are only two queues for the CLOCK_REALTIME and 
 * CLOCK_MONOTONIC clocks, respectively.
 */
extern struct clock_waiter_t waiter_head[];


long clock_wait(struct clock_waiter_t *head, pid_t pid,
                int64_t delta, ktimer_t timerid);


/**
 * @brief Initialise kernel clock.
 *
 * Read RTC clock, initialise clocks, and set the \ref startup_time.
 * Called once during boot.
 *
 * @return  nothing.
 */
void init_clock(void);

/**
 * @brief Initialise clock waiter list.
 *
 * Initialise the clock waiter queues. Called once during boot.
 *
 * @return  nothing.
 */
void init_clock_waiters(void);

/**
 * @brief Handler for syscall clock_getres().
 *
 * Get the clock resolution of the given \a clock_id, in nanoseconds.
 *
 * @param   clock_id    clock id
 * @param   res         clock resolution is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_clock_getres(clockid_t clock_id, struct timespec *res);


/**
 * @brief Get clock time.
 *
 * Get current time of clock \a clock_id. Internal function that is
 * called by syscall_clock_gettime() and syscall_gettimeofday().
 *
 * @param   clock_id    clock id
 * @param   tp          clock time is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
long do_clock_gettime(clockid_t clock_id, struct timespec *tp);


/**
 * @brief Handler for syscall clock_gettime().
 *
 * Get current time of clock \a clock_id.
 *
 * @param   clock_id    clock id
 * @param   tp          clock time is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_clock_gettime(clockid_t clock_id, struct timespec *tp);


/**
 * @brief Set clock time.
 *
 * Set current time of clock \a clock_id. Internal function that is
 * called by syscall_clock_settime() and syscall_settimeofday().
 *
 * @param   clock_id    clock id
 * @param   tp          new clock time
 *
 * @return  zero on success, -(errno) on failure.
 */
long do_clock_settime(clockid_t clock_id, struct timespec *tp);


/**
 * @brief Handler for syscall clock_settime().
 *
 * Set current time of clock \a clock_id.
 *
 * @param   clock_id    clock id
 * @param   tp          new clock time
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_clock_settime(clockid_t clock_id, struct timespec *tp);


/**
 * @brief Check expired timers.
 *
 * Called by the timer IRQ handler. Checks for expired timers and, if there
 * are any, schedules a soft interrupt (\ref SOFTINT_SLEEP) to handle them.
 *
 * @return  nothing.
 *
 * @see     SOFTINT_SLEEP, softsleep()
 */
void clock_check_waiters(void);


/**
 * @brief Get the clock_waiter_t struct for a task.
 *
 * If \a task is a task waiting on a timer, there will be a \a clock_waiter_t
 * struct representing the task on the waiters queue.
 *
 * @param   head                pointer to the waiters queue head
 * @param   pid                 the waiting task's pid
 * @param   timerid             timer id of the POSIX timer the task is
 *                                waiting on
 * @param   remaining_ticks     if the timer has not expired yet, the 
 *                                remaining time (in ticks) is returned here
 * @param   unlink              if non-zero, the task is removed from the
 *                                waiters queue
 *
 * @return  pointer to a \a clock_waiter_t on success, NULL on failure.
 */
struct clock_waiter_t *get_waiter(volatile struct clock_waiter_t *head,
                                  pid_t pid, ktimer_t timerid,
                                  int64_t *remaining_ticks, int unlink);

void waiter_free(struct clock_waiter_t *w);

/**
 * @brief Nanosleep on a clock.
 *
 * Perform a nanosleep on clock \a clock_id. Internal function that is
 * called by syscall_clock_nanosleep() and syscall_timer_settime().
 *
 * @param   pid         the waiting task's pid
 * @param   clock_id    clock id (only CLOCK_REALTIME and CLOCK_MONOTONIC are
 *                        currently supported)
 * @param   flags       flags, either 0 or TIMER_ABSTIME
 * @param   __rqtp      requested wait time
 * @param   __rmtp      if not NULL, the remaining time is returned here
 * @param   timerid     timer id of the POSIX timer the task is waiting on
 *
 * @return  zero on success, -(errno) on failure.
 */
long do_clock_nanosleep(pid_t pid, clockid_t clock_id, int flags, 
                        struct timespec *__rqtp, struct timespec *__rmtp,
                        ktimer_t timerid);

/**
 * @brief Handler for syscall clock_nanosleep().
 *
 * Nanosleep on clock \a clock_id.
 *
 * @param   clock_id    clock id (only CLOCK_REALTIME and CLOCK_MONOTONIC are
 *                        currently supported)
 * @param   flags       flags, either 0 or TIMER_ABSTIME
 * @param   __rqtp      requested wait time
 * @param   __rmtp      if not NULL, the remaining time is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_clock_nanosleep(clockid_t clock_id, int flags, 
                             struct timespec *__rqtp, struct timespec *__rmtp);


/**
 * @brief Handler for syscall nanosleep().
 *
 * Nanosleep on the CLOCK_REALTIME clock.
 *
 * @param   __rqtp      requested wait time
 * @param   __rmtp      if not NULL, the remaining time is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_nanosleep(struct timespec *__rqtp, struct timespec *__rmtp);


/**
 * @brief Get startup time.
 *
 * Returns system startup time in seconds since 1 January 1970.
 *
 * @return  system startup time in seconds.
 */
time_t get_startup_time(void);


/**
 * @brief Get current time in microseconds.
 *
 * Returns current system time in microseconds.
 *
 * @param   tvp     timeval struct to be filled with current time value
 *
 * @return  nothing.
 */
void microtime(struct timeval *tvp);

#endif      /* __KERNEL_CLOCK__ */
