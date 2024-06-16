/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: msg.c
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
 *  \file msg.c
 *
 *  SysV message queue implementation.
 */

#include <errno.h>
#include <string.h>
#include <kernel/ipc.h>
#include <kernel/user.h>
#include <kernel/task.h>
#include <kernel/clock.h>
#include <mm/kheap.h>


#define MSGQ(index)         ipc_msg[index].msqid
#define MSGQPERM(index)     ipc_msg[index].msqid.msg_perm
#define MSGQLOCK(index)     kernel_mutex_lock(&ipc_msg[index].lock)
#define MSGQUNLOCK(index)   kernel_mutex_unlock(&ipc_msg[index].lock)

// macro to extract the struct msgbuf address from our struct msgmap_hdr_t
#define MSGBUF(msgh)        ((uintptr_t)msgh + sizeof(struct msgmap_hdr_t))

// macro to extract the mtype field from struct msgbuf contained within
// our struct msgmap_hdr_t
#define MSGTYPE(msgh)       *(long *)MSGBUF(msgh)

struct ipcq_t *ipc_msg;
struct kernel_mutex_t ipc_msg_lock;


/*
 * Initialise SysV message queues.
 */
void msg_init(void)
{
    int i;
    size_t sz = IPC_MSG_MAX_QUEUES * sizeof(struct ipcq_t);

    if(!(ipc_msg = (struct ipcq_t *)kmalloc(sz)))
    {
        kpanic("Insufficient memory to init msg queues");
    }
    
    memset(ipc_msg, 0, sz);

    for(i = 0; i < IPC_MSG_MAX_QUEUES; i++)
    {
        ipc_msg[i].queue_id = i;
        init_kernel_mutex(&ipc_msg[i].lock);
    }

    init_kernel_mutex(&ipc_msg_lock);
}


/*
 * Handler for syscall msgctl().
 */
int syscall_msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
    int index;
    struct msqid_ds tmp;
    struct task_t *ct = cur_task;

    if(msqid < 0 || !buf || !ipc_msg)
    {
        return -EINVAL;
    }

    index = msqid % IPC_MSG_MAX_QUEUES;

    /* accessing a removed entry? */
    if(ipc_msg[index].queue_id != msqid)
    {
        return -EIDRM;
    }

    MSGQLOCK(index);
    
    /* Query status: verify read permission and then copy data to user */
    if(cmd == IPC_STAT)
    {
        if(!ipc_has_perm(&MSGQPERM(index), ct, READ_PERMISSION))
        {
            MSGQUNLOCK(index);
            return -EACCES;
        }

        memcpy(&tmp, &MSGQ(index), sizeof(struct msqid_ds));
        MSGQUNLOCK(index);
        
        return copy_to_user(buf, &tmp, sizeof(struct msqid_ds));
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
            if(ct->euid != MSGQPERM(index).uid &&
               ct->euid != MSGQPERM(index).cuid)
            {
                MSGQUNLOCK(index);
                return -EPERM;
            }
        }
        
        COPY_FROM_USER(&tmp, buf, sizeof(struct msqid_ds));

        if(tmp.msg_qbytes > MSGQ(index).msg_qbytes)
        {
            /*
             * We don't allow user tasks to increase their message quota.
             * We don't allow superuser tasks to increase the quota above
             * max system-defined value.
             */
            if(ct->euid != 0)
            {
                MSGQUNLOCK(index);
                return -EPERM;
            }
            
            if(tmp.msg_qbytes > IPC_MSG_MAXDATA_BYTES)
            {
                MSGQUNLOCK(index);
                return -EPERM;
            }
        }

        MSGQPERM(index).uid  = tmp.msg_perm.uid;
        MSGQPERM(index).gid  = tmp.msg_perm.gid;
        MSGQPERM(index).mode = tmp.msg_perm.mode & 0777;
        MSGQ(index).msg_qbytes = tmp.msg_qbytes;
        MSGQ(index).msg_ctime  = now();
        MSGQUNLOCK(index);

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
            if(ct->euid != MSGQPERM(index).uid &&
               ct->euid != MSGQPERM(index).cuid)
            {
                MSGQUNLOCK(index);
                return -EPERM;
            }
        }
        
        struct msgmap_hdr_t *hdr = ipc_msg[index].msg_head;

        while(hdr)
        {
            struct msgmap_hdr_t *next = hdr->next;

            kfree(hdr);
            hdr = next;
        }

        MSGQ(index).msg_qnum = 0;
        MSGQ(index).msg_cbytes = 0;
        //ipc_msg[index].total_bytes = 0;
        ipc_msg[index].key = 0;

        /* invalidate old descriptor */
        ipc_msg[index].queue_id += IPC_MSG_MAX_QUEUES;
        MSGQUNLOCK(index);
        
        /* wake sleepers */
        unblock_tasks(&MSGQ(index));

        return 0;
    }
    
    /* unknown op */
    MSGQUNLOCK(index);
    return -EINVAL;
}


