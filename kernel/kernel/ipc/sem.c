/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: sem.c
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
 *  \file sem.c
 *
 *  SysV semaphore implementation.
 */

#include <errno.h>
#include <string.h>
#include <kernel/ipc.h>
#include <kernel/user.h>
#include <kernel/task.h>
#include <kernel/clock.h>
#include <mm/kheap.h>

#define SEMQ(index)             ipc_sem[index].semid
#define SEMQPERM(index)         ipc_sem[index].semid.sem_perm
#define SEMQARRAY(index)        ipc_sem[index].sem_array
#define SEMQARRAYN(index, j)    ipc_sem[index].sem_array[j]
#define SEMQLOCK(index)         kernel_mutex_lock(&ipc_sem[index].lock)
#define SEMQUNLOCK(index)       kernel_mutex_unlock(&ipc_sem[index].lock)

#define ASSERT_SEMUN(index, semnum)                         \
    if(semnum < 0 || semnum >= (int)SEMQ(index).sem_nsems)  \
    {                                                       \
        SEMQUNLOCK(index);                                  \
        return -EFBIG;                                      \
    }

#define WAKE_SLEEPERS(sem)                                  \
    if((sem)->semval && (sem)->semncnt != 0)                \
        unblock_tasks(&((sem)->semncnt));                   \
    if((sem)->semval == 0 && (sem)->semzcnt != 0)           \
        unblock_tasks(&((sem)->semzcnt));


struct ipcq_t *ipc_sem;
struct kernel_mutex_t ipc_sem_lock;

/* undo struct */
struct semadj_t
{
    int semid;
    unsigned short sem_num;
    short adj_val;
    struct semadj_t *prev, *next;
};

/* system-wide array for undoing semaphore operations, indexed by task */
struct task_semadj_t
{
    struct task_t *task;
    struct semadj_t *head, *tail;
    struct task_semadj_t *prev, *next;
} semadj_head;

struct kernel_mutex_t semadj_lock;


static void remove_semadj(int semid, unsigned short sem_num)
{
    struct task_semadj_t *sa;
    struct semadj_t *undo, *next;

    kernel_mutex_lock(&semadj_lock);

    for(sa = semadj_head.next; sa != NULL; sa = sa->next)
    {
        if(sa->task == NULL)
        {
            continue;
        }

        undo = sa->head;

        while(undo)
        {
            next = undo->next;

            if(undo->semid != semid || undo->sem_num != sem_num)
            {
                undo = next;
                continue;
            }

            if(undo->prev)
            {
                undo->prev->next = undo->next;
            }

            if(undo->next)
            {
                undo->next->prev = undo->prev;
            }

            if(sa->head == undo)
            {
                sa->head = undo->next;
            }

            if(sa->tail == undo)
            {
                sa->tail = undo->prev;
            }

            kfree(undo);
            undo = next;
        }
    }

    kernel_mutex_unlock(&semadj_lock);
}


static int add_sem_undo(int semid, unsigned short sem_num,
                        short val, struct task_t *task)
{
    struct task_semadj_t *sa;
    struct semadj_t *undo;

    kernel_mutex_lock(&semadj_lock);

    for(sa = semadj_head.next; sa != NULL; sa = sa->next)
    {
        if(sa->task == task)
        {
            break;
        }
    }

    /* first time? give task some space */
    if(!sa)
    {
        if(!(sa = kmalloc(sizeof(struct task_semadj_t))))
        {
            kernel_mutex_unlock(&semadj_lock);
            return -ENOMEM;
        }
        
        memset(sa, 0, sizeof(struct task_semadj_t));
        
        if(semadj_head.next)
        {
            semadj_head.next->prev = sa;
        }

        sa->next = semadj_head.next;
        semadj_head.next = sa;
        sa->task = task;
    }

    /* find our undo struct for this task */
    undo = sa->head;

    while(undo)
    {
        if(undo->semid == semid && undo->sem_num == sem_num)
        {
            break;
        }

        undo = undo->next;
    }

    /* not found? */
    if(!undo)
    {
        if(!(undo = kmalloc(sizeof(struct semadj_t))))
        {
            kernel_mutex_unlock(&semadj_lock);
            return -ENOMEM;
        }

        undo->semid   = semid;
        undo->sem_num = sem_num;
        undo->adj_val = 0;
        undo->prev = NULL;
        undo->next = NULL;

        /* add to queue */
        if(sa->head == NULL)
        {
            sa->head = undo;
            sa->tail = undo;
        }
        else
        {
            sa->tail->next = undo;
            undo->prev = sa->tail;
            sa->tail = undo;
        }
    }

    undo->adj_val -= val;
    kernel_mutex_unlock(&semadj_lock);

    /*
    if(undo->adj_val == 0)
    {
        if(undo->prev) undo->prev->next = undo->next;
        if(undo->next) undo->next->prev = undo->prev;
        if(semadj[i].head == undo) semadj[i].head = undo->next;
        if(semadj[i].tail == undo) semadj[i].tail = undo->prev;
        kfree((void *)undo);
    }
    */
    
    return 0;
}


