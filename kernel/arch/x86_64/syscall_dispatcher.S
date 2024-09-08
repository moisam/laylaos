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

#include <sys/syscall.h>
#include <bits/syscall.h>        // __NR_* macros
#include "task_offsets.h"
#include "macros.S"


// defined in switch_task.S
.extern unlock_scheduler_no_sti


.section .text
.code64
.align 8


.global do_user_sighandler
.global syscall_entry64
.global syscall_entry
.global resume_user
.global enter_user


/*
 * Fast system call entry point.
 *
 * See: https://www.felixcloutier.com/x86/syscall.html
 *      https://blog.packagecloud.io/the-definitive-guide-to-linux-system-calls/
 *      https://x86.lol/generic/2019/07/04/kernel-entry.html
 *
 * Registers are set up as follows on syscall entry:
 *      %rax    syscall number
 *      %rcx    return RIP
 *      %r11    RFLAGS
 *      %rdi    arg1
 *      %rsi    arg2
 *      %rdx    arg3
 *      %r10    arg4
 *      %r8     arg5
 *      %r9     arg6
 */
syscall_entry64:
    cli
    swapgs

    //xchg %bx, %bx
    mov %rsp, %r9

    // move to the task's kernel stack, using ESP0 field of the TSS
    movabsq $tss_entry, %rsp
    movq 4(%rsp), %rsp
    
    // push the values the processor usually pushes on an interrupt
    pushq $0x23     // SS
    pushq %r9       // USERRSP
    pushq %r11      // RFLAGS
    pushq $0x1B     // CS
    pushq %rcx      // RIP

    pushq $0
    pushq $0
    PUSHAQ
    mov %rsp, %rdi
    call syscall_dispatcher

    cli
    POPAQ
    add $16, %rsp   // Clean up the pushed error code and pushed ISR number
    
    // get our pushed values back
    popq %rcx       // RIP
    add $8, %rsp    // discard CS
    popq %r11       // RFLAGS
    popq %rsp       // USERRSP
                    // we will ignore the SS we pushed above

    swapgs
    sysretq


/*
 * Traditional (old) system call entry point.
 */
/*
syscall_entry:
    # push dummy into_no and err_code to fit our struct regs

    xchg %bx, %bx

    cli
    #xchg %bx, %bx
    #cmpq $21, %rax
    #jne 1f
    #xchg %bx, %bx
1:
    pushq $0
    pushq $0
    _SWAPGS
    PUSHAQ

    mov %rsp, %rdi
    call syscall_dispatcher

    POPAQ
    _SWAPGS

    #xchg %bx, %bx
    add $16, %rsp     # Clean up the pushed error code and pushed ISR number
    iretq             # pops 5 things at once: CS, RIP, RFLAGS, SS, and RSP!
*/


/*
 * Called from kfork().
 * When the new task runs, we need to pretend as if we are coming back from
 * a call to preempt() or scheduler(), and therefore unlock the scheduler,
 * but we don't want to set interrupts yet.
 */
resume_user:
    cli
    call unlock_scheduler_no_sti

    // if user task, release the 'running in kernel mode' flag
    movabsq $cur_task, %rdi
    movq (%rdi), %rdi
    call unset_syscall_flags
    
    // don't popa as we want to preserve our esp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    addq $8, %rsp
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    
    /*
     * Go out this way, instead of via sysret, as the first fork (init) will
     * not work in the initial phase (while we are still in kernel land) if
     * we switch to usermode CS (which is done implicitly in sysret).
     */
    swapgs
    iretq            // pops 5 things at once: CS, RIP, RFLAGS, SS, and RSP!

    /*
    popq %rcx       # RIP
    add $8, %rsp    # discard CS
    popq %r11       # RFLAGS
    popq %rsp       # USERRSP
    swapgs
    sysretq
    */


/*
 * Called from syscall_execve().
 * Here we manipulate our kernel stack to fool the processor into
 * jumping "back" to our new executable's entry point.
 */
enter_user:
    cli

    movabsq $cur_task, %rax
    movq (%rax), %rax

    leaq TASK_EXECVE_REGS(%rax), %rax
    movq (%rax), %rbp
    movq 8(%rax), %rdi
    movq 16(%rax), %rsi
    movq 24(%rax), %rdx
    movq 32(%rax), %r8

    movq 40(%rax), %rcx     // RIP
    movq $0x200, %r11       // RFLAGS
    movq 48(%rax), %rsp     // USERRSP

    swapgs
    sysretq


/*
 * Call a user-space signal handler.
 *
 * NOTE: Should be called with interrupts disabled!
 */

do_user_sighandler:
    movq %rsi, %rcx     // RIP
    movq $0x200, %r11   // RFLAGS = enable interrupts
    movq %rdi, %rsp     // USERRSP

    // get the handler's arguments from the stack
    movq 24(%rdi), %rdx // arg3
    movq 16(%rdi), %rsi // arg2
    movq 8(%rdi), %rdi  // arg1

    swapgs
    sysretq

