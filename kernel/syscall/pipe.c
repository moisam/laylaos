/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: pipe.c
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
 *  \file pipe.c
 *
 *  Functions for creating pipes. Two file descriptors are returned: the
 *  first is used to read from the pipe, while the second is used to write
 *  to the pipe.
 */

#include <errno.h>
#include <fcntl.h>
#include <kernel/mutex.h>
#include <kernel/syscall.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <fs/pipefs.h>


/*
 * Handler for syscall pipe2().
 */
int syscall_pipe2(int *fildes, int flags)
{
    struct task_t *t = cur_task;
    struct fs_node_t *node;
    struct file_t *f[2];
    int fd[2];
    int i, j = 0;
    
    // TODO: add support for this flag
    if(flags & O_DIRECT)
    {
        return -EINVAL;
    }

    // try to find 2 fds in the master file table
    for(i = 0; j < 2 && i < NR_FILE; i++)
    {
    	kernel_mutex_lock(&ftab[i].lock);
        if(ftab[i].refs == 0)
        {
            f[j++] = &ftab[i];
            ftab[i].refs++;
        }
    	kernel_mutex_unlock(&ftab[i].lock);
    }
    
    // found only 1 - cancel it
    if(j == 1)
    {
        f[0]->refs = 0;
    }
    
    // bail out
    if(j < 2)
    {
        return -ENFILE;
    }
    
    // try to find 2 fds in the task's file table
    for(i = 0, j = 0; j < 2 && i < NR_OPEN; i++)
    {
        if(t->ofiles->ofile[i] == NULL)
        {
            fd[j] = i;
            t->ofiles->ofile[i] = f[j++];
        }
    }
    
    // found only 1 - cancel it
    if(j == 1)
    {
        t->ofiles->ofile[fd[0]] = NULL;
    }
    
    // bail out
    if(j < 2)
    {
        f[0]->refs = 0;
        f[1]->refs = 0;
        return -EMFILE;
    }
    
    if(!(node = pipefs_get_node()))
    {
        t->ofiles->ofile[fd[0]] = NULL;
        t->ofiles->ofile[fd[1]] = NULL;
        f[0]->refs = 0;
        f[1]->refs = 0;
        return -ENFILE;
    }
    
    // set flags
    if(flags & O_CLOEXEC)
    {
        cloexec_set(t, fd[0]);
        cloexec_set(t, fd[1]);
    }

    f[0]->node = node;
    f[1]->node = node;
    f[0]->pos = 0;
    f[1]->pos = 0;
    f[0]->mode = 1;        /* reading end */
    f[0]->flags = O_RDONLY;
    f[1]->mode = 2;        /* writing end */
    f[1]->flags = O_WRONLY;
    
    if(flags & O_NONBLOCK)
    {
        f[0]->flags |= O_NONBLOCK;
        f[1]->flags |= O_NONBLOCK;
    }
    
    COPY_TO_USER(fildes, &fd, 2 * sizeof(int));

    return 0;
}


/*
 * Handler for syscall pipe().
 */
int syscall_pipe(int *fildes)
{
    return syscall_pipe2(fildes, 0);
}

