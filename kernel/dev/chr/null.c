/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: null.c
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
 *  \file null.c
 *
 *  Read and write functions for the character null device (major = 1,
 *  minor = 3).
 */

#include <kernel/laylaos.h>
#include <kernel/vfs.h>


/*
 * Read from char device /dev/null.
 */
ssize_t nulldev_read(dev_t dev, unsigned char *buf, size_t count)
{
    UNUSED(dev);
    UNUSED(buf);
    UNUSED(count);

    return 0;
}


/*
 * Write to char device /dev/null.
 */
ssize_t nulldev_write(dev_t dev, unsigned char *buf, size_t count)
{
    UNUSED(dev);
    UNUSED(buf);
    
    return count;
}

