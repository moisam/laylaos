/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
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


static void change_priority(struct task_t *t, int new_prio, int new_policy)
{
    int old_prio = t->priority;

    lock_scheduler();

    t->sched_policy = new_policy;
    
    /*
     * The sched(7) manpage says:
     *    If a call to sched_setscheduler(2), sched_setparam(2),
     *    sched_setattr(2), pthread_setschedparam(3), or 
     *    pthread_setschedprio(3) changes the priority of the running or
     *    runnable SCHED_FIFO thread identified by pid the effect on the
     *    thread's position in the list depends on the direction of the
     *    change to threads priority:
     *     •  If the thread's priority is raised, it is placed at the end
     *        of the list for its new priority.  As a consequence, it may
     *        preempt a currently running thread with the same priority.
     *     •  If the thread's priority is unchanged, its position in the
     *        run list is unchanged.
     *     •  If the thread's priority is lowered, it is placed at the
     *        front of the list for its new priority.
     */

    if(old_prio != new_prio &&
       (new_policy == SCHED_FIFO || new_policy == SCHED_RR) &&
       (t->state == TASK_READY || t->state == TASK_RUNNING))
    {
        KDEBUG("%s: pid %d\n", __func__, t->pid);
        remove_from_ready_queue(t);
        t->priority = new_prio;
        
        if(new_prio > old_prio)
        {
            append_to_ready_queue(t);
        }
        else //if(new_prio < old_prio)
        {
            prepend_to_ready_queue(t);
        }
    }
    else
    {
        t->priority = new_prio;
    }

    unlock_scheduler();
}


/*
 * Check if the given scheduling priority is valid for the given scheduling
 * policy and is within the task's resource limit.
 *
 * Returns 1 if the priority is valid, 0 if not.
 */
static int valid_priority(struct task_t *t, int prio, int policy)
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
int syscall_sched_rr_get_interval(pid_t pid, struct timespec *tp)
{
    struct timespec tmp;
    struct task_t *t = NULL;
    struct task_t *ct = cur_task;
    
    if(pid < 0 || !tp)
    {
        return -EINVAL;
    }
    
    if(!(t = get_task_by_id((pid == 0) ? ct->pid : pid)))
    {
        return -ESRCH;
    }
    
    if(!suser(ct) && ct->uid != t->uid && ct->euid != t->euid)
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
int syscall_sched_getparam(pid_t pid, struct sched_param *param)
{
    struct sched_param tmp;
    struct task_t *t = NULL;
    struct task_t *ct = cur_task;
    
    if(pid < 0 || !param)
    {
        return -EINVAL;
    }
    
    if(!(t = get_task_by_id((pid == 0) ? ct->pid : pid)))
    {
        return -ESRCH;
    }
    
    if(!suser(ct) && ct->uid != t->uid && ct->euid != t->euid)
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
int syscall_sched_setparam(pid_t pid, struct sched_param *param)
{
    struct sched_param tmp;
    struct task_t *t = NULL;
    struct task_t *ct = cur_task;
    
    if(pid < 0 || !param)
    {
        return -EINVAL;
    }
    
    if(!(t = get_task_by_id((pid == 0) ? ct->pid : pid)))
    {
        return -ESRCH;
    }
    
    if(!suser(ct) && ct->uid != t->uid && ct->euid != t->uid)
    {
        return -EPERM;
    }
    
    COPY_FROM_USER(&tmp, param, sizeof(struct sched_param));

    if(!valid_priority(t, tmp.sched_priority, t->sched_policy))
    {
        return -EINVAL;
    }
    
    change_priority(t, tmp.sched_priority, t->sched_policy);
    
    return 0;
}


/*
 * Handler for syscall sched_getscheduler().
 */
int syscall_sched_getscheduler(pid_t pid)
{
    struct task_t *t = NULL;
    struct task_t *ct = cur_task;

    if(pid < 0)
    {
        return -EINVAL;
    }
    
    if(!(t = get_task_by_id((pid == 0) ? ct->pid : pid)))
    {
        return -ESRCH;
    }
    
    if(!suser(ct) && ct->uid != t->uid && ct->euid != t->euid)
    {
        return -EPERM;
    }
    
    return t->sched_policy;
}


/*
 * Handler for syscall sched_setscheduler().
 */
int syscall_sched_setscheduler(pid_t pid, int policy, 
                               struct sched_param *param)
{
    struct sched_param tmp;
    struct task_t *t = NULL;
    struct task_t *ct = cur_task;
    
    if(pid < 0 || !param)
    {
        return -EINVAL;
    }
    
    if(policy != SCHED_FIFO && policy != SCHED_RR && policy != SCHED_OTHER)
    {
        return -EINVAL;
    }
    
    if(!(t = get_task_by_id((pid == 0) ? ct->pid : pid)))
    {
        return -ESRCH;
    }
    
    if(!suser(ct) && ct->uid != t->uid && ct->euid != t->uid)
    {
        return -EPERM;
    }
    
    COPY_FROM_USER(&tmp, param, sizeof(struct sched_param));
    
    if(!valid_priority(ct, tmp.sched_priority, policy))
    {
        return -EINVAL;
    }

    change_priority(t, tmp.sched_priority, policy);
    
    return 0;
}


/*
 * Handler for syscall sched_get_priority_max().
 */
int syscall_sched_get_priority_max(int policy)
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
int syscall_sched_get_priority_min(int policy)
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
int syscall_sched_yield(void)
{
    struct task_t *ct = cur_task;
    lock_scheduler();

    /*
     * The sched (7) manpage says:
     *    A [SCHED_FIFO] thread calling sched_yield(2) will be put at the end
     *    of the list.
     */
    if(ct->sched_policy == SCHED_FIFO || ct->sched_policy == SCHED_RR)
    {
        KDEBUG("%s: pid %d\n", __func__, ct->pid);
        move_to_queue_end(ct);
    }

    scheduler();
    unlock_scheduler();
    return 0;
}

