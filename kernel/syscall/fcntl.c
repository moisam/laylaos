/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: fcntl.c
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
 *  \file fcntl.c
 *
 *  Advisory lock interface and handler function for the fnctl syscall.
 *  Most of the heavy work is done in fcntl_internal.c.
 */

//#define __DEBUG
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/filio.h>
//#include <kernel/socketvar.h>
#include <kernel/laylaos.h>
#include <kernel/syscall.h>
#include <kernel/task.h>
#include <kernel/dev.h>
#include <kernel/user.h>
#include <kernel/fcntl.h>
#include <kernel/fio.h>
#include <fs/pipefs.h>

#include "../kernel/task_funcs.c"


// defined in dup.c
extern long do_dup(int fd, int arg);

// defined in fcntl_internal.c
extern long add_lock(struct file_t *fp, struct flock *flock);
extern long remove_lock(struct file_t *fp, struct flock *flock);


#define CHECK_LOCK_TYPE(lock)                               \
    if(lock.l_type != F_RDLCK && lock.l_type != F_WRLCK &&  \
        lock.l_type != F_UNLCK)                             \
            return -EINVAL;

#define CHECK_LOCK_WHENCE(lock)                                     \
    if(lock.l_whence != SEEK_SET && lock.l_whence != SEEK_CUR &&    \
        lock.l_whence != SEEK_END)                                  \
            return -EINVAL;


/*
 * Helper function to acquire a lock.
 */
long fcntl_setlock(struct file_t *fp, int cmd, struct flock *lock)
{
    long res;
    struct flock olock;

    /* can't go before start of file */
    if(get_start(fp, lock) < 0)
    {
        return -EINVAL;
    }

    /* then, apply the requested operation */
    if(lock->l_type == F_UNLCK)
    {
        return remove_lock(fp, lock);
    }

    if((res = can_acquire_lock(fp, lock, (cmd == F_SETLKW), &olock)) != 0)
    {
        /* could be -EAGAIN or -EINTR */
        return res;
    }

    if(lock->l_type == F_RDLCK)
    {
        if(!(fp->flags & O_RDONLY) && !(fp->flags & O_RDWR))
        {
            return -EBADF;
        }

        return add_lock(fp, lock);
    }

    if(!(fp->flags & O_WRONLY) && !(fp->flags & O_RDWR))
    {
        return -EBADF;
    }

    return add_lock(fp, lock);
}


/*
 * Handler for syscall fcntl().
 */
