/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: zero.c
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
 *  \file zero.c
 *
 *  Read and write functions for the character null device (major = 1,
 *  minor = 5).
 */

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/user.h>


/*
 * Read from char device /dev/zero.
 */
ssize_t zerodev_read(dev_t dev, unsigned char *buf, size_t count)
{
    UNUSED(dev);

    size_t res = count;
    unsigned char *p = buf;
    unsigned char z = 0;

    if(!buf)
    {
        return -EINVAL;
    }
    
    while(count-- != 0)
    {
        copy_to_user(p++, &z, 1);
    }
    
    return res;
}


/*
 * Write to char device /dev/zero.
 */
ssize_t zerodev_write(dev_t dev, unsigned char *buf, size_t count)
{
    UNUSED(dev);
    UNUSED(buf);

    return count;
}

