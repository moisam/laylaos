/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: chdir.c
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
 *  \file chdir.c
 *
 *  Functions for working with task root and current working directories.
 */

//#define __DEBUG

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/fio.h>


static int do_chdir(struct fs_node_t *node)
{
	int res;

	if(!node)
	{
	    return -EBADF;
	}
	
	volatile mode_t mode = node->mode;
	
	if(!S_ISDIR(mode))
	{
		release_node(node);
		return -ENOTDIR;
	}

	//__asm__("xchg %%bx, %%bx"::);

    if((res = has_access(node, EXECUTE, 0)) == 0)
    {
        struct task_t *ct = cur_task;
    	volatile struct task_fs_t *fs = ct->fs;
    	volatile struct fs_node_t *tmp = fs->cwd;
    	release_node((struct fs_node_t *)tmp);
    	tmp = node;
    	fs->cwd = (struct fs_node_t *)tmp;
    }
    else
    {
    	release_node(node);
	}
	
	return res;
}


/*
 * Handler for syscall chdir().
 */
int syscall_chdir(char *f)
{
    volatile char *filename = f;
	struct fs_node_t *node = NULL;
	int res;
	int open_flags = OPEN_USER_CALLER | OPEN_FOLLOW_SYMLINK;
	
	if((res = vfs_open_internal((char *)filename, AT_FDCWD, 
	                                    &node, open_flags)) < 0)
	{
    	KDEBUG("syscall_chdir: res = %d\n", res);
    	//__asm__("xchg %%bx, %%bx"::);

		return res;
	}
	
	return do_chdir(node);
}


/*
 * Handler for syscall fchdir().
 */
int syscall_fchdir(int fd)
{
	struct file_t *f;
    struct fs_node_t *node;
    struct task_t *t = cur_task;

    if(fdnode(fd, t, &f, &node) != 0)
    {
        return -EBADF;
    }

    // the call to do_chdir() will either use the node or release it, so
    // increment the node's refs before the call
    INC_NODE_REFS(node);

	return do_chdir(node);
}


/*
 * Handler for syscall chroot().
 */
int syscall_chroot(char *filename)
{
	struct fs_node_t *node = NULL;
	int res;
	int open_flags = OPEN_USER_CALLER | OPEN_FOLLOW_SYMLINK;
    struct task_t *t = cur_task;

    if(!t || !t->fs)
    {
        return 0;
    }
    
    if(!suser(t))
    {
        return -EPERM;
    }
    
	if((res = vfs_open_internal(filename, AT_FDCWD, &node, open_flags)) < 0)
	{
		return res;
	}

	if(!node)
	{
	    return -ENOENT;
	}

	volatile mode_t mode = node->mode;
	
	if(!S_ISDIR(mode))
	{
		release_node(node);
		return -ENOTDIR;
	}

    if((res = has_access(node, EXECUTE, 0)) == 0)
    {
    	release_node(t->fs->root);
    	t->fs->root = node;
    }
    else
    {
    	release_node(node);
	}
	
	return res;
}

