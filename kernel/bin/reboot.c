/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: reboot.c
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
 *  \file reboot.c
 *
 *  A utility program to shutdown or reboot the system.
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#define _GNU_SOURCE     1
#undef __GNU_VISIBLE
#define __GNU_VISIBLE   1
#include <string.h>
#include <kernel/reboot.h>

extern int reboot(int cmd);

const char *ver = "1.0";


int main(int argc, char **argv)
{
    /*
     * what we do depends on what name were we called by
     */
    int c;
    int action;
    char *base = basename(argv[0]);
    
    if(strcmp(base, "poweroff") == 0)
    {
        action = KERNEL_REBOOT_POWEROFF;
    }
    else if(strcmp(base, "halt") == 0)
    {
        action = KERNEL_REBOOT_HALT;
    }
    else
    {
        action = KERNEL_REBOOT_RESTART;
    }

    /*
     * parse command line arguments
     */
    static struct option long_options[] =
    {
        { "help",       no_argument, 0, 'h' },
        { "version",    no_argument, 0, 'v' },
        { "halt",       no_argument, 0, 't' },
        { "poweroff",   no_argument, 0, 'p' },
        { "reboot",     no_argument, 0, 'r' },
        { 0, 0, 0, 0 }
    };

    while((c = getopt_long(argc, argv, "vhtpr", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 'v':	//show version & exit
                printf("%s\n", ver);
                exit(EXIT_SUCCESS);
                break;

            case 'h':	//show help & exit
                printf("reboot utility for LaylaOS, Version %s\n\n", ver);
                printf("Usage: %s [OPTIONS...]\n\n"
                       /* "Reboot the system.\n\n" */
                       "Options:\n"
                       "  -h, --help        Show this help and exit\n"
                       "  -p, --poweroff    Switch off the machine\n"
                       "  -r, --reboot      Reboot the machine\n"
                       "  -t, --halt        Halt the machine\n"
                       "  -v, --version     Print version and exit\n"
                       "\n", argv[0]);
                exit(EXIT_SUCCESS);
                break;

            case 'p':
                action = KERNEL_REBOOT_POWEROFF;
                break;

            case 'r':
                action = KERNEL_REBOOT_RESTART;
                break;

            case 't':
                action = KERNEL_REBOOT_HALT;
                break;

            case '?':
                break;

            default:
                abort();
		}
	}

	if(optind < argc)
	{
	    fprintf(stderr, "%s: excess arguments passed to command\n"
	                    "See %s --help for syntax\n",
	                    argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }
    
    exit((reboot(action) == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
}

