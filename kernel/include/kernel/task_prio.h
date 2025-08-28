/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: task_prio.h
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
 *  \file task_prio.h
 *
 *  Functions and macros to help with changing task priorities.
 */

#ifndef __TASK_PRIO_H__
#define __TASK_PRIO_H__

#if !defined(__TASK_H__) && !defined(__KERNEL_MUTEX_H__)
#error Do not include this file directly, instead include "task.h" or "mutex.h"
#endif


#include <sched.h>

/*
 * Temporarily elevate a task's priority.
 *
 * static inline void elevate_priority(volatile struct task_t *task, 
 *                                     int *old_prio, int *old_policy);
 *
 */

#define elevate_priority(task, old_prio, old_policy)    \
    if(task)                                            \
    {                                                   \
        *(old_prio) = (task)->priority;                 \
        *(old_policy) = (task)->sched_policy;           \
        (task)->priority = MAX_FIFO_PRIO;               \
        (task)->sched_policy = SCHED_FIFO;              \
    }

/*
 * Restore a temporarily elevated task's priority.
 *
 * static inline void restore_priority(volatile struct task_t *task, 
 *                                     int old_prio, int old_policy);
 */

#define restore_priority(task, old_prio, old_policy)    \
    if(task)                                            \
    {                                                   \
        (task)->priority = (old_prio);                  \
        (task)->sched_policy = (old_policy);            \
    }

#endif      /* __TASK_PRIO_H__ */
