/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
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
int syscall_fdatasync(int fd)
{
    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    struct cached_page_t *buf;
    size_t i, j;
    int res = 0;
    struct task_t *ct = cur_task;
    
    // sanity checks
    if(fdnode(fd, ct, &f, &node) != 0)
    {
        return -EBADF;
    }

    if(IS_PIPE(node) || IS_SOCKET(node))
    {
        return -EINVAL;
    }

    j = node->size;
    i = 0;

    // try to bmap each block from the file, check to see if the block has
    // already been read by someone (i.e. the block should be available in
    // the block cache), then write the block out to disk
    while(i < j)
    {
        if((buf = get_cached_page(node, i, PCACHE_PEEK_ONLY)))
        {
            if(sync_cached_page(buf) < 0)
            {
                res = -EIO;
            }

            release_cached_page(buf);
        }

        i += PAGE_SIZE;
    }
    
    return res;
}


/*
 * Handler for syscall fsync().
 */
int syscall_fsync(int fd)
{
    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    int res, res2;
    struct task_t *ct = cur_task;

    // sanity checks
    if(fdnode(fd, ct, &f, &node) != 0)
    {
        return -EBADF;
    }

    // sync data
    res = syscall_fdatasync(fd);
    
    // then metadata
    res2 = write_node(node);
    
    return res ? res : res2;
}


/*
 * Handler for syscall sync().
 */
int syscall_sync(void)
{
    // defined in fs/update.c
    update(NODEV);
	return 0;
}


/*
 * Handler for syscall syncfs().
 */
int syscall_syncfs(int fd)
{
	struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    struct task_t *ct = cur_task;

    if(fdnode(fd, ct, &f, &node) != 0)
    {
        return -EBADF;
    }

    // defined in fs/update.c
    update(node->dev);
	return 0;
}

