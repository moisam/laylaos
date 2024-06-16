/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: boot.S
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
 *  \file boot.S
 *
 *  The kernel's entry point for the x86_64 arch. Also contains the multiboot
 *  structure and initial Global Descriptor Table (GDT).
 */

/*
 * Declare constants for the multiboot header.
 * See: https://www.gnu.org/software/grub/manual/multiboot/multiboot.html#Header-graphics-fields
 */
.set ALIGN,    1<<0             // align loaded modules on page boundaries
.set MEMINFO,  1<<1             // provide memory map
.set VIDEOINFO,1<<2             // provide video mode info
.set FLAGS,    ALIGN | MEMINFO | VIDEOINFO  // this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       // 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS) // checksum of above, to prove we are multiboot

// Declare constants for the video mode
.set VIDEO_MODE,    0           // 0 = linear graphics, 1 = text mode
.set VIDEO_WIDTH,   1024        // pixels (graphics mode), cols (text mode)
.set VIDEO_HEIGHT,  768         // pixels (graphics mode), cols (text mode)
.set VIDEO_BPP,     32          // bpp (graphics mode), 0 (text mode)
/*
.set VIDEO_MODE,    1           // 0 = linear graphics, 1 = text mode
.set VIDEO_WIDTH,   80          // pixels (graphics mode), cols (text mode)
.set VIDEO_HEIGHT,  25          // pixels (graphics mode), cols (text mode)
.set VIDEO_BPP,     0           // bpp (graphics mode), 0 (text mode)
*/

.section .data
.align 0x1000

/*
 * Preallocate pages used for paging. Don't hard-code addresses and assume they
 * are available, as the bootloader might have loaded its multiboot structures or
 * modules there. This lets the bootloader know it must avoid the addresses.
 */

BootPageDirectory:
    .skip 0x3000

/*
 * NOTE: Further page tables may be required if the kernel grows beyond 3 MiB.
 */


// Declare a multiboot header that marks the program as a kernel.
.section .text
.code32
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0   // header_addr (if flags[16] is set)
.long 0   // load_addr (if flags[16] is set)
.long 0   // load_end_addr (if flags[16] is set)
.long 0   // bss_end_addr (if flags[16] is set)
.long 0   // entry_addr (if flags[16] is set)
.long VIDEO_MODE
.long VIDEO_WIDTH
.long VIDEO_HEIGHT
.long VIDEO_BPP

// reserve initial kernel stack space -- that's 16k.
.set STACKSIZE, 0x4000


/*
 * The kernel entry point.
 */
.global _start
.type _start, @function
_start:
	// Set cr3 to the address of the boot_page_directory.
    mov $(BootPageDirectory - 0xFFFF800000000000), %edi
    mov %edi, %cr3
    
    //xchg %bx, %bx

	// Map the page table to both virtual addresses 0x00000000_00000000
	// and 0xFFFF8000_00000000.
    // PML4[0] = &PDP[0] | PRESENT | WRITEABLE
    // PML4[256] = &PDP[0] | PRESENT | WRITEABLE
    mov $0x1003, %ecx
    add %edi, %ecx
    mov %ecx, (%edi)
    add $2048, %edi
    mov %ecx, (%edi)
    add $2048, %edi

    // PDP[0] = &PD[0] | PRESENT | WRITEABLE
    //add $0x1000, %edi
    mov $0x1003, %ecx
    add %edi, %ecx
    mov %ecx, (%edi)
    
    // PD[0]
    // Map 32 pages x 2Mib per page = 64Mib
    add $0x1000, %edi
    mov $0x83, %edx
    mov $32, %ecx

.set_entry:
    mov %edx, (%edi)

	// Size of page is 2MiB.
    add $0x200000, %edx

	// Size of entry is 8 bytes.
    add $8, %edi

	// Loop to the next entry if we haven't finished.
    loop .set_entry
    
    // Enable PAE.
    mov %cr4, %ecx
    or $32, %ecx
    mov %ecx, %cr4
    
    // EFER.
    mov %eax, %edi
    mov $0xC0000080, %ecx
    rdmsr
    // Set the LME (Long Mode Enable) and SCE (System Call Extensions) bits.
    //or $256, %eax       // Long Mode Enable
    //xchg %bx, %bx
    or $((1 << 8) | 1), %eax
    wrmsr
    //mov %edi, %eax
    
    // Set PG.
    mov %cr0, %edx
    or $0x80000000, %edx
    mov %edx, %cr0
    
    // Load GDT and jump to long mode
    //lgdt null_gdtr
    mov $(null_gdtr - 0xFFFF800000000000), %eax
    lgdt (%eax)

	//xchg %bx, %bx
	
    ljmp $0x08, $(long_mode - 0xFFFF800000000000)
    
.align 8
null_gdtr:
    .word null_gdt_end - null_gdt_base
    .quad null_gdt_base

null_gdt_base:
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
null_gdt_end:


.section .text
.code64
.align 8

long_mode:
    cli
    //xor %rdi, %rdi
    //mov %eax, %edi
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    xor %rsi, %rsi
    mov %ebx, %esi


	// Set up the stack.
	//leaq stack_top, %rsp
	movabsq $stack_top, %rsp
	//movq %rsp, %rbp
	xor %rbp, %rbp

    // Reset RFLAGS.
    pushq   $0
    popfq

	// Transfer control to the main kernel.
	//call kernel_main
	movabsq $kernel_main, %rax
	call *%rax

	// Infinite loop if the system has nothing more to do.
	cli
2:	hlt
	jmp 2b

.section .bss
.align 32
.global stack
.global stack_top
stack:
.skip STACKSIZE
stack_top:
