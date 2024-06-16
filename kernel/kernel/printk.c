/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
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

#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS    1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS      1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS          0
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS          1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS         1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS      1
//#define NANOPRINTF_VISIBILITY_STATIC                    1
#include "nanoprintf.h"

static char buf[4096];

// defined in console.c
extern void twritestr(const char *data);


/*
 * Kernel vprintf function.
 */
int vprintk(const char *fmt, va_list args)
{
    // don't write to screen if this is not the system console
    if(cur_tty != 1)
    {
        return 0;
    }

    uintptr_t s = int_off();
	int i;

    buf[0] = '\0';
	i = npf_vsnprintf(buf, sizeof(buf), fmt, args);

	if(i > 0)
	{
	    //if(cur_tty == 1)
	    {
            twritestr(buf);
        }
    }
    
    int_on(s);

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

