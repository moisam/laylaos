/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: interrupt.S
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
 *  \file interrupt.S
 *
 *  Interrupt Service Routines (ISRs).
 */

#include "task_offsets.h"
#include "macros.S"

.section .text
.code64
.align 8


.extern isr_handler
.extern apic_running

/********************************
 * Handling ISRs
 ********************************/

.macro ISR_NOERRCODE n
    .global isr\n
    isr\n:
        cli
        pushq $0     // dummy error code
        pushq $\n
        jmp isr_common_stub
.endm

.macro ISR_ERRCODE n
    .global isr\n
    isr\n:
        cli
        // don't push error code, cpu has already done that
        // pushq $0
        pushq $\n
        jmp isr_common_stub
.endm

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_ERRCODE   21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

/*
 * Timer interrupt for other processors
 */
.global isr123
isr123:
    cli
    pushq $0     // dummy error code
    pushq $123
    jmp isr_common_stub

/*
 * TLB shootdown
 */
.global isr124
isr124:
    cli
    pushq $0     // dummy error code
    pushq $124
    _SWAPGS
    PUSHAQ
    call handle_tlb_shootdown
    movabsq $lapic_virt, %r12
    movq (%r12), %r12
    movl $0x0, 0xb0(%r12)
    POPAQ
    _SWAPGS
    add $16, %rsp     // Clean up the pushed error code and pushed ISR number
    iretq

/*
 * Fatal -- stop everything
 */
.global isr125
isr125:
    cli
1:
    hlt
    jmp 1b

/*
 * Wakeup from idle
 */
.global isr126
isr126:
    pushq %r12
    movabsq $lapic_virt, %r12
    movq (%r12), %r12
    movl $0x0, 0xb0(%r12)
    popq %r12
    iretq

/*
 * Spurious interrupt -- do nothing
 */
.global isr255
isr255:
    xchg %bx, %bx
    iretq


/*
 * This is our common ISR stub. It saves the processor state, sets
 * up for kernel mode segments, calls the C-level fault handler,
 * and finally restores the stack frame.
 */
isr_common_stub:
    //xchg %bx, %bx
    _SWAPGS

    /*
    # instead of pushing everything (PUSHAQ), we only save the registers
    # that are not saved by the callee, while maintaining our struct regs_t
    # structure.
    # The callee saved registers are: rbx, rsp, rbp, r12, r13, r14, r15
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    sub $16, %rsp   # rbp & rsp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    sub $32, %rsp
    */

    PUSHAQ

    mov %rsp, %rdi
    call isr_handler

    cli
    POPAQ

    /*
    add $32, %rsp
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    add $16, %rsp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    */

    _SWAPGS

    add $16, %rsp     // Clean up the pushed error code and pushed ISR number
    iretq             // pops 5 things at once: CS, RIP, RFLAGS, SS, and RSP!


/********************************
 * Handling IRQs
 ********************************/

.extern irq_handler
.extern check_signals_after_irq

.macro IRQ n m
    .global irq\n
    irq\n:
        cli
        pushq $0     // dummy error code
        pushq $\m
        jmp irq_common_stub
.endm

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46


/*
 * Special handler for IRQ-7 to check and handle spurious interrupts.
 * See:
 *    https://forum.osdev.org/viewtopic.php?f=1&t=11673
 *    https://wiki.osdev.org/8259_PIC
 */

.global irq7
irq7:

    cli
    //xchg %bx, %bx
    //xchg %bx, %bx
    pushq %rax

    // If the APIC is running, do not check for a spurious IRQ7
    movabsq $apic_running, %rax
    movl (%rax), %eax
    test %eax, %eax
    jnz 0f

    // PIC.OCW3 set function to read ISR (In Service Register)
    movb $0x0b, %al
    
    // write to PIC.OCW3
    outb %al, $0x20

    // read ISR
    inb $0x20, %al
    
    // if the in-service register does not have IR7 bit set
    // this would be a spurious interrupt.
    test $0x80, %al
    jz 1f
    
    // mutex for spurious guard
    //bts $0x01, flags
    //jc 1f

0:
    popq %rax

    // normal IRQ-7 routine
    pushq $0     // dummy error code
    pushq $39
    jmp irq_common_stub

1:
    // do not send EOI to master pic for a spurious IRQ7
    popq %rax
    iretq


/*
 * Special handler for IRQ-15 to check and handle spurious interrupts.
 * See:
 *    https://forum.osdev.org/viewtopic.php?f=1&t=11673
 *    https://wiki.osdev.org/8259_PIC
 */

.global irq15
irq15:

    cli
    //xchg %bx, %bx
    //xchg %bx, %bx
    pushq %rax

    // If the APIC is running, do not check for a spurious IRQ15
    movabsq $apic_running, %rax
    movl (%rax), %eax
    test %eax, %eax
    jnz 0f
    
    // PIC.OCW3 set function to read ISR (In Service Register)
    movb $0x0b, %al
    //movb $0x03, %al
    
    // write to PIC.OCW3
    outb %al, $0xa0
    //outb %al, $0xa3

    // read ISR
    inb $0xa0, %al
    
    // if the in-service register does not have IR7 bit set
    // this would be a spurious interrupt.
    test $0x80, %al
    jz 1f
    
    // mutex for spurious guard
    //bts $0x01, flags
    //jc 1f

0:
    popq %rax

    // normal IRQ-15 routine
    pushq $0     // dummy error code
    pushq $47
    jmp irq_common_stub

1:
    // NOTE: Specific EOI used for master PIC cascade line!!!!
    //movb $0x20, %al
    movb $0x62, %al
    
    // send EOI to slave PIC (clear the ISR flag for IRQ15)
    outb %al, $0x20
    
    popq %rax
    iretq
    

/*
 * This is our common IRQ stub. It saves the processor state, sets
 * up for kernel mode segments, calls the C-level fault handler,
 * and finally restores the stack frame.
 */

irq_common_stub:
    _SWAPGS

    /*
    # instead of pushing everything (PUSHAQ), we only save the registers
    # that are not saved by the callee, while maintaining our struct regs_t
    # structure.
    # The callee saved registers are: rbx, rsp, rbp, r12, r13, r14, r15
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    sub $16, %rsp   # rbp & rsp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    sub $32, %rsp
    */

    PUSHAQ

    mov %rsp, %rdi
    call irq_handler

    /*
     * only check signals if all the following is true:
     *   - tasking is initialized 
     *   - the current running task is a user task, not a kernel task
     *   - we are not inside an IRQ that happened inside another IRQ
     *     (we check this in check_signals_after_irq())
     */

    //xchg %bx, %bx
    cli
    /*
    movabsq $cur_task, %rax
    movq (%rax), %rax
    test %rax, %rax
    jz 2f

    movl TASK_USER_FIELD(%rax), %eax
    test %eax, %eax
    jz 2f
    */

    mov %rsp, %rdi
    call check_signals_after_irq

2:
    cli
    POPAQ

    /*
    add $32, %rsp
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    add $16, %rsp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    */

    _SWAPGS

    add $16, %rsp     // Clean up the pushed error code and pushed ISR number
    iretq             // pops 5 things at once: CS, RIP, RFLAGS, SS, and RSP!


.global get_cr2
get_cr2:
    movq %cr2, %rax
    retq

