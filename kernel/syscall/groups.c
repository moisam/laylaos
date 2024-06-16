/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: groups.c
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
 *  \file groups.c
 *
 *  Functions for getting and setting user groups.
 */

#include <errno.h>
#include <unistd.h>
#include <grp.h>
#include <sys/types.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/syscall.h>


/*
 * Check group permissions.
 */
int gid_perm(gid_t gid, int use_rgid)
{
    struct task_t *ct = cur_task;
    gid_t mygid = use_rgid ? ct->gid : ct->egid;
    
    // check for invalid gid
    if(gid == (gid_t)-1)
    {
        return 0;
    }
    
    if(mygid == gid)
    {
        return 1;
    }
    
    /* check for supplementary group IDs as well */
    int i;

    for(i = 0; i < NGROUPS_MAX; i++)
    {
        if(ct->extra_groups[i] == gid)
        {
            return 1;
        }
    }
    
    return 0;
}


/*
 * get/set list of supplementary group IDs.
 *
 * NOTE: supplementary gids that are not used should be
 *       set to -1 (not 0) as zero is the root gid.
 *
 * See: https://man7.org/linux/man-pages/man2/getgroups.2.html
 */

/*
 * Handler for syscall getgroups().
 */
int syscall_getgroups(int gidsetsize, gid_t grouplist[])
{
    int count = 0, i;
    struct task_t *ct = cur_task;

    for(i = 0; i < NGROUPS_MAX; i++)
    {
        if(ct->extra_groups[i] != (gid_t)-1)
        {
            count++;
        }
    }
    
    if(gidsetsize == 0)
    {
        return count;
    }
    
    if(gidsetsize < 0 || gidsetsize < count)
    {
        return -EINVAL;
    }
    
    gid_t list[count];
    count = 0;

    for(i = 0; i < NGROUPS_MAX; i++)
    {
        if(ct->extra_groups[i] != (gid_t)-1)
        {
            list[count++] = ct->extra_groups[i];
        }
    }
    
    if(!grouplist)
    {
        return -EFAULT;
    }

    if(count)
    {
        COPY_TO_USER(grouplist, list, sizeof(gid_t) * count);
    }
    
    return count;
}


/*
 * Handler for syscall setgroups().
 */
int syscall_setgroups(int ngroups, gid_t grouplist[])
{
    int i;
    register struct task_t *thread, *ct = cur_task;

    if(ngroups < 0 || ngroups > NGROUPS_MAX)
    {
        return -EINVAL;
    }
    
    if(!suser(ct))
    {
        return -EPERM;
    }
    
    gid_t list[ngroups];
    COPY_FROM_USER(list, grouplist, sizeof(gid_t) * ngroups);
    
    kernel_mutex_lock(&(ct->threads->mutex));

    for_each_thread(thread, ct)
    {
        /* wipe out current supplementary group information */
        for(i = 0; i < NGROUPS_MAX; i++)
        {
            thread->extra_groups[i] = (gid_t)-1;
        }

        /*
         * A process can drop all of its supplementary groups with the call:
         *     setgroups(0, NULL);
         */
        if(!grouplist)
        {
            continue;
        }

        /* then add the new information */
        for(i = 0; i < ngroups; i++)
        {
            thread->extra_groups[i] = list[i];
        }
    }

    kernel_mutex_unlock(&(ct->threads->mutex));
    
    return 0;
}

