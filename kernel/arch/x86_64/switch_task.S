/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: switch_task.S
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
 *  \file switch_task.S
 *
 *  Functions to save and restore context when switching tasks.
 */

#include "task_offsets.h"

.extern tss_entry

.set CR0_TS,        (1 << 3)

.section .text
.code64
.align 8

.global unlock_scheduler_no_sti
.type unlock_scheduler_no_sti, @function

unlock_scheduler_no_sti:

    movabsq $IRQ_disable_counter, %rax
    movl (%rax), %eax
    lea -1(%rax), %edx
    movabsq $IRQ_disable_counter, %rax
    movl %edx, (%rax)
    retq


/*
 * C declaration:
 *   int save_context(struct task_t *task)
 */

.global save_context
.type save_context, @function

/*
 * at the beginning of this function, the stack looks like:
 *
 *  0(%esp) - return address
 */
save_context:
    //xchg %bx, %bx
    //xchg %bx, %bx

    movq %rax, TASK_RAX_FIELD(%rdi)
    //leaq 8(%rsp), %rax
    movq %rsp, %rax
    movq %rax, TASK_RSP_FIELD(%rdi)
    movq %rbp, TASK_RBP_FIELD(%rdi)
    movq (%rsp), %rax
    movq %rax, TASK_RIP_FIELD(%rdi)
    movq %rbx, TASK_RBX_FIELD(%rdi)
    movq %rcx, TASK_RCX_FIELD(%rdi)
    movq %rdx, TASK_RDX_FIELD(%rdi)
    movq %rdi, TASK_RDI_FIELD(%rdi)
    movq %rsi, TASK_RSI_FIELD(%rdi)

    movq %r8, TASK_R8_FIELD(%rdi)
    movq %r9, TASK_R9_FIELD(%rdi)
    movq %r10, TASK_R10_FIELD(%rdi)
    movq %r11, TASK_R11_FIELD(%rdi)
    movq %r12, TASK_R12_FIELD(%rdi)
    movq %r13, TASK_R13_FIELD(%rdi)
    movq %r14, TASK_R14_FIELD(%rdi)
    movq %r15, TASK_R15_FIELD(%rdi)

    pushfq
    popq %rax
    movq %rax, TASK_RFLAGS_FIELD(%rdi)

    xor %rax,%rax
    movw %cs, %ax
    movq %rax, TASK_CS_FIELD(%rdi)
    movw %ss, %ax
    movq %rax, TASK_SS_FIELD(%rdi)

    xor %ax, %ax

    retq


/*
 * C declaration:
 *   int restore_context(struct task_t *task)
 */

.global restore_context
.type restore_context, @function

/*
 * at the beginning of this function, the stack looks like:
 *
 *  0(%esp) - return address
 */
restore_context:
    //movq %rdi, (cur_task)
    movabsq $cur_task, %rax
    movq %rdi, (%rax)

    movq TASK_RBX_FIELD(%rdi), %rbx
    movq TASK_RDX_FIELD(%rdi), %rdx
    movq TASK_RSI_FIELD(%rdi), %rsi
    movq TASK_RBP_FIELD(%rdi), %rbp

    movq TASK_R8_FIELD(%rdi), %r8
    movq TASK_R9_FIELD(%rdi), %r9
    movq TASK_R10_FIELD(%rdi), %r10
    movq TASK_R11_FIELD(%rdi), %r11
    movq TASK_R12_FIELD(%rdi), %r12
    movq TASK_R13_FIELD(%rdi), %r13
    movq TASK_R14_FIELD(%rdi), %r14
    movq TASK_R15_FIELD(%rdi), %r15

    movq TASK_RSP_FIELD(%rdi), %rsp

    movq TASK_RFLAGS_FIELD(%rdi), %rax
    andq $~0x200, %rax
    pushq %rax
    popfq

    pushq TASK_RCX_FIELD(%rdi)
    pushq TASK_RDI_FIELD(%rdi)
    
    movq TASK_KSTACK_VIRT(%rdi), %rax       // rax = address for the top of
                                            // the next task's kernel stack
    
    movabsq $tss_entry, %rcx
    movq %rax, 4(%rcx)          // Adjust the ESP0 field in the TSS (used by CPU
                                // for CPL=3 -> CPL=0 privilege level changes)
    
    movq TASK_PDIR_PHYS(%rdi), %rax         // rax = address of page directory 
                                            // for next task

    movq %cr3, %rcx             // rcx = previous task's virtual address space
    
    cmpq %rcx, %rax             // Does the virtual address space need to be changed?
    je 1f                       // no, virtual address space is the same, so
                                // don't reload it and cause TLB flushes

    movq %rax, %cr3             // yes, load the next task's virtual address space
    
1:
    // We need to update or VMM pointers
    // This is similar to what we do in vmmngr_switch_pdirectory() 
    // in mmngr_virtual.c

    movabsq $_cur_directory_phys, %rcx
    movq %rax, (%rcx)
    movq 48(%rdi), %rax
    movabsq $_cur_directory_virt, %rcx
    movq %rax, (%rcx)
    
    // set TS in CR0 so the FPU will trap to exception #7
    //movq %cr0, %rax
    //orq  $CR0_TS, %rax
    //movq %rax, %cr0
    
    popq %rdi
    popq %rcx
    
    movq $1, %rax

    retq                        // Load next task's RIP from its kernel stack

