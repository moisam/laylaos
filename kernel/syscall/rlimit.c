/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: rlimit.c
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
 *  \file rlimit.c
 *
 *  Functions for getting and setting resource usage limits.
 */

//#define __DEBUG

#include <string.h>
#include <errno.h>
#include <ulimit.h>
#include <sys/resource.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/timer.h>
#include <kernel/syscall.h>

#include "../kernel/task_funcs.c"


struct task_rlimit_t default_rlimits[] =
{
    { "Max cpu time", "seconds", { RLIM_INFINITY, RLIM_INFINITY, } },
    { "Max file size", "bytes", { RLIM_INFINITY, RLIM_INFINITY, } },
    { "Max data size", "bytes", { RLIM_INFINITY, RLIM_INFINITY, } },
    { "Max stack size", "bytes", { 1024 * 1024 /* 1024K */, RLIM_INFINITY, } },
    { "Max core file size", "bytes", { 0, RLIM_INFINITY, } },
    { "Max resident set", "bytes", { RLIM_INFINITY, RLIM_INFINITY, } },
    { "Max processes", "processes", { NR_TASKS, NR_TASKS, } },
    { "Max open files", "files", { NR_OPEN, NR_OPEN, } },
    { "Max locked memory", "bytes", { 0, 0, } },
    { "Max address space", "bytes", { RLIM_INFINITY, RLIM_INFINITY, } },
    { "Max file locks", "locks", { RLIM_INFINITY, RLIM_INFINITY, } },
    { "Max pending signals", "integer", { RLIM_INFINITY, RLIM_INFINITY, } },
    { "Max message queue", "bytes", { RLIM_INFINITY, RLIM_INFINITY, } },
    { "Max nice value", "integer", { 40, 40, } },
    { "Max realtime priority", "integer", { MAX_RR_PRIO, MAX_RR_PRIO, } },
    { "Max realtime", "mseconds", { RLIM_INFINITY, RLIM_INFINITY, } },
};


#define ADD_TIMEVAL(d, s)           \
{                                   \
    d .tv_sec += s .tv_sec;         \
    d .tv_usec += s .tv_usec;       \
    while(d .tv_usec > 1000000)     \
    {                               \
        d .tv_usec -= 1000000;      \
        d .tv_sec++;                \
    }                               \
}


/*
 * Handler for syscall getrusage().
 */
long syscall_getrusage(int who, struct rusage *r_usage)
{
    struct rusage res;
    struct timeval ut, st;
    register struct task_t *thread;
	volatile struct task_t *ct = this_core->cur_task;
    
    if(!r_usage)
    {
        return -EFAULT;
    }

    if(who != RUSAGE_CHILDREN && who != RUSAGE_SELF && who != RUSAGE_THREAD)
    {
        return -EINVAL;
    }
    
    A_memset(&res, 0, sizeof(struct rusage));

    if(who == RUSAGE_SELF)
    {
        A_memset(&ut, 0, sizeof(ut));
        A_memset(&st, 0, sizeof(st));
        res.ru_minflt = 0;
        res.ru_majflt = 0;
        
        kernel_mutex_lock(&(ct->threads->mutex));

        for_each_thread(thread, ct)
        {
            ticks_to_timeval(thread->user_time, &ut);
            ticks_to_timeval(thread->sys_time, &st);
            
            ADD_TIMEVAL(res.ru_utime, ut);
            ADD_TIMEVAL(res.ru_stime, st);

            res.ru_minflt += thread->minflt;
            res.ru_majflt += thread->majflt;
        }

        kernel_mutex_unlock(&(ct->threads->mutex));
    }
    else if(who == RUSAGE_THREAD)
    {
        ticks_to_timeval(ct->user_time, &res.ru_utime);
        ticks_to_timeval(ct->sys_time, &res.ru_stime);
        res.ru_minflt = ct->minflt;
        res.ru_majflt = ct->majflt;
    }
    else    // RUSAGE_CHILDREN
    {
        ticks_to_timeval(ct->children_user_time, &res.ru_utime);
        ticks_to_timeval(ct->children_sys_time, &res.ru_stime);
        res.ru_minflt = ct->children_minflt;
        res.ru_majflt = ct->children_majflt;
    }
    
    // Maximum Resident Size (RSS) in 1kB units
    res.ru_maxrss = ct->task_rlimits[RLIMIT_RSS].rlim_cur / 1024;

    /*
     * TODO: fill in the rest of the rusage struct.
     * TODO: fix the code in wait.c as well.
     *       see: http://man7.org/linux/man-pages/man2/getrusage.2.html
     */
    return copy_to_user(r_usage, &res, sizeof(struct rusage));
}


/*
 * Handler for syscall getrlimit().
 */
long syscall_getrlimit(int resource, struct rlimit *rlim)
{
    return syscall_prlimit(0, resource, NULL, rlim);
}


/*
 * Handler for syscall setrlimit().
 */
long syscall_setrlimit(int resource, struct rlimit *rlim)
{
    return syscall_prlimit(0, resource, rlim, NULL);
}


