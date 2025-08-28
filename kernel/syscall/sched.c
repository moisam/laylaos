/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: sched.c
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
 *  \file sched.c
 *
 *  Functions for getting and setting scheduling parameters.
 */

#include <errno.h>
#include <string.h>
//#include <sys/sched.h>
#include <sched.h>
#include <sys/types.h>
#include <kernel/syscall.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/asm.h>

#include "../kernel/task_funcs.c"


/*
 * Check if the given scheduling priority is valid for the given scheduling
 * policy and is within the task's resource limit.
 *
 * Returns 1 if the priority is valid, 0 if not.
 */
static int valid_priority(volatile struct task_t *t, int prio, int policy)
{
    if(prio < syscall_sched_get_priority_min(policy) ||
       prio > syscall_sched_get_priority_max(policy))
    {
        return 0;
    }

    if((policy == SCHED_RR || policy == SCHED_FIFO) &&
       exceeds_rlimit(t, RLIMIT_RTPRIO, prio))
    {
        return 0;
    }

    return 1;
}


/*
 * Handler for syscall sched_rr_get_interval().
 */
long syscall_sched_rr_get_interval(pid_t pid, struct timespec *tp)
{
    struct timespec tmp;
    volatile struct task_t *t = NULL;
    
    if(pid < 0 || !tp)
    {
        return -EINVAL;
    }
    
    if(!(t = get_task_by_id((pid == 0) ? this_core->cur_task->pid : pid)))
    {
        return -ESRCH;
    }
    
    if(!suser(this_core->cur_task) &&
       this_core->cur_task->uid != t->uid &&
       this_core->cur_task->euid != t->euid)
    {
        return -EPERM;
    }
    
    if(t->sched_policy != SCHED_RR)
    {
        return -EINVAL;
    }

    /* NOTE: convert to microseconds. */
    int ticks = get_task_timeslice(t);

    ticks_to_timespec(ticks, &tmp);
    return copy_to_user(tp, &tmp, sizeof(struct timespec));
}


/*
 * Handler for syscall sched_getparam().
 */
long syscall_sched_getparam(pid_t pid, struct sched_param *param)
{
    struct sched_param tmp;
    volatile struct task_t *t = NULL;
    
    if(pid < 0 || !param)
    {
        return -EINVAL;
    }
    
    if(!(t = get_task_by_id((pid == 0) ? this_core->cur_task->pid : pid)))
    {
        return -ESRCH;
    }
    
    if(!suser(this_core->cur_task) &&
       this_core->cur_task->uid != t->uid &&
       this_core->cur_task->euid != t->euid)
    {
        return -EPERM;
    }
    
    A_memset(&tmp, 0, sizeof(struct sched_param));
    tmp.sched_priority = t->priority;
    return copy_to_user(param, &tmp, sizeof(struct sched_param));
}


/*
 * Handler for syscall sched_setparam().
 */
long syscall_sched_setparam(pid_t pid, struct sched_param *param)
{
    struct sched_param tmp;
    volatile struct task_t *t = NULL;
    
    if(pid < 0 || !param)
    {
        return -EINVAL;
    }
    
    if(!(t = get_task_by_id((pid == 0) ? this_core->cur_task->pid : pid)))
    {
        return -ESRCH;
    }
    
    if(!suser(this_core->cur_task) &&
       this_core->cur_task->uid != t->uid &&
       this_core->cur_task->euid != t->uid)
    {
        return -EPERM;
    }
    
    COPY_FROM_USER(&tmp, param, sizeof(struct sched_param));

    if(!valid_priority(t, tmp.sched_priority, t->sched_policy))
    {
        return -EINVAL;
    }
    
    task_change_priority(t, tmp.sched_priority, t->sched_policy);
    
    return 0;
}


/*
 * Handler for syscall sched_getscheduler().
 */
long syscall_sched_getscheduler(pid_t pid)
{
    volatile struct task_t *t = NULL;

    if(pid < 0)
    {
        return -EINVAL;
    }
    
    if(!(t = get_task_by_id((pid == 0) ? this_core->cur_task->pid : pid)))
    {
        return -ESRCH;
    }
    
    if(!suser(this_core->cur_task) &&
       this_core->cur_task->uid != t->uid &&
       this_core->cur_task->euid != t->euid)
    {
        return -EPERM;
    }
    
    return t->sched_policy;
}


/*
 * Handler for syscall sched_setscheduler().
 */
long syscall_sched_setscheduler(pid_t pid, int policy, 
                                struct sched_param *param)
{
    struct sched_param tmp;
    volatile struct task_t *t = NULL;
    
    if(pid < 0 || !param)
    {
        return -EINVAL;
    }
    
    if(policy != SCHED_FIFO && policy != SCHED_RR && policy != SCHED_OTHER)
    {
        return -EINVAL;
    }
    
    if(!(t = get_task_by_id((pid == 0) ? this_core->cur_task->pid : pid)))
    {
        return -ESRCH;
    }
    
    if(!suser(this_core->cur_task) &&
       this_core->cur_task->uid != t->uid &&
       this_core->cur_task->euid != t->uid)
    {
        return -EPERM;
    }
    
    COPY_FROM_USER(&tmp, param, sizeof(struct sched_param));
    
    if(!valid_priority(this_core->cur_task, tmp.sched_priority, policy))
    {
        return -EINVAL;
    }

    task_change_priority(t, tmp.sched_priority, policy);
    
    return 0;
}


/*
 * Handler for syscall sched_get_priority_max().
 */
long syscall_sched_get_priority_max(int policy)
{
    switch(policy)
    {
        case SCHED_FIFO:
            return MAX_FIFO_PRIO;
            
        case SCHED_RR:
            return MAX_RR_PRIO;
        
        case SCHED_OTHER:
            return MAX_USER_PRIO;
    }
    
	return -EINVAL;
}


/*
 * Handler for syscall sched_get_priority_min().
 */
long syscall_sched_get_priority_min(int policy)
{
    switch(policy)
    {
        case SCHED_FIFO:
            return MIN_FIFO_PRIO;
            
        case SCHED_RR:
            return MIN_RR_PRIO;
        
        case SCHED_OTHER:
            return MIN_USER_PRIO;
    }
    
	return -EINVAL;
}


/*
 * Handler for syscall sched_yield().
 */
long syscall_sched_yield(void)
{
    /*
     * The sched (7) manpage says:
     *    A [SCHED_FIFO] thread calling sched_yield(2) will be put at the end
     *    of the list.
     */
    if(this_core->cur_task->sched_policy == SCHED_FIFO ||
       this_core->cur_task->sched_policy == SCHED_RR)
    {
        KDEBUG("%s: pid %d\n", __func__, this_core->cur_task->pid);
        move_to_queue_end_locked(this_core->cur_task);
    }

    scheduler();

    return 0;
}

