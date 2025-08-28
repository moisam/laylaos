/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: fcntl.h
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
 *  \file fcntl.h
 *
 *  Functions and struct definitions to allow us to implement the fcntl
 *  function and advisory locks in the kernel.
 */

#ifndef __KERNEL_LOCKS_H__
#define __KERNEL_LOCKS_H__

#include <fcntl.h>
#include <kernel/vfs.h>
#include <kernel/bits/task-defs.h>

#ifndef FREAD
#define	FREAD		0x0001	/* read enabled */
#endif

#ifndef FWRITE
#define	FWRITE		0x0002	/* write enabled */
#endif


/**
 * @struct alock_t
 * @brief The alock_t structure.
 *
 * A structure to represent an advisory lock, for use within the kernel.
 */
struct alock_t
{
    struct flock internal_lock;     /**< the actual lock */
    struct alock_t *next,           /**< pointer to next lock in list */
                   *prev;           /**< pointer to previous lock in list */
};


struct file_t;


/**
 * @brief Acquire a lock.
 *
 * Helper function to acquire a lock. Called by syscall_fcntl() and 
 * syscall_flock().
 *
 * @param   fp          file to which the lock applies
 * @param   cmd         command to perform, either F_SETLK or F_SETLKW
 * @param   lock        the lock to operate on
 *
 * @return  zero on success, -(errno) on failure.
 */
long fcntl_setlock(struct file_t *fp, int cmd, struct flock *lock);


/*************************************************
 * Helper functions defined in fcntl_internal.c.
 *************************************************/

/**
 * @brief Calculate lock's start offset.
 *
 * Helper function to calculate the requested lock's start offset.
 *
 * @param   fp          file to which the lock applies
 * @param   lock        the lock to operate on
 *
 * @return  lock's start offset in \a fp.
 */
off_t get_start(struct file_t *fp, struct flock *lock);

/**
 * @brief Calculate lock's start and end offsets.
 *
 * Helper function to calculate the requested lock's start and end offsets.
 *
 * If l_len is positive, the lock range is l_start up to and including 
 * (l_start + l_len - 1). A l_len value of 0 has the special meaning: lock all
 * bytes starting at l_start (interpreted according to l_whence) through to the
 * end of file. POSIX.1-2001 allows (but does not require) support of a 
 * negative l_len value. If l_len is negative, the lock covers bytes
 * (l_start + l_len) up to and including (l_start - 1).
 *
 * @param   fp          file to which the lock applies
 * @param   lock        the lock to operate on
 * @param   __start     lock's start offset is returned here
 * @param   __end       lock's end offset is returned here
 *
 * @return  nothing.
 */
void get_start_end(struct file_t *fp, struct flock *lock,
                   off_t *__start, off_t *__end);

/**
 * @brief Check if a lock can be acquired.
 *
 * Helper function to check if a lock can be acquired.
 * If there is a conflicting lock to the requested lock, return the conflicting
 * alock_t struct. The lock mutex will be locked - it is the caller's
 * responsibility to ensure it is unlocked.
 *
 * @param   fp          file to which the lock applies
 * @param   flock       the lock to operate on
 * @param   wait        if non-zero, call will block until the lock is released
 * @param   oldflock    if there is a conflicting lock, it will be
 *                        returned here
 *
 * @return  zero if lock can be acquired, -(errno) otherwise.
 */
int can_acquire_lock(struct file_t *fp, struct flock *flock,
                     int wait, struct flock *oldflock);

/**
 * @brief Remove task locks.
 *
 * Remove all advisory locks placed by the given task on the given file.
 * Called from syscall_close() when a file is being closed either when the
 * user syscalls close, or when the task is being terminated.
 *
 * @param   task        task structure
 * @param   fp          file to which the lock applies
 *
 * @return  nothing.
 */
void remove_task_locks(struct task_t *task, struct file_t *fp);


/**
 * @brief Handler for syscall fcntl().
 *
 * General file control (fnctl) function.
 *
 * @param   fd          file descriptor
 * @param   cmd         command (see fcntl.h for possible values)
 * @param   arg         optional command argument, depends on \a cmd
 *
 * @return  zero on success, -(errno) on failure.
 */
long syscall_fcntl(int fd, int cmd, void *arg);

#endif      /* __KERNEL_LOCKS_H__ */
