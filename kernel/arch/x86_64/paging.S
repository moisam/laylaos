/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: paging.S
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
 *  \file paging.S
 *
 *  Functions to enable and disable paging.
 */

.section .text
.code64
.align 8


.global enable_paging
.global disable_paging
.global vmmngr_flush_tlb_entry


enable_paging:
    movq %cr0, %rax
    orl $0x80000000, %eax
    movq %rax, %cr0
    retq


disable_paging:
    movq %cr0, %rax
    andl $0x7fffffff, %eax
    movq %rax, %cr0
    retq

