/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: fpu_init.S
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
 *  \file fpu_init.S
 *
 *  Initialise the Floating Point Unit (FPU).
 */

.section .text
.code64
.align 8


.global fpu_init

fpu_init:
    clts
    mov %cr0, %rax
    and $0xFFFD, %ax
    or $0x10, %ax
    mov %rax, %cr0
    fninit
    mov %cr0, %rax
    and $0xFFFB, %ax
    or $0x0002, %ax
    mov %rax, %cr0
    mov %cr4, %rax
    or $0x600, %rax
    mov %rax, %cr4
    push $0x1F80
    ldmxcsr (%rsp)
    addq $8, %rsp
    retq

