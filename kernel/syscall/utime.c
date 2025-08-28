/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
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


static long do_utime(int dirfd, char *filename, 
                     struct timespec *times0, struct timespec *times1,
                     int flags)
{
    struct mount_info_t *dinfo;
	struct fs_node_t *node;
	long res;
	long atime, mtime;
	int follow_symlink = (flags & OPEN_FOLLOW_SYMLINK);
	int open_flags = OPEN_USER_CALLER | follow_symlink;
	volatile struct task_t *ct = this_core->cur_task;

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
    
        if(!(node = get_node(node->dev, node->inode, GETNODE_FOLLOW_MPOINTS)))
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

    // check permissions
   	if(!suser(ct) && ct->euid != node->uid)
   	{
       	release_node(node);
   		return (!times0 && !times1) ? -EACCES : -EPERM;
    }

	if(times0 && times1)
	{
	    atime = times0->tv_sec;
	    mtime = times1->tv_sec;
	}
	else if(!times0 && !times1)
	{
		atime = mtime = now();
	}
	else
	{
	    atime = times0 ? times0->tv_sec : node->atime;
	    mtime = times1 ? times1->tv_sec : node->mtime;
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
long syscall_utime(char *filename, struct utimbuf *__times)
{
	time_t atime, mtime;
	struct timespec times[2];
    
    if(__times)
    {
    	COPY_FROM_USER(&atime, &__times->actime, sizeof(time_t));
    	COPY_FROM_USER(&mtime, &__times->modtime, sizeof(time_t));
    	times[0].tv_sec = atime;
    	times[1].tv_sec = mtime;
    	times[0].tv_nsec = 0;
    	times[1].tv_nsec = 0;

        return do_utime(AT_FDCWD, filename, &times[0], &times[1], OPEN_FOLLOW_SYMLINK);
	}
	else
	{
        return do_utime(AT_FDCWD, filename, NULL, NULL, OPEN_FOLLOW_SYMLINK);
	}
}


/*
 * Handler for syscall utimes().
 */
long syscall_utimes(char *filename, struct timeval *times)
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

long syscall_futimesat(int dirfd, char *filename, struct timeval *__times)
{
	long atime, mtime;
	struct timespec times[2];

    if(__times)
    {
    	COPY_FROM_USER(&atime, &__times[0].tv_sec, sizeof(long));
    	COPY_FROM_USER(&mtime, &__times[1].tv_sec, sizeof(long));
    	times[0].tv_sec = atime;
    	times[1].tv_sec = mtime;
    	times[0].tv_nsec = 0;
    	times[1].tv_nsec = 0;

        return do_utime(dirfd, filename, &times[0], &times[1], OPEN_FOLLOW_SYMLINK);
	}
	else
	{
        return do_utime(dirfd, filename, NULL, NULL, OPEN_FOLLOW_SYMLINK);
	}
}


/*
 * Handler for syscall futimesat().
 *
 * Lazily, we just convert the times to seconds and call utime().
 *
 * TODO: Fix this!
 */

long syscall_utimensat(int dirfd, char *filename, struct timespec *__times, int __flags)
{
	struct timespec times[2];
    time_t i = now();
    int flags = (__flags & AT_SYMLINK_NOFOLLOW) ?
                    OPEN_NOFOLLOW_SYMLINK : OPEN_FOLLOW_SYMLINK;

    if(__times)
    {
    	COPY_FROM_USER(&times, __times, sizeof(struct timespec) * 2);

        if(times[0].tv_nsec == UTIME_OMIT && times[1].tv_nsec == UTIME_OMIT)
        {
            return 0;
        }

        if(times[0].tv_nsec == UTIME_NOW && times[1].tv_nsec == UTIME_NOW)
        {
            return do_utime(dirfd, filename, NULL, NULL, flags);
        }

        if(times[0].tv_nsec == UTIME_OMIT)
        {
        	if(times[1].tv_nsec == UTIME_NOW)
        	{
                times[1].tv_sec = i;
                times[1].tv_nsec = 0;
            }

            return do_utime(dirfd, filename, NULL, &times[1], flags);
        }

        if(times[1].tv_nsec == UTIME_OMIT)
        {
        	if(times[0].tv_nsec == UTIME_NOW)
        	{
                times[0].tv_sec = i;
                times[0].tv_nsec = 0;
            }

            return do_utime(dirfd, filename, &times[0], NULL, flags);
        }

        return do_utime(dirfd, filename, &times[0], &times[1], flags);
	}
	else
	{
        return do_utime(dirfd, filename, NULL, NULL, flags);
	}
}

