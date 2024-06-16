/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: access.c
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
 *  \file access.c
 *
 *  Functions for checking file access permissions.
 */

#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <kernel/syscall.h>


/*
 * Handler for syscall access().
 */
int syscall_access(char *filename, int mode)
{
    return syscall_faccessat(AT_FDCWD, filename, mode, 0);
}


#define VALID_FLAGS         (AT_EACCESS | AT_SYMLINK_NOFOLLOW)

/*
 * Handler for syscall faccessat().
 *
 * See:
 *    https://man7.org/linux/man-pages/man2/access.2.html
 */
int syscall_faccessat(int dirfd, char *filename, int mode, int flags)
{
	struct fs_node_t *node = NULL;
	int res, perm = 0;
	int followlink = !(flags & AT_SYMLINK_NOFOLLOW);
	int use_ruid = !(flags & AT_EACCESS);
	int open_flags = OPEN_USER_CALLER |
	               (followlink ? OPEN_FOLLOW_SYMLINK : OPEN_NOFOLLOW_SYMLINK);

	mode &= 0777;

    // check for unknown flags
    if(flags & ~VALID_FLAGS)
    {
        return -EINVAL;
    }

	if((res = vfs_open_internal(filename, dirfd, &node, open_flags)) < 0)
	{
		return res;
	}

    if(!node)
    {
        return -ENOENT;
    }
    
    // check only for file existence
    if(mode == F_OK)
    {
    	release_node(node);
    	return 0;
    }
    
    if(mode & R_OK)
    {
        perm |= READ;
    }

    if(mode & W_OK)
    {
        perm |= WRITE;
    }

    if(mode & X_OK)
    {
        perm |= EXECUTE;
    }
    
    if(perm == 0)
    {
    	release_node(node);
        return -EINVAL;
    }
    
    /*
     * TODO: we should handle the case where write access is requested for
     *       an executable that is being executed, in which case we should
     *       return -ETXTBSY.
     */

    res = has_access(node, perm, use_ruid);
	release_node(node);
		
	return res;
}

