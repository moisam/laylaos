/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: fsync.c
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
 *  \file fsync.c
 *
 *  Functions for synchronizing files and filesystem data.
 */

#include <errno.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/fio.h>
#include <kernel/pcache.h>


/*
 * Handler for syscall fdatasync().
 */
long syscall_fdatasync(int fd)
{
    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    
    // sanity checks
    if(fdnode(fd, this_core->cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

    if(IS_PIPE(node) || IS_SOCKET(node))
    {
        return -EINVAL;
    }

    return vfs_fdatasync(node);
}


/*
 * Handler for syscall fsync().
 */
long syscall_fsync(int fd)
{
    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;

    // sanity checks
    if(fdnode(fd, this_core->cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

    return vfs_fsync(node);
}


/*
 * Handler for syscall sync().
 */
long syscall_sync(void)
{
    remove_unreferenced_cached_pages(NULL);
    // defined in fs/update.c
    update(NODEV);
	return 0;
}


/*
 * Handler for syscall syncfs().
 */
long syscall_syncfs(int fd)
{
	struct file_t *f = NULL;
    struct fs_node_t *node = NULL;

    if(fdnode(fd, this_core->cur_task, &f, &node) != 0)
    {
        return -EBADF;
    }

    remove_unreferenced_cached_pages(NULL);
    // defined in fs/update.c
    update(node->dev);
	return 0;
}

