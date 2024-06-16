/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: nice.c
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
 *  \file nice.c
 *
 *  Functions for getting and setting task priorities.
 */

#include <errno.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/syscall.h>

#include "../kernel/task_funcs.c"

/*
 * User's nice values are between -20 to 19, while kernel's nice values
 * are between 40 and 1. This is because negative values are usually reserved
 * for errors (at least in Linux). The exchange formula is:
 *
 *    unice = 20 - knice
 *
 * For getpriority() and setpriority() syscalls, the glibc wrapper functions
 * should handle the exchange.
 *
 * Read more at: https://man7.org/linux/man-pages/man2/setpriority.2.html
 */


static int adjust_nice(struct task_t *ct, int nice)
{
    if(nice < 1)
    {
        nice = 1;
    }
    else if(nice > 40)
    {
        nice = 40;
    }

    if(exceeds_rlimit(ct, RLIMIT_NICE, nice))
    {
        nice = ct->task_rlimits[RLIMIT_NICE].rlim_cur;
    }
    
    return nice;
}


/*
 * Handler for syscall nice().
 */
int syscall_nice(int increment)
{
    struct task_t *ct = cur_task;
    int nice = ct->nice + increment;

    nice = adjust_nice(ct, nice);

    // only root can reduce nice values
    if(ct->nice < nice && !suser(ct))
    {
        return -EPERM;
    }
    
    ct->nice = nice;

    return 0;
}


/*
 * Handler for syscall getpriority().
 */
int syscall_getpriority(int which, id_t who, int *__nice)
{
    int nice = 40, found = 0;
    struct task_t *ct = cur_task;

    if(!__nice)
    {
        return -EINVAL;
    }
    
    if(which == PRIO_PROCESS)
    {
        struct task_t *task = who ? get_task_by_id((pid_t)who) : ct;

        if(!task)
        {
            return -ESRCH;
        }

        nice = task->nice;
        found = 1;
    }
    else if(which == PRIO_PGRP)
    {
        if(who == 0)
        {
            who = (id_t)ct->pgid;
        }
        
        elevated_priority_lock(&task_table_lock);

        for_each_taskptr(t)
        {
            if(*t && (*t)->pgid == (pid_t)who)
            {
                found = 1;
                if((*t)->nice > nice)
                {
                    nice = (*t)->nice;
                }
            }
        }

        elevated_priority_unlock(&task_table_lock);
    }
    else if(which == PRIO_USER)
    {
        uid_t uid = (who == 0) ? ct->uid : who;
        
        elevated_priority_lock(&task_table_lock);

        for_each_taskptr(t)
        {
            if(*t && (*t)->uid == uid)
            {
                found = 1;
                if((*t)->nice > nice)
                {
                    nice = (*t)->nice;
                }
            }
        }

        elevated_priority_unlock(&task_table_lock);
    }
    else
    {
        return -EINVAL;
    }

    if(!found)
    {
        return -ESRCH;
    }
    
    copy_to_user(__nice, &nice, sizeof(int));

    return 0;
}


/*
 * Handler for syscall setpriority().
 */
int syscall_setpriority(int which, id_t who, int value)
{
    struct task_t *ct = cur_task;

    value = adjust_nice(ct, value);
    
    if(which == PRIO_PROCESS)
    {
        struct task_t *task = who ? get_task_by_id((pid_t)who) : ct;

        if(!task)
        {
            return -ESRCH;
        }

        /* only root can lower a nice value */
        if((task->nice < value) && !suser(ct))
        {
            return -EACCES;
        }
        
        /* check permissions */
        if(!suser(ct) && ct->uid != task->euid && ct->euid != task->euid)
        {
            return -EPERM;
        }
        
        if(task->sched_policy == SCHED_OTHER)
        {
            task->nice = value;
        }
    }
    else if(which == PRIO_PGRP)
    {
        if(who == 0)
        {
            who = (id_t)ct->pgid;
        }
        
        elevated_priority_lock(&task_table_lock);

        for_each_taskptr(t)
        {
            if(*t && (*t)->pgid == (pid_t)who)
            {
                /* only root can lower a nice value */
                if((*t)->nice < value && !suser(ct))
                {
                    elevated_priority_unlock(&task_table_lock);
                    return -EACCES;
                }

                /* check permissions */
                if(!suser(ct) && ct->uid != (*t)->euid && 
                                 ct->euid != (*t)->euid)
                {
                    elevated_priority_unlock(&task_table_lock);
                    return -EPERM;
                }
                
                if((*t)->sched_policy == SCHED_OTHER)
                {
                    (*t)->nice = value;
                }
            }
        }

        elevated_priority_unlock(&task_table_lock);
    }
    else if(which == PRIO_USER)
    {
        uid_t uid = (who == 0) ? ct->uid : who;

        /* check permissions */
        if(!suser(ct) && ct->uid != uid && ct->euid != uid)
        {
            return -EPERM;
        }
        
        elevated_priority_lock(&task_table_lock);

        for_each_taskptr(t)
        {
            if(*t && (*t)->uid == uid)
            {
                /* only root can lower a nice value */
                if((*t)->nice < value && !suser(ct))
                {
                    elevated_priority_unlock(&task_table_lock);
                    return -EACCES;
                }

                if((*t)->sched_policy == SCHED_OTHER)
                {
                    (*t)->nice = value;
                }
            }
        }

        elevated_priority_unlock(&task_table_lock);
    }
    else
    {
        return -EINVAL;
    }
    
    return 0;
}

