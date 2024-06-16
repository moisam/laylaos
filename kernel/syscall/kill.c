/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: kill.c
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
 *  \file kill.c
 *
 *  Syscall handler for the kill() syscall, used to send signals to tasks.
 */

#include <errno.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/ksignal.h>


/*
 * The kill (2) manpage says:
 *    The only signals that can be sent to process ID 1, the init process,
 *    are those for which init has explicitly installed signal handlers.
 *    This is done to assure the system is not brought down accidentally.
 */
#define SEND_SIGNAL(t, sig, force)                              \
    if((t)->pid == 1)                                           \
    {                                                           \
        if((err = kill_init(sig, force)) == 0)                  \
            sent++;                                             \
    }                                                           \
    else if((t)->user &&                                        \
            (err = user_add_task_signal(t, signum, force)) == 0)\
    {                                                           \
        sent++;                                                 \
    }


int kill_init(int signum, int force)
{
    register struct task_t *init = get_init_task();
    struct sigaction *action;
    
    if(!init || !init->sig)
    {
        return -ESRCH;
    }
    
    action = &init->sig->signal_actions[signum];

    if(action->sa_handler == SIG_IGN || action->sa_handler == SIG_DFL)
    {
        return -EPERM;
    }
    
    return user_add_task_signal(init, signum, force);
}


/*
 * Handler for syscall kill().
 */
int syscall_kill(pid_t pid, int signum)
{
    struct task_t *ct = cur_task;
    int force = suser(ct);
    int sent = 0;
    int err = 0;
    
    KDEBUG("syscall_kill: pid 0x%x (src 0x%x), signum 0x%x\n", pid, ct->pid, signum);
    //__asm__("xchg %%bx, %%bx"::);
    
    elevated_priority_lock(&task_table_lock);

    if(pid == 0)
    {
        for_each_taskptr(t)
        {
            if(*t && (*t)->pgid == ct->pgid)
            {
                SEND_SIGNAL(*t, signum, force);
            }
        }
    }
    else if(pid > 0)
    {
        for_each_taskptr(t)
        {
            if(*t && (*t)->pid == pid)
            {
                SEND_SIGNAL(*t, signum, force);
                break;
            }
        }
    }
    else if(pid == -1)
    {
        for_each_taskptr(t)
        {
            if(*t && *t != ct)
            {
                SEND_SIGNAL(*t, signum, force);
            }
        }
    }
    else
    {
        for_each_taskptr(t)
        {
            if(*t && (*t)->pgid == -pid)
            {
                SEND_SIGNAL(*t, signum, force);
            }
        }
    }

    elevated_priority_unlock(&task_table_lock);

    /*
     * On success (at least one signal was sent), zero is returned.
     * On error, -errno is returned.
     */
    return sent ? 0 : err;
}

