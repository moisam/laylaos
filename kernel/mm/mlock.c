/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: mlock.c
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
 *  \file mlock.c
 *
 *  Memory lock implementation.
 *
 *  Currently, we indicate a locked segment of memory by setting the
 *  MEMREGION_FLAG_STICKY_BIT flag on the containing memregion. This means we
 *  can only lock whole memregions. It also means mlock2() works exactly
 *  like mlock() for now, i.e. the MLOCK_ONFAULT flag has no effect.
 *
 *  See: https://man7.org/linux/man-pages/man2/mlock.2.html
 */

#include <errno.h>
#define __USE_GNU           // for MLOCK_ONFAULT
#define _GNU_SOURCE
#include <sys/mman.h>
#include <mm/memregion.h>
#include <mm/mmap.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>


static int update_mlock(void *addr, size_t len, int unlock)
{
    virtual_addr aligned_addr, aligned_size, end;
    struct memregion_t *memregion;
    struct task_t *ct = cur_task;
    
    aligned_addr = align_down((virtual_addr)addr);
    aligned_size = align_up((virtual_addr)len);
    end = aligned_addr + aligned_size;

    // check we're not trying to unmap kernel memory
    if(((size_t)aligned_addr >= USER_MEM_END) ||
                (end > USER_MEM_END))
    {
        return -EINVAL;
    }

    if((memregion = memregion_containing(ct, aligned_addr)) == NULL)
    {
        return -ENOMEM;
    }
    
    if(unlock)
    {
        memregion->flags &= ~MEMREGION_FLAG_STICKY_BIT;
    }
    else
    {
        memregion->flags |= MEMREGION_FLAG_STICKY_BIT;
    }

    return 0;
}


/*
 * Handler for syscall mlock().
 */
int syscall_mlock(void *addr, size_t len)
{
    return update_mlock(addr, len, 0);
}


/*
 * Handler for syscall mlock2().
 */
int syscall_mlock2(void *addr, size_t len, unsigned int flags)
{
    // we check flags though we don't use them yet
    if(flags & ~MLOCK_ONFAULT)
    {
        return -EINVAL;
    }

    return syscall_mlock(addr, len);
}


/*
 * Handler for syscall munlock().
 */
int syscall_munlock(void *addr, size_t len)
{
    return update_mlock(addr, len, 1);
}


int update_mlockall(int unlock)
{
    register struct memregion_t *memregion;
    struct task_t *ct = cur_task;

    kernel_mutex_lock(&(ct->mem->mutex));

    for(memregion = ct->mem->first_region;
        memregion != NULL;
        memregion = memregion->next)
    {
        if(unlock)
        {
            memregion->flags &= ~MEMREGION_FLAG_STICKY_BIT;
        }
        else
        {
            memregion->flags |= MEMREGION_FLAG_STICKY_BIT;
        }
    }

    kernel_mutex_unlock(&(ct->mem->mutex));

    return 0;
}


#define VALID_FLAGS         (MCL_CURRENT | MCL_FUTURE | MCL_ONFAULT)

/*
 * Handler for syscall mlockall().
 */
int syscall_mlockall(int flags)
{
    if(flags & ~VALID_FLAGS)
    {
        return -EINVAL;
    }

    return update_mlockall(0);
}

#undef VALID_FLAGS


/*
 * Handler for syscall munlockall().
 */
int syscall_munlockall(void)
{
    return update_mlockall(1);
}

