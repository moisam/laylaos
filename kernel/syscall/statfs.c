/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: statfs.c
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
 *  \file statfs.c
 *
 *  Functions for reading filesystem statistics.
 *
 *  See: https://man7.org/linux/man-pages/man2/statfs.2.html
 */

#include <errno.h>
#include <fcntl.h>              // AT_FDCWD
#include <sys/stat.h>
#include <kernel/vfs.h>
#include <kernel/syscall.h>
#include <kernel/user.h>
#include <kernel/task.h>
#include <kernel/fio.h>


static long do_statfs(struct fs_node_t *node, struct statfs *statbuf)
{
	struct statfs tmp;
    struct mount_info_t *d;
    long res;

    // get the device's mount info
    if((d = get_mount_info(node->dev)) == NULL ||
       !d->fs || !d->fs->ops || !d->fs->ops->statfs)
    {
        return -ENOSYS;
    }
    
    if((res = d->fs->ops->statfs(d, &tmp)) < 0)
    {
        return res;
    }

    return copy_to_user(statbuf, &tmp, sizeof(struct statfs));
}


/*
 * Handler for syscall statfs().
 */
long syscall_statfs(char *path, struct statfs *statbuf)
{
    struct fs_node_t *node = NULL;
	long res;
	int open_flags = OPEN_USER_CALLER | OPEN_FOLLOW_SYMLINK;

    if(!path || !statbuf)
    {
        return -EFAULT;
    }

	if((res = vfs_open_internal(path, AT_FDCWD, &node, open_flags)) < 0)
	{
		return res;
	}

	res = do_statfs(node, statbuf);
	release_node(node);
	
	return res;
}


/*
 * Handler for syscall fstatfs().
 */
long syscall_fstatfs(int fd, struct statfs *statbuf)
{
	struct file_t *f = NULL;
    struct fs_node_t *node = NULL;

    if(!statbuf)
    {
        return -EFAULT;
    }

    if(fdnode(fd, this_core->cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }
	
	return do_statfs(node, statbuf);
}


/*
 * Handler for syscall ustat().
 *
 * See: https://man7.org/linux/man-pages/man2/ustat.2.html
 */
long syscall_ustat(dev_t dev, struct ustat *ubuf)
{
    struct mount_info_t *d;
    struct ustat tmp;
    long res;

    if(!ubuf)
    {
        return -EFAULT;
    }

    // get the device's mount info
    if((d = get_mount_info(dev)) == NULL)
    {
        return -EINVAL;
    }
    
    if(!d->fs->ops->ustat)
    {
        return -ENOSYS;
    }
    
    if((res = d->fs->ops->ustat(d, &tmp)) < 0)
    {
        return res;
    }

    tmp.f_fname[0] = '\0';
    tmp.f_fpack[0] = '\0';
    
    return copy_to_user(ubuf, &tmp, sizeof(struct ustat));
}

