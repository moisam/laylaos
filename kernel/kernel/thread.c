/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: thread.c
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
 *  \file thread.c
 *
 *  The kernel threads implementation.
 */

#include <errno.h>
#include <stdlib.h>
#include <kernel/thread.h>
#include <kernel/ksignal.h>
#include <kernel/user.h>

#include "task_funcs.c"


/*
 * Handler for syscall tgkill().
 *
 * See: https://man7.org/linux/man-pages/man2/tgkill.2.html
 */
long syscall_tgkill(pid_t tgid, pid_t tid, int sig)
{
	volatile struct task_t *ct = this_core->cur_task;
    int force = !(ct->uid || ct->euid);
    volatile struct task_t *task = get_task_by_id(tid);

    if(!task || !task->threads || !task->threads->thread_group_leader ||
       sig < 1 || sig >= NSIG)
    {
        return -EINVAL;
    }
    
    if(task->threads->tgid != tgid)
    {
        return -ESRCH;
    }


    kernel_mutex_lock(&(task->threads->mutex));
    user_add_task_signal(task, sig, force);
    kernel_mutex_unlock(&(task->threads->mutex));
    return 0;
}


/*
 * Handler for syscall gettid().
 */
long syscall_gettid(void)
{
    return this_core->cur_task->pid;
}


/*
 * Check if the other threads (other than the calling thread) in this task
 * are dead.
 *
 * The caller MUST lock the threads struct before calling this function!
 *
 * Returns 1 if all other threads are zombies, 0 if at least one is alive.
 */
int other_threads_dead(volatile struct task_t *task)
{
    struct task_t *t;

    if(!task || !task->threads)
    {
        return -EINVAL;
    }

    for_each_thread(t, task)
    {
        if(t == task)
        {
            continue;
        }
        
        if(t->state != TASK_ZOMBIE)
        {
            return 0;
        }
    }
    
    return 1;
}


/*
 * Terminate (exit) all threads in the current running task.
 * Does the actual work of the exit_group syscall, which is called by the
 * _exit() C library function.
 */
void terminate_thread_group(int code)
{
    if(this_core->cur_task->threads)
    {
        kernel_mutex_lock(&(this_core->cur_task->threads->mutex));

        if(!(this_core->cur_task->threads->flags & TG_FLAG_EXITING))
        {
            this_core->cur_task->threads->flags |= TG_FLAG_EXITING;
            kernel_mutex_unlock(&(this_core->cur_task->threads->mutex));

            if(this_core->cur_task->threads->thread_count > 1)
            {
                /* kill the other threads */
                __terminate_thread_group();
            }
        }
        else
        {
            kernel_mutex_unlock(&(this_core->cur_task->threads->mutex));
        }
    }

    /* now kill us */
    terminate_task(code);
}


/*
 * Does the actual work of terminating the thread group.
 */
void __terminate_thread_group(void)
{
    volatile int retry = 1;
    struct task_t *t;

    if(!this_core->cur_task->threads)
    {
        return;
    }

    while(retry)
    {
        retry = 0;

        kernel_mutex_lock(&(this_core->cur_task->threads->mutex));

        for_each_thread(t, this_core->cur_task)
        {
            if(t == this_core->cur_task || t->state == TASK_ZOMBIE)
            {
                continue;
            }
            
            user_add_task_signal(t, SIGKILL, 1);
            retry = 1;
        }

        kernel_mutex_unlock(&(this_core->cur_task->threads->mutex));

        /* give the threads a chance to die */
        if(retry)
        {
            scheduler();
        }
    }
}


/*
 * Reap zombie threads.
 */
/*
void reap_dead_threads(struct task_t *task)
{
    struct task_t *t, *prev = NULL, *next = NULL;
    time_t user_time, sys_time;

    if(!task->threads || task->threads->thread_count == 1)
    {
        return;
    }

    kernel_mutex_lock(&(task->threads->mutex));

    user_time = task->threads->group_user_time;
    sys_time = task->threads->group_sys_time;
    
    for(t = task->threads->thread_group_leader; t != NULL; )
    {
        next = t->thread_group_next;

        if(t->state != TASK_ZOMBIE || t == task)
        {
            // this shouldn't happen (except for the calling thread)
            prev = t;
            t = next;
            continue;
        }
        
        // update process times
        user_time += (t->user_time + t->children_user_time);
        sys_time  += (t->sys_time  + t->children_sys_time );

        t->ofiles = NULL;
        t->fs = NULL;
        t->sig = NULL;
        t->threads = NULL;
        t->common = NULL;
        t->mem = NULL;

        // reap the zombie thread
        reap_zombie(t);

        task->threads->thread_count--;
        
        if(!prev)
        {
            task->threads->thread_group_leader = next;
        }
        else
        {
            prev->thread_group_next = next;
        }
        
        //t = next;
        t = task->threads->thread_group_leader;
    }

    task->threads->group_user_time = user_time;
    task->threads->group_sys_time = sys_time;

    kernel_mutex_unlock(&(task->threads->mutex));
}
*/


/*
 * Get thread group id.
 */
long get_tgid(struct task_t *task)
{
    return tgid(task);
}

