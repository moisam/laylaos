ENTRY(_start)

SECTIONS
{
	. = 0x00;
	phys = .;

	.text :
	{
		code = .;
		*(.text)
	}

	.eh_frame :
	{
		*(.eh_frame)
	}

	/DISCARD/ :
	{
		*(.comment)
		*(.note.gnu.build-id)
		*(.data)
		*(.rodata)
		*(.bss)
	}
}

