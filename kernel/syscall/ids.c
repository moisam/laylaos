/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
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
int syscall_setgid(gid_t newgid)
{
    struct task_t *ct = cur_task;

    if(!suser(ct))
    {
        // normal user
        if(gid_perm(newgid, 0) || ct->ssgid == newgid)
        {
            setid(ct, egid, newgid);
        }
        else
        {
            return -EPERM;
        }
    }
    else
    {
        // root user
        setrootid(ct, gid, newgid);
    }
    
    return 0;
}


/*
 * Handler for syscall getgid().
 */
int syscall_getgid(void)
{
    struct task_t *ct = cur_task;
    return (int)ct->gid;
}


/*
 * Handler for syscall getegid().
 */
int syscall_getegid(void)
{
    struct task_t *ct = cur_task;
    return (int)ct->egid;
}


/*
 * Handler for syscall setuid().
 */
int syscall_setuid(uid_t newuid)
{
    struct task_t *ct = cur_task;

    if(!suser(ct))
    {
        // regular user
        if(newuid == ct->uid || ct->ssuid == ct->uid)
        {
            setid(ct, euid, newuid);
        }
        else
        {
            return -EPERM;
        }
    }
    else
    {
        // root can do whatever
        setrootid(ct, uid, newuid);
    }

    return 0;
}


/*
 * Handler for syscall getuid().
 */
int syscall_getuid(void)
{
    struct task_t *ct = cur_task;
    return (int)ct->uid;
}


/*
 * Handler for syscall geteuid().
 */
int syscall_geteuid(void)
{
    struct task_t *ct = cur_task;
    return (int)ct->euid;
}


/*
 * Handler for syscall setpgid().
 */
int syscall_setpgid(pid_t pid, pid_t pgid)
{
    struct task_t *ct = cur_task;
    int found = 0;

    if(pgid < 0)
    {
        return -EINVAL;
    }

    if(pid == 0)
    {
        pid = ct->pid;
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
            
            if((*t)->sid != ct->sid)
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
int syscall_getpgid(pid_t pid)
{
    struct task_t *task = pid ? get_task_by_id(pid) : cur_task;
    
    if(!task)
    {
        return -ESRCH;
    }
    
    return (int)task->pgid;
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
int syscall_getpgrp(void)
{
    struct task_t *ct = cur_task;
    return (int)ct->pgid;
}


/*
 * Handler for syscall getpid().
 */
int syscall_getpid(void)
{
    struct task_t *ct = cur_task;
    return ct->threads ? ct->threads->tgid : ct->pid;
}


/*
 * Handler for syscall getppid().
 */
int syscall_getppid(void)
{
    struct task_t *ct = cur_task;
    return ct->parent ? ct->parent->pid : 1;
}


/*
 * Handler for syscall getsid().
 */
int syscall_getsid(pid_t pid)
{
    struct task_t *task = pid ? get_task_by_id(pid) : cur_task;
    
    if(!task)
    {
        return -ESRCH;
    }
    
    return task->sid;
}


/*
 * Handler for syscall setsid().
 */
int syscall_setsid(void)
{
    struct task_t *ct = cur_task;

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
int syscall_setreuid(uid_t newruid, uid_t neweuid)
{
    struct task_t *t = cur_task;
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


int syscall_setregid(gid_t newrgid, gid_t newegid)
{
    struct task_t *t = cur_task;
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
int syscall_setresuid(uid_t newruid, uid_t neweuid, uid_t newsuid)
{
    struct task_t *t = cur_task;
    
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


int syscall_setresgid(gid_t newrgid, gid_t newegid, gid_t newsgid)
{
    struct task_t *t = cur_task;
    
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
int syscall_getresuid(uid_t *ruid, uid_t *euid, uid_t *suid)
{
    struct task_t *t = cur_task;
    
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


int syscall_getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid)
{
    struct task_t *t = cur_task;
    
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

