/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: open.c
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
 *  \file open.c
 *
 *  Functions for opening files.
 */

//#define __DEBUG

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             TTY_BUF_SIZE

#include <errno.h>
#include <fcntl.h>              // AT_SYMLINK_NOFOLLOW
#include <kernel/laylaos.h>
#include <kernel/syscall.h>
#include <kernel/tty.h>
#include <kernel/loop.h>
#include <kernel/loop_internal.h>
#include <fs/procfs.h>
#include <fs/devpts.h>

#include "../kernel/tty_inlines.h"


#define MAY_SET_CTTY()                                              \
	if(group_leader(this_core->cur_task) &&                         \
	   this_core->cur_task->ctty <= 0 && !(flags & O_NOCTTY))       \
	{                                                               \
        if((res = set_controlling_tty(node->blocks[0],              \
                         get_struct_tty(node->blocks[0]), 1)) < 0)  \
        {                                                           \
			goto error;                                             \
        }                                                           \
	}


/*
 * Handler for syscall open().
 */
long syscall_open(char *filename, int flags, mode_t mode)
{
    //KDEBUG("syscall_open: filename = %s\n", filename);
    return syscall_openat(AT_FDCWD, filename, flags, mode);
}


/*
 * Handler for syscall openat().
 */
long syscall_openat(int dirfd, char *filename, int flags, mode_t mode)
{
	struct fs_node_t *node = NULL, *pty_master = NULL;
	struct file_t *f = NULL;
	int fd = 0;
	long i, res = 0;

    // add write access if truncate is requested without write/rw access
    if((flags & O_TRUNC) && !(flags & (O_WRONLY | O_RDWR)))
    {
        flags |= O_WRONLY;
    }

    // if O_PATH is specified, flags can only contain the following
    if(flags & O_PATH)
    {
        flags &= (O_PATH | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    }

    if(!filename || !this_core->cur_task ||
       !this_core->cur_task->fs || !this_core->cur_task->ofiles)
    {
        return -EINVAL;
    }

	mode &= (0777 & ~this_core->cur_task->fs->umask);
	
	if((res = falloc(&fd, &f)) != 0)
	{
	    return res;
	}

    // try to open the file
    if((i = vfs_open(filename, flags, mode, dirfd, &node,
                     OPEN_USER_CALLER | OPEN_CREATE_DENTRY)) != 0)
    {
        KDEBUG("syscall_openat: 4 - i %d\n", i);
    	this_core->cur_task->ofiles->ofile[fd] = NULL;
    	f->refs = 0;
		return i;
	}

    // special treatment for ttys (dev major 4 and 5)
	if(S_ISCHR(node->mode))
	{
	    int major = MAJOR(node->blocks[0]);

		if(major == 4)         // ttyx device
		{
            // make it the controlling terminal if the task has none
		    MAY_SET_CTTY();
		}
		// pseudoterminal multiplexor device
		// perform this check before the next else clause, as the multiplexor
		// is a special case of MAJOR == 5
		else if(node->blocks[0] == PTMX_DEVID)
		{
	        if((res = pty_master_create(&pty_master)) < 0)
	        {
	            goto error;
		    }
		    
		    release_node(node);
		    node = pty_master;

            // make it the controlling terminal if the task has none
		    MAY_SET_CTTY();
		}
		else if(major == 5)
		{
			if(this_core->cur_task->ctty <= 0)    // cur_task tty device
			{
			    // current task has no controlling terminal
			    res = -EPERM;
			    goto error;
			}
		}
		else if(major == PTY_SLAVE_MAJ)
		{
		    if((res = pty_slave_open(node)) < 0)
	        {
	            goto error;
		    }

            // make it the controlling terminal if the task has none
		    MAY_SET_CTTY();
		}
	}
	else if(S_ISBLK(node->mode))
	{
	    int major = MAJOR(node->blocks[0]);

		if(major == LODEV_MAJ)
		{
		    if((res = lodev_open(node->blocks[0])) < 0)
	        {
	            goto error;
		    }
		}
	}

    // set the close-on-exec flag
    if(flags & O_CLOEXEC)
    {
        cloexec_set(this_core->cur_task, fd);
    }
    
    // ignore O_NOATIME if file is not ours
    if((flags & O_NOATIME) && (this_core->cur_task->euid != node->uid))
    {
        flags &= ~O_NOATIME;
    }
	
    f->mode = node->mode;
    f->flags = flags;
    f->refs = 1;
    f->node = node;
    f->pos = 0;

    KDEBUG("syscall_openat: done\n");

    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    
    return fd;
    
error:
	release_node(node);
	this_core->cur_task->ofiles->ofile[fd] = NULL;
	f->refs = 0;
	return res;
}


/*
 * Open a temporary file descriptor.
 */
/*
int open_tmp_fd(struct fs_node_t *node)
{
	struct file_t *f;
	int fd, res = 0;

    if(!node)
    {
        return -EINVAL;
    }

	if((res = falloc(&fd, &f)) != 0)
	{
	    return res;
	}

    f->mode = node->mode;
    f->flags = O_RDWR;
    f->refs = 1;
    f->node = node;
    f->pos = 0;

    INC_NODE_REFS(node);
    
    return fd;
}
*/

