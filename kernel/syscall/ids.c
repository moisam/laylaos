/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: ids.c
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
 *  \file ids.c
 *
 *  Functions for getting and setting group and user ids.
 */

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             TTY_BUF_SIZE

#include <errno.h>
#include <kernel/syscall.h>
#include <kernel/task.h>
#include <kernel/tty.h>
#include <kernel/user.h>

#include "../kernel/task_funcs.c"
#include "../kernel/tty_inlines.h"


/*
 * Handler for syscall setgid().
 */
long syscall_setgid(gid_t newgid)
{
    if(!suser(this_core->cur_task))
    {
        // normal user
        if(gid_perm(newgid, 0) || this_core->cur_task->ssgid == newgid)
        {
            setid(this_core->cur_task, egid, newgid);
        }
        else
        {
            return -EPERM;
        }
    }
    else
    {
        // root user
        setrootid(this_core->cur_task, gid, newgid);
    }
    
    return 0;
}


/*
 * Handler for syscall getgid().
 */
long syscall_getgid(void)
{
    return (long)this_core->cur_task->gid;
}


/*
 * Handler for syscall getegid().
 */
long syscall_getegid(void)
{
    return (long)this_core->cur_task->egid;
}


/*
 * Handler for syscall setuid().
 */
long syscall_setuid(uid_t newuid)
{
    if(!suser(this_core->cur_task))
    {
        // regular user
        if(newuid == this_core->cur_task->uid || 
           this_core->cur_task->ssuid == this_core->cur_task->uid)
        {
            setid(this_core->cur_task, euid, newuid);
        }
        else
        {
            return -EPERM;
        }
    }
    else
    {
        // root can do whatever
        setrootid(this_core->cur_task, uid, newuid);
    }

    return 0;
}


/*
 * Handler for syscall getuid().
 */
long syscall_getuid(void)
{
    return (long)this_core->cur_task->uid;
}


/*
 * Handler for syscall geteuid().
 */
long syscall_geteuid(void)
{
    return (long)this_core->cur_task->euid;
}


/*
 * Handler for syscall setpgid().
 */
long syscall_setpgid(pid_t pid, pid_t pgid)
{
    int found = 0;

    if(pgid < 0)
    {
        return -EINVAL;
    }

    if(pid == 0)
    {
        pid = this_core->cur_task->pid;
    }
    
    if(pgid == 0)
    {
        pgid = pid;
    }
    
    elevated_priority_lock(&task_table_lock);

    for_each_taskptr(t)
    {
        if(*t && (*t)->threads && 
           (*t)->threads->thread_group_leader->pid == pid)
        {
            /*
             * TODO: The setpgid (2) manpage says:
             *     If setpgid() is used to move a process from one process
             *     group to another (as is done by some shells when creating
             *     pipelines), both process groups must be part of the same
             *     session (see setsid(2) and credentials(7)). In this case,
             *     the pgid specifies an existing process group to be joined
             *     and the session ID of that group must match the session ID
             *     of the joining process.
             */

            //printk("syscall_setpgid: leader %d\n", group_leader(*t));

            if(group_leader(*t) && (*t)->pgid != pgid)
            {
                elevated_priority_unlock(&task_table_lock);
                return -EPERM;
            }
            
            //printk("syscall_setpgid: sid1 %d, sid2 %d\n", (*t)->sid, ct->sid);
            
            if((*t)->sid != this_core->cur_task->sid)
            {
                elevated_priority_unlock(&task_table_lock);
                return -EPERM;
            }
            
            (*t)->pgid = pgid;
            found = 1;

            //return 0;
        }
    }

    elevated_priority_unlock(&task_table_lock);
    
    return found ? 0 : -ESRCH;
}


/*
 * Handler for syscall getpgid().
 */
long syscall_getpgid(pid_t pid)
{
    volatile struct task_t *task = pid ? get_task_by_id(pid) : 
                                         this_core->cur_task;

    if(!task)
    {
        return -ESRCH;
    }
    
    return (long)task->pgid;
}


/*
int syscall_setpgrp(pid_t pid, pid_t pgid)
{
    return setpgid(pid, pgid);
}
*/


/*
 * Handler for syscall getpgrp().
 */
long syscall_getpgrp(void)
{
    return (long)this_core->cur_task->pgid;
}


/*
 * Handler for syscall getpid().
 */
long syscall_getpid(void)
{
    return this_core->cur_task->threads ? 
                this_core->cur_task->threads->tgid :
                this_core->cur_task->pid;
}


/*
 * Handler for syscall getppid().
 */
long syscall_getppid(void)
{
    return this_core->cur_task->parent ? this_core->cur_task->parent->pid : 1;
}


/*
 * Handler for syscall getsid().
 */
long syscall_getsid(pid_t pid)
{
    volatile struct task_t *task = pid ? get_task_by_id(pid) : 
                                         this_core->cur_task;

    if(!task)
    {
        return -ESRCH;
    }
    
    return task->sid;
}


/*
 * Handler for syscall setsid().
 */
long syscall_setsid(void)
{
	struct task_t *ct = (struct task_t *)this_core->cur_task;

    elevated_priority_lock(&task_table_lock);

    /* 
     * Make sure there aren't any other processes whose pgid equals our pid,
     * including ourselves (that is, if this task is a group leader, i.e. its
     * PGID == PID, it can't bail on other tasks in its session).
     */
    for_each_taskptr(t)
    {
        if(*t && (*t)->pgid == ct->pid)
        {
            elevated_priority_unlock(&task_table_lock);
            return -EPERM;
        }
    }

    elevated_priority_unlock(&task_table_lock);

    /* release tty */
    set_controlling_tty(ct->ctty, get_struct_tty(ct->ctty), 0);

    setid(ct, sid, ct->pid);
    setid(ct, pgid, ct->pid);

    return ct->pgid;
}


