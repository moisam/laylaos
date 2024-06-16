/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: sysinfo.c
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
 *  \file sysinfo.c
 *
 *  Get system info.
 */

#include <string.h>
#include <sys/sysinfo.h>
#include <kernel/task.h>
#include <kernel/clock.h>
#include <kernel/user.h>
#include <kernel/pcache.h>
#include <mm/mmngr_phys.h>


/*
 * Handler for syscall sysinfo().
 */
int syscall_sysinfo(struct sysinfo *info)
{
    struct sysinfo tmp;
    
    A_memset(&tmp, 0, sizeof(struct sysinfo));
    
    /*
     * TODO: fill the fields for system load, shared ram, total/free swap,
     *       total/free high memory.
     */
    tmp.uptime = monotonic_time.tv_sec;
    tmp.totalram = pmmngr_get_block_count();
    tmp.freeram = pmmngr_get_free_block_count();
    tmp.bufferram = get_cached_page_count() + get_cached_block_count();
    tmp.procs = total_tasks;
    tmp.mem_unit = PAGE_SIZE;
    
    return copy_to_user(info, &tmp, sizeof(struct sysinfo));
}

