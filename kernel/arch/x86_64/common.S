/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: common.S
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
 *  \file common.S
 *
 *  Some common kernel functions.
 */

.section .text
.code64
.align 8

.global empty_loop
.global get_rip
.global get_rsp
.global has_cpuid


empty_loop:
    sti
    hlt
    jmp empty_loop


get_rip:
    popq %rax
    jmpq *%rax


get_rsp:
    movq %rsp, %rax
    retq

/*
 * Code from OSDev wiki: https://wiki.osdev.org/CPUID
 */

has_cpuid:
    pushfq
    pushfq
    xorq $0x0000000000200000, (%rsp)     // Invert the ID bit in stored RFLAGS
    popfq                       // Load stored RFLAGS (with ID bit inverted)
    pushfq                      // Store RFLAGS again (ID bit may or may not be inverted)
    popq %rax                   // rax = modified RFLAGS (ID bit may or may not be inverted)
    xor (%rsp), %rax            // rax = whichever bits were changed
    popfq                       // Restore original RFLAGS
    and $0x00200000, %rax       // rax = zero if ID bit can't be changed, else non-zero
    ret

