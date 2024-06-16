/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: syscall_dispatcher.S
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
 *  \file syscall_dispatcher.S
 *
 *  The main entry point for the syscall dispatcher. The assembly entry point
 *  hands over to the "proper" C code dispatcher. The file also contains the
 *  following functions:
 *  - resume_user(): called during fork
 *  - enter_user(): called during execve
 *  - do_user_sighandler(): called when executing a user signal handler.
 */

#include <sys/siglist.h>
#include <sys/errnolist.h>
#include <sys/syscall.h>
#include "task_offsets.h"


.global do_user_sighandler
.global syscall_entry
.global resume_user
.global enter_user


syscall_entry:
    // push dummy into_no and err_code to fit our struct regs
    pushl $0
    pushl $0

    pusha
    push %ds
    push %es
    push %fs
    push %gs

    mov %esp, %eax   // push &r on the stack
    push %eax

    /*
    movl $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %gs
    mov %ax, %fs
    */
    
    call syscall_dispatcher

    add $4, %esp    // pop &r
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa

    add $8, %esp
    
    iret           // pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP!


resume_user:

    // When the new task runs, we need to pretend as if we are coming back from
    // a call to preempt() or scheduler(), and therefore unlock the scheduler

    call unlock_scheduler

    /* if user task, release the 'running in kernel mode' flag */
    //cli
    movl (cur_task), %eax
    pushl %eax
    call unset_syscall_flags
    popl %eax

    pop %gs
    pop %fs
    pop %es
    pop %ds
    
    // don't popa as we want to preserve our esp
    popl %edi
    popl %esi
    popl %ebp
    add $4, %esp
    popl %ebx
    popl %edx
    popl %ecx
    popl %eax

    add $8, %esp
    
    iret           // pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP!


enter_user:
    /*
     * here we manipulate our kernel stack to fool the processor into
     * jumping "back" to our new executable's entry point.
     */
    cli

    mov $0x23, %ax		// user mode data selector is 0x20 + RPL 3
    mov %ax, %ds
    mov %ax, %es
    //mov %ax, %fs
    //mov %ax, %gs

    movw $0x33, %ax
    movw %ax, %fs
    movw %ax, %gs

    // create stack frame
    movl (cur_task), %eax
    movl TASK_USERESP_FIELD(%eax), %ebx
    pushl $0x23		    // SS
    pushl %ebx			// ESP
    movl $0x200, %ebx   // enable interrupts
    pushl %ebx
    pushl $0x1B		    // CS user mode code selector is 0x18 + RPL 3

    // load entry address into EIP
    movl TASK_EIP_FIELD(%eax), %ebx
    pushl %ebx
    
    iret


do_user_sighandler:
    cli
    movw $0x23, %ax		// user mode data selector is 0x20 + RPL 3
    movw %ax, %ds
    movw %ax, %es

    movw $0x33, %ax
    movw %ax, %fs
    movw %ax, %gs

    // create stack frame
    movl %esp, %eax
    pushl $0x23		    // SS
    pushl 4(%eax)		// ESP
    pushl $0x200        // enable interrupts
    pushl $0x1B		    // CS user mode code selector is 0x18 + RPL 3
    pushl 8(%eax)       // EIP
    
    iret                // pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP!

