/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: chown.c
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
 *  \file chown.c
 *
 *  Functions for changing file ownership.
 */

#include <errno.h>
#include <fcntl.h>              // AT_SYMLINK_NOFOLLOW
#include <sys/types.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/fio.h>


static void clear_exec_bit(struct fs_node_t *node)
{
    /* 
     * mimic Linux behaviour here: clear the sticky bits for group-executable
     * files. if it is not a group-executable, the S_ISGID bit indicates 
     * mandatory locking, and is not cleared.
     */
    if((node->mode & S_IXGRP) == 0)
    {
        node->mode &= ~(S_ISUID);
    }
    else
    {
        node->mode &= ~(S_ISUID|S_ISGID);
    }
}


static long do_chown(struct fs_node_t *node, uid_t uid, gid_t gid)
{
    struct mount_info_t *dinfo;

	if(!node)
	{
	    return -EBADF;
	}

    // can't chown if the filesystem was mount readonly
    if((dinfo = get_mount_info(node->dev)) && (dinfo->mountflags & MS_RDONLY))
    {
        return -EROFS;
    }

    if(uid != (uid_t)-1)
    {
    	if(!suser(this_core->cur_task))
    	{
    	    // regular user -- only root can chown a file
    		return -EPERM;
    	}

	    node->uid = uid;
        clear_exec_bit(node);
    	node->flags |= FS_NODE_DIRTY;
	}

    if(gid == (gid_t)-1)
    {
    	return 0;
    }

   	// root can change the group to anything, while regular users can only
   	// change to a group they are a member of
  	if(!suser(this_core->cur_task))
   	{
        // not the file owner?
        if(this_core->cur_task->euid != node->uid)
        {
            return -EPERM;
        }
        
        if(!gid_perm(node->gid, 0))
        {
            return -EPERM;
        }
   	}

    node->gid = gid;
    clear_exec_bit(node);
	node->flags |= FS_NODE_DIRTY;

	return 0;
}


/*
 * Handler for syscall chown().
 */
long syscall_chown(char *filename, uid_t uid, gid_t gid)
{
    return syscall_fchownat(AT_FDCWD, filename, uid, gid, 0);
}


/*
 * Handler for syscall lchown().
 */
long syscall_lchown(char *filename, uid_t uid, gid_t gid)
{
    return syscall_fchownat(AT_FDCWD, filename, uid, gid, 
                                        AT_SYMLINK_NOFOLLOW);
}


/*
 * Handler for syscall fchown().
 */
long syscall_fchown(int fd, uid_t uid, gid_t gid)
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

	return do_chown(node, uid, gid);
}


#define VALID_FLAGS         AT_SYMLINK_NOFOLLOW

/*
 * Handler for syscall fchownat().
 */
long syscall_fchownat(int dirfd, char *filename, uid_t uid, gid_t gid, 
                      int flags)
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

	res = do_chown(node, uid, gid);
	release_node(node);

	return res;
}

