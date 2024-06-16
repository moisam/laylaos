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

//
// See the Intel FPU manual at:
// https://www.ragestorm.net/downloads/387intel.txt
//

.global fpu_init

// CR0 flags
.set CR0_MP,        (1 << 1)    // Monitor Coprocessor flag
.set CR0_EM,        (1 << 2)    // Emulation Mode flag
.set CR0_TS,        (1 << 3)    // Task Switched flag

.set CR0_NOTMP,     ~(1 << 1)   // Monitor Coprocessor flag
.set CR0_NOTEM,     ~(1 << 2)   // Emulation Mode flag

.section .data
.align 4

temp:
.word 0x0

//st:
//.word 0x0

.section .text

fpu_init:
    
    #xchg %bx, %bx
    fninit                  // Must use non-wait form
    lea temp, %esi
    movw $0x5A5A, (%esi)    // Initialize temp to non-zero value
    fnstsw (%esi)           // Must use non-wait form of fstsw
                            // It is not necessary to use a WAIT instruction
                            // after fnstsw or fnstcw.  Do not use one here.

    cmpw $0, (%esi)         // See if correct status with zeroes was read
    jne no_npx              // Jump if not a valid status word, meaning no NPX

    // Now see if ones can be correctly written from the control word.

    fnstcw (%esi)           // Look at the control word; do not use WAIT form
                            // Do not use a WAIT instruction here!
    mov (%esi), %ax         // See if ones can be written by NPX
    and $0x103f, %ax        // See if selected parts of control word look OK
    cmp $0x3f, %ax          // Check that ones and zeroes were correctly read
    jne no_npx              // Jump if no NPX is installed

    // Some numerics chip is installed.  NPX instructions and WAIT are now safe.
    // See if the NPX is an 8087, 80287, or 80387.
    // This code is necessary if a denormal exception handler is used or the
    // new 80387 instructions will be used.

    fld1                    // Must use default control word from FNINIT
    fldz                    // Form infinity
    fdiv                    // 8087/287 says +inf = .inf
    fld %st                 // Form negative infinity
    fchs                    // 80387 says +inf <> -inf
    fcompp                  // See if they are the same and remove them
    fstsw (%esi)            // Look at status from FCOMPP
    mov (%esi), %ax
    sahf                    // See if the infinities matched
    je found_87_287         // Jump if 8087/287 is present

    // An 80387 is present.  If denormal exceptions are used for an 8087/287,
    // they must be masked.  The 80387 will automatically normalize denormal
    // operands faster than an exception handler can.

    jmp found_387

no_npx:

    // set up for no NPX

    movl %cr0, %eax
    orl  $CR0_EM, %eax
    andl $CR0_NOTMP, %eax
    movl %eax, %cr0
    jmp done

found_87_287:

    // set up for 87/287

found_387:

    // set up for 387

    fninit
    movl %cr0, %eax
    andl $CR0_NOTEM, %eax
    orl $CR0_MP, %eax
    movl %eax, %cr0
    jmp done

done:
    ret