/*
 * Handler for syscall msgget().
 */
int syscall_msgget(key_t key, int msgflg)
{
    int i, qid;
    struct task_t *ct = cur_task;

    if(!ipc_msg)
    {
        return -ENOENT;
    }

    /* explicit request for a new key? */
    if(key == IPC_PRIVATE)
    {
        i = IPC_MSG_MAX_QUEUES;
        msgflg |= IPC_CREAT;
    }
    else
    {
        kernel_mutex_lock(&ipc_msg_lock);

        for(i = 0; i < IPC_MSG_MAX_QUEUES; i++)
        {
            if(ipc_msg[i].key == key)
            {
                break;
            }
        }

        kernel_mutex_unlock(&ipc_msg_lock);

        if(i < IPC_MSG_MAX_QUEUES)
        {
            if((msgflg & IPC_CREAT) && (msgflg & IPC_EXCL))
            {
                return -EEXIST;
            }

            MSGQLOCK(i);
        }
    }

    /* no existing entry with this key? */
    if(i == IPC_MSG_MAX_QUEUES)
    {
        if(!(msgflg & IPC_CREAT))
        {
            return -ENOENT;
        }

        kernel_mutex_lock(&ipc_msg_lock);

        for(i = 0; i < IPC_MSG_MAX_QUEUES; i++)
        {
            if(ipc_msg[i].key == 0)
            {
                break;
            }
        }

        kernel_mutex_unlock(&ipc_msg_lock);

        /* no free slot? */
        if(i == IPC_MSG_MAX_QUEUES)
        {
            return -ENOSPC;
        }

        MSGQLOCK(i);

        ipc_msg[i].key = key;
        MSGQPERM(i).cuid = ct->euid;
        MSGQPERM(i).uid  = ct->euid;
        MSGQPERM(i).cgid = ct->egid;
        MSGQPERM(i).gid  = ct->egid;
        MSGQPERM(i).mode = msgflg & 0777;   //get the lower 9 bits
        MSGQ(i).msg_qnum  = 0;
        MSGQ(i).msg_lspid = 0;
        MSGQ(i).msg_lrpid = 0;
        MSGQ(i).msg_stime = 0;
        MSGQ(i).msg_rtime = 0;
        MSGQ(i).msg_ctime = now();
        MSGQ(i).msg_qbytes = IPC_MSG_MAXDATA_BYTES;
        MSGQ(i).msg_cbytes = 0;
        //ipc_msg[i].total_bytes = 0;
        ipc_msg[i].msg_head = NULL;
        ipc_msg[i].msg_tail = NULL;
    }
    else
    {
    /* check permissions for an existing entry */
        if(!ipc_has_perm(&MSGQPERM(i), ct, READ_PERMISSION))
        {
            MSGQUNLOCK(i);
            return -EACCES;
        }
    }

    qid = ipc_msg[i].queue_id;
    MSGQUNLOCK(i);

    return qid;
}


/*
 * Handler for syscall msgget().
 */