long syscall_fcntl(int fd, int cmd, void *arg)
{	
	struct file_t *fp = NULL;
    struct fs_node_t *node = NULL;
    long res;
	int tmp /* , otmp */;
	pid_t pid;
	mode_t mode;
    struct flock lock, olock;
	volatile struct task_t *ct = this_core->cur_task;

    //KDEBUG("syscall_fcntl: fd %d, cmd %d, arg " _XPTR_ "\n", fd, cmd, arg);

    if(fdnode(fd, ct, &fp, &node) != 0)
	{
		return -EBADF;
	}

	switch(cmd)
	{
        /*********************************
         * (1) duplicate file descriptor
         *********************************/

		case F_DUPFD:
        	if(!validfd(fd, ct))
        	{
        		return -EBADF;
        	}

			return do_dup(fd, (int)(uintptr_t)arg);

		case F_DUPFD_CLOEXEC:
        	if(!validfd(fd, ct))
        	{
        		return -EBADF;
        	}

			if((res = do_dup(fd, (int)(uintptr_t)arg)) < 0)   // error result
			{
			    return res;
			}

        	// set the close-on-exec flag - do_dup() clears it by default
        	cloexec_set(ct, res);
        	return res;

        /*********************************
         * (2) file descriptor flags
         *********************************/

		case F_GETFD:       // get file descriptor flags
            return (is_cloexec(ct, fd)) ? FD_CLOEXEC : 0;

		case F_SETFD:       // set file descriptor flags
            res = (int)(uintptr_t)arg;
            //KDEBUG("syscall_fcntl: fd %d, res %d\n", fd, res);

            if(res & FD_CLOEXEC)
            {
                cloexec_set(ct, fd);        //   set the close-on-exec flag
            }
            else
            {
                cloexec_clear(ct, fd);      // clear the close-on-exec flag
            }
            
            return 0;

        /*********************************
         * (3) file status flags
         *********************************/

		case F_GETFL:       // get file status flags
            //KDEBUG("syscall_fcntl: fd %d, flags %d\n", fd, fp->flags);
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
			return fp->flags;
			
		case F_SETFL:       // set file status flags
            res = (int)(uintptr_t)arg;
            /* 
             * we will ignore the file access mode bits and the file
             * creation flags, as advised by POSIX.
             */
            res &= ~(O_ACCMODE | O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC);

		    //otmp = fp->flags & FNONBLOCK;
		    //__asm__ __volatile__("xchg %%bx, %%bx"::);
			fp->flags &= ~(O_APPEND | O_NONBLOCK | O_ASYNC |
			               O_DIRECT | O_NOATIME);
			fp->flags |= res & (O_APPEND | O_NONBLOCK | O_ASYNC |
			                    O_DIRECT | O_NOATIME);
			
			// set device flags
			mode = node->mode;
			
			if(IS_SOCKET(node) || S_ISCHR(mode) || S_ISBLK(mode))
			{
			    tmp = !!(fp->flags & O_NONBLOCK);
        		return syscall_ioctl_internal(fd, FIONBIO, (char *)&tmp, 1);
			    /*
			    tmp = fp->flags & FNONBLOCK;
                if((res = syscall_ioctl_internal(fd, FIONBIO, (char *)&tmp, 1)))
                {
                    return res;
                }

        		tmp = fp->flags & FASYNC;
                if(!(res = syscall_ioctl_internal(fd, FIOASYNC, (char *)&tmp, 1)))
                {
                    return res;
                }

                // restore the non-block flag
        		if(otmp)
        		{
            		fp->flags |= FNONBLOCK;
        		}
        		else
        		{
            		fp->flags &= ~FNONBLOCK;
        		}
        		
        		syscall_ioctl_internal(fd, FIONBIO, (char *)&otmp, 1);
        		return res;
        		*/
			}
			
			return 0;

        /*********************************
         * (4) advisory record locking
         *
         * TODO: See: https://man7.org/linux/man-pages/man2/fcntl.2.html
         *********************************/

        /* NOTE: Compatibility between different lock types:
         * 
         *                               Request for
         *                         +-----------+------------+
         *                         | Read lock | Write lock |
         *           +-------------+-----------+------------+
         *           | No locks    |    OK     |     OK     |
         *           +-------------+-----------+------------+
         * Region    | One or more |    OK     |   Denied   |
         * currently | read locks  |           |            |
         * has       +-------------+-----------+------------+
         *           | One write   |  Denied   |   Denied   |
         *           | lock        |           |            |
         *           +-------------+-----------+------------+
         */

		case F_GETLK:
            /*
             * - check if the lock described by flockptr is blocked by another
             *   lock (by another task).
             * - if another lock prevents the request from succeeding, info on
             *   the existing lock overwrites flockptr.
             * - if we can create the requested lock, keep flockptr as-is,
             *   except for l_type, which we should set to F_UNLCK.
             */

            COPY_FROM_USER(&lock, arg, sizeof(struct flock));

            /* first, sanity checks */
            CHECK_LOCK_TYPE(lock);
            CHECK_LOCK_WHENCE(lock);

            /* can't go before start of file */
            if(get_start(fp, &lock) < 0)
            {
                return -EINVAL;
            }

            /* then, apply the requested operation */
            if(can_acquire_lock(fp, &lock, 0, &olock) != 0)
            {
                lock.l_type = F_UNLCK;
                COPY_TO_USER(arg, &lock, sizeof(struct flock));
                return 0;
            }
            else
            {
                COPY_TO_USER(arg, &olock, sizeof(struct flock));
                return olock.l_pid;
            }
            
            break;

		case F_SETLK:
		case F_SETLKW:
            /* F_SETLK:
             * - set the lock described by flockptr. check the Compatibility
             *   rules above.
             * - if the rules prevent us from getting the lock, return with
             *   errno = EAGAIN.
             * - if l_type is F_UNLCK, clear the lock described by flockptr.
             *
             * F_SETLKW:
             * - a blocking version of F_SETLK.
             * - If the requested R or W lock cannot be granted because another
             *   task currently has some part of the requested region locked,
             *   the calling task is put to sleep. The task wakes up either
             *   when the lock becomes available or when interrupted by a
             *   signal.
             * 
             * TODO: detect (and handle) deadlock situations.
             */

            COPY_FROM_USER(&lock, arg, sizeof(struct flock));

            /* first, sanity checks */
            CHECK_LOCK_TYPE(lock);
            CHECK_LOCK_WHENCE(lock);
            
            return fcntl_setlock(fp, cmd, &lock);

        /*********************************
         * (5) managing signals
         *
         * TODO: See: https://man7.org/linux/man-pages/man2/fcntl.2.html
         *********************************/
        
        case F_GETOWN:
			mode = node->mode;

			if(IS_SOCKET(node))
			{
			    if(node->data)
			    {
    			    return ((struct socket_t *)node->data)->pid;
    			}
    			
    			return -EINVAL;
			}
			
			if(S_ISCHR(mode) || S_ISBLK(mode))
			{
                res = syscall_ioctl_internal(fd, TIOCGPGRP, (char *)&pid, 1);
                // return error (res != 0) or the result
                return res ? res : pid;
			}
			
			return -EINVAL;

 
        case F_SETOWN:
			mode = node->mode;

			if(IS_SOCKET(node))
			{
			    if(node->data)
			    {
    			    return
    			      copy_from_user(&(((struct socket_t *)node->data)->pid),
    			                            (pid_t *)arg, sizeof(pid_t));
    			}
    			
    			return -EINVAL;
			}

			if(!S_ISCHR(mode) && !S_ISBLK(mode))
			{
    			return -EINVAL;
			}
			
			pid = (pid_t)(uintptr_t)arg;
			
			if(pid <= 0)        // pgid
			{
			    pid = -pid;
			}
			else                // pid
			{
			    volatile struct task_t *t = get_task_by_id(pid);

			    if(!t)
			    {
			        return -ESRCH;
			    }

			    pid = t->pgid;
			}

            res = syscall_ioctl_internal(fd, TIOCSPGRP, (char *)&pid, 1);
            // return error (res != 0) or the result
            return res ? res : pid;

#if 0

        /**************************************
         * (6) changing the capacity of a pipe
         **************************************/
        
        case F_GETPIPE_SZ:
            return pipefs_get_size(node);

        case F_SETPIPE_SZ:
            return pipefs_set_size(node, (int)arg);

#endif

        /*********************************
         * TODO: Handle the other fcntl flags (see link below).
         *
         *       https://man7.org/linux/man-pages/man2/fcntl.2.html
         *********************************/

        /* unknown command */
		default:
			return -EINVAL;
	}
}

