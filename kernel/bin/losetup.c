/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: losetup.c
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
 *  \file losetup.c
 *
 *  A loopback device manipulation utility. This is based on Linux's losetup
 *  utility, which is part of the util-linux package.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mount.h>          // BLKSSZGET
#include <kernel/loop.h>

char *ver = "1.0";
char *myname = NULL;

int action = 0, loflags = 0;
int set_blocksz = 0, set_offset = 0, set_directio = 0, set_sizelimit = 0;
unsigned long blocksz = 0, offset = 0, directio = 0, sizelimit = 0;
int list = 0, nooverlap = 0, noheadings = 0, raw = 0;
int showdev = 0;
char *loopname = NULL;
char *refname = NULL;
char *filename = NULL;
char *outcols = NULL;

#define MAX_COLS    12

char *standardcols = "NAME,SIZELIMIT,OFFSET,AUTOCLEAR,RO,BACK-FILE,DIO,LOG-SEC";
char *allcols = "NAME,AUTOCLEAR,BACK-FILE,BACK-INO,BACK-MAJ:MIN,MAJ:MIN,OFFSET,PARTSCAN,RO,SIZELIMIT,DIO,LOG-SEC";

enum
{
    COL_NAME = 0,
    COL_AUTOCLEAR,
    COL_BACK_FILE,
    COL_BACK_INO,
    COL_BACK_MAJMIN,
    COL_MAJMIN,
    COL_OFFSET,
    COL_PARTSCAN,
    COL_RO,
    COL_SIZELIMIT,
    COL_DIO,
    COL_LOGSEC,
};

int colhdrs[MAX_COLS];
int colcount;


static void err_and_exit(char *str)
{
    fprintf(stderr, "%s: %s\n", myname, str);
    fprintf(stderr, "%s: use `%s --help` for usage\n", myname, myname);
    exit(EXIT_FAILURE);
}


static void err_exit_add_device(char *str)
{
    fprintf(stderr, "%s: %s: %s\n", myname, str, strerror(errno));
    err_and_exit("failed to add device");
}


#include "losetup-funcs.c"


static unsigned long parse_size(char *str)
{
    char *p = str, *end = NULL;
    unsigned long res, base = 1;

    while(isspace(*p))
    {
        p++;
    }

    // do not accept negative sizes
    if(*p == '-')
    {
        fprintf(stderr, "%s: invalid format: %s\n", myname, str);
        exit(EXIT_FAILURE);
    }

    errno = 0;
    res = strtoul(str, &end, 0);

    if(end == str || (errno && (res == 0 || res == ULONG_MAX)))    
    {
        fprintf(stderr, "%s: invalid format: %s\n", myname, str);
        exit(EXIT_FAILURE);
    }

    if(!end || !*end)
    {
        return res;
    }

    p = end;

    /*
     * Accepted suffixes:
     *
     * {K,M,G,T,P,E}iB = suffix * 1024
     * {K,M,G,T,P,E}   = suffix * 1024
     * {K,M,G,T,P,E}B  = suffix * 1000
     *
     * All the letters can be passed in capital or small letters, e.g. 'K' or 
     * 'k' for kilobytes.
     *
     * We do not support fractions for now.
     */
    if(p[1] == 'i' && (p[2] == 'B' || p[2] == 'b') && p[3] == '\0')
    {
        base = 1024;
    }
    else if((p[1] == 'B' || p[1] == 'b') && p[2] == '\0')
    {
        base = 1000;
    }
    else if(p[1] == '\0')
    {
        base = 1024;
    }
    else
    {
        fprintf(stderr, "%s: invalid format: %s\n", myname, str);
        exit(EXIT_FAILURE);
    }

    switch(p[0])
    {
        case 'k':
        case 'K':
            break;

        case 'm':
        case 'M':
            base = base * base;
            break;

        case 'g':
        case 'G':
            base = base * base * base;
            break;

        case 't':
        case 'T':
            base = base * base * base * base;
            break;

        case 'p':
        case 'P':
            base = base * base * base * base * base;
            break;

        case 'e':
        case 'E':
            base = base * base * base * base * base * base;
            break;

        default:
            fprintf(stderr, "%s: invalid format: %s\n", myname, str);
            exit(EXIT_FAILURE);
    }

    return (res * base);
}


