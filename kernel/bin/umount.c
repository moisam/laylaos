/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: umount.c
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
 *  \file umount.c
 *
 *  Filesystem un-mounting program.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/mount.h>

char *ver = "1.0";
int flags = 0;


static void print_short_usage(char *myname)
{
        fprintf(stderr, "Usage: %s [options] {device|mpoint}...\n\n"
                        "See %s --help for details\n",
                        myname, myname);
}

static void exit_missing_arg(char *myname, char *argname)
{
        fprintf(stderr, "%s: missing argument: %s\n",
                        myname, argname);

        print_short_usage(myname);
        exit(1);
}


int main(int argc, char **argv)
{
    int c, argc2, res;
    char *mpoint;
    struct stat st;

    static struct option long_options[] =
    {
        {"help",       no_argument      , 0, 'h'},
        {"force",      no_argument      , 0, 'f'},
        {"version",    no_argument      , 0, 'v'},
        {0, 0, 0, 0}
    };

    while((c = getopt_long(argc, argv, "hfv", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 'f':
                flags |= MNT_FORCE;
                break;

            case 'v':
                printf("%s\n", ver);
                exit(0);
                break;

            case 'h':
                printf("umount utility for LaylaOS, Version %s\n\n", ver);
                printf("Usage: %s [options] mpoint...\n\n"
                       "Options:\n"
                       "  -h, --help            Show this help and exit\n"
                       "  -f, --force           Force the unmount of mpoint\n"
                       "  -v, --version         Print version and exit\n"
                       "\nArguments:\n"
                       "  mpoint        Mount point (must be an existing directory)\n"
                       "\n", argv[0]);
                exit(0);
                break;

            case '?':
                break;

            default:
                abort();
        }
    }
  
    argc2 = argc - optind;

    if(!argc2)
    {
        exit_missing_arg(argv[0], "mpoint");
    }
    
    while(argc2 < argc)
    {
        mpoint = argv[argc2++];

        if(stat(mpoint, &st) < 0)
        {
            fprintf(stderr, "%s: cannot stat %s: %s\n",
                            argv[0], mpoint, strerror(errno));
            exit(1);
        }

        if(!S_ISDIR(st.st_mode) && !S_ISBLK(st.st_mode))
        {
            fprintf(stderr, "%s: cannot umount %s: not a directory or a block device\n",
                            argv[0], mpoint);
            exit(1);
        }

        if((res = umount2(mpoint, flags)) < 0)
        {
            fprintf(stderr, "%s: failed to umount %s: %s\n",
                            argv[0], mpoint,
                            (errno == EINVAL) ? "not mounted" : strerror(errno));
            exit(32);
        }
    }

    exit(0);
}

