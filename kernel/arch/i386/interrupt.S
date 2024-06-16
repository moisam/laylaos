/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
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

.extern isr_handler

/********************************
 * Handling ISRs
 ********************************/

.macro ISR_NOERRCODE n
    .global isr\n
    isr\n:
        cli
        pushl $0     // dummy error code
        pushl $\n
        jmp isr_common_stub
.endm

.macro ISR_ERRCODE n
    .global isr\n
    isr\n:
        cli
        // don't push error code, cpu has already done that
        // pushl $0
        pushl $\n
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
 * This is our common ISR stub. It saves the processor state, sets
 * up for kernel mode segments, calls the C-level fault handler,
 * and finally restores the stack frame.
 */
isr_common_stub:
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    mov $0x10, %ax   // Load the Kernel Data Segment descriptor!
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    mov %esp, %eax   // Push us on the stack
    push %eax

    call isr_handler

    pop %eax
    pop %gs
    pop %fs
    pop %es
    pop %ds

    popa

    add $8, %esp   // Cleans up the pushed error code and pushed ISR number
    //sti
    iret           // pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP!


/********************************
 * Handling IRQs
 ********************************/

.extern irq_handler
.extern check_signals_after_irq

.macro IRQ n m
    .global irq\n
    irq\n:
        cli
        pushl $0     // dummy error code
        pushl $\m
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

#IRQ 7, 39
#IRQ 15, 47


/*
 * Special handler for IRQ-7 to check and handle spurious interrupts.
 * See:
 *    https://forum.osdev.org/viewtopic.php?f=1&t=11673
 *    https://wiki.osdev.org/8259_PIC
 */

.global irq7
irq7:

    cli
    pushl %eax
    
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

    popl %eax

    // normal IRQ-7 routine
    pushl $0     // dummy error code
    pushl $39
    jmp irq_common_stub

1:
    // do not send EOI to master pic for a spurious IRQ7
    popl %eax
    iret


/*
 * Special handler for IRQ-15 to check and handle spurious interrupts.
 * See:
 *    https://forum.osdev.org/viewtopic.php?f=1&t=11673
 *    https://wiki.osdev.org/8259_PIC
 */

.global irq15
irq15:

    cli
    pushl %eax
    
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

    popl %eax

    // normal IRQ-15 routine
    pushl $0     // dummy error code
    pushl $47
    jmp irq_common_stub

1:
    // NOTE: Specific EOI used for master PIC cascade line!!!!
    //movb $0x20, %al
    movb $0x62, %al
    
    // send EOI to slave PIC (clear the ISR flag for IRQ15)
    outb %al, $0x20
    
    popl %eax
    iret
    

/*
 * This is our common IRQ stub. It saves the processor state, sets
 * up for kernel mode segments, calls the C-level fault handler,
 * and finally restores the stack frame.
 */

irq_common_stub:
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    mov $0x10, %ax   // Load the Kernel Data Segment descriptor!
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    mov %esp, %eax   // Push us on the stack
    push %eax

    call irq_handler

    // pop and then re-push the &regs pointer on the stack, just in case
    // the irq_handler call above messed with it
    pop %eax
    mov %esp, %eax   // Push us on the stack, again
    push %eax

    call check_signals_after_irq

    pop %eax
    pop %gs
    pop %fs
    pop %es
    pop %ds

    popa

    add $8, %esp   // Cleans up the pushed error code and pushed ISR number
    //sti
    iret           // pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP!


.global get_cr2
get_cr2:
    mov %cr2, %eax
    ret

