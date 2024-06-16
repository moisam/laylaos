/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: task_funcs.c
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
 *  \file task_funcs.c
 *
 *  Inlined functions for the kernel scheduler use.
 */

//#define __DEBUG
#include <kernel/laylaos.h>
#include <kernel/task.h>

#undef INLINE
#define INLINE      static inline __attribute__((always_inline))

#define TIMESLICE_RR(t)                                                 \
    ((t)->task_rlimits[RLIMIT_RTTIME].rlim_cur == RLIM_INFINITY) ?      \
        (rlim_t)((t)->priority >> 1) :                                  \
        ((t)->task_rlimits[RLIMIT_RTTIME].rlim_cur / USECS_PER_TICK)

#define TIMESLICE_OTHER(t)      (2 + ((t)->nice >> 3))
#define TIMESLICE_FIFO(t)       (0)


extern int user_has_ready_tasks;
extern int rr_has_ready_tasks;
extern int fifo_has_ready_tasks;


INLINE int get_task_timeslice(struct task_t *task)
{
    if(task->sched_policy == SCHED_RR)
    {
        return TIMESLICE_RR(task);
    }
    else if(task->sched_policy == SCHED_OTHER)
    {
        return TIMESLICE_OTHER(task);
    }
    else
    {
        return TIMESLICE_FIFO(task);
    }
}


INLINE void reset_task_timeslice(struct task_t *task)
{
    task->time_left = task->timeslice;
}


INLINE void append_to_queue(struct task_t *t, struct task_queue_t *queue)
{
    t->prev = queue->head.prev;
    queue->head.prev = t;
    t->prev->next = t;
    t->next = &queue->head;
}


INLINE void prepend_to_queue(struct task_t *t, struct task_queue_t *queue)
{
    t->next = queue->head.next;
    queue->head.next = t;
    t->next->prev = t;
    t->prev = &queue->head;
}


INLINE void remove_from_queue(struct task_t *task)
{
    task->prev->next = task->next;
    task->next->prev = task->prev;
    task->next = NULL;
    task->prev = NULL;
}


INLINE void append_to_ready_queue(struct task_t *t)
{
    struct task_queue_t *queue = &ready_queue[t->priority];

    append_to_queue(t, queue);
    
    int *p = (t->priority == 0) ? &user_has_ready_tasks :
             (t->priority < MIN_RR_PRIO) ? &fifo_has_ready_tasks :
                                           &rr_has_ready_tasks;
    *p = 1;
}


INLINE void prepend_to_ready_queue(struct task_t *t)
{
    struct task_queue_t *queue = &ready_queue[t->priority];

    prepend_to_queue(t, queue);

    int *p = (t->priority == 0) ? &user_has_ready_tasks :
             (t->priority < MIN_RR_PRIO) ? &fifo_has_ready_tasks :
                                           &rr_has_ready_tasks;
    *p = 1;
}


INLINE void remove_from_ready_queue(struct task_t *t)
{
    struct task_queue_t *queue = &ready_queue[t->priority];

    remove_from_queue(t);

    if(queue->head.next == &queue->head)
    {
        int *p = (t->priority == 0) ? &user_has_ready_tasks :
                 (t->priority < MIN_RR_PRIO) ? &fifo_has_ready_tasks :
                                               &rr_has_ready_tasks;
        *p = 0;
    }
}


INLINE void move_to_queue_end(struct task_t *task)
{
    struct task_queue_t *queue = &ready_queue[task->priority];

    remove_from_queue(task);
    append_to_queue(task, queue);
    
    int *p = (task->priority == 0) ? &user_has_ready_tasks :
             (task->priority < MIN_RR_PRIO) ? &fifo_has_ready_tasks :
                                              &rr_has_ready_tasks;
    *p = 1;
}


INLINE void update_task_times(struct task_t *t)
{
    unsigned long elapsed = ticks - prev_ticks;

    prev_ticks = ticks;

    if(elapsed == 0)
    {
        return;
    }

    /* update user and system times */
    if(t->user && !t->user_in_kernel_mode)
    {
        t->user_time += elapsed;
    }
    else
    {
        t->sys_time += elapsed;
    }
}


/*
 * Get a task by it's ID. 
 */
INLINE struct task_t *get_task_by_id(pid_t pid)
{
    struct task_t *res = NULL;

    elevated_priority_lock(&task_table_lock);

    for_each_taskptr(t)
    {
        if(*t && (*t)->pid == pid)
        {
            res = *t;
            break;
        }
    }

    elevated_priority_unlock(&task_table_lock);

    return res;
}


/*
 * Get the first thread with the given TGID. 
 */
INLINE struct task_t *get_task_by_tgid(pid_t tgid)
{
    struct task_t *res = NULL;

    elevated_priority_lock(&task_table_lock);

    for_each_taskptr(t)
    {
        if(*t && tgid(*t) == tgid)
        {
            res = *t;
            break;
        }
    }
    
    elevated_priority_unlock(&task_table_lock);

    return res;
}


/*
 * Get a task by it's thread ID.
 */
INLINE struct task_t *get_task_by_tid(pid_t tid)
{
    return get_task_by_id(tid);
}

