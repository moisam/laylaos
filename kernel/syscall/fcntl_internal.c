/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: fcntl_internal.c
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
 *  \file fcntl_internal.c
 *
 *  Advisory lock implementation. The syscalls that use the functions defined
 *  in this file can be found in fcntl.c and flock.c.
 */

#include <errno.h>
#include <string.h>
#include <kernel/task.h>
#include <kernel/fcntl.h>
#include <kernel/syscall.h>
#include <mm/kheap.h>


/*
 * Free an advisory lock struct.
 */
static inline void free_lock(struct alock_t *alock)
{
    kfree(alock);
    
    /*
    alock->internal_lock.l_pid  = 0;
    alock->internal_lock.l_type = 0;
    alock->next        = 0;
    alock->prev        = 0;
    kernel_mutex_unlock(&lock->lock_mutex);
    */
}


/*
 * Alloc memory for and return a new advisory lock struct.
 */
static struct alock_t *alloc_lock(void)
{
    struct alock_t *lock;
    
    if(!(lock = kmalloc(sizeof(struct alock_t))))
    {
        return NULL;
    }
    
    A_memset(lock, 0, sizeof(struct alock_t));

    return lock;
}


/* 
 * Helper function to calculate the requested lock's start offset.
 */
off_t get_start(struct file_t *fp, struct flock *lock)
{
    if(lock->l_whence == SEEK_SET)
    {
        return lock->l_start;
    }
    
    if(lock->l_whence == SEEK_CUR)
    {
        return fp->pos + lock->l_start;
    }
    
    /* SEEK_END */
    return fp->node->size + lock->l_start;
}


#define FIX_START(start)        if(start < 0) start = 0

/* 
 * Helper function to calculate the requested lock's start and end offsets.
 *
 * If l_len is positive, the lock range is l_start up to and including 
 * (l_start + l_len - 1). A l_len value of 0 has the special meaning: lock all
 * bytes starting at l_start (interpreted according to l_whence) through to the
 * end of file. POSIX.1-2001 allows (but does not require) support of a 
 * negative l_len value. If l_len is negative, the lock covers bytes
 * (l_start + l_len) up to and including (l_start - 1).
 */
void get_start_end(struct file_t *fp, struct flock *lock,
                   off_t *__start, off_t *__end)
{
    off_t start = get_start(fp, lock);
    off_t end;

    /* can't go before start of file */
    FIX_START(start);

    if(lock->l_len < 0)
    {
        end = start - 1;
        start += lock->l_len;
        FIX_START(start);
    }
    else
    {
        if(lock->l_len == 0)
        {
            end = fp->node->size - 1;
        }
        else
        {
            end = start + lock->l_len - 1;
        }
    }

    *__start = start;
    *__end   = end  ;
}


/*
 * If there is a conflicting lock to the requested lock, return the conflicting
 * alock_t struct. The lock mutex will be locked - it is the caller's
 * responsibility to ensure it is unlocked.
 */
int can_acquire_lock(struct file_t *fp, struct flock *flock,
                     int wait, struct flock *oldflock)
{
    struct alock_t *alock;
    struct fs_node_t *node;
    off_t start , end ;
    off_t start2, end2;

loop:
    node = fp->node;
    kernel_mutex_lock(&node->lock);
    alock = node->alocks;
    get_start_end(fp, flock, &start, &end);

    while(alock)
    {
        //kernel_mutex_lock(&alock->lock_mutex);
        get_start_end(fp, &(alock->internal_lock), &start2, &end2);

        /* any overlap? */
        if((start < start2 && end < start2) || (start > end2 && end > end2))
        {        /* nope */
            //kernel_mutex_unlock(&alock->lock_mutex);
            alock = alock->next;
            continue;
        }

        /*
         * yes. is it exclusive (or the requested lock is) and the old lock
         * was placed by another process?
         */
        if((alock->internal_lock.l_pid != this_core->cur_task->pid) &&
           (flock->l_type == F_WRLCK ||
                alock->internal_lock.l_type == F_WRLCK))
        {
            if(wait)        /* wait for lock to be released */
            {
                //kernel_mutex_unlock(&lock->lock_mutex);
                kernel_mutex_unlock(&node->lock);
                block_task(node->alocks, 1);
                
                if(this_core->cur_task->woke_by_signal)
                {
                    return -ERESTARTSYS;
                    //return -EINTR;
                }
                
                goto loop;
            }
            
            A_memcpy(oldflock, &(alock->internal_lock),
                            sizeof(struct flock));
            kernel_mutex_unlock(&node->lock);

            return -EAGAIN;
        }

        //kernel_mutex_unlock(&lock->lock_mutex);
        alock = alock->next;
    }

    kernel_mutex_unlock(&node->lock);

    return 0;
}


