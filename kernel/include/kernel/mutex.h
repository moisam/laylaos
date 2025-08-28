/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: mutex.h
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
 *  \file mutex.h
 *
 *  Functions for working with kernel mutex locks.
 */

#ifndef __KERNEL_MUTEX_H__
#define __KERNEL_MUTEX_H__

#include <stdint.h>
#include <sys/types.h>


/**
 * @struct kernel_mutex_t
 * @brief The kernel_mutex_t structure.
 *
 * A structure to represent a kernel mutex.
 */
struct kernel_mutex_t
{
    volatile uint32_t lock;       /**< mutex lock */
    volatile int recursive_count; /**< non-zero if mutex is recuresively locked */
    volatile struct task_t *holder;    /**< pointer to task holding the mutex */
    volatile const char *from_func;    /**< name of function that called 
                                            kernel_mutex_lock() */
    volatile int from_line;            /**< source line number where 
                                            kernel_mutex_lock() was called */
};


void init_kernel_mutex(volatile struct kernel_mutex_t *mutex);
void __kernel_mutex_lock(volatile struct kernel_mutex_t *mutex, const char *func, int line);
uint32_t __kernel_mutex_trylock(volatile struct kernel_mutex_t *mutex, const char *func, int line);
void kernel_mutex_unlock(volatile struct kernel_mutex_t *mutex);

#define kernel_mutex_lock(m)     __kernel_mutex_lock(m, __func__, __LINE__)
#define kernel_mutex_trylock(m)  __kernel_mutex_trylock(m, __func__, __LINE__)

#define kernel_mutex_lock_recursive(lock)                           \
    if(__kernel_mutex_trylock(lock, __func__, __LINE__)) {          \
        if(!cur_task || (lock)->holder != cur_task)                 \
            __kernel_mutex_lock(lock, __func__, __LINE__);          \
        else (lock)->recursive_count = (lock)->recursive_count + 1; \
    }

#define kernel_mutex_unlock_recursive(lock)                         \
    if((lock)->recursive_count != 0)                                \
        (lock)->recursive_count = (lock)->recursive_count - 1;      \
    else kernel_mutex_unlock(lock);


void scheduler(void);

#include <kernel/smp.h>
#include "task_prio.h"
#include "task_locks.h"

#if 0

/**
 * @brief Initialise a kernel mutex.
 *
 * Initialize a kernel mutex.
 *
 * @param   mutex       the mutex to be initialised
 *
 * @return  1 always.
 */
static inline void init_kernel_mutex(struct kernel_mutex_t *mutex)
{
    mutex->lock = 0;
    mutex->wanted = 0;
    mutex->holder = 0;
}

/**
 * @brief Lock a kernel mutex.
 *
 * This function sleeps until the lock is acquired.
 *
 * @param   mutex       the mutex to lock
 * @param   caller      caller function name
 *
 * @return  nothing.
 */
static inline void kernel_mutex_lock(struct kernel_mutex_t *mutex)
{
    while(__sync_lock_test_and_set(&mutex->lock, 1))
    {
        lock_scheduler();
        scheduler();
        unlock_scheduler();
    }
}

/**
 * @brief Try to lock a kernel mutex.
 *
 * If lock is already locked, 1 is returned. Otherwise 0 is returned.
 *
 * @param   mutex       the mutex to lock
 *
 * @return  0 if lock acquired, 1 otherwise.
 */
static inline uint32_t kernel_mutex_trylock(struct kernel_mutex_t *mutex)
{
    return __sync_lock_test_and_set(&mutex->lock, 1);
}

/**
 * @brief Unlock a kernel mutex.
 *
 * Unlock a locked kernel mutex.
 *
 * @param   mutex       the mutex to unlock
 * @param   caller      caller function name
 *
 * @return  nothing.
 */
static inline void kernel_mutex_unlock(struct kernel_mutex_t *mutex)
{
    __sync_lock_release(&mutex->lock);
    mutex->holder = 0;
}

#endif

#endif      /* __KERNEL_MUTEX_H__ */
