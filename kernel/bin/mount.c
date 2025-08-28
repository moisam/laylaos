/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: mount.c
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
 *  \file mount.c
 *
 *  Filesystem mounting program.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include "mount.h"

#define MS_DEFAULTS         (0)

char *ver = "1.0";
char *fstype = NULL;
char *dev = NULL;
char *mpoint = NULL;
char *optstring = NULL;
int fsopts = 0;


void print_mounts(char *myname)
{
    FILE *f;
    char buf[4096];

    if(!(f = fopen("/proc/mounts", "r")))
    {
        fprintf(stderr, "%s: failed to open %s: %s\n",
                myname, "/proc/mounts", strerror(errno));

        exit(16);
    }

    while(fgets(buf, sizeof(buf), f))
    {
        printf("%s", buf);
    }
    
    fclose(f);

    exit(0);
}


static void print_short_usage(char *myname)
{
        fprintf(stderr, "Usage: %s [options] [-t fstype] dev mpoint\n\n"
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
    int c, argc2;
    struct stat st;

    static struct option long_options[] =
    {
        {"help",       no_argument      , 0, 'h'},
        {"options",    required_argument, 0, 'o'},
        {"read-only",  no_argument      , 0, 'r'},
        {"read-write", no_argument      , 0, 'w'},
        {"source",     required_argument, 0, 'S'},
        {"target",     required_argument, 0, 'T'},
        {"type",       required_argument, 0, 't'},
        {"version",    no_argument      , 0, 'v'},
        {0, 0, 0, 0}
    };
  
    if(argc == 1)
    {
        print_mounts(argv[0]);
    }

    while((c = getopt_long(argc, argv, "ho:rS:T:t:vw", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 'o':
                optstring = optarg;
                break;

            case 'S':
                dev = optarg;
                break;

            case 'T':
                mpoint = optarg;
                break;

            case 't':
                fstype = optarg;
                break;

            case 'r':
                fsopts |= MS_RDONLY;
                break;

            case 'w':
                fsopts &= ~MS_RDONLY;
                break;

            case 'v':
                printf("%s\n", ver);
                exit(0);
                break;

            case 'h':
                printf("mount utility for LaylaOS, Version %s\n\n", ver);
                printf("Usage: %s [options] [-t fstype] dev mpoint\n\n"
                       "Options:\n"
                       "  -h, --help            Show this help and exit\n"
                       "  -o, --options opt     Specify mount options as opt, a comma-\n"
                       "                          separated option string\n"
                       "  -r, --read-only       Mount the filesystem read-only\n"
                       "  -S, --source dev      Specify mount source (dev)\n"
                       "  -T, --target mpoint   Specify mount target (mpoint)\n"
                       "  -t, --type fstype     Specify the filesystem type\n"
                       "  -v, --version         Print version and exit\n"
                       "  -w, --read-write      Mount the filesystem read-write\n"
                       "\nArguments:\n"
                       "  fstype        Type of filesystem to be mounted (see\n"
                       "                  '/proc/filesystems' for possible values)\n"
                       "  dev           Device containing the filesystem to be mounted\n"
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

    /*
    if(!fstype)
    {
        exit_missing_arg(argv[0], "fstype");
    }
    */

    argc2 = argc - optind;
    
    if(dev)
    {
        if(!mpoint)
        {
            if(!argc2)
            {
                exit_missing_arg(argv[0], "mpoint");
            }

            mpoint = argv[optind];
            optind++;
            argc2--;
        }
    }

    if(mpoint)
    {
        if(!dev)
        {
            if(!argc2)
            {
                exit_missing_arg(argv[0], "dev");
            }

            dev = argv[optind];
            optind++;
            argc2--;
        }
    }
    
    if(!dev && !mpoint)
    {
        if(argc2 < 2)
        {
            exit_missing_arg(argv[0], (argc2 == 1) ? "mpoint" : "dev, mpoint");
        }

        dev = argv[optind];
        mpoint = argv[optind + 1];
        optind += 2;
    }

    if((argc - optind))
    {
        fprintf(stderr, "%s: too many arguments\n", argv[0]);
        print_short_usage(argv[0]);
        exit(1);
    }

    if(stat(mpoint, &st) < 0)
    {
        fprintf(stderr, "%s: failed to stat %s: %s\n",
                        argv[0], mpoint, strerror(errno));
        exit(1);
    }

    if(!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "%s: failed to mount %s: not a directory\n",
                        argv[0], mpoint);
        exit(1);
    }

    /* interpret the option string */
    if(optstring)
    {
        char *cp = strdup(optstring);
        //char *cp = optstring;
        char *opt;

        while((opt = strsep(&cp, ",")) != NULL)
        {
            if(strcmp(opt, "async") == 0)
            {
                fsopts &= ~MS_SYNCHRONOUS;
            }
            else if(strcmp(opt, "atime") == 0)
            {
                fsopts &= ~MS_NOATIME;
            }
            else if(strcmp(opt, "defaults") == 0)
            {
                fsopts = MS_DEFAULTS;
            }
            else if(strcmp(opt, "dev") == 0)
            {
                fsopts &= ~MS_NODEV;
            }
            else if(strcmp(opt, "diratime") == 0)
            {
                fsopts &= ~MS_NODIRATIME;
            }
            else if(strcmp(opt, "exec") == 0)
            {
                fsopts &= ~MS_NOEXEC;
            }
            else if(strcmp(opt, "mand") == 0)
            {
                fsopts |= MS_MANDLOCK;
            }
            else if(strcmp(opt, "noatime") == 0)
            {
                fsopts |= MS_NOATIME;
            }
            else if(strcmp(opt, "nodev") == 0)
            {
                fsopts |= MS_NODEV;
            }
            else if(strcmp(opt, "nodiratime") == 0)
            {
                fsopts |= MS_NODIRATIME;
            }
            else if(strcmp(opt, "noexec") == 0)
            {
                fsopts |= MS_NOEXEC;
            }
            else if(strcmp(opt, "nomand") == 0)
            {
                fsopts &= ~MS_MANDLOCK;
            }
            else if(strcmp(opt, "nosuid") == 0)
            {
                fsopts |= MS_NOSUID;
            }
            else if(strcmp(opt, "remount") == 0)
            {
                fsopts |= MS_REMOUNT;
            }
            else if(strcmp(opt, "ro") == 0)
            {
                fsopts |= MS_RDONLY;
            }
            else if(strcmp(opt, "rw") == 0)
            {
                fsopts &= ~MS_RDONLY;
            }
            else if(strcmp(opt, "suid") == 0)
            {
                fsopts &= ~MS_NOSUID;
            }
            else if(strcmp(opt, "sync") == 0)
            {
                fsopts |= MS_SYNCHRONOUS;
            }
        }
        
        free(cp);
    }
    else if(!fsopts)
    {
        fsopts = MS_DEFAULTS;
    }

    //printf("mount: optstring '%s'\n", optstring);

    if(!fstype)
    {
        fstype = guess_fstype(argv[0], dev);
        printf("mount: filesystem type: %s\n", fstype);
    }

    if(mount(dev, mpoint, fstype, fsopts, optstring) < 0)
    {
        fprintf(stderr, "%s: failed to mount %s: %s\n",
                        argv[0], mpoint, strerror(errno));
        exit(32);
    }
    
    exit(0);
}

