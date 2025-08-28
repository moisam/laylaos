/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: mem_chr.c
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
 *  \file mem_chr.c
 *
 *  Read and write switch functions for character memory devices (major = 1).
 */

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>
#include <kernel/fcntl.h>
#include <kernel/task.h>

/*
 * Char memory devices (major = 1) read/write interface.
 */

rw_char_t cread[] =
{
    NULL,
    memdev_read,
    kmemdev_read,
    nulldev_read,
    NULL, //ioport_read,
    zerodev_read,
    NULL, //core_read,
    fulldev_read,
    randdev_read,
    uranddev_read,
};

rw_char_t cwrite[] =
{
    NULL,
    memdev_write,
    kmemdev_write,
    nulldev_write,
    NULL, //ioport_write,
    zerodev_write,
    NULL, //core_write,
    fulldev_write,
    randdev_write,
    uranddev_write,
};

#define NCHAR   (int)(sizeof(cwrite)/sizeof(rw_char_t))


/*
 * Read from a memory char device (major = 1).
 */
ssize_t memdev_char_read(struct file_t *f, off_t *pos,
                         unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(kernel);

    dev_t dev = f->node->blocks[0];
    int n = MINOR(dev);
    
    if(n < 1 || n >= NCHAR || !cread[n])
    {
        return -ENODEV;
    }

    /*
    if(!buf)
    {
        return -EINVAL;
    }
    */
    
    return cread[n](dev, buf, count);
}


/*
 * Write to a memory char device (major = 1).
 */
ssize_t memdev_char_write(struct file_t *f, off_t *pos,
                          unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(kernel);

    dev_t dev = f->node->blocks[0];
    int n = MINOR(dev);
    
    if(n < 1 || n >= NCHAR || !cwrite[n])
    {
        return -ENODEV;
    }
    
    /*
    if(!buf)
    {
        return -EINVAL;
    }
    */
    
    return cwrite[n](dev, buf, count);
}


/*
 * Perform a select operation on a memory core device (major = 1).
 */
long memdev_char_select(struct file_t *f, int which)
{
    dev_t dev;
    int n;

    if(!f || !f->node || !S_ISCHR(f->node->mode))
	{
	    return 0;
	}
	
	dev = f->node->blocks[0];
    n = MINOR(dev);
    
    if(MAJOR(dev) != 1 || n < 1 || n >= NCHAR)
    {
        return 0;
    }

    switch(n)
    {
        case 1:         // mem
        case 3:         // null
        case 5:         // zero
        case 8:         // rand
        case 9:         // urand
            return (which == FREAD || which == FWRITE);

        case 2:         // kmem
            return !!suser(this_core->cur_task);

        case 7:         // full
            return 0;
    }

    return 0;
}


/*
 * Perform a poll operation on a memory core device (major = 1).
 */
long memdev_char_poll(struct file_t *f, struct pollfd *pfd)
{
    dev_t dev;
    int n;
    long res = 0;

    if(!f || !f->node || !S_ISCHR(f->node->mode))
	{
	    return 0;
	}
	
	dev = f->node->blocks[0];
    n = MINOR(dev);
    
    if(MAJOR(dev) != 1 || n < 1 || n >= NCHAR)
    {
        return 0;
    }

    switch(n)
    {
        case 2:         // kmem
            if(!suser(this_core->cur_task))
            {
                break;
            }
            /* fallthrough */

        case 1:         // mem
        case 3:         // null
        case 5:         // zero
        case 8:         // rand
        case 9:         // urand
            if(pfd->events & POLLIN)
            {
                pfd->revents |= POLLIN;
                res = 1;
            }

            if(pfd->events & POLLOUT)
            {
                pfd->revents |= POLLOUT;
                res = 1;
            }
            break;

        case 7:         // full
            break;
    }

    return res;
}

