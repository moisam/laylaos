/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: stat.c
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
 *  \file stat.c
 *
 *  Functions for reading file information.
 *
 *  See: https://man7.org/linux/man-pages/man2/oldlstat.2.html
 *
 *  NOTE: lstat() is identical to stat(), except that if pathname is a symbolic
 *        link, then it returns information about the link itself, not the file
 *        that it refers to.
 */

//#define __DEBUG

#include <errno.h>
#include <fcntl.h>              // AT_FDCWD
#include <sys/stat.h>
#include <kernel/vfs.h>
#include <kernel/syscall.h>
#include <kernel/user.h>
#include <kernel/task.h>
#include <kernel/fio.h>
#include <fs/sockfs.h>


static int copy_stat(struct fs_node_t *node, struct stat *statbuf)
{
    struct mount_info_t *dinfo;
	struct stat tmp;
	
	//A_memset(&tmp, 0, sizeof(struct stat));

	tmp.st_dev = node->dev;
	tmp.st_ino = node->inode;
	tmp.st_mode = node->mode;
	tmp.st_nlink = node->links;
	tmp.st_uid = node->uid;
	tmp.st_gid = node->gid;
	tmp.st_rdev = 0;
	tmp.st_size = node->size;
	tmp.st_atim.tv_sec = node->atime;
	tmp.st_mtim.tv_sec = node->mtime;
	tmp.st_ctim.tv_sec = node->ctime;
	tmp.st_blksize = 0;
	
    if((dinfo = get_mount_info(node->dev)))
    {
    	tmp.st_blksize = dinfo->block_size;
	}

	if(!tmp.st_blksize)
	{
    	tmp.st_blksize = 512;
	}

   	tmp.st_blocks = tmp.st_size / tmp.st_blksize;

    if(S_ISBLK(node->mode) || S_ISCHR(node->mode))
    {
        tmp.st_rdev = (dev_t)node->blocks[0];
    }

    return copy_to_user((void *)statbuf, (void *)&tmp, sizeof(struct stat));
}


static int do_stat(char *filename, int dirfd, struct stat *statbuf, 
                   int followlink)
{
    struct fs_node_t *node = NULL;
	int res;
	int open_flags = OPEN_USER_CALLER | 
	                 (followlink ? OPEN_FOLLOW_SYMLINK : 
	                               OPEN_NOFOLLOW_SYMLINK);

    if(!filename || !statbuf)
    {
        return -EFAULT;
    }
    
	if((res = vfs_open_internal(filename, dirfd, &node, open_flags)) < 0)
	{
	    KDEBUG("do_stat: res %d\n", res);
		return res;
	}

    /* sockets do their own thing */
    /*
    if(IS_SOCKET(node))
    {
        return -ENOENT;
    }
    else
    */
    {
    	res = copy_stat(node, statbuf);
    }

	release_node(node);
	
	return res;
}


/*
 * Handler for syscall stat().
 */
int syscall_stat(char *filename, struct stat *statbuf)
{
    KDEBUG("syscall_stat: filename %s\n", filename);

    return do_stat(filename, AT_FDCWD, statbuf, 1);
}


/*
 * Handler for syscall lstat().
 */
int syscall_lstat(char *filename, struct stat *statbuf)
{
    KDEBUG("syscall_lstat: filename %s\n", filename);

    return do_stat(filename, AT_FDCWD, statbuf, 0);
}


// We currently only support one flag. For the other unimplemented flags, see:
//     https://man7.org/linux/man-pages/man2/oldfstat.2.html
#define VALID_FLAGS         AT_SYMLINK_NOFOLLOW

/*
 * Handler for syscall fstatat().
 */
int syscall_fstatat(int fd, char *filename, struct stat *statbuf, int flags)
{
    KDEBUG("syscall_fstatat: filename %s\n", filename);

    // check for unknown flags
    if(flags & ~VALID_FLAGS)
    {
        return -EINVAL;
    }

    return do_stat(filename, fd, statbuf, !(flags & AT_SYMLINK_NOFOLLOW));
}


/*
 * Handler for syscall fstat().
 */
int syscall_fstat(int fd, struct stat *statbuf)
{
    KDEBUG("syscall_fstat: fd %d\n", fd);
    
	struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    int res;
    struct task_t *ct = cur_task;

    if(!statbuf)
    {
        return -EFAULT;
    }

    if(fdnode(fd, ct, &f, &node) != 0)
    {
        return -EBADF;
    }

    /* sockets do their own thing */
    /*
    if(IS_SOCKET(node))
    {
        return -ENOENT;
    }
    else
    */
    {
    	res = copy_stat(node, statbuf);
    }

	return res;
}

