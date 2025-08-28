/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: task_locks.h
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
 *  \file task_locks.h
 *
 *  Functions and macros to help with recursive task locks and the global
 *  scheduler lock (when running in SMP mode).
 */

#ifndef __TASK_LOCKS_H__
#define __TASK_LOCKS_H__

#if !defined(__TASK_H__) && !defined(__KERNEL_MUTEX_H__)
#error Do not include this file directly, instead include "task.h" or "mutex.h"
#endif


/*
 * A priority inversion issue happens when one of our higher priority
 * kernel tasks try to lock some mutex while a lower priority user task
 * has it locked. This happens with some locks like the select table mutex 
 * and the master task table mutex, which are contended by user, fifo and 
 * round-robin tasks alike.
 *
 * To avoid this, we temporarily assign the task holding the lock a high
 * priority, which should be held for a very short time only to avoid 
 * starving other processes. This is only one solution, known as the 
 * priority ceiling protocol.
 *
 * See: https://en.wikipedia.org/wiki/Priority_inversion
 */

#define elevated_priority_lock(lock)    \
    int old_prio = 0, old_policy = 0;   \
    elevate_priority(this_core->cur_task, &old_prio, &old_policy); \
    kernel_mutex_lock(lock);

#define elevated_priority_relock(lock)  \
    elevate_priority(this_core->cur_task, &old_prio, &old_policy); \
    kernel_mutex_lock(lock);

#define elevated_priority_unlock(lock)  \
    kernel_mutex_unlock(lock);          \
    restore_priority(this_core->cur_task, old_prio, old_policy);

#define elevated_priority_lock_recursive(lock, count)   \
    int old_prio = 0, old_policy = 0;                   \
    elevate_priority(this_core->cur_task, &old_prio, &old_policy); \
    if(kernel_mutex_trylock(lock)) {                    \
        if(!this_core->cur_task || (lock)->holder != this_core->cur_task /* ->pid */)\
            kernel_mutex_lock(lock);                    \
        else (count) = (count) + 1;                     \
    } else {                                            \
        if(this_core->cur_task)                                    \
            (lock)->holder = this_core->cur_task /* ->pid */;             \
        (count) = 0;                                    \
    }

#define elevated_priority_unlock_recursive(lock, count) \
    if((count) != 0) (count) = (count) - 1;             \
    else {                                              \
        kernel_mutex_unlock(lock);                      \
        restore_priority(this_core->cur_task, old_prio, old_policy); \
    }


#ifdef SMP
//extern struct kernel_mutex_t scheduler_lock;
extern volatile int scheduler_holding_cpu;
#endif

extern volatile int IRQ_disable_counter;

#if 0
/**
 * @brief Lock the scheduler.
 *
 * Lock the scheduler.
 *
 * The following function is taken from:
 *    https://wiki.osdev.org/Brendan%27s_Multi-tasking_Tutorial
 *
 * @return  nothing.
 */
static inline void lock_scheduler(void)
{
#ifdef SMP
    //kernel_mutex_lock(&scheduler_lock);

    while(!__sync_bool_compare_and_swap(&scheduler_holding_cpu, -1, this_core->cpuid))
    {
        if(scheduler_holding_cpu == this_core->cpuid)
        {
            //__asm__ __volatile__("xchg %%bx, %%bx":::);
            break;
        }
    }
#else
    __asm__ __volatile__ ("cli");
#endif
    IRQ_disable_counter++;
}

/**
 * @brief Unlock the scheduler.
 *
 * Unlock the scheduler.
 *
 * The following function is taken from:
 *    https://wiki.osdev.org/Brendan%27s_Multi-tasking_Tutorial
 *
 * @return  nothing.
 */
static inline void unlock_scheduler(void)
{
    IRQ_disable_counter--;

    if(IRQ_disable_counter == 0)
    {
#ifdef SMP
        //kernel_mutex_unlock(&scheduler_lock);
        __sync_bool_compare_and_swap(&scheduler_holding_cpu, this_core->cpuid, -1);
#else
        __asm__ __volatile__ ("sti");
#endif
    }
}
#endif

#endif      /* __TASK_LOCKS_H__ */