#define CHECK_BOUNDS(r, lim, min, max)                      \
    if(r . lim < min || r . lim > max) return -EPERM;


/*
 * Handler for syscall prlimit().
 */
long syscall_prlimit(pid_t pid, int resource, struct rlimit *new_limit,
                     struct rlimit *old_limit)
{
    struct rlimit *which_rlim, tmp;
	volatile struct task_t *ct = this_core->cur_task;
    volatile struct task_t *task = pid ? get_task_by_id(pid) : ct;

    if(!task)
    {
        return -ESRCH;
    }

    if(task != ct)
    {
        if(ct->uid != task->uid || ct->uid != task->euid ||
           ct->uid != task->ssuid || ct->gid != task->gid ||
           ct->gid != task->egid || ct->gid != task->ssgid)
        {
            return -EPERM;
        }
    }

    if(!new_limit && !old_limit)
    {
        return -EFAULT;
    }

    if(resource < 0 || resource >= RLIMIT_NLIMITS)
    {
        return -EINVAL;
    }
    
    which_rlim = &ct->task_rlimits[resource];

    if(old_limit)
    {
        //COPY_TO_USER(old_limit, which_rlim, sizeof(struct rlimit));
        COPY_VAL_TO_USER(&old_limit->rlim_cur, &which_rlim->rlim_cur);
        COPY_VAL_TO_USER(&old_limit->rlim_max, &which_rlim->rlim_max);
    }
    
    if(new_limit)
    {
        //COPY_FROM_USER(&tmp, new_limit, sizeof(struct rlimit));
        COPY_VAL_FROM_USER(&tmp.rlim_cur, &new_limit->rlim_cur);
        COPY_VAL_FROM_USER(&tmp.rlim_max, &new_limit->rlim_max);

        if(tmp.rlim_max != RLIM_INFINITY && tmp.rlim_cur > tmp.rlim_max)
        {
            return -EINVAL;
        }

        /* 
         * NOTE: an unprivileged process may set only its soft limit to a
         *       value in the range from 0 up to the hard limit, and 
         *       (irreversibly) lower its hard limit.
         */
        if(task == ct && !suser(ct))
        {
            if(tmp.rlim_cur > which_rlim->rlim_max ||
               tmp.rlim_max > which_rlim->rlim_max)
            {
                return -EPERM;
            }
        }

        if(resource == RLIMIT_NOFILE)
        {
            if(tmp.rlim_max > FOPEN_MAX || tmp.rlim_cur > FOPEN_MAX)
            {
                return -EPERM;
            }
        }
        else if(resource == RLIMIT_NICE)
        {
            CHECK_BOUNDS(tmp, rlim_max, 1, 40);
            CHECK_BOUNDS(tmp, rlim_cur, 1, 40);
        }
        else if(resource == RLIMIT_RTPRIO)
        {
            CHECK_BOUNDS(tmp, rlim_max, MIN_RR_PRIO, MAX_RR_PRIO);
            CHECK_BOUNDS(tmp, rlim_cur, MIN_RR_PRIO, MAX_RR_PRIO);
        }

        which_rlim->rlim_cur = tmp.rlim_cur;
        which_rlim->rlim_max = tmp.rlim_max;
    }

    return 0;
}


/*
 * Handler for syscall ulimit().
 *
 * Obsolete syscall.
 *
 * See: https://man7.org/linux/man-pages/man3/ulimit.3.html
 */
long syscall_ulimit(int cmd, long newlimit)
{
	volatile struct task_t *ct = this_core->cur_task;

    switch(cmd)
    {
        /* Return the limit on the size of a file, in units of 512 bytes */
        case UL_GETFSIZE:
            return ct->task_rlimits[RLIMIT_FSIZE].rlim_cur / 512;

        /*
         * Set the limit on the size of a file.
         * 
         * NOTE: an unprivileged process may set only its soft limit to a
         *       value in the range from 0 up to the hard limit, and 
         *       (irreversibly) lower its hard limit.
         */
        case UL_SETFSIZE:
            newlimit *= 512;

            if(!suser(ct))
            {
                if((unsigned)newlimit > 
                            ct->task_rlimits[RLIMIT_FSIZE].rlim_max)
                {
                    return -EPERM;
                }
            }
            
            ct->task_rlimits[RLIMIT_FSIZE].rlim_cur = newlimit;

            return 0;

        case 3:
            return ct->task_rlimits[RLIMIT_DATA].rlim_cur;

        case 4:
            return ct->task_rlimits[RLIMIT_NOFILE].rlim_cur;

        default:
            return -EINVAL;
    }
}


/*
 * Set default resource limits.
 */
void set_task_rlimits(struct task_t *task)
{
    int i;
    
    if(!task)
    {
        return;
    }
    
    for(i = 0; i < RLIMIT_NLIMITS; i++)
    {
        task->task_rlimits[i].rlim_cur = default_rlimits[i].rlimit.rlim_cur;
        task->task_rlimits[i].rlim_max = default_rlimits[i].rlimit.rlim_max;
    }
}

