/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025, 2026 (c)
 * 
 *    file: posix_timers_inlines.h
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
 *  \file posix_timers_inlines.h
 *
 *  Inlined functions used by POSIX timers.
 */

#ifdef DEFINE_POSIX_TIMER_INLINES

#undef INLINE
#define INLINE      static inline __attribute__((always_inline))

/*
 * Reset a POSIX timer.
 */
INLINE void timer_reset(volatile struct posix_timer_t *timer)
{
    if(timer)
    {
        if(timer->val.it_interval.tv_sec || timer->val.it_interval.tv_nsec)
        {
            do_clock_nanosleep(timer->flags, &timer->val.it_interval, NULL, timer);
        }
    }
}


/*
 * Notify task of POSIX timer expiration.
 */
INLINE void timer_notify_expired(volatile struct posix_timer_t *timer)
{
    volatile struct task_t *task = get_task_by_tgid(timer->tgid);

    if(task && timer)
    {
        if(timer->timerid == ITIMER_REAL_ID || timer->timerid == ITIMER_PROF_ID)
        {
            siginfo_nopad_t itimer_siginfo = { .sinp_code = SI_TIMER };

            add_task_signal((struct task_t *)task, timer->sigev.sigev_signo, &itimer_siginfo, 1);
        }
        else if(timer->sigev.sigev_notify == SIGEV_SIGNAL)
        {
            if(++timer->cur_overruns >= DELAYTIMER_MAX)
            {
                timer->cur_overruns = 1;
            }

            add_task_timer_signal(task, timer->sigev.sigev_signo, timer->timerid);
        }
    }
}


INLINE void timer_unwait(volatile struct posix_timer_t *head,
                         volatile struct posix_timer_t *timer)
{
    get_waiter(head, timer, NULL, 1);
}

#endif      /* DEFINE_POSIX_TIMER_INLINES */
