/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: tss.c
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
 *  \file tss.c
 *
 * Define and install the Task State Segment [TSS] for x86 and x64 processors.
 */

#include <kernel/laylaos.h>
#include <kernel/tss.h>

tss_entry_t tss_entry;


/*
 * Initialise and install the TSS.
 */
void tss_install(uint32_t kernel_ss, uintptr_t kernel_esp)
{
    memset(&tss_entry, 0, sizeof(tss_entry));

#ifdef __x86_64__

    UNUSED(kernel_ss);

#else       /* !__x86_64__ */

    tss_entry.ss0 = kernel_ss;
    tss_entry.cs = 0x0b;
    tss_entry.ss = 0x13;
    tss_entry.es = 0x13;
    tss_entry.ds = 0x13;
    tss_entry.fs = 0x13;
    tss_entry.gs = 0x13;

#endif      /* !__x86_64__ */

    tss_entry.sp0 = kernel_esp;
}

/*
 * Flush the TSS.
 */
void tss_flush(void)
{

#ifdef __x86_64__
	__asm__ __volatile__("mov $0x30, %%ax\n\t"
#else       /* !__x86_64__ */
	__asm__ __volatile__("mov $0x28, %%ax\n\t"
#endif      /* !__x86_64__ */
                         "ltr %%ax"
                        :::);
}

