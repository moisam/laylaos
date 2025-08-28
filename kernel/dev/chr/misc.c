/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: misc.c
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
 *  \file misc.c
 *
 *  Read, write, select and poll switch functions for miscellaneous devices
 *  (major = 10).
 */

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>
#include <kernel/loop.h>
#include <kernel/loop_internal.h>

/*
 * Non-serial mice, misc features (major = 10) read/write interface.
 *
 * Currently, we only support /dev/loop-control (minor = 237).
 */

/*
 * Read from a misc device (major = 10).
 */
ssize_t miscdev_read(struct file_t *f, off_t *pos,
                     unsigned char *buf, size_t count, int kernel)
{
    UNUSED(f);
    UNUSED(pos);
    UNUSED(buf);
    UNUSED(count);
    UNUSED(kernel);

    return -EINVAL;
}


/*
 * Write to a misc device (major = 10).
 */
ssize_t miscdev_write(struct file_t *f, off_t *pos,
                      unsigned char *buf, size_t count, int kernel)
{
    UNUSED(f);
    UNUSED(pos);
    UNUSED(buf);
    UNUSED(count);
    UNUSED(kernel);

    return -EINVAL;
}


long miscdev_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel)
{
    int n = MINOR(dev);

    UNUSED(kernel);

    // handle ioctl on the loop-control device
    if(n != 237)
    {
        return -ENODEV;
    }

    switch(cmd)
    {
        case LOOP_CTL_GET_FREE:
            return lodev_first_free();

        case LOOP_CTL_ADD:
            return lodev_add_index((long)arg);

        case LOOP_CTL_REMOVE:
            return lodev_remove_index((long)arg);
    }    

    return -EINVAL;
}


/*
 * Perform a select operation on a misc device (major = 10).
 */
long miscdev_select(struct file_t *f, int which)
{
    UNUSED(f);
    UNUSED(which);

    return 0;
}


/*
 * Perform a poll operation on a misc device (major = 10).
 */
long miscdev_poll(struct file_t *f, struct pollfd *pfd)
{
    UNUSED(f);
    UNUSED(pfd);

    return 0;
}

