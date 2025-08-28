/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: symbols.c
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
 *  \file symbols.c
 *
 *  Definitions of system-wide symbols.
 */

#include <sys/param.h>
#include <sys/utsname.h>

#ifdef __x86_64__
# define MACHINE        "x86_64"
#else
# define MACHINE        "i686"
#endif

#define OSNAME          "LaylaOS"
#define OSRELEASE       "0.0.3"
#define OSREVISION      1
#define OSVERSION       "0.0.3"

char osrelease[] = OSRELEASE;
char ostype[] = OSNAME;
int  osrev = OSREVISION;
char version[] = OSVERSION;

char machine[] = MACHINE;
char cpu_model[100] = MACHINE;

char kernel_cmdline[256] = { 0, };

unsigned long system_context_switches = 0;
unsigned long system_forks = 0;

struct utsname myname =
{
    OSNAME,         // sysname
    "localhost",    // nodename
    OSRELEASE,      // release
    OSVERSION,      // version
    MACHINE,        // machine
    "",             // domainname
};

