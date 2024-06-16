/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: dummy.c
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
 *  \file dummy.c
 *
 *  This file implements no-op dummy functions for filesystems that do
 *  not need to implement all the filesystem ops functions.
 */

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/dev.h>
#include <fs/dummy.h>

/*
 * General block device control function.
 */
int dummyfs_ioctl(dev_t dev_id, unsigned int cmd, char *arg, int kernel)
{
    UNUSED(dev_id);
    UNUSED(arg);
    UNUSED(kernel);

    switch(cmd)
    {
        case DEV_IOCTL_GET_BLOCKSIZE:
            // get the block size in bytes
            return 0;
    }
    
    return -EINVAL;
}


/*
 * Perform a dummy select operation.
 */
int dummyfs_select(struct file_t *f, int which)
{
    UNUSED(f);
    UNUSED(which);

	return 1;
}


/*
 * Perform a dummy poll operation.
 */
int dummyfs_poll(struct file_t *f, struct pollfd *pfd)
{
    UNUSED(f);
    //UNUSED(pfd);

    if(pfd->events & POLLIN)
    {
        pfd->revents |= POLLIN;
    }

    if(pfd->events & POLLOUT)
    {
        pfd->revents |= POLLOUT;
    }

	return 1;
}


ssize_t dummyfs_read(struct file_t *f, off_t *pos,
                     unsigned char *buf, size_t count, int kernel)
{
    UNUSED(f);
    UNUSED(pos);
    UNUSED(buf);
    UNUSED(count);
    UNUSED(kernel);

    return -ENODEV;
}


ssize_t dummyfs_write(struct file_t *f, off_t *pos,
                      unsigned char *buf, size_t count, int kernel)
{
    UNUSED(f);
    UNUSED(pos);
    UNUSED(buf);
    UNUSED(count);
    UNUSED(kernel);

    return -ENODEV;
}

