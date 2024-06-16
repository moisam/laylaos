/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: truncate.c
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
 *  \file truncate.c
 *
 *  Functions for truncating files.
 *
 *  See: https://man7.org/linux/man-pages/man2/truncate.2.html
 */

#include <errno.h>
#include <fcntl.h>              // AT_FDCWD
#include <kernel/vfs.h>
#include <kernel/syscall.h>
#include <kernel/task.h>
#include <kernel/ksignal.h>
#include <kernel/fio.h>


/*
 * TODO: Handle the ETXTBSY error, which should arise when we try to truncate
 *       an executable file that is being executed currently.
 *
 *       For details, see:
 *           https://man7.org/linux/man-pages/man2/truncate.2.html
 */
static int do_truncate(struct fs_node_t *node, off_t length)
{
    struct task_t *ct = cur_task;

    if(!node)
    {
        return -EINVAL;
    }

    if(has_access(node, WRITE, 0) != 0)
    {
        return -EPERM;
    }

    if(S_ISDIR(node->mode))
    {
        return -EISDIR;
    }

    if(!S_ISREG(node->mode))
    {
        return -EPERM;
    }

    if(exceeds_rlimit(ct, RLIMIT_FSIZE, length))
    {
        user_add_task_signal(ct, SIGXFSZ, 1);
        return -EFBIG;
    }

    return truncate_node(node, length);
}


/*
 * Handler for syscall truncate().
 */
int syscall_truncate(char *pathname, off_t length)
{
	int res;
    struct fs_node_t *node = NULL;
	int open_flags = OPEN_USER_CALLER | OPEN_FOLLOW_SYMLINK;

    if(!pathname)
    {
        return -EFAULT;
    }

    if(length < 0)
    {
        return -EINVAL;
    }

	if((res = vfs_open_internal(pathname, AT_FDCWD, &node, open_flags)) < 0)
	{
		return res;
	}
    
    res = do_truncate(node, length);
	release_node(node);
	
	return res;
}


/*
 * Handler for syscall ftruncate().
 */
int syscall_ftruncate(int fd, off_t length)
{
	struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    struct task_t *ct = cur_task;

	if(length < 0)
	{
	    return -EBADF;
	}
	
    if(fdnode(fd, ct, &f, &node) != 0)
	{
		return -ENOENT;
	}

    if(!(f->flags & O_RDWR) && !(f->flags & O_WRONLY))
    {
        return -EPERM;
    }
	
    return do_truncate(node, length);
}