int syscall_msgsnd(int msqid, void *msgp, size_t msgsz, int msgflg)
{
    int index;
    size_t actual_msgsz = msgsz + sizeof(long); // sizeof mtype
    struct msgmap_hdr_t *msgh;
    struct task_t *ct = cur_task;

    if(msqid < 0 || !msgp || !ipc_msg)
    {
        return -EINVAL;
    }

    index = msqid % IPC_MSG_MAX_QUEUES;

    /* accessing a removed entry? */
    if(ipc_msg[index].queue_id != msqid)
    {
        return -EIDRM;
    }

    MSGQLOCK(index);
    
    /* Check write permission */
    if(!ipc_has_perm(&MSGQPERM(index), ct, WRITE_PERMISSION))
    {
        MSGQUNLOCK(index);
        return -EACCES;
    }

    if((msgsz > IPC_MSG_MAXMSG_SIZE) && !(msgflg & MSG_NOERROR))
    {
        MSGQUNLOCK(index);
        return -E2BIG;
    }
    
check:

    /*
     * Check two conditions to ensure the msg queue is not full:
     *   - current bytes in queue plus new msg size doesn't exceed queue's
     *     max allowed bytes.
     *   - number of msgs in queue doesn't exceed queue's max allowed bytes,
     *     this is to prevent an unlimited number of 0-length msgs from
     *     flooding kernel memory.
     */
    if(((MSGQ(index).msg_cbytes + actual_msgsz) >= MSGQ(index).msg_qbytes) ||
       (MSGQ(index).msg_qnum >= MSGQ(index).msg_qbytes))
    {
        MSGQUNLOCK(index);

        if(msgflg & IPC_NOWAIT)
        {
            return -EAGAIN;
        }
        
        /* sleep and wait */
        if(block_task(&MSGQ(index), 1) != 0)
        {
            /* sleep interrupted by a signal */
            return -EINTR;
        }

        MSGQLOCK(index);

        /*
         * Retry the send after waking up, but first check the msg queue has 
         * not been removed while we slept.
         */
        if(ipc_msg[index].queue_id != msqid)
        {
            MSGQUNLOCK(index);
            return -EIDRM;
        }
        
        goto check;
    }
    
    /* Copy the message to kernel memory and append to the queue */
    if(!(msgh = (struct msgmap_hdr_t *)kmalloc(actual_msgsz +
                                                sizeof(struct msgmap_hdr_t))))
    {
        MSGQUNLOCK(index);
        return -ENOMEM;
    }
    
    msgh->size = actual_msgsz;
    msgh->next = NULL;
    COPY_FROM_USER((void *)MSGBUF(msgh), msgp, actual_msgsz);
    
    /* mtype field must be a positive number */
    if(MSGTYPE(msgh) < 1)
    {
        MSGQUNLOCK(index);
        kfree(msgh);
        return -EINVAL;
    }

    /* add to message queue */
    if(!ipc_msg[index].msg_head)
    {
        ipc_msg[index].msg_head = msgh;
        ipc_msg[index].msg_tail = msgh;
        msgh->prev = NULL;
    }
    else
    {
        msgh->prev = ipc_msg[index].msg_tail;
        ipc_msg[index].msg_tail->next = msgh;
        ipc_msg[index].msg_tail = msgh;
    }

    MSGQ(index).msg_qnum++;
    MSGQ(index).msg_stime = now();
    MSGQ(index).msg_lspid = ct->pid;
    //ipc_msg[index].total_bytes += msgsz;
    MSGQ(index).msg_cbytes += msgsz;

    MSGQUNLOCK(index);

    /* wake sleepers */
    unblock_tasks(&MSGQ(index));

    return 0;
}


/*
 * NOTE: this syscall returns the count of bytes copied in the 'copied' field,
 *       which is of type size_t, while the C library function msgrcv() returns
 *       the count in its return value, of type ssize_t.
 */
