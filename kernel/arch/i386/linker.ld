ENTRY (_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
	/* The kernel will live at 3GB + 1MB in the virtual address space, */
	/* which will be mapped to 1MB in the physical address space. */
	/* Note that we page-align the sections. */

	. = 0xC0100000;

	/* Add a symbol that indicates the start address of the kernel. */
	kernel_start = .;

	kernel_ro_start = .;
	.text ALIGN (0x1000) : AT (ADDR (.text) - 0xC0000000)
	{
		*(.text)
		*(.rodata)
	}
	kernel_ro_end = .;
	
	kernel_data_start = .;
	.data ALIGN (0x1000) : AT (ADDR (.data) - 0xC0000000)
	{
		*(.data)
	}
	kernel_data_end = .;

	kernel_bss_start = .;
	.bss ALIGN (0x1000) : AT (ADDR (.bss) - 0xC0000000)
	{
		*(COMMON)
		*(.bss)
	}
	kernel_bss_end = .;

	/* Add a symbol that indicates the end address of the kernel. */
	kernel_end = .;
}

