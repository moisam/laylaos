/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: chvt.c
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
 *  \file chvt.c
 *
 *  A utility program to change the current virtual console.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <kernel/tty.h>

char *ver = "1.0";

int extra_argc = 0;
char **extra_argv = NULL;


void print_help(char *myname)
{
    printf("chvt for LaylaOS, version %s\n\n", ver);

    printf("Usage: %s [options] N\n\n", myname);
    printf("Change the foreground virtual terminal to /dev/ttyN.\n\n");
    printf("Options:\n");
    printf("  -h, --help            Show help (this page) and exit\n");
    printf("  -v, --version         Show version and exit\n");
    printf("Unknown options and/or arguments are ignored\n\n");
}


void parse_line_args(int argc, char **argv) 
{
    int c;
    static struct option long_options[] =
    {
        {"help",        no_argument      , 0, 'h'},
        {"version",    no_argument       , 0, 'v'},
        {0, 0, 0, 0}
    };
  
    while((c = getopt_long(argc, argv, "hv", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 'v':
                printf("%s\n", ver);
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
                break;

            case '?':
                break;

            default:
                abort();
        }
    }
    
    extra_argc = argc - optind;
    extra_argv = argv + optind;
}


int main(int argc, char **argv)
{
    int fd, res;
    char ttypath[32];

    parse_line_args(argc, argv);

    if(!extra_argc)
    {
        fprintf(stderr, "%s: missing tty number\n", argv[0]);
        fprintf(stderr, "Type `%s --help` for usage\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    sprintf(ttypath, "/dev/tty%s", *extra_argv++);

    if((fd = open(ttypath, O_RDONLY|O_NOCTTY|O_NONBLOCK)) < 0)
    {
        fprintf(stderr, "%s: failed to open tty: %s\n",
                        argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }

    // if 0 is passed as arg, use the tty device referenced by
    // the given file descriptor
    res = ioctl(fd, VT_SWITCH_TTY, 0);

    close(fd);

    return res;
}