int syscall_msgrcv(struct syscall_args *__args)
{
    struct syscall_args a;
    struct task_t *ct = cur_task;
    struct msgmap_hdr_t *msgh;
    size_t actual_msgsz;
    int res, index;

    // syscall args
    int msqid;
    void *msgp;
    size_t msgsz;
    long msgtyp;
    int msgflg;
    size_t *copied;    // # of bytes copied is placed here

    // get the args
    COPY_SYSCALL6_ARGS(a, __args);
    msqid = (int)(a.args[0]);
    msgp = (void *)(a.args[1]);
    msgsz = (size_t)(a.args[2]);
    msgtyp = (long)(a.args[3]);
    msgflg = (int)(a.args[4]);
    copied = (size_t *)(a.args[5]);

    if(msqid < 0 || !msgp || !copied || !ipc_msg)
    {
        return -EINVAL;
    }

    index = msqid % IPC_MSG_MAX_QUEUES;
    actual_msgsz = msgsz + sizeof(long); // sizeof mtype

    /* accessing a removed entry? */
    if(ipc_msg[index].queue_id != msqid)
    {
        return -EIDRM;
    }

    MSGQLOCK(index);
    
    /* Check read permission */
    if(!ipc_has_perm(&MSGQPERM(index), ct, READ_PERMISSION))
    {
        MSGQUNLOCK(index);
        return -EACCES;
    }

    while(1)
    {
        msgh = ipc_msg[index].msg_head;

        /* get the 1st msg on queue if msgtyp == 0, otherwise... */

        /* get the 1st msg with the requested type */
        if(msgtyp > 0)
        {
            while(msgh)
            {
                if((MSGTYPE(msgh) == msgtyp) || (msgflg & MSG_EXCEPT))
                {
                    break;
                }

                msgh = msgh->next;
            }
        }
        /* get the lowest type of msgs <= requested type */
        else if(msgtyp < 0)
        {
            long abs_msgtyp = -msgtyp;
            struct msgmap_hdr_t *curhdr = NULL;

            while(msgh)
            {
                if(MSGTYPE(msgh) > abs_msgtyp)
                {
                    msgh = msgh->next;
                    continue;
                }

                if(!curhdr)
                {
                    curhdr = msgh;
                }
                else if(MSGTYPE(msgh) < MSGTYPE(curhdr))
                {
                    curhdr = msgh;
                }

                msgh = msgh->next;
            }

            msgh = curhdr;
        }

        if(!msgh)
        {
            MSGQUNLOCK(index);

            if(msgflg & IPC_NOWAIT)
            {
                return -ENOMSG;
            }
        
            /* sleep and wait */
            if(block_task(&MSGQ(index), 1) != 0)
            {
                /* sleep interrupted by a signal */
                return -EINTR;
            }

            MSGQLOCK(index);

            /*
             * Retry the receive after waking up, but first check the msg 
             * queue has not been removed while we slept.
             */
            if(ipc_msg[index].queue_id != msqid)
            {
                MSGQUNLOCK(index);
                return -EIDRM;
            }

            continue;
        }

        size_t count = msgh->size;

        if(count > actual_msgsz)
        {
            if(!(msgflg & MSG_NOERROR))
            {
                MSGQUNLOCK(index);
                return -E2BIG;
            }
            
            /* truncate the msg if MSG_NOERROR is set */
            count = actual_msgsz;
        }
        
        /* update queue fields */
        MSGQ(index).msg_qnum--;
        MSGQ(index).msg_rtime = now();
        MSGQ(index).msg_lrpid = ct->pid;
        //ipc_msg[index].total_bytes -= (msgh->size - sizeof(long));
        MSGQ(index).msg_cbytes -= (msgh->size - sizeof(long));

        /* remove the msg from queue */
        if(msgh->prev)
        {
            msgh->prev->next = msgh->next;
        }

        if(msgh->next)
        {
            msgh->next->prev = msgh->prev;
        }

        if(msgh == ipc_msg[index].msg_tail)
        {
            ipc_msg[index].msg_tail = msgh->prev;
        }

        if(msgh == ipc_msg[index].msg_head)
        {
            ipc_msg[index].msg_head = msgh->next;
        }

        /*
         * unlock queue so that if we SIGSEGV while copying to userspace,
         * we don't die holding the lock.
         */
        MSGQUNLOCK(index);

        /* wake sleepers */
        unblock_tasks(&MSGQ(index));
        
        /* copy msg to user and update queue fields */
        COPY_TO_USER(msgp, (void *)MSGBUF(msgh), count);
        kfree(msgh);
        COPY_TO_USER(copied, &count, sizeof(size_t));

        return 0;
    }
}


/*
 * Remove all msg queues open by this task.
 * Called from execve(), via a call to ipc_killall().
 */
void msg_killall(struct task_t *task)
{
    int i;

    kernel_mutex_lock(&ipc_msg_lock);

    for(i = 0; i < IPC_MSG_MAX_QUEUES; i++)
    {
        if((MSGQPERM(i).cuid != task->euid) &&
           (MSGQPERM(i).uid  != task->euid))
        {
            continue;
        }

        if(ipc_msg[i].key != 0)
        {
            syscall_msgctl(ipc_msg[i].queue_id, IPC_RMID, NULL);
        }
    }

    kernel_mutex_unlock(&ipc_msg_lock);
}

