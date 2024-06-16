/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: dup.c
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
 *  \file dup.c
 *
 *  Functions for duplicating open file descriptors.
 */

#include <errno.h>
#include <fcntl.h>
#include <kernel/vfs.h>
#include <kernel/syscall.h>
#include <kernel/task.h>
#include <kernel/fio.h>
#include <fs/procfs.h>


int do_dup(int fd, int arg)
{
    struct task_t *ct = cur_task;

	if(!validfd(fd, ct))
	{
		return -EBADF;
	}
	
	if(arg < 0 || arg >= NR_OPEN)
	{
		return -EINVAL;
	}
	
	while(arg < NR_OPEN)
	{
		if(ct->ofiles->ofile[arg])    // fd in use
		{
			arg++;
		}
		else                        // free fd
		{
			break;
		}
	}
	
	if(arg >= NR_OPEN)
	{
		return -EMFILE;
	}
	
	// clear the close-on-exec flag
	cloexec_clear(ct, arg);

    // duplicate fd
	ct->ofiles->ofile[arg] = ct->ofiles->ofile[fd];
	ct->ofiles->ofile[arg]->refs++;
	
	return arg;
}


/*
 * Handler for syscall dup3().
 */
int syscall_dup3(int oldfd, int newfd, int flags)
{
    struct task_t *ct = cur_task;
    int res;

	if(!validfd(oldfd, ct))
	{
		return -EBADF;
	}
	
	if(oldfd == newfd)
	{
	    return -EINVAL;
	}
	
	// check for unsupported flags
	if(flags & ~O_CLOEXEC)
	{
	    return -EINVAL;
	}

	syscall_close(newfd);

	if((res = do_dup(oldfd, newfd)) >= 0)
	{
	    if(flags & O_CLOEXEC)
	    {
	        // set the close-on-exec flag
	        cloexec_set(ct, res);
        }
	}
	
	return res;
}


/*
 * Handler for syscall dup2().
 */
int syscall_dup2(int oldfd, int newfd)
{
    struct task_t *ct = cur_task;

	if(!validfd(oldfd, ct))
	{
		return -EBADF;
	}
	
	if(oldfd == newfd)
	{
	    return newfd;
	}

	syscall_close(newfd);

	return do_dup(oldfd, newfd);
}


/*
 * Handler for syscall dup().
 *
 * NOTE: POSIX says:
 *       The call dup(fildes) shall be equivalent to:
 *
 *       fcntl(fildes, F_DUPFD, 0);
 */
int syscall_dup(int fildes)
{
	return do_dup(fildes, 0);
}