#define UPDATE_POINTERS(alock, newlock)     \
{                                           \
    newlock->next = alock->next;            \
    newlock->prev = alock;                  \
    alock->next = newlock;                  \
    if(newlock->next)                       \
        newlock->next->prev = newlock;      \
}


#define ALLOC_LOCK(newlock)                 \
    if(!(newlock = alloc_lock())) return -ENOLCK;


/*
 * Create a new lock. If there is an overlap with a current lock that is
 * held by the calling process, merge the two locks.
 */
long add_lock(struct file_t *fp, struct flock *flock)
{
    struct alock_t *alock, *newlock, *newlock2;
    struct fs_node_t *node;
    off_t start , end ;
    off_t start2, end2;

    node = fp->node;
    kernel_mutex_lock(&node->lock);
    alock = node->alocks;
    get_start_end(fp, flock, &start, &end);

    while(alock)
    {
        get_start_end(fp, &(alock->internal_lock), &start2, &end2);

        /* not our lock? */
        if(alock->internal_lock.l_pid != this_core->cur_task->pid)
        {
            alock = alock->next;
            continue;
        }

        /* any overlap? */
        if((start < start2 && end < start2) || (start > end2 && end > end2))
        {           /* nope */
            alock = alock->next;
            continue;
        }

        if(start <= start2)
        {
            /*
             *   +--+------------------------+--+
             *   |  |     requested lock     |  |
             *   +--+------------------------+--+
             *      |      current lock      |
             *      +------------------------+
             */
            if(end >= end2)
            {
                /* promote the lock */
                alock->internal_lock.l_start = start;
                alock->internal_lock.l_len = end - start + 1;
                alock->internal_lock.l_whence = SEEK_SET;
                alock->internal_lock.l_type = flock->l_type;
            }
            /*
             *   +--+---------------------+
             *   |  |    requested lock   |
             *   +--+---------------------+--+
             *      |      current lock      |
             *      +------------------------+
             */
            else
            {
                /* if new and old locks are both the same type, merge */
                if(flock->l_type == alock->internal_lock.l_type)
                {
                    alock->internal_lock.l_start = start;
                    alock->internal_lock.l_whence = SEEK_SET;
                    alock->internal_lock.l_len = end2 - start + 1;
                }
                else
                {
                    ALLOC_LOCK(newlock);
                    A_memcpy(&newlock->internal_lock, flock,
                                            sizeof(struct flock));
                    
                    /* shrink the old lock, removing the first part */
                    alock->internal_lock.l_start = end + 1;
                    alock->internal_lock.l_whence = SEEK_SET;

                    /* don't change lock len if it is to EOF */
                    if(alock->internal_lock.l_len != 0)
                    {
                        alock->internal_lock.l_len = end2 - end - 1;
                    }

                    /* add the new lock */
                    UPDATE_POINTERS(alock, newlock);
                }
            }
        }
        else
        {
            /*
             *         +---------------------+--+
             *         |     requested lock  |  |
             *      +--+---------------------+--+
             *      |      current lock      |
             *      +------------------------+
             */
            if(end >= end2)
            {
                /* if new and old locks are both the same type, merge */
                if(flock->l_type == alock->internal_lock.l_type)
                {
                    alock->internal_lock.l_start = start2;
                    alock->internal_lock.l_whence = SEEK_SET;
                    alock->internal_lock.l_len = end - start2 + 1;
                }
                else
                {
                    ALLOC_LOCK(newlock);
                    A_memcpy(&newlock->internal_lock, flock,
                                            sizeof(struct flock));

                    /* shrink the old lock, removing the last part */
                    alock->internal_lock.l_start = start2;
                    alock->internal_lock.l_whence = SEEK_SET;
                    alock->internal_lock.l_len = start - start2;

                    /* add the new lock */
                    UPDATE_POINTERS(alock, newlock);
                }
            }
            /*
             *      +------------------------+
             *      |     requested lock     |
             *   +--+------------------------+--+
             *   |         current lock         |
             *   +------------------------------+
             */
            else
            {
                /*
                 * if both locks are of the same type, we don't do anything,
                 * otherwise we splice the old lock into three parts, inserting
                 * the new lock in the middle.
                 */
                if(flock->l_type != alock->internal_lock.l_type)
                {
                    ALLOC_LOCK(newlock);

                    if(!(newlock2 = alloc_lock()))
                    {
                        kernel_mutex_unlock(&node->lock);
                        free_lock(newlock);
                        return -ENOLCK;
                    }

                    /* the middle part */
                    A_memcpy(&newlock2->internal_lock, flock,
                                            sizeof(struct flock));

                    /* the first part */
                    alock->internal_lock.l_start = start2;
                    alock->internal_lock.l_whence = SEEK_SET;
                    alock->internal_lock.l_len = start - start2;

                    /* the last part */
                    newlock->internal_lock.l_start = end + 1;
                    //newlock->internal_lock.l_len = end2 - end - 1;
                    newlock->internal_lock.l_whence = SEEK_SET;
                    newlock->internal_lock.l_type =
                                        alock->internal_lock.l_type;
                    newlock->internal_lock.l_pid = alock->internal_lock.l_pid;

                    /* don't change lock len if it is to EOF */
                    if(alock->internal_lock.l_len != 0)
                    {
                        newlock->internal_lock.l_len = end2 - end - 1;
                    }
                    else
                    {
                        newlock->internal_lock.l_len = 0;
                    }

                    /* add the new lock */
                    UPDATE_POINTERS(alock, newlock2);
                    UPDATE_POINTERS(newlock2, newlock);
                }
            }
        }

        kernel_mutex_unlock(&node->lock);

        return 0;
    }

    /*
     * no lock is overlapping with our request. create a new lock.
     */
    //ALLOC_LOCK(newlock);
    if(!(newlock = alloc_lock()))
    {
        kernel_mutex_unlock(&node->lock);
        return -ENOLCK;
    }

    //A_memcpy(&nlock->lock, flockptr, sizeof(struct flock));
    newlock->internal_lock.l_start = start;
    newlock->internal_lock.l_whence = SEEK_SET;

    /* don't change lock len if it is to EOF */
    if(flock->l_len == 0)
    {
         newlock->internal_lock.l_len = 0;
    }
    else
    {
        newlock->internal_lock.l_len = end - start + 1;
    }

    newlock->internal_lock.l_pid = this_core->cur_task->pid;

    /* add the new lock */
    newlock->prev = NULL;
    newlock->next = node->alocks;

    if(node->alocks)
    {
        node->alocks->prev = newlock;
    }

    node->alocks = newlock;
    kernel_mutex_unlock(&node->lock);

    return 0;
}