/*
 * Called during task exit, to undo semaphore operations not finished by the
 * dying task.
 */
void do_sem_undo(struct task_t *task)
{
    struct task_semadj_t *sa;
    struct semadj_t *undo, *next;
    struct semaphore_t *sem;

    kernel_mutex_lock(&semadj_lock);

    for(sa = semadj_head.next; sa != NULL; sa = sa->next)
    {
        if(sa->task == task)
        {
            break;
        }
    }

    if(!sa)
    {
        kernel_mutex_unlock(&semadj_lock);
        return;
    }

    kernel_mutex_unlock(&semadj_lock);

    /* iterate through undo structs */
    undo = sa->head;

    while(undo)
    {
        int index = undo->semid % IPC_SEM_MAX_QUEUES;

        /* accessing a removed entry? */
        if(ipc_sem[index].queue_id != undo->semid)
        {
            undo = undo->next;
            continue;
        }

        SEMQLOCK(index);
        sem = &SEMQARRAYN(index, undo->sem_num);
        sem->semval += undo->adj_val;
        
        /*
        if(sem->semval < 0)
        {
            sem->semval = 0;
        }
        */

        SEMQUNLOCK(index);
        next = undo->next;
        kfree(undo);
        undo = next;
    }

    sa->task = NULL;
    sa->head = NULL;
    sa->tail = NULL;
}


/*
 * Initialise SysV semaphore queues.
 */
void sem_init(void)
{
    int i;
    size_t sz = IPC_SEM_MAX_QUEUES * sizeof(struct ipcq_t);

    if(!(ipc_sem = kmalloc(sz)))
    {
        kpanic("Insufficient memory to init mem queues");
    }
    
    memset(ipc_sem, 0, sz);

    for(i = 0; i < IPC_SEM_MAX_QUEUES; i++)
    {
        ipc_sem[i].queue_id = i;
        init_kernel_mutex(&ipc_sem[i].lock);
    }

    init_kernel_mutex(&ipc_sem_lock);
    memset(&semadj_head, 0, sizeof(semadj_head));
    init_kernel_mutex(&semadj_lock);
}


#define FIELD_SEMNCNT       1
#define FIELD_SEMZCNT       2
#define FIELD_SEMPID        3
#define FIELD_SEMVAL        4

static int get_field(struct task_t *ct, int index, int semnum, int which)
{
    int res;

    if(!ipc_has_perm(&SEMQPERM(index), ct, READ_PERMISSION))
    {
        SEMQUNLOCK(index);
        return -EACCES;
    }

    ASSERT_SEMUN(index, semnum);

    switch(which)
    {
        case FIELD_SEMNCNT:
            res = SEMQARRAYN(index, semnum).semncnt;
            break;

        case FIELD_SEMZCNT:
            res = SEMQARRAYN(index, semnum).semzcnt;
            break;

        case FIELD_SEMPID:
            res = SEMQARRAYN(index, semnum).sempid;
            break;

        case FIELD_SEMVAL:
            res = SEMQARRAYN(index, semnum).semval;
            break;

        default:
            res = 0;
    }

    SEMQUNLOCK(index);

    return res;
}


