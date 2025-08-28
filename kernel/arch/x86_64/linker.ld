ENTRY(_start)
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH("i386:x86-64")

SECTIONS
{
	/* The kernel will live at 3GB + 1MB in the virtual address space, */
	/* which will be mapped to 1MB in the physical address space. */
	/* Note that we page-align the sections. */

	/* . = 0xFFFF800000100000; */
	. = 0x00100000;

	/* Add a symbol that indicates the start address of the kernel. */
	kernel_start = . + 0xFFFF800000000000;
	kernel_start_phys = .;

    .multiboot.data : {
        *(.multiboot.data)
    }

    .multiboot.text : {
        *(.multiboot.text)
    }

    . += 0xFFFF800000000000;

	kernel_ro_start = .;
	.text ALIGN (0x1000) : AT (ADDR (.text) - 0xFFFF800000000000)
	{
		*(.text)
	}
	.rodata ALIGN (0x1000) : AT (ADDR (.rodata) - 0xFFFF800000000000)
	{
		*(.rodata)
	}
	kernel_ro_end = .;
	
	kernel_data_start = .;
	.data ALIGN (0x1000) : AT (ADDR (.data) - 0xFFFF800000000000)
	{
		*(.data)
	}
	kernel_data_end = .;


	.eh_frame ALIGN (0x1000) : AT (ADDR (.eh_frame) - 0xFFFF800000000000)
	{
		*(.eh_frame)
	}


	kernel_bss_start = .;
	kernel_bss_start_phys = . - 0xFFFF800000000000;
	.bss ALIGN (0x1000) : AT (ADDR (.bss) - 0xFFFF800000000000)
	{
		*(COMMON)
		*(.bss)
		*(.bootstrap_stack)
	}
	kernel_bss_end = .;

	/* Add a symbol that indicates the end address of the kernel. */
	kernel_end = .;
	kernel_end_phys = . - 0xFFFF800000000000;
}

