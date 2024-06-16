/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: input.c
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
 *  \file input.c
 *
 *  Read, write, select and poll switch functions for input core devices
 *  (major = 13).
 */

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>

/*
 * Input core devices (major = 13) read/write interface.
 */

rw_char_t inputread[] =
{
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    mousedev_read,  // 32 = mouse0 = first mouse
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    //guiev_read,     // 64 = guiev = GUI event queue
};

rw_char_t inputwrite[] =
{
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL,   // mouse_write,    // 32 = mouse0 = first mouse
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    //guiev_write,    // 64 = guiev = GUI event queue
};

/*

int (*ioctl_func)(dev_t dev, unsigned int cmd, char *arg);

ioctl_func inputioctl[] =
{
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    mouse_ioctl,    // 32 = mouse0 = first mouse
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL,
};

*/

typedef int (*select_func)(dev_t dev, int which);
typedef int (*poll_func)(dev_t dev, struct pollfd *pfd);

select_func inputselect[] =
{
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    mousedev_select,    // 32 = mouse0 = first mouse
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    //guiev_select,       // 64 = guiev = GUI event queue
};

poll_func inputpoll[] =
{
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    mousedev_poll,      // 32 = mouse0 = first mouse
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    //guiev_poll,         // 64 = guiev = GUI event queue
};

#define NCHAR   (int)(sizeof(inputwrite)/sizeof(rw_char_t))


/*
 * Read from an input core device (major = 13).
 */
ssize_t inputdev_read(struct file_t *f, off_t *pos,
                      unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(kernel);

    dev_t dev = f->node->blocks[0];
    int n = MINOR(dev);
    
    if(n < 0 || n >= NCHAR || !inputread[n])
    {
        return -ENODEV;
    }

    /*
    if(!buf)
    {
        return -EINVAL;
    }
    */
    
    return inputread[n](dev, buf, count);
}


/*
 * Write to an input core device (major = 13).
 */
ssize_t inputdev_write(struct file_t *f, off_t *pos,
                       unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(kernel);

    dev_t dev = f->node->blocks[0];
    int n = MINOR(dev);
    
    if(n < 0 || n >= NCHAR || !inputwrite[n])
    {
        return -ENODEV;
    }
    
    /*
    if(!buf)
    {
        return -EINVAL;
    }
    */
    
    return inputwrite[n](dev, buf, count);
}


/*

int inputdev_ioctl(dev_t dev, unsigned int cmd, char *arg)
{
    int n = MINOR(dev);
    
    if(n < 0 || n >= NCHAR || !inputioctl[n])
    {
        return -ENODEV;
    }
    
    if(!buf)
    {
        return -EINVAL;
    }
    
    return inputioctl[n](dev, cmd, arg);
}

*/


/*
 * Perform a select operation on an input core device (major = 13).
 */
int inputdev_select(struct file_t *f, int which)
{
    dev_t dev;
    int n;

    if(!f || !f->node)
    {
        return 0;
    }
    
	if(!S_ISCHR(f->node->mode))
	{
	    return 0;
	}
	
	dev = f->node->blocks[0];
    n = MINOR(dev);
    
    if(n < 0 || n >= NCHAR || !inputselect[n])
    {
        return 0;
    }
    
    return inputselect[n](dev, which);
}


/*
 * Perform a poll operation on an input core device (major = 13).
 */
int inputdev_poll(struct file_t *f, struct pollfd *pfd)
{
    dev_t dev;
    int n;

    if(!f || !f->node)
    {
        return 0;
    }
    
	if(!S_ISCHR(f->node->mode))
	{
	    return 0;
	}
	
	dev = f->node->blocks[0];
    n = MINOR(dev);
    
    if(n < 0 || n >= NCHAR || !inputpoll[n])
    {
        return 0;
    }
    
    return inputpoll[n](dev, pfd);
}

