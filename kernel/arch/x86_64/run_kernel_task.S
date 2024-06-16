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

.section .text
.code64
.align 8

/*
 * C declaraction:
 *    void run_kernel_task(void);
 */

.global run_kernel_task

/*
 * Same flag(s) as in kernel_task.c
 */
.set KERNEL_TASK_ELEVATED_PRIORITY, 1


run_kernel_task:
    movabsq $cur_task, %rax
    movq (%rax), %rax
    //call get_cur_task

    xor %rbx, %rbx
    movl 0(%rax), %ebx
    
    //call elevate_priority
    
1:
    // Loop until our parent (init) adds us to the kernel task list.
    // We only do this once, so we can busy loop here.
    movq %rbx, %rdi
    call get_ktask
    test %rax, %rax
    //jz 1b
    jnz 2f

    //call lock_scheduler
    //call scheduler
    //call unlock_scheduler
    jmp 1b

2:

    // Test the kernel task's flags, and elevate its priority if needed.
    movl 24(%rax), %ebx
    andl $KERNEL_TASK_ELEVATED_PRIORITY, %ebx
    test %ebx, %ebx
    jz 3f
    
    // Remember the kernel task pointer
    pushq %rax
    
    // Elevate the priority
    call ktask_elevate_priority
    
    // Now restore the kernel task pointer for the next part
    popq %rax

    
3:
    // Call the kernel task function
    movq 8(%rax), %rbx
    movq 16(%rax), %rdi
    call *%rbx

    // The function returned, which shouldn't happen.
    // Try to get the ktask struct and restart the function.
    movabsq $cur_task, %rax
    movq (%rax), %rax
    //call get_cur_task

    xor %rbx, %rbx
    movl 0(%rax), %ebx
    movq %rbx, %rdi
    call get_ktask
    test %rax, %rax
    jnz 3b
    
    // Couldn't get the ktask struct. Terminate this task.
    movq $1, %rdi
    call terminate_task
    
    // Shouldn't be reached!
    call empty_loop

