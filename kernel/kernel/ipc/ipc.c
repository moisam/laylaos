/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: ipc.c
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
 *  \file ipc.c
 *
 *  Initialise SysV IPC (Inter-Process Communication) queues and define the
 *  general handler for the ipc syscall.
 */

#include <errno.h>
#include <sys/ipc_ops.h>
#include <kernel/ipc.h>
#include <kernel/user.h>


/*
 * Initialise SysV IPC queues.
 */
void ipc_init(void)
{
    msg_init();
    sem_init();
    shm_init();
}


/*
 * Check SysV IPC permissions.
 */
int ipc_has_perm(struct ipc_perm *perm, struct task_t *task, int what)
{
    int umode = 0, gmode = 0, omode = 0;

    if(what == READ_PERMISSION)
    {
        umode = S_IRUSR;
        gmode = S_IRGRP;
        omode = S_IROTH;
    }
    else if(what == WRITE_PERMISSION)
    {
        umode = S_IWUSR;
        gmode = S_IWGRP;
        omode = S_IWOTH;
    }
    else
    {
        return 0;
    }

    if(task->euid == perm->cuid || task->euid == perm->uid)
    {
        if(perm->mode & umode)
        {
            return 1;
        }
    }

    if(task->egid == perm->cgid || task->egid == perm->gid)
    {
        if(perm->mode & gmode)
        {
            return 1;
        }
    }

    if(perm->mode & omode)
    {
        return 1;
    }

    return 0;
}


/*
 * Remove all msg queues and semaphores opened by this task.
 * Called from execve() and terminate_task().
 */
void ipc_killall(struct task_t *task)
{
    msg_killall(task);
    sem_killall(task);
    //shm_killall(task);
}


#define asz(x)      ((x) * sizeof(unsigned long))

static int argsz[25] =
{
    0, asz(3), asz(3), asz(4),
       0, 0, 0, 0, 0, 0, 0,
       asz(4), asz(1), asz(2), asz(3),
       0, 0, 0, 0, 0, 0,
       asz(4), asz(1), asz(3), asz(3),
};


/*
 * Handler for syscall ipc().
 */
int syscall_ipc(int call, unsigned long *args)
{
    KDEBUG("syscall_ipc:\n");
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    unsigned long a[asz(6)];
    unsigned int len;

    if(call < 1 || call > IPCOP_shmctl)
    {
        return -EINVAL;
    }

	if(!args)
	{
        return -EINVAL;
	}
    
    if(!(len = argsz[call]))
    {
        return -EINVAL;
    }
    
    if(copy_from_user(a, args, len) != 0)
    {
        return -EFAULT;
    }

    switch(call)
    {
        case IPCOP_semop:
            return syscall_semop((int)(a[0]),
                                 (struct sembuf *)(a[1]), (size_t)(a[2]));
            
        case IPCOP_semget:
            return syscall_semget((key_t)(a[0]), (int)(a[1]), (int)(a[2]));
            
        case IPCOP_semctl:
            return syscall_semctl((int)(a[0]), (int)(a[1]), (int)(a[2]),
                                  (union semun *)(a[3]));
            
        case IPCOP_semtimedop:
            /*
             * TODO: implement this syscall.
             */
            return -ENOSYS;

            
        case IPCOP_msgsnd:
            return syscall_msgsnd((int)(a[0]), (void *)(a[1]),
                                  (size_t)(a[2]), (int)(a[3]));
            
        case IPCOP_msgrcv:
            return syscall_msgrcv((struct syscall_args *)(a[0]));
            
        case IPCOP_msgget:
            return syscall_msgget((key_t)(a[0]), (int)(a[1]));
            
        case IPCOP_msgctl:
            return syscall_msgctl((int)(a[0]), (int)(a[1]), 
                                  (struct msqid_ds *)(a[2]));
            
        case IPCOP_shmat:
            return syscall_shmat((int)(a[0]), (void *)(a[1]), 
                                 (int)(a[2]), (volatile void **)(a[3]));
            
        case IPCOP_shmdt:
            return syscall_shmdt((void *)(a[0]));
            
        case IPCOP_shmget:
            return syscall_shmget((key_t)(a[0]), (size_t)(a[1]), (int)(a[2]));
            
        case IPCOP_shmctl:
            return syscall_shmctl((int)(a[0]), (int)(a[1]), 
                                  (struct shmid_ds *)(a[2]));
    }
    
    return -EINVAL;
}