/*
 * Handler for syscall semctl().
 */
int syscall_semctl(int semid, int semnum, int cmd, union semun *arg)
{
    int index;
    unsigned long i;
    struct semid_ds tmp;
    struct semaphore_t *sem, *lsem;
    struct task_t *ct = cur_task;

    if(semid < 0 || !ipc_sem)
    {
        return -EINVAL;
    }

    index = semid % IPC_SEM_MAX_QUEUES;

    /* accessing a removed entry? */
    if(ipc_sem[index].queue_id != semid)
    {
        return -EIDRM;
    }

    SEMQLOCK(index);

    /* Query status: verify read permission and then copy data to user */
    if(cmd == IPC_STAT)
    {
        if(!ipc_has_perm(&SEMQPERM(index), ct, READ_PERMISSION))
        {
            SEMQUNLOCK(index);
            return -EACCES;
        }

        memcpy(&tmp, &SEMQ(index), sizeof(struct semid_ds));
        SEMQUNLOCK(index);
        
        return copy_to_user(arg->buf, &tmp, sizeof(struct semid_ds));
    }

    /*
     * Set params: verify process uid == (uid or creator uid), or process is
     *             superuser, then copy uid, gid, permissions and other fields,
     *             but DON'T CHANGE creator uid & gid.
     */
    else if(cmd == IPC_SET)
    {
        /* check permissions */
        if(ct->euid != 0)
        {
            if(ct->euid != SEMQPERM(index).uid &&
               ct->euid != SEMQPERM(index).cuid)
            {
                SEMQUNLOCK(index);
                return -EPERM;
            }
        }
        
        COPY_FROM_USER(&tmp, arg->buf, sizeof(struct semid_ds));

        SEMQPERM(index).uid   = arg->buf->sem_perm.uid;
        SEMQPERM(index).gid   = arg->buf->sem_perm.gid;
        SEMQPERM(index).mode  = arg->buf->sem_perm.mode & 0777;
        SEMQ(index).sem_ctime = now();
        SEMQUNLOCK(index);

        return 0;
    }

    /*
     * Remove entry: verify task uid == (uid or creator uid), or task
     * is superuser.
     */
    else if(cmd == IPC_RMID)
    {
        /* check permissions */
        if(ct->euid != 0)
        {
            if(ct->euid != SEMQPERM(index).uid &&
               ct->euid != SEMQPERM(index).cuid)
            {
                SEMQUNLOCK(index);
                return -EPERM;
            }
        }

        sem = &SEMQARRAYN(index, 0);
        lsem = &SEMQARRAYN(index, SEMQ(index).sem_nsems);

        /*
         * do this now so if the loop below wakes anyone, they return with 
         * an -EIDRM error.
         */
        SEMQARRAY(index) = NULL;
        ipc_sem[index].key = 0;
        /* invalidate old descriptor */
        ipc_sem[index].queue_id += IPC_SEM_MAX_QUEUES;
        
        for( ; sem < lsem; sem++)
        {
            /* wake up tasks sleeping on this semaphore id */
            if(sem->semzcnt != 0)
            {
                unblock_tasks(&sem->semzcnt);
            }

            if(sem->semncnt != 0)
            {
                unblock_tasks(&sem->semncnt);
            }
        }

        kfree(SEMQARRAY(index));
        SEMQUNLOCK(index);

        return 0;
    }

    /* Return values for all semaphores in the set */
    else if(cmd == GETALL)
    {
        unsigned long j = SEMQ(index).sem_nsems;
        unsigned short atmp[j];

        if(!ipc_has_perm(&SEMQPERM(index), ct, READ_PERMISSION))
        {
            SEMQUNLOCK(index);
            return -EACCES;
        }
        
        for(i = 0; i < j; i++)
        {
            atmp[i] = SEMQARRAYN(index, i).semval;
        }

        SEMQUNLOCK(index);
        
        return copy_to_user(arg->array, atmp, j * sizeof(unsigned short));
    }

    /* Set values for all semaphores in the set */
    else if(cmd == SETALL)
    {
        unsigned long j = SEMQ(index).sem_nsems;
        unsigned short atmp[j];

        if(!ipc_has_perm(&SEMQPERM(index), ct, WRITE_PERMISSION))
        {
            SEMQUNLOCK(index);
            return -EACCES;
        }
        
        COPY_FROM_USER(atmp, arg->array, j * sizeof(unsigned short));

        for(i = 0; i < j; i++)
        {
            sem = &SEMQARRAYN(index, i);
            sem->semval = atmp[i];
            remove_semadj(semid, i);
            //unblock_tasks(&SEMQARRAYN(index, i).semval);
            WAKE_SLEEPERS(sem);

            /*
            if(atmp[i] && sem->semncnt != 0)
            {
                unblock_tasks(&sem->semncnt);
            }

            if(!atmp[i] && sem->semzcnt != 0)
            {
                unblock_tasks(&sem->semzcnt);
            }
            */
        }

        SEMQ(index).sem_ctime = now();
        SEMQUNLOCK(index);

        return 0;
    }

    /* Return the # of tasks waiting for semaphore val to increase */
    else if(cmd == GETNCNT)
    {
        return get_field(ct, index, semnum, FIELD_SEMNCNT);
    }

    /* Return the # of tasks waiting for semaphore val to become zero */
    else if(cmd == GETZCNT)
    {
        return get_field(ct, index, semnum, FIELD_SEMZCNT);
    }

    /* Return the PID that last performed an operation on the semaphore */
    else if(cmd == GETPID)
    {
        return get_field(ct, index, semnum, FIELD_SEMPID);
    }

    /* Return the semaphore value */
    else if(cmd == GETVAL)
    {
        return get_field(ct, index, semnum, FIELD_SEMVAL);
    }

    /* Set the semaphore value */
    else if(cmd == SETVAL)
    {
        if(!ipc_has_perm(&SEMQPERM(index), ct, WRITE_PERMISSION))
        {
            SEMQUNLOCK(index);
            return -EACCES;
        }
        
        if(arg->val < 0 || arg->val > IPC_SEM_MAX_VAL)
        {
            SEMQUNLOCK(index);
            return -ERANGE;
        }

        ASSERT_SEMUN(index, semnum);
        sem = &SEMQARRAYN(index, semnum);
        sem->semval = arg->val;
        remove_semadj(semid, semnum);
        //unblock_tasks(&SEMQARRAYN(index, semnum).semval);
        SEMQ(index).sem_ctime = now();
        WAKE_SLEEPERS(sem);
        SEMQUNLOCK(index);

        return 0;
    }

    SEMQUNLOCK(index);
    
    /* unknown op */
    return -EINVAL;
}


