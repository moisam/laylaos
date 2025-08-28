/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: chmod.c
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
 *  \file chmod.c
 *
 *  Functions for changing file access permissions.
 */

#include <errno.h>
#include <fcntl.h>              // AT_SYMLINK_NOFOLLOW
#include <sys/types.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/fio.h>


static long do_chmod(struct fs_node_t *node, mode_t mode)
{
    struct mount_info_t *dinfo;

	if(!node)
	{
	    return -EBADF;
	}


    // can't chmod if the filesystem was mount readonly
    if((dinfo = get_mount_info(node->dev)) && (dinfo->mountflags & MS_RDONLY))
    {
        return -EROFS;
    }

	if(!suser(this_core->cur_task))
	{
	    // regular user -- check permissions
		if(this_core->cur_task->uid != node->uid && 
		   this_core->cur_task->euid != node->uid)
		{
		    // not your file, can't mess with it!
			return -EPERM;
		}
		else 
		{
		    // change the file's mode
			mode = (mode & 0777) | (node->mode & 07000);
		}

        /* only superuser can set sticky bit on regular files.
         * NOTE: what use we have for the sticky bit on regular files?
         */
        if((mode & S_ISVTX) && (S_ISREG(node->mode)))
        {
            mode &= ~S_ISVTX;
        }

        /* prevent SGID bit set for underprivileged tasks */
        if(mode & S_ISGID)
        {
            if(!gid_perm(node->gid, 0))
            {
                mode &= ~S_ISGID;
            }
        }
	}
	
	node->mode = (mode & 07777) | (node->mode & ~07777);
	node->flags |= FS_NODE_DIRTY;
	
	return 0;
}


/*
 * Handler for syscall chmod().
 */
long syscall_chmod(char *filename, mode_t mode)
{
    return syscall_fchmodat(AT_FDCWD, filename, mode, 0);
}


/*
 * Handler for syscall fchmod().
 */
long syscall_fchmod(int fd, mode_t mode)
{
	struct file_t *f = NULL;
    struct fs_node_t *node = NULL;

    if(fdnode(fd, this_core->cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

    if(f->flags & O_PATH)
    {
        return -EBADF;
    }

	return do_chmod(node, mode);
}


#define VALID_FLAGS         AT_SYMLINK_NOFOLLOW

/*
 * Handler for syscall fchmodat().
 */
long syscall_fchmodat(int dirfd, char *filename, mode_t mode, int flags)
{
    struct fs_node_t *node = NULL;
	long res;
	int followlink = !(flags & AT_SYMLINK_NOFOLLOW);
	int open_flags = OPEN_USER_CALLER |
	               (followlink ? OPEN_FOLLOW_SYMLINK : OPEN_NOFOLLOW_SYMLINK);

    // check for unknown flags
    if(flags & ~VALID_FLAGS)
    {
        return -EINVAL;
    }

    if(!filename)
    {
        return -EFAULT;
    }
    
	if((res = vfs_open_internal(filename, dirfd, &node, open_flags)) < 0)
	{
		return res;
	}

    res = do_chmod(node, mode);
	release_node(node);

	return res;
}

