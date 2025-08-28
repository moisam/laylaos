/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: posixtimer-def.h
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
 *  \file posixtimer-def.h
 *
 *  POSIX timer-related structure and macro definitions.
 */

#ifndef __POSIX_TIMER_T_DEFINED__

#define __POSIX_TIMER_T_DEFINED__

#include <time.h>

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

#endif
