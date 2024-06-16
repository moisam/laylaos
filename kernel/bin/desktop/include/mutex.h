/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
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
 *  Fast mutex operations. Mostly useful on the server side, where speed
 *  is important in responding to clients and updating the screen.
 */

#ifndef GUI_INLINE_MUTEX_H
#define GUI_INLINE_MUTEX_H

#define _POSIX_PRIORITY_SCHEDULING      1       // for sched_yield()
#include <sched.h>
//#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    volatile int lock;
    int type;
    int owner;
    int recursion;
} mutex_t;

#define MUTEX_INITIALIZER               { 0, 0, 0, 0 }

#undef __INLINE__
#define __INLINE__          static inline __attribute__ ((always_inline))

__INLINE__ void mutex_init(mutex_t *mutex)
{
    mutex->type = 0;
    mutex->lock = 0;
    mutex->owner = 0;
    mutex->recursion = 0;
}


__INLINE__ void mutex_lock(mutex_t *mutex)
{
    while(__sync_lock_test_and_set(&mutex->lock, 1))
    //while(!__sync_bool_compare_and_swap(&mutex->lock, 0, 1))
    {
        sched_yield();
    }
}


__INLINE__ void mutex_unlock(mutex_t *mutex)
{
    mutex->owner = 0;
    //mutex->lock = 0;
    __sync_lock_release(&mutex->lock);
}


__INLINE__ void mutex_destroy(mutex_t *mutex)
{
    //mutex->lock = -1;
    __sync_lock_test_and_set(&mutex->lock, -1);
}

__INLINE__ int mutex_trylock(mutex_t *mutex)
{
    return __sync_lock_test_and_set(&mutex->lock, 1);
    /*
    if(!__sync_bool_compare_and_swap(&mutex->lock, 0, 1))
    {
        return -1;
    }
    
    return 0;
    */
}

#ifdef __cplusplus
}
#endif

#endif      /* GUI_INLINE_MUTEX_H */