static void remove_lock_internal(struct fs_node_t *node, struct alock_t *alock)
{
    if(alock->next)
    {
        alock->next->prev = alock->prev;
    }
    
    if(alock->prev)
    {
        alock->prev->next = alock->next;
    }
    
    if(node->alocks == alock)
    {
        node->alocks = alock->next;
    }
    
    free_lock(alock);
}


/*
 * Remove the given advisory lock. This might splice the current lock, creating
 * one or two new locks, depending on the overlap between the requested and
 * current locks.
 */
long remove_lock(struct file_t *fp, struct flock *flock)
{
    struct alock_t *alock, *newlock;
    struct fs_node_t *node;
    off_t start , end ;
    off_t start2, end2;

    node = fp->node;
    kernel_mutex_lock(&node->lock);
    alock = node->alocks;
    get_start_end(fp, flock, &start, &end);

    while(alock)
    {
        get_start_end(fp, &(alock->internal_lock), &start2, &end2);

        /* not our lock? */
        if(alock->internal_lock.l_pid != this_core->cur_task->pid)
        {
            alock = alock->next;
            continue;
        }

        /* any overlap? */
        if((start < start2 && end < start2) || (start > end2 && end > end2))
        {           /* nope */
            alock = alock->next;
            continue;
        }
        
        if(start <= start2)
        {
            /*
             *   +--+------------------------+--+
             *   |  |     requested lock     |  |
             *   +--+------------------------+--+
             *      |      current lock      |
             *      +------------------------+
             */
            if(end >= end2)
            {
                /* remove the lock entirely */
                remove_lock_internal(node, alock);
            }
            /*
             *   +--+---------------------+
             *   |  |    requested lock   |
             *   +--+---------------------+--+
             *      |      current lock      |
             *      +------------------------+
             */
            else
            {
                /* splice the lock, removing the first part */
                alock->internal_lock.l_start = end + 1;

                /* don't change lock len if it is to EOF */
                if(alock->internal_lock.l_len != 0)
                {
                    alock->internal_lock.l_len = end2 - end - 1;
                }
            }
        }
        else
        {
            /*
             *         +---------------------+--+
             *         |     requested lock  |  |
             *      +--+---------------------+--+
             *      |      current lock      |
             *      +------------------------+
             */
            if(end >= end2)
            {
                /* splice the lock, removing the last part */
                alock->internal_lock.l_len = start - start2;
            }
            /*
             *      +------------------------+
             *      |     requested lock     |
             *   +--+------------------------+--+
             *   |         current lock         |
             *   +------------------------------+
             */
            else
            {
                /* splice the lock into three parts, removing the 
                 * middle part
                 */
                ALLOC_LOCK(newlock);
               
                alock->internal_lock.l_start = start2;
                alock->internal_lock.l_whence = SEEK_SET;
                alock->internal_lock.l_len = start - start2;

                newlock->internal_lock.l_start  = end + 1;
                newlock->internal_lock.l_len = end2 - end - 1;
                newlock->internal_lock.l_whence = SEEK_SET;
                newlock->internal_lock.l_type = alock->internal_lock.l_type;
                newlock->internal_lock.l_pid = alock->internal_lock.l_pid;
                
                UPDATE_POINTERS(alock, newlock);

                /*
                newlock->next = alock->next;
                newlock->prev = alock;
                alock->next = newlock;
               
                if(newlock->next)
                {
                    newlock->next->prev = newlock;
                }
                */
            }
        }

        kernel_mutex_unlock(&node->lock);
        unblock_tasks(&node->alocks);

        return 0;
    }

    kernel_mutex_unlock(&node->lock);
    return -ENOENT;
}


/*
 * Remove all advisory locks placed by the given task on the given file.
 */
void remove_task_locks(struct task_t *task, struct file_t *fp)
{
    struct alock_t *alock, *nextlock;
    struct fs_node_t *node;

    node = fp->node;
    kernel_mutex_lock(&node->lock);
    alock = node->alocks;

    while(alock)
    {
        nextlock = alock->next;

        /* not our lock? */
        if(alock->internal_lock.l_pid == task->pid)
        {
            remove_lock_internal(node, alock);
        }

        alock = nextlock;
    }

    kernel_mutex_unlock(&node->lock);
}

