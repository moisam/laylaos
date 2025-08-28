/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
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


/*
 * This function assumes the caller has checked fd is a valid descriptor.
 * It is called from functions here and in fcntl.c.
 */
long do_dup(int fd, int arg)
{
	if(arg < 0 || arg >= NR_OPEN)
	{
		return -EINVAL;
	}
	
	while(arg < NR_OPEN)
	{
		if(this_core->cur_task->ofiles->ofile[arg])    // fd in use
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
	cloexec_clear(this_core->cur_task, arg);

    // duplicate fd
	this_core->cur_task->ofiles->ofile[arg] = this_core->cur_task->ofiles->ofile[fd];
	//ct->ofiles->ofile[arg]->refs++;
    __sync_fetch_and_add(&(this_core->cur_task->ofiles->ofile[arg]->refs), 1);

	return arg;
}


/*
 * Handler for syscall dup3().
 */
long syscall_dup3(int oldfd, int newfd, int flags)
{
    long res;

	if(!validfd(oldfd, this_core->cur_task))
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
	        cloexec_set(this_core->cur_task, res);
        }
	}
	
	return res;
}


/*
 * Handler for syscall dup2().
 */
long syscall_dup2(int oldfd, int newfd)
{
	if(!validfd(oldfd, this_core->cur_task))
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
long syscall_dup(int fildes)
{
	if(!validfd(fildes, this_core->cur_task))
	{
		return -EBADF;
	}

	return do_dup(fildes, 0);
}