#define CHECK_ACTION_NOT_SET()                                          \
    if(action != 0 && action != c)                                      \
    {                                                                   \
        fprintf(stderr, "%s: mutually exclusive args: -%c and -%c\n",   \
                        argv[0], action, c);                            \
        exit(EXIT_FAILURE);                                             \
    }


static void parse_line_args(int argc, char **argv) 
{
    int c;
    static struct option long_options[] =
    {
        {"help",         no_argument,       0, 'h'},
        {"version",      no_argument,       0, 'V'},
        {"verbose",      no_argument,       0, 'v'},

        {"all",          no_argument,       0, 'a'},
        {"detach-all",   no_argument,       0, 'D'},
        {"find",         no_argument,       0, 'f'},
        {"nooverlap",    no_argument,       0, 'L'},
        //{"json",         no_argument,       0, 'J'},
        {"list",         no_argument,       0, 'l'},
        {"noheadings",   no_argument,       0, 'n'},
        {"output-all",   no_argument,       0, 'A'},
        {"portscan",     no_argument,       0, 'P'},
        {"read-only",    no_argument,       0, 'r'},
        {"raw",          no_argument,       0, 'R'},
        {"show",         no_argument,       0, 'S'},

        {"set-capacity", required_argument, 0, 'c'},
        {"detach",       required_argument, 0, 'd'},
        {"associated",   required_argument, 0, 'j'},
        {"sector-size",  required_argument, 0, 'b'},
        {"offset",       required_argument, 0, 'o'},
        {"output",       required_argument, 0, 'O'},
        {"sizelimit",    required_argument, 0, 'Z'},
        {"loop-ref",     required_argument, 0, 'F'},
        {"direct-io",    required_argument, 0, 'I'},

        {0, 0, 0, 0}
    };

    while((c = getopt_long(argc, argv, "aAb:c:d:DfF:hI:j:lLno:O:PrRSvVZ:", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 'a':
            case 'D':
            case 'f':
                CHECK_ACTION_NOT_SET();
                action = c;
                break;

            case 'b':
                set_blocksz = 1;
                blocksz = parse_size(optarg);
                break;

            case 'c':
            case 'd':
                CHECK_ACTION_NOT_SET();
                action = c;
                loopname = optarg;
                break;

            case 'r':
                loflags |= LO_FLAGS_READ_ONLY;
                break;

            case 'F':
                refname = optarg;
                break;

            case 'j':
                CHECK_ACTION_NOT_SET();
                action = 'a';
                filename = optarg;
                break;

            case 'l':
                list = 1;
                break;

            case 'L':
                nooverlap = 1;
                break;

            case 'n':
                noheadings = 1;
                break;

            case 'R':
                raw = 1;
                break;

            case 'o':
                set_offset = 1;
                offset = parse_size(optarg);
                break;

            case 'O':
                outcols = optarg;
                list = 1;
                break;

            case 'A':
                outcols = allcols;
                list = 1;
                break;

            case 'P':
                loflags |= LO_FLAGS_PARTSCAN;
                break;

            case 'S':
                showdev = 1;
                break;

            case 'I':
                set_directio = 1;

                if(strcasecmp(optarg, "off") == 0)
                {
                    directio = 0;
                    loflags &= ~LO_FLAGS_DIRECT_IO;
                }
                else if(strcasecmp(optarg, "on") == 0)
                {
                    directio = 1;
                    loflags |= LO_FLAGS_DIRECT_IO;
                }
                else
                {
                    fprintf(stderr, "%s: invalid option arg: %s\n", argv[0], optarg);
                    exit(EXIT_FAILURE);
                }

                break;

            case 'Z':
                set_sizelimit = 1;
                sizelimit = parse_size(optarg);
                break;

            case 'v':
                break;

            case 'V':
                printf("%s\n", ver);
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                printf("losetup utility for LaylaOS, Version %s\n\n", ver);
                printf("Usage: %s [options] [<loopdev>]\n\n"
                       "Commands:\n"
                       "  -a, --all                     Display all used devices\n"
                       "  -d, --detach <loopdev>        Detach a loop device\n"
                       "  -D, --detach-all              Detach all used devices\n"
                       "  -f, --find                    Find the first unused device\n"
                       "  -c, --set-capacity <loopdev>  Resize a loop device\n"
                       "  -j, --associated <file>       List devices associated with <file>\n"
                       "  -L, --nooverlap               Avoid possible conflict between devices\n"
                       "\nCommad options:\n"
                       "  -o, --offset <n>              Start at offset <n> in file\n"
                       "  -Z, --sizelimit <n>           Limit device to <n> bytes of file\n"
                       "  -b, --sector-size <n>         Set device sector size to <n> bytes\n"
                       "  -P, --partscan                Create a partitioned loop device\n"
                       "  -r, --read-only               Create a read-only loop device\n"
                       "  -I, --direct-io=<on|off>      Open backing file with O_DIRECT\n"
                       "  -F, --loop-ref <string>       Loop device reference\n"
                       "  -S, --show                    Print device name after setup with -f\n"
                       "  -v, --verbose                 Print verbose output (currently a no-op)\n"
                       "\nOutput options:\n"
                       "  -l, --list                    List info about all or the specified devices\n"
                       "  -n, --noheadings              Do not print headings with --list\n"
                       "  -O, --output <cols>           Specify which columns to print with --list\n"
                       "  -A, --output-all              Output all columns\n"
                       "  -R, --raw                     Use raw --list output\n"
                       "\nMisc options:\n"
                       "  -h, --help        Show this help and exit\n"
                       "  -v, --version     Print version and exit\n"
                       "\n", argv[0]);
                exit(EXIT_SUCCESS);
                break;

            case '?':
                exit(EXIT_FAILURE);
                break;

            default:
                abort();
        }
    }
}


static void parse_cols(char *outcols)
{
    char *p = outcols, *p2;

    colcount = 0;

    while(isspace(*p))
    {
        p++;
    }

    if(!*p)
    {
        err_and_exit("invalid argument to --output");
    }

    while(*p)
    {
        p2 = p;

        while(*p2 && *p2 != ',')
        {
            p2++;
        }

        if(p == p2)
        {
            err_and_exit("invalid argument to --output");
        }

        if(colcount >= MAX_COLS)
        {
            err_and_exit("too many columns specified to --output");
        }

        if(strncasecmp(p, "NAME", p2 - p) == 0)
        {
            colhdrs[colcount++] = COL_NAME;
        }
        else if(strncasecmp(p, "AUTOCLEAR", p2 - p) == 0)
        {
            colhdrs[colcount++] = COL_AUTOCLEAR;
        }
        else if(strncasecmp(p, "BACK-FILE", p2 - p) == 0)
        {
            colhdrs[colcount++] = COL_BACK_FILE;
        }
        else if(strncasecmp(p, "BACK-INO", p2 - p) == 0)
        {
            colhdrs[colcount++] = COL_BACK_INO;
        }
        else if(strncasecmp(p, "BACK-MAJ:MIN", p2 - p) == 0)
        {
            colhdrs[colcount++] = COL_BACK_MAJMIN;
        }
        else if(strncasecmp(p, "MAJ:MIN", p2 - p) == 0)
        {
            colhdrs[colcount++] = COL_MAJMIN;
        }
        else if(strncasecmp(p, "OFFSET", p2 - p) == 0)
        {
            colhdrs[colcount++] = COL_OFFSET;
        }
        else if(strncasecmp(p, "PARTSCAN", p2 - p) == 0)
        {
            colhdrs[colcount++] = COL_PARTSCAN;
        }
        else if(strncasecmp(p, "RO", p2 - p) == 0)
        {
            colhdrs[colcount++] = COL_RO;
        }
        else if(strncasecmp(p, "SIZELIMIT", p2 - p) == 0)
        {
            colhdrs[colcount++] = COL_SIZELIMIT;
        }
        else if(strncasecmp(p, "DIO", p2 - p) == 0)
        {
            colhdrs[colcount++] = COL_DIO;
        }
        else if(strncasecmp(p, "LOG-SEC", p2 - p) == 0)
        {
            colhdrs[colcount++] = COL_LOGSEC;
        }
        else
        {
            err_and_exit("invalid argument to --output");
        }

        p = (*p2 == ',') ? (p2 + 1) : p2;
    }
}


int main(int argc, char **argv)
{
    int res = 0;

    myname = argv[0];
    parse_line_args(argc, argv);

    // if no options given, assume --list & --all
    if(argc == 1)
    {
        action = 'a';
        list = 1;
    }

    if(action == 0 && argc == 2 && raw)
    {
        action = 'a';
        list = 1;
    }

    // default list columns
    if(list && outcols == NULL)
    {
        outcols = standardcols;
    }

    // parse -f optional arg
    if(action == 'f' && optind < argc)
    {
        action = 'C';           // create
        filename = argv[optind++];

        if(optind < argc)
        {
            err_and_exit("unexpected arguments");
        }
    }

    // --list is equal to --all
    if(list && action == 0 && optind == argc)
    {
        action = 'a';
    }

    // --list <device> or ... <device>
    if(action == 0 && argc == (optind + 1))
    {
        if(set_directio)
        {
            action = 'I';
            loflags &= ~LO_FLAGS_DIRECT_IO;
        }
        else if(set_blocksz)
        {
            action = 'b';
        }
        else
        {
            action = '1';
        }

        loopname = argv[optind++];
    }

    if(action == 0)
    {
        action = 'C';           // create

        if(optind >= argc)
        {
            err_and_exit("missing loop device name");
        }

        loopname = argv[optind++];

        if(optind >= argc)
        {
            err_and_exit("missing backing file name");
        }

        filename = argv[optind++];
    }

    if(action != 'C' && (sizelimit || loflags || showdev))
    {
        fprintf(stderr, "%s: one of these options has been used: "
                        "--sizelimit, --partscan, --read-only, --show\n", 
                        argv[0]);
        fprintf(stderr, "%s: they can only be used during loop device setup\n", 
                        argv[0]);
        fprintf(stderr, "%s: use `%s --help` for usage\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    if(set_offset && action != 'C' && (action != 'a' || !filename))
    {
        err_and_exit("option --offset is not allowed in this context");
    }

    if(outcols)
    {
        parse_cols(outcols);
    }

    // now perform the specified action
    if(action == 'C')       // create
    {
        create_lodev();
    }
    else if(action == 'd')       // detach one
    {
        res = delete_lodev(loopname);

        while(optind < argc)
        {
            res |= delete_lodev(argv[optind++]);
        }
    }
    else if(action == 'D')       // detach all
    {
        res = delete_all_lodevs();
    }
    else if(action == 'f')       // find first free
    {
        printf("/dev/loop%d", lodev_first_free());
        res = 0;
    }
    else if(action == 'a' || action == '1')       // show all or one
    {
        res = show_list(loopname, filename);
    }
    else if(action == 'c')       // set capacity
    {
        res = set_lodev_capacity(loopname);
    }
    else if(action == 'I')       // set direct I/O
    {
        res = set_lodev_directio(loopname);
    }
    else if(action == 'b')       // set block size
    {
        res = set_lodev_blocksz(loopname);
    }
    else
    {
        err_and_exit("invalid command");
    }

    exit(res ? EXIT_FAILURE : EXIT_SUCCESS);
}