/*
 * Handler for syscall semget().
 */
int syscall_semget(key_t key, int nsems, int semflg)
{
    int i, qid;
    struct semaphore_t *sems;
    struct task_t *ct = cur_task;

    if(!ipc_sem)
    {
        return -ENOENT;
    }

    /* explicit request for a new key? */
    if(key == IPC_PRIVATE)
    {
        i = IPC_SEM_MAX_QUEUES;
        semflg |= IPC_CREAT;
    }
    else
    {
        kernel_mutex_lock(&ipc_sem_lock);

        for(i = 0; i < IPC_SEM_MAX_QUEUES; i++)
        {
            if(ipc_sem[i].key == key)
            {
                break;
            }
        }

        kernel_mutex_unlock(&ipc_sem_lock);

        if(i < IPC_SEM_MAX_QUEUES)
        {
            if((semflg & IPC_CREAT) && (semflg & IPC_EXCL))
            {
                return -EEXIST;
            }

            SEMQLOCK(i);
        }
    }

    /* no existing entry with this key? */
    if(i == IPC_SEM_MAX_QUEUES)
    {
        if(!(semflg & IPC_CREAT))
        {
            return -ENOENT;
        }

        kernel_mutex_lock(&ipc_sem_lock);

        for(i = 0; i < IPC_SEM_MAX_QUEUES; i++)
        {
            if(ipc_sem[i].key == 0)
            {
                break;
            }
        }

        kernel_mutex_unlock(&ipc_sem_lock);

        /* no free slot? */
        if(i == IPC_SEM_MAX_QUEUES)
        {
            return -ENOSPC;
        }

        SEMQLOCK(i);

        /* check size bounds */
        if(nsems <= 0 || nsems > IPC_SEM_NSEMS_MAX)
        {
            SEMQUNLOCK(i);
            return -EINVAL;
        }

        if(!(sems = kmalloc(nsems * sizeof(struct semaphore_t))))
        {
            SEMQUNLOCK(i);
            return -ENOMEM;
        }

        memset(sems, 0, nsems * sizeof(struct semaphore_t));
        ipc_sem[i].sem_array = sems;
        ipc_sem[i].key    = key;
        SEMQPERM(i).cuid  = ct->euid;
        SEMQPERM(i).uid   = ct->euid;
        SEMQPERM(i).cgid  = ct->egid;
        SEMQPERM(i).gid   = ct->egid;
        SEMQPERM(i).mode  = semflg & 0777;   //get the lower 9 bits
        SEMQ(i).sem_otime = 0;
        SEMQ(i).sem_nsems = nsems;
        SEMQ(i).sem_ctime = now();
    }
    else
    {
        /* check permissions for an existing entry */
        if(!ipc_has_perm(&SEMQPERM(i), ct, READ_PERMISSION))
        {
            SEMQUNLOCK(i);
            return -EACCES;
        }
    }

    qid = ipc_sem[i].queue_id;
    SEMQUNLOCK(i);

    return qid;
}


