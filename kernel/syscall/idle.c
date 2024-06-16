/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: idle.c
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
 *  \file idle.c
 *
 *  Idle task function.
 */

#include <errno.h>
#include <kernel/asm.h>
#include <kernel/task.h>
#include <kernel/dev.h>


/*
 * Handler for syscall idle().
 *
 * Run the idle task. Only task #0 can call this function, which doesn't
 * return. Everyone else gets -EPERM.
 *
 * Returns:
 *    0 on success, -errno on failure
 */
int syscall_idle(void)
{
    struct task_t *ct = cur_task;
    
    if(ct->pid)
    {
        return -EPERM;
    }

idle_loop:
    sti();
    hlt();
    goto idle_loop;
}

