/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: sse_init.S
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
 *  \file sse_init.S
 *
 *  Enable SSE if the CPU supports it.
 */

.global sse_init

.section .text

sse_init:

    movl $1, %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    cpuid
    test $(1 << 25), %edx
    jz .noSSE
    //xchg %bx, %bx
    mov %cr0, %eax
    and $0xFFFB, %ax
    or $2, %ax
    mov %eax, %cr0
    mov %cr4, %eax
    or $(3 << 9), %ax
    mov %eax, %cr4
.noSSE:
    popl %edx
    popl %ecx
    popl %ebx

    //xchg %bx, %bx

    ret

