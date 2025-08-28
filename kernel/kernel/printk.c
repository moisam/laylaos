/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: printk.c
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
 *  \file printk.c
 *
 *  The kernel's printing functions.
 */

#include <kernel/asm.h>
#include <kernel/tty.h>
#include <kernel/apic.h>
#include <mm/kheap.h>

#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS    1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS      1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS          0
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS          1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS         1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS      1
//#define NANOPRINTF_VISIBILITY_STATIC                    1
#include "nanoprintf.h"

char global_printk_buf[4096];

static volatile int currently_printing_cpu = -1;

// defined in console.c
extern void twritestr(const char *data);


/*
 * Kernel vprintf function.
 */
int vprintk(const char *fmt, va_list args)
{
	volatile int i, unlock = 1;
	char *buf = NULL;

    // don't write to screen if this is not the system console
    if(cur_tty != 1)
    {
        return 0;
    }

    /*
    if(this_core->cpuid != 0)
    {
        return 0;
    }
    */

    if(apic_running)
    {
        if(!(buf = kmalloc(2048)))
        {
            __asm__ __volatile__("xchg %%bx, %%bx"::);
            return 0;
        }

        buf[0] = '\0';
    	i = npf_vsnprintf(buf, 2048, fmt, args);

    	if(i > 0)
    	{
            uintptr_t s = int_off();

            while(!__sync_bool_compare_and_swap(&currently_printing_cpu, -1, this_core->cpuid))
            {
                if(currently_printing_cpu == this_core->cpuid)
                {
                    unlock = 0;
                    break;
                }
            }

            twritestr(buf);

            if(unlock)
            {
                __sync_bool_compare_and_swap(&currently_printing_cpu, this_core->cpuid, -1);
            }

            int_on(s);
        }

        kfree(buf);
    }
    else
    {
        buf = global_printk_buf;
        buf[0] = '\0';
    	i = npf_vsnprintf(buf, 4096, fmt, args);

    	if(i > 0)
    	{
            twritestr(buf);
        }
    }

	return i;
}


/*
 * Kernel printf function.
 */
int printk(const char *fmt, ...)
{
	va_list args;
	int i;
	
	va_start(args, fmt);
	i = vprintk(fmt, args);
	va_end(args);

	return i;
}