/*
 * set real and/or effective user or group ID (the following 2 functions).
 *
 * See: https://man7.org/linux/man-pages/man2/setreuid.2.html
 */
long syscall_setreuid(uid_t newruid, uid_t neweuid)
{
	struct task_t *t = (struct task_t *)this_core->cur_task;
    uid_t olduid = t->uid;
    
    if(newruid != (uid_t)-1)
    {
        // regular user
        if(!suser(t))
        {
            // can only set real uid to real uid or euid
            if(newruid != t->uid && newruid != t->euid)
            {
                return -EPERM;
            }
        }

        setid(t, uid, newruid);
    }
    
    if(neweuid != (uid_t)-1)
    {
        // regular user
        if(!suser(t))
        {
            // can only set real uid to real uid or euid or ssuid
            if(neweuid != t->uid && neweuid != t->euid && neweuid != t->ssuid)
            {
                return -EPERM;
            }
        }

        setid(t, euid, neweuid);
    }
    
    if(newruid != (uid_t)-1 || (neweuid != (uid_t)-1 && neweuid != olduid))
    {
        setid(t, ssuid, t->euid);
    }
    
    return 0;
}


long syscall_setregid(gid_t newrgid, gid_t newegid)
{
	struct task_t *t = (struct task_t *)this_core->cur_task;
    gid_t oldgid = t->gid;
    
    if(newrgid != (gid_t)-1)
    {
        // regular user
        if(t->gid || t->egid)
        {
            // can only set real gid to real gid or egid
            if(newrgid != t->gid && newrgid != t->egid)
            {
                return -EPERM;
            }
        }

        setid(t, gid, newrgid);
    }
    
    if(newegid != (gid_t)-1)
    {
        // regular user
        if(t->gid || t->egid)
        {
            // can only set real uid to real uid or euid
            if(newegid != t->gid && newegid != t->egid && newegid != t->ssgid)
            {
                return -EPERM;
            }
        }

        setid(t, egid, newegid);
    }
    
    if(newrgid != (gid_t)-1 || (newegid != (gid_t)-1 && newegid != oldgid))
    {
        setid(t, ssgid, t->egid);
    }
    
    return 0;
}


#define CHECK_ID(t, newid, id)                                          \
    if(!suser(t))                                                       \
        if(newid != t-> id && newid != t->e##id && newid != t->ss##id)  \
            return -EPERM;


/*
 * Set the real, effective and/or the saved set user or group IDs (the
 * following 2 functions).
 *
 * See: https://man7.org/linux/man-pages/man2/setresuid.2.html
 */
long syscall_setresuid(uid_t newruid, uid_t neweuid, uid_t newsuid)
{
	struct task_t *t = (struct task_t *)this_core->cur_task;
    
    if(newruid != (uid_t)-1)
    {
        CHECK_ID(t, newruid, uid);
        setid(t, uid, newruid);
    }
    
    if(neweuid != (uid_t)-1)
    {
        CHECK_ID(t, neweuid, uid);
        setid(t, euid, neweuid);
    }

    if(newsuid != (uid_t)-1)
    {
        CHECK_ID(t, newsuid, uid);
        setid(t, ssuid, newsuid);
    }
    
    return 0;
}


long syscall_setresgid(gid_t newrgid, gid_t newegid, gid_t newsgid)
{
	struct task_t *t = (struct task_t *)this_core->cur_task;
    
    if(newrgid != (gid_t)-1)
    {
        CHECK_ID(t, newrgid, gid);
        setid(t, gid, newrgid);
    }
    
    if(newegid != (gid_t)-1)
    {
        CHECK_ID(t, newegid, gid);
        setid(t, egid, newegid);
    }

    if(newsgid != (gid_t)-1)
    {
        CHECK_ID(t, newsgid, gid);
        setid(t, ssgid, newsgid);
    }
    
    return 0;
}


/*
 * Get the real, effective and/or the saved set user or group IDs (the
 * following 2 functions).
 *
 * See: https://man7.org/linux/man-pages/man2/getresuid.2.html
 */
long syscall_getresuid(uid_t *ruid, uid_t *euid, uid_t *suid)
{
	struct task_t *t = (struct task_t *)this_core->cur_task;
    
    if(ruid)
    {
        COPY_TO_USER(ruid, &t->uid, sizeof(uid_t));
    }

    if(euid)
    {
        COPY_TO_USER(euid, &t->euid, sizeof(uid_t));
    }

    if(suid)
    {
        COPY_TO_USER(suid, &t->ssuid, sizeof(uid_t));
    }

    return 0;
}


long syscall_getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid)
{
	struct task_t *t = (struct task_t *)this_core->cur_task;

    if(rgid)
    {
        COPY_TO_USER(rgid, &t->gid, sizeof(gid_t));
    }

    if(egid)
    {
        COPY_TO_USER(egid, &t->egid, sizeof(gid_t));
    }

    if(sgid)
    {
        COPY_TO_USER(sgid, &t->ssgid, sizeof(gid_t));
    }

    return 0;
}