/*
 * Handler for syscall semop().
 */
int syscall_semop(int semid, struct sembuf *__sops, size_t nsops)
{
    int index, interrupted;
    struct sembuf sops[nsops];
    struct sembuf *op, *op2, *lop;
    struct task_t *ct = cur_task;

    if(semid < 0 || !ipc_sem)
    {
        return -EINVAL;
    }

    if(!__sops || nsops == 0)
    {
        return -EINVAL;
    }

    if(nsops > IPC_SEM_NSOPS_MAX)
    {
        return -E2BIG;
    }

    index = semid % IPC_SEM_MAX_QUEUES;

    /* accessing a removed entry? */
    if(ipc_sem[index].queue_id != semid)
    {
        return -EIDRM;
    }
    
    COPY_FROM_USER(&sops, __sops, nsops * sizeof(struct sembuf));
    SEMQLOCK(index);

start:

    for(op = sops, lop = &sops[nsops]; op < lop; op++)
    {
        if(op->sem_num >= SEMQ(index).sem_nsems)
        {
            SEMQUNLOCK(index);
            return -EFBIG;
        }
        

        if(!ipc_has_perm(&SEMQPERM(index), ct,
                                (op->sem_op == 0) ?
                                    READ_PERMISSION : WRITE_PERMISSION))
        {
            SEMQUNLOCK(index);
            return -EACCES;
        }
    }

    //unsigned short last_val = 0;

    /* loop to perform the requested operations */
    for(op = sops, lop = &sops[nsops]; op < lop; op++)
    {
        struct semaphore_t *sem = &SEMQARRAYN(index, op->sem_num);

        //last_val = sem->semval;

        if(op->sem_op > 0)
        {
            sem->semval += op->sem_op;

            if(op->sem_flg & SEM_UNDO)
            {
                /* update task undo structure */
                if(add_sem_undo(semid, op->sem_num, op->sem_op, ct) != 0)
                {
                    SEMQUNLOCK(index);
                    return -ENOMEM;
                }
            }
            
            WAKE_SLEEPERS(sem);
        }
        else if(op->sem_op < 0)
        {
            if((op->sem_op + sem->semval) >= 0)
            {
                sem->semval += op->sem_op;

                if(op->sem_flg & SEM_UNDO)
                {
                    /* update process undo structure */
                    if(add_sem_undo(semid, op->sem_num, op->sem_op, ct) != 0)
                    {
                        SEMQUNLOCK(index);
                        return -ENOMEM;
                    }
                }

                WAKE_SLEEPERS(sem);

                continue;
            }

            /********  REVERSE  *********/
            for(op2 = sops; op2 < op; op2++)
            {
                sem = &SEMQARRAYN(index, op2->sem_num);
                sem->semval -= op2->sem_op;
            }

            if(op->sem_flg & IPC_NOWAIT)
            {
                SEMQUNLOCK(index);
                return -EAGAIN;
            }

            sem->semncnt++;
            interrupted = 0;
            SEMQUNLOCK(index);
            
            /* sleep and wait */
            if(block_task(&sem->semncnt, 1) != 0)
            {
                /*
                 * Set a flag and return -EINTR below. We do it this way to
                 * ensure we decrement semzcnt on the right queue, as the
                 * queue might have been removed by another task while we
                 * slept.
                 */
                //return -EINTR;
                interrupted = 1;
            }

            SEMQLOCK(index);

            /*
             * Retry the send after waking up, but first check the msg queue 
             * has not been removed while we slept.
             */
            if(ipc_sem[index].queue_id != semid)
            {
                SEMQUNLOCK(index);
                return -EIDRM;
            }

            sem->semncnt--;

            if(interrupted)
            {
                /* sleep interrupted by a signal */
                SEMQUNLOCK(index);
                return -EINTR;
            }

            goto start;
        }
        else        /* semaphore operation == zero */
        {
            if(sem->semval != 0)
            {
                /********  REVERSE  *********/
                for(op2 = sops; op2 < op; op2++)
                {
                    sem = &SEMQARRAYN(index, op2->sem_num);
                    sem->semval -= op2->sem_op;
                }

                if(op->sem_flg & IPC_NOWAIT)
                {
                    SEMQUNLOCK(index);
                    return -EAGAIN;
                }

                sem->semzcnt++;
                interrupted = 0;
                SEMQUNLOCK(index);

                /* sleep and wait */
                if(block_task(&sem->semzcnt, 1) != 0)
                {
                    /*
                     * Set a flag and return -EINTR below. We do it this way to
                     * ensure we decrement semzcnt on the right queue, as the
                     * queue might have been removed by another task while we
                     * slept.
                     */
                    //return -EINTR;
                    interrupted = 1;
                }

                SEMQLOCK(index);

                /*
                 * Retry the send after waking up, but first check the msg 
                 * queue has not been removed while we slept.
                 */
                if(ipc_sem[index].queue_id != semid)
                {
                    SEMQUNLOCK(index);
                    return -EIDRM;
                }

                sem->semzcnt--;
                
                if(interrupted)
                {
                    /* sleep interrupted by a signal */
                    SEMQUNLOCK(index);
                    return -EINTR;
                }

                goto start;
            }
        }
    }
    
    /* update process ID's */
    unsigned short k;

    for(k = 0; k < SEMQ(index).sem_nsems; k++)
    {
        SEMQARRAYN(index, k).sempid = ct->pid;
    }

    /* update time stamp */
    SEMQ(index).sem_otime = now();
    SEMQUNLOCK(index);

    //return last_val;
    return 0;
}


/*
 * Remove all sem queues open by this task.
 * Called from execve(), via a call to ipc_killall().
 */
void sem_killall(struct task_t *task)
{
    int i;

    kernel_mutex_lock(&ipc_sem_lock);

    for(i = 0; i < IPC_SEM_MAX_QUEUES; i++)
    {
        if((SEMQPERM(i).cuid != task->euid) &&
           (SEMQPERM(i).uid  != task->euid))
        {
            continue;
        }

        if(ipc_sem[i].key != 0)
        {
            syscall_msgctl(ipc_sem[i].queue_id, IPC_RMID, NULL);
        }
    }

    kernel_mutex_unlock(&ipc_sem_lock);
}

