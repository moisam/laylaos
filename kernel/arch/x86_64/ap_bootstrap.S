/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: ap_bootstrap.S
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
 *  \file ap_bootstrap.S
 *
 *  Bootstrap code for APs on multiprocessor systems.
 */

.extern ap_stack_base_virt
.extern ap_main

.global ap_bootstrap_start
.global ap_bootstrap_end
.global ap_bootstrap_gdtp
.global ap_premain


/*
 * Part 1 - real mode
 */
ap_bootstrap_start:

.section .text
.code16
.align 16

    cli
    cld
    ljmp $0, $0x8040

.align 16
ap_protect_mode_gdt:
.long 0, 0
.long 0x0000FFFF, 0x00CF9A00    // flat code
.long 0x0000FFFF, 0x008F9200    // flat data
.long 0x00000068, 0x00CF8900    // tss

ap_protect_mode_gdtp:
.word ap_protect_mode_gdtp - ap_protect_mode_gdt - 1
.long 0x8010
.long 0, 0

.align 64
.L8040:

    // set up initial GDT
    xorw %ax, %ax
    movw %ax, %ds
    lgdtl 0x8030
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0

    // Jump into protected mode
    ljmp $0x08, $(ap_bootstrap_32 - ap_bootstrap_start + 0x8000)


/*
 * Part 2 - 32-bit protected mode
 */
ap_bootstrap_32:

.section .text
.code32
.align 16

	// Set cr3 to the address of the kernel's PML
    mov $0x77777777, %edi
    mov %edi, %cr3

    // Enable PAE.
    mov %cr4, %ecx
    or $32, %ecx
    mov %ecx, %cr4

    // EFER.
    mov %eax, %edi
    mov $0xC0000080, %ecx
    rdmsr
    // Set the LME (Long Mode Enable) and SCE (System Call Extensions) bits.
    or $((1 << 8) | 1), %eax
    wrmsr

    // Set PG.
    mov %cr0, %edx
    or $0x80000000, %edx
    mov %edx, %cr0

    // Load GDT and jump to long mode
    mov $(ap_null_gdtr - ap_bootstrap_start + 0x8000), %eax
    //movl $(ap_bootstrap_gdtp - ap_bootstrap_start + 0x8000), %eax
    lgdt (%eax)

    ljmp $0x08, $(ap_premain - ap_bootstrap_start + 0x8000)


.align 8
ap_null_gdtr:
    .word ap_null_gdt_end - ap_null_gdt_base
    .quad ap_null_gdt_base - ap_bootstrap_start + 0x8000

ap_null_gdt_base:
    .quad 0
    .word 0
    .word 0
    .byte 0
    .byte 0x9a
    .byte 0x20
    .byte 0
    .word 0xffff
    .word 0
    .byte 0
    .byte 0x92
    .byte 0
    .byte 0
ap_null_gdt_end:


/*
 * Part 3 - 64-bit long mode
 */
.align 16
ap_bootstrap_gdtp:
.word 0
.quad 0

.section .text
.code64
.align 8

ap_premain:

    movq $(ap_bootstrap_gdtp - ap_bootstrap_start + 0x8000), %rax
    lgdtq (%rax)

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    // Load TSS

#ifdef __x86_64__
    mov $0x30, %ax
#else
    mov $0x28, %ax
#endif

    ltr %ax

    // Set up the stack
    movabsq $ap_stack_base_virt, %rsp
    movq (%rsp), %rsp
	xor %rbp, %rbp

    // Reset RFLAGS.
    pushq   $0
    popfq

	movabsq $ap_main, %rax
	call *%rax

	// Infinite loop if the system has nothing more to do.
	cli
2:	hlt
	jmp 2b

ap_bootstrap_end:

