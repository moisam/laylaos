/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: gdt_install.S
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
 *  \file gdt_install.S
 *
 *  Install the Global Descriptor Table (GDT).
 */

.section .text
.code64
.align 8

.global _gdt_install
.extern GDTR

_gdt_install:
    //lgdt (GDTR)
    movabsq $GDTR, %rax
    lgdt (%rax)
    
    // https://stackoverflow.com/questions/57212012/how-to-load-address-of-function-or-label-into-register
    
    pushq $0x08
    //lea flush2(%rip), %rax
    movabsq $flush2, %rax
    pushq %rax
    retfq

flush2:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    retq

