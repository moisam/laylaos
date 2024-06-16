/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: run_kernel_task.S
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
 *  \file run_kernel_task.S
 *
 *  Starter function that is called when a kernel task is to be started.
 */

/*
 * C declaraction:
 *    void run_kernel_task(void);
 */

.global run_kernel_task

/*
 * Same flag(s) as in kernel_task.c
 */
.set KERNEL_TASK_ELEVATED_PRIORITY,         1


run_kernel_task:
    call get_cur_task
    movl 0(%eax), %ebx
    
    //call elevate_priority
    
1:
    // Loop until our parent (init) adds us to the kernel task list.
    // We only do this once, so we can busy loop here.
    pushl %ebx
    call get_ktask
    popl %ebx
    test %eax, %eax
    jz 1b
    

    // Test the kernel task's flags, and elevate its priority if needed.
    movl 16(%eax), %ebx
    andl $KERNEL_TASK_ELEVATED_PRIORITY, %ebx
    test %ebx, %ebx
    jz 2f
    
    // Remember the kernel task pointer
    pushl %eax
    
    // Elevate the priority
    call elevate_priority
    
    // Now restore the kernel task pointer for the next part
    popl %eax

    
2:
    // Call the kernel task function
    movl 8(%eax), %ebx
    pushl 12(%eax)
    call *%ebx
    popl %eax

    // The function returned, which shouldn't happen.
    // Try to get the ktask struct and restart the function.
    call get_cur_task
    movl 0(%eax), %ebx
    pushl %ebx
    call get_ktask
    popl %ebx
    test %eax, %eax
    jnz 2b
    
    // Couldn't get the ktask struct. Terminate this task.
    pushl $1
    call terminate_task
    
    // Shouldn't be reached!
    call empty_loop

