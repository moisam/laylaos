/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: utime.c
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
 *  \file utime.c
 *
 *  Functions for changing file last access and modification times.
 *
 *  See: https://man7.org/linux/man-pages/man2/utimes.2.html
 */

#include <errno.h>
#include <fcntl.h>              // AT_FDCWD
#include <kernel/user.h>
#include <kernel/syscall.h>
#include <kernel/clock.h>


static int do_utime(int dirfd, char *filename, struct utimbuf *times)
{
    struct mount_info_t *dinfo;
	struct fs_node_t *node;
	int res;
	long atime, mtime;
	int open_flags = OPEN_USER_CALLER | OPEN_FOLLOW_SYMLINK;
    struct task_t *ct = cur_task;

    if(!filename)
    {
        /*
         * The utimensat manpage says:
         *    ... the Linux utimensat() system call implements a nonstandard
         *    feature:  if  pathname  is NULL,  then  the  call  modifies the
         *    timestamps of the file referred to by the file descriptor dirfd
         *    (which may refer to any type of file).
         *
         * Here we handle this case as some utilities (e.g. coreutils touch)
         * use it.
         */
        if(dirfd != AT_FDCWD)
        {
            if(!ct->ofiles || dirfd < 0 || dirfd >= NR_OPEN ||
               ct->ofiles->ofile[dirfd] == NULL ||
               (node = ct->ofiles->ofile[dirfd]->node) == NULL)
            {
                return -EBADF;
            }
        }
        else
        {
            if(!ct->fs || !ct->fs->cwd || ct->fs->cwd->refs == 0)
            {
                return -EBADF;
            }

            node = ct->fs->cwd;
        }
    
        if(!(node = get_node(node->dev, node->inode, 1)))
        {
            return -EBADF;
        }
    }
    else
    {
        // filename is not NULL -- "normal" behaviour
    	if((res = vfs_open_internal(filename, dirfd, &node, open_flags)) < 0)
    	{
    		return res;
    	}
	}

    // can't chmod if the filesystem was mount readonly
    if((dinfo = get_mount_info(node->dev)) && (dinfo->mountflags & MS_RDONLY))
    {
    	release_node(node);
        return -EROFS;
    }

	if(times)
	{
	    // check permissions
    	if(!suser(ct) && ct->euid != node->uid)
    	{
        	release_node(node);
    		return -EPERM;
        }

	    atime = times->actime;
	    mtime = times->modtime;
	}
	else
	{
	    // check permissions
    	if(!suser(ct) && ct->euid != node->uid)
    	{
        	release_node(node);
    		return -EACCES;
        }

		atime = mtime = now();
	}
	
	node->atime = atime;
	node->mtime = mtime;
	node->flags |= FS_NODE_DIRTY;
	release_node(node);

	return 0;
}


/*
 * Handler for syscall utime().
 */
int syscall_utime(char *filename, struct utimbuf *times)
{
    struct utimbuf tmp;
    struct utimbuf *ptr;
    
    if(times)
    {
    	COPY_FROM_USER(&tmp.actime, &times->actime, sizeof(time_t));
    	COPY_FROM_USER(&tmp.modtime, &times->modtime, sizeof(time_t));
    	ptr = &tmp;
	}
	else
	{
	    ptr = NULL;
	}

    return do_utime(AT_FDCWD, filename, ptr);
}


/*
 * Handler for syscall utimes().
 */
int syscall_utimes(char *filename, struct timeval *times)
{
    return syscall_futimesat(AT_FDCWD, filename, times);
}


/*
 * Handler for syscall futimesat().
 *
 * Lazily, we just convert the times to seconds and call utime().
 *
 * TODO: Fix this!
 */

int syscall_futimesat(int dirfd, char *filename, struct timeval *times)
{
	long atime, mtime;
	struct utimbuf tmp;
    struct utimbuf *ptr;

    if(times)
    {
    	COPY_FROM_USER(&atime, &times[0].tv_sec, sizeof(long));
    	COPY_FROM_USER(&mtime, &times[1].tv_sec, sizeof(long));
    	tmp.actime = atime;
    	tmp.modtime = mtime;
    	ptr = &tmp;
	}
	else
	{
	    ptr = NULL;
	}

    return do_utime(dirfd, filename, ptr);
}

