/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: flock.c
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
 *  \file flock.c
 *
 *  Advisory lock interface and handler function for the flock syscall.
 */

#include <errno.h>
#include <fcntl.h>
#include <kernel/task.h>
#include <kernel/fio.h>
#include <kernel/fcntl.h>


/*
 * Handler for syscall flock().
 */
int syscall_flock(int fd, int operation)
{
	struct file_t *fp = NULL;
    struct fs_node_t *node = NULL;
    struct flock lock;
    struct task_t *ct = cur_task;
    int noblock = (operation & LOCK_NB);

    if(fdnode(fd, ct, &fp, &node) != 0)
    {
        return -EBADF;
    }

    operation &= ~LOCK_NB;
    
    if(operation != LOCK_SH && operation != LOCK_EX && operation != LOCK_UN)
    {
        return -EINVAL;
    }

    lock.l_type = (operation == LOCK_SH) ? F_RDLCK :
                  ((operation == LOCK_EX) ? F_WRLCK : F_UNLCK);
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = 0;
    
    return fcntl_setlock(fp, noblock ? F_SETLK : F_SETLKW, &lock);
}

