/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: wait.c
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
 *  \file wait.c
 *
 *  Functions to wait for a process to change state.
 */

//#define __DEBUG
#define __USE_XOPEN_EXTENDED

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <kernel/laylaos.h>
#include <kernel/ksignal.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/user.h>
#include <kernel/ptrace.h>


/*
 * Any thread can wait for children of other processes in the same thread
 * group. Here we check if 'parent', or another thread in its thread
 * group, is the parent of 'child'.
 */
static int is_parent_of(struct task_t *parent, struct task_t *child)
{
    struct task_t *thread;

    if(child->parent == parent)
    {
        return 1;
    }

    kernel_mutex_lock(&(parent->threads->mutex));
    
    for_each_thread(thread, parent)
    {
        if(child->parent == thread)
        {
            kernel_mutex_unlock(&(parent->threads->mutex));
            return 1;
        }
    }
    
    kernel_mutex_unlock(&(parent->threads->mutex));
    return 0;
}


static int waitpid_internal(pid_t pid, int options, int *stat_addr,
                            siginfo_t *siginfo_addr, struct rusage *rusage)
{
    int flag = 0;
    struct task_t *parent, *ct = cur_task;

repeat:

    KDEBUG("waitpid_internal: mypid %d\n", ct->pid);

    elevated_priority_lock(&task_table_lock);
    
    for_each_taskptr(t)
    {
        if(*t && *t != ct && is_parent_of(ct, *t) &&
           (pid == -1 || (*t)->pid == pid ||
            (pid == 0 && (*t)->pgid == ct->pgid) ||
            (pid < 0 && (*t)->pgid == -pid)))
        {
            KDEBUG("waitpid_internal: found task 0x%x\n", (*t)->pid);

            flag = 1;
            
            /*
             * Return child status if:
             *    - child is a zombie
             *    - child resumed execution and WCONTINUED is specified
             *    - child stopped execution and one of the following two
             *      conditions is true:
             *         (1) child is being traced
             *         (2) child is not being traced and WUNTRACED is specified
             */
            if(((*t)->state == TASK_ZOMBIE && (options & WEXITED)) ||
              ((WIFCONTINUED((*t)->exit_status)) && (options & WCONTINUED)) ||
              ((WIFSTOPPED  ((*t)->exit_status)) && (options & WSTOPPED) &&
               (((*t)->properties & PROPERTY_TRACE_SIGNALS) || 
                                                    (options & WUNTRACED))))
            {
                elevated_priority_unlock(&task_table_lock);

                KDEBUG("waitpid_internal: pid %d\n", (*t)->pid);
                
                if(stat_addr)
                {
                    COPY_TO_USER(stat_addr, &(*t)->exit_status, 
                                                        sizeof(uint32_t));
                }
                
                if(siginfo_addr)
                {
                    siginfo_t tmp;
                    int code = WCOREDUMP((*t)->exit_status) ? CLD_DUMPED :
                             WIFCONTINUED((*t)->exit_status) ? CLD_CONTINUED :
                             WIFSTOPPED((*t)->exit_status) ? CLD_STOPPED :
                             WIFSIGNALED((*t)->exit_status) ?
                                                    CLD_KILLED : CLD_EXITED;
                    
                    tmp.si_pid = (*t)->pid;
                    tmp.si_uid = (*t)->uid;
                    tmp.si_signo = SIGCHLD;
                    tmp.si_status = (*t)->exit_status;
                    tmp.si_code = code;

                    COPY_TO_USER(siginfo_addr, &tmp, sizeof(siginfo_t));
                }
                
                if(rusage)
                {
                    struct rusage res;

                    ticks_to_timeval((*t)->user_time, &res.ru_utime);
                    ticks_to_timeval((*t)->sys_time, &res.ru_stime);
                    res.ru_minflt = (*t)->children_minflt;
                    res.ru_majflt = (*t)->children_majflt;
    
                    // Maximum Resident Size (RSS) in 1kB units
                    res.ru_maxrss = 
                            (*t)->task_rlimits[RLIMIT_RSS].rlim_cur / 1024;

                    /*
                     * TODO: fill in the rest of the rusage struct.
                     * TODO: fix the code in rlimit.c as well.
                     *       see: http://man7.org/linux/man-pages/man2/getrusage.2.html
                     */
                    COPY_TO_USER(rusage, &res, sizeof(struct rusage));
                }

                /* collect the times */
                parent = (*t)->parent ? (*t)->parent : get_init_task();

                parent->children_user_time += 
                            ((*t)->user_time + (*t)->children_user_time);
                parent->children_sys_time  += 
                            ((*t)->sys_time  + (*t)->children_sys_time );

                parent->children_minflt += 
                            ((*t)->minflt + (*t)->children_minflt);
                parent->children_majflt += 
                            ((*t)->majflt + (*t)->children_majflt);

                flag = (*t)->pid;
                (*t)->exit_status = 0;

                if((*t)->state == TASK_ZOMBIE)
                {
                    KDEBUG("waitpid_internal: pid %d\n", (*t)->pid);
                    reap_zombie(*t);
                }

                KDEBUG("syscall_waitpid: res %d\n", flag);

                return flag;
            }
        }
    }

    elevated_priority_unlock(&task_table_lock);

    KDEBUG("syscall_waitpid: children %d (pid %d)\n", ct->children, ct->pid);

    if(!ct->children)
    {
        return -ECHILD;
    }

    if(options & WNOHANG)
    {
        KDEBUG("waitpid_internal: returning as WNOHANG (pid %d)\n", ct->pid);
        return 0;
    }

    KDEBUG("waitpid_internal: pid %d going to sleep\n", ct->pid);

    ct->properties |= PROPERTY_IN_WAIT;
    //block_task(ct, 1);
    block_task2(ct, 500);
    ct->properties &= ~PROPERTY_IN_WAIT;

    if(ct->woke_by_signal)
    {
        KDEBUG("waitpid_internal: awoken by signal (pid %d)\n", ct->pid);
        return -ERESTARTSYS;
    }

    goto repeat;
}


/*
 * Handler for syscall waitpid().
 */
int syscall_waitpid(pid_t pid, int *stat_addr, int options)
{
    return waitpid_internal(pid, options | WEXITED | WSTOPPED, 
                                 stat_addr, NULL, NULL);
}


/*
 * Handler for syscall wait4().
 */
int syscall_wait4(pid_t pid, int *stat_addr,
                  int options, struct rusage *rusage)
{
    return waitpid_internal(pid, options | WEXITED | WSTOPPED, 
                                 stat_addr, NULL, rusage);
}


/*
 * Handler for syscall waitid().
 */
int syscall_waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options)
{
    pid_t pid;
    int res;
    
    if(!infop)
    {
        return -EINVAL;
    }
    
    if(idtype == P_PID)
    {
        if((pid = id) <= 0)
        {
            return -EINVAL;
        }
    }
    else if(idtype == P_PGID)
    {
        if((pid = id) < 0)
        {
            return -EINVAL;
        }
        
        pid = -pid;
    }
    else if(idtype == P_ALL)
    {
        pid = -1;
    }
    else
    {
        // NOTE: we currently don't support (idtype == P_PIDFD)
        return -EINVAL;
    }

    /*
     * waitpid_internal() returns the child's pid on success, or -errno on
     * failure. We return 0 on success and -errno on failure.
     */
    if((res = waitpid_internal(pid, options, NULL, infop, NULL)) < 0)
    {
        return res;
    }
    
    return 0;
}

