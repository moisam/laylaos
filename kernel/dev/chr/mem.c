/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: mem.c
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
 *  \file mem.c
 *
 *  Read and write functions for the character memory device (major = 1,
 *  minor = 1).
 */

#include <string.h>
#include <errno.h>
#include <kernel/task.h>
#include <kernel/vfs.h>
#include <kernel/user.h>
#include <mm/mmngr_virtual.h>


/* defined in paging.S */
extern void enable_paging(void);
extern void disable_paging(void);


/*
 * Read from char device /dev/mem.
 */
ssize_t memdev_read(dev_t dev, unsigned char *buf, size_t count)
{
    UNUSED(dev);

    if(this_core->cur_task->euid != 0)
    {
        return -EPERM;
    }

    unsigned char *addr = (unsigned char *)get_phys_addr((virtual_addr)buf);
    unsigned char *addr2 = 0;
    size_t i = count;
    
    if(!buf || !addr)
    {
        return -EINVAL;
    }
    
    disable_paging();
    
    /* TODO: we are reading from physical address 0. is it correct?? */
    //memcpy((void *)addr, (void *)addr2, count);
    while(i-- != 0)
    {
        *addr++ = *addr2++;
    }
    
    enable_paging();

    return count;
}


/*
 * Write to char device /dev/mem.
 */
ssize_t memdev_write(dev_t dev, unsigned char *buf, size_t count)
{
    UNUSED(dev);
    
    if(this_core->cur_task->euid != 0)
    {
        return -EPERM;
    }

    unsigned char *addr = (unsigned char *)get_phys_addr((virtual_addr)buf);
    unsigned char *addr2 = 0;
    size_t i = count;
    
    if(!buf || !addr)
    {
        return -EINVAL;
    }
    
    disable_paging();

    /* TODO: we are writing to physical address 0. is it correct?? */
    //memcpy((void *)addr2, (void *)addr, count);
    while(i-- != 0)
    {
        *addr2++ = *addr++;
    }

    enable_paging();

    return count;
}

