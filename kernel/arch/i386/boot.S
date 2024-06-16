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
 *  The kernel's entry point for the i686 arch. Also contains the multiboot
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

.section .data
.align 0x1000

/*
 * Preallocate pages used for paging. Don't hard-code addresses and assume they
 * are available, as the bootloader might have loaded its multiboot structures or
 * modules there. This lets the bootloader know it must avoid the addresses.
 */

BootPageDirectory:
    .skip 0x1000

BootPageTable1:
    .skip 0x1000

/*
 * NOTE: Further page tables may be required if the kernel grows beyond 3 MiB.
 */

    
// Declare a multiboot header that marks the program as a kernel.
.section .text
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
    // Physical address of boot_page_table1.
    movl $(BootPageTable1 - 0xC0000000), %edi

    /*
     * First address to map is address 0.
     * TODO: Start at the first kernel page instead. Alternatively map the first
	 *       1 MiB as it can be generally useful, and there's no need to
	 *       specially map the VGA buffer.
	 */
    movl $0, %esi

   	// Map 1023 pages. The 1024th will be the VGA text buffer.
    movl $1023, %ecx
    
1:
	// Map physical address as "present, writable". Note that this maps
	// .text and .rodata as writable. Mind security and map them as non-writable.
	movl %esi, %edx
	orl $0x003, %edx
	movl %edx, (%edi)

2:
	// Size of page is 4096 bytes.
	addl $4096, %esi

	// Size of entries in boot_page_table1 is 4 bytes.
	addl $4, %edi

	// Loop to the next entry if we haven't finished.
	loop 1b

3:
	// Map VGA video memory to 0xC03FF000 as "present, writable".
	movl $(0x000B8000 | 0x003), BootPageTable1 - 0xC0000000 + 1023 * 4
	
	/*
	 * The page table is used at both page directory entry 0 (virtually from 0x0
	 * to 0x3FFFFF) (thus identity mapping the kernel) and page directory entry
	 * 768 (virtually from 0xC0000000 to 0xC03FFFFF) (thus mapping it in the
	 * higher half). The kernel is identity mapped because enabling paging does
	 * not change the next instruction, which continues to be physical. The CPU
	 * would instead page fault if there was no identity mapping.
	 */

	// Map the page table to both virtual addresses 0x00000000 and 0xC0000000.
	movl $(BootPageTable1 - 0xC0000000 + 0x003), BootPageDirectory - 0xC0000000 + 0
	movl $(BootPageTable1 - 0xC0000000 + 0x003), BootPageDirectory - 0xC0000000 + 768 * 4

	// Set cr3 to the address of the boot_page_directory.
    movl $(BootPageDirectory - 0xC0000000), %ecx
    movl %ecx, %cr3                  // Load Page Directory Base Register.

	// Enable paging and the write-protect bit.
	movl %cr0, %ecx
    orl  $0x80010000, %ecx           // Set PG bit in CR0 to enable paging.
    movl %ecx, %cr0
 
	// Jump to higher half with an absolute jump. 
	leal ready, %ecx
	jmp *%ecx

ready:
 	// Set up the stack.
	leal stack_top, %esp
	movl %esp, %ebp

    // Reset EFLAGS.
    pushl   $0
    popf

    // push the pointer to the stack
    pushl   %esp
             
    // Push the pointer to the Multiboot information structure.
    // NOTE - it doesn't matter. We'll access the multiboot info struct in low
    // memory anyway.
    pushl   %ebx
   
    // Push the magic value.
    pushl   %eax
     
	// Initialize the core kernel before running the global constructors.
	call kernel_early

	// Call the global constructors.
	// call _init

	// Transfer control to the main kernel.
	call kernel_main

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
