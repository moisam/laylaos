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

/*
 * C declaration:
 *   void switch_task(struct task_t *task)
 *
 * The following code is adopted from:
 *   https://wiki.osdev.org/Kernel_Multitasking
 */

.extern tss_entry

.set CR0_TS,        (1 << 3)


/*
 * C declaration:
 *   int save_context(struct task_t *task)
 */

.global save_context
.type save_context, @function

/*
 * at the beginning of this function, the stack looks like:
 *
 *	0(%esp) - pushed %ebx
 *	4(%esp) - pushed %eax
 *  8(%esp) - return address
 *	12(%esp) - task
 */
save_context:
    pushl %eax
    pushl %ebx
    movl 12(%esp), %eax
    xor %ebx, %ebx

    movl 4(%esp), %ebx
    movl %ebx, TASK_EAX_FIELD(%eax)
    movl 0(%esp), %ebx
    movl %ebx, TASK_EBX_FIELD(%eax)
    movl %ecx, TASK_ECX_FIELD(%eax)
    movl %edx, TASK_EDX_FIELD(%eax)
    movl %edi, TASK_EDI_FIELD(%eax)
    movl %esi, TASK_ESI_FIELD(%eax)
    movl %ebp, TASK_EBP_FIELD(%eax)

    pushf
    popl %ebx
    movl %ebx, TASK_EFLAGS_FIELD(%eax)

    xor %ebx, %ebx
    movw %gs, %bx
    movl %ebx, TASK_GS_FIELD(%eax)
    movw %fs, %bx
    movl %ebx, TASK_FS_FIELD(%eax)
    movw %es, %bx
    movl %ebx, TASK_ES_FIELD(%eax)
    movw %ds, %bx
    movl %ebx, TASK_DS_FIELD(%eax)
    movw %cs, %bx
    movl %ebx, TASK_CS_FIELD(%eax)
    movw %ss, %bx
    movl %ebx, TASK_SS_FIELD(%eax)

    movl %esp, %ebx
    //addl 12, %ebx
    addl $8, %ebx
    movl %ebx, TASK_ESP_FIELD(%eax)

    movl 8(%esp), %ebx
    movl %ebx, TASK_EIP_FIELD(%eax)

    //movl 64(%esp), %ebx
    //movl %ebx, TASK_USERESP_FIELD(%eax)

    popl %ebx
    popl %eax
    xor %eax, %eax

    ret


.global restore_context
.type restore_context, @function

/*
 * at the beginning of this function, the stack looks like:
 *
 *  0(%esp) - return address
 *	4(%esp) - task
 */
restore_context:
    movl 4(%esp), %eax
    xor %ebx, %ebx
    
    movl %eax, (cur_task)

    movl TASK_ECX_FIELD(%eax), %ecx
    movl TASK_EDX_FIELD(%eax), %edx
    movl TASK_EDI_FIELD(%eax), %edi
    movl TASK_ESI_FIELD(%eax), %esi
    movl TASK_EBP_FIELD(%eax), %ebp

    movl TASK_EFLAGS_FIELD(%eax), %ebx
    pushl %ebx
    popf

    movl TASK_GS_FIELD(%eax), %ebx
    movw %gs, %bx
    movl TASK_FS_FIELD(%eax), %ebx
    movw %fs, %bx
    movl TASK_ES_FIELD(%eax), %ebx
    movw %es, %bx
    movl TASK_DS_FIELD(%eax), %ebx
    movw %ds, %bx
    movl TASK_CS_FIELD(%eax), %ebx
    movw %cs, %bx
    movl TASK_SS_FIELD(%eax), %ebx
    movw %ss, %bx

    movl TASK_ESP_FIELD(%eax), %esp

    pushl %esi
    pushl %ecx
    pushl TASK_EBX_FIELD(%eax)
    pushl TASK_EAX_FIELD(%eax)

    movl %eax, %esi
    
    movl 20(%esi), %eax         // eax = address of page directory for next task
    movl 16(%esi), %ebx         // ebx = address for the top of the next task's
                                // kernel stack
    
    movl $tss_entry, %ecx
    movl %ebx, 4(%ecx)          // Adjust the ESP0 field in the TSS (used by CPU
                                // for CPL=3 -> CPL=0 privilege level changes)
    
    movl %eax, %cr3             // load the next task's virtual address space
    
    // We need to update or VMM pointers
    // This is similar to what we do in vmmngr_switch_pdirectory() in mmngr_virtual.c
    
    movl %eax, (_cur_directory_phys)
    movl 24(%esi), %eax
    movl %eax, (_cur_directory_virt)
    
    // set TS in CR0 so the FPU will trap to exception #7

    movl %cr0, %eax
    orl  $CR0_TS, %eax
    movl %eax, %cr0
    
    popl %eax
    popl %ebx
    popl %ecx
    popl %esi
    
    movl $1, %eax

    ret                         // Load next task's EIP from its kernel stack

