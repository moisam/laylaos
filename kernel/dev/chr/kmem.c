/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: kmem.c
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
 *  \file kmem.c
 *
 *  Read and write functions for the character kernel memory device (major = 1,
 *  minor = 2).
 */

#include <string.h>
#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/vfs.h>
#include <kernel/user.h>
#include <mm/mmngr_virtual.h>


/*
 * Read from char device /dev/kmem.
 */
ssize_t kmemdev_read(dev_t dev, unsigned char *buf, size_t count)
{
    UNUSED(dev);
    struct task_t *ct = cur_task;
    
    if(ct->euid != 0)
    {
        return -EPERM;
    }

    if(!buf || count >= kernel_size)
    {
        return -EINVAL;
    }

    void *start = (void *)&kernel_start;
    
    copy_to_user((void *)buf, start, count);

    return count;
}


/*
 * Write to char device /dev/kmem.
 */
ssize_t kmemdev_write(dev_t dev, unsigned char *buf, size_t count)
{
    UNUSED(dev);
    struct task_t *ct = cur_task;
    
    if(ct->euid != 0)
    {
        return -EPERM;
    }

    if(!buf || count >= kernel_size)
    {
        return -EINVAL;
    }
    
    void *start = (void *)&kernel_start;

    /*
     * NOTE: we will do this for now, but we're probably f****d!
     */
    copy_from_user(start, (void *)buf, count);

    return count;
}

