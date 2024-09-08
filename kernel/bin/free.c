/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: free.c
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
 *  \file free.c
 *
 *  A program to display the amount of free and used memory in the system.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#define UNIT_BYTES          1
#define UNIT_KILOS          2
#define UNIT_MEGAS          3
#define UNIT_GIGAS          4

char *ver = "1.0";

char *units_normal_strs[] = { NULL, "b", "Ki", "Mi", "Gi", };
char *units_si_strs[] = { NULL, "b", "K", "M", "G", };
size_t denom_normal[] = { 1, 1, 1024, 1024 * 1024, 1024 * 1024 * 1024 };
size_t denom_si[] = { 1, 1, 1000, 1000 * 1000, 1000 * 1000 * 1000 };

char *unit_str = NULL;
int unit = UNIT_KILOS;
int si_units = 0;
int human_units = 0;
int show_total = 0;


void parse_line_args(int argc, char **argv) 
{
    int c;
    static struct option long_options[] =
    {
        {"help",    no_argument,       0, 'h'},
        {"version", no_argument,       0, 'v'},
        {"bytes",   no_argument,       0, 'b'},
        {"kibi",    no_argument,       0, 'k'},
        {"mebi",    no_argument,       0, 'm'},
        {"gibi",    no_argument,       0, 'g'},
        {"total",   no_argument,       0, 't'},
        {"kilo",    no_argument,       0, 'K'},
        {"mega",    no_argument,       0, 'M'},
        {"giga",    no_argument,       0, 'G'},
        {"human",   no_argument,       0, 'H'},
        {"si",      no_argument,       0, 'S'},
        {0, 0, 0, 0}
    };
  
    while((c = getopt_long(argc, argv, "hvbkmgtKMGHS", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 'b':
                unit = UNIT_BYTES;
                break;

            case 'k':
                unit = UNIT_KILOS;
                break;

            case 'm':
                unit = UNIT_MEGAS;
                break;

            case 'g':
                unit = UNIT_GIGAS;
                break;

            case 't':
                show_total = 1;
                break;

            case 'K':
                unit = UNIT_KILOS;
                si_units = 1;
                break;

            case 'M':
                unit = UNIT_MEGAS;
                si_units = 1;
                break;

            case 'G':
                unit = UNIT_GIGAS;
                si_units = 1;
                break;

            case 'H':
                human_units = 1;
                break;

            case 'S':
                si_units = 1;
                break;

            case 'v':
                printf("%s\n", ver);
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                printf("free utility for LaylaOS, Version %s\n\n", ver);
                printf("Usage: %s [options]\n\n"
                       "Options:\n"
                       "  -b, --bytes       Display the amount of memory in bytes\n"
                       "  -g, --gibi        Display the amount of memory in gibibytes\n"
                       "  -h, --help        Show this help and exit\n"
                       "  -k, --kibi        Display the amount of memory in kibibytes (default)\n"
                       "  -m, --mebi        Display the amount of memory in mebibytes\n"
                       "  -t, --total       Display a line showing the column totals\n"
                       "  -v, --version     Print version and exit\n"
                       "  -G, --giga        Display the amount of memory in gigabytes (implies --si)\n"
                       "  -H, --human       Display the amount of memory in human-readable format\n"
                       "  -K, --kilo        Display the amount of memory in kilobytes (implies --si)\n"
                       "  -M, --mega        Display the amount of memory in megabytes (implies --si)\n"
                       "  -S, --si          Use kilo, mega, giga etc (power of 1000) instead \n"
                       "                      of kibi, mebi, gibi (power of 1024)\n"
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


static inline size_t get_num(char *p)
{
    size_t res = 0;

    while(*p >= '0' && *p <= '9')
    {
        res = (res * 10) + (*p - '0');
        p++;
    }

    return res;
}


static inline size_t largest_unit(size_t n, int unit)
{
    int new_unit;

    if(n >= 1024 * 1024 * 1024)
    {
        new_unit = UNIT_GIGAS;
    }
    else if(n >= 1024 * 1024)
    {
        new_unit = UNIT_MEGAS;
    }
    else if(n >= 1024)
    {
        new_unit = UNIT_KILOS;
    }
    else
    {
        new_unit = UNIT_BYTES;
    }

    return (new_unit > unit) ? new_unit : unit;
}


int main(int argc, char **argv)
{
    FILE *f;
    char *p, buf[1024];
    size_t memtotal = 0, memused = 0, memfree = 0;
    size_t swaptotal = 0, swapused = 0, swapfree = 0;
    size_t buffers = 0, cached = 0;
    size_t denom = 1;

    parse_line_args(argc, argv);

    if(!(f = fopen("/proc/meminfo", "r")))
    {
        fprintf(stderr, "%s: failed to open /proc/meminfo: %s\n", 
                        argv[0], strerror(errno));
        exit(1);
    }

    while(fgets(buf, sizeof(buf), f))
    {
        if(strstr(buf, "MemTotal") == buf)
        {
            p = buf + 9;
            while(*p == ' ') p++;
            memtotal += get_num(p);
        }
        else if(strstr(buf, "SwapTotal") == buf)
        {
            p = buf + 10;
            while(*p == ' ') p++;
            swaptotal += get_num(p);
        }
        else if(strstr(buf, "MemFree") == buf)
        {
            p = buf + 8;
            while(*p == ' ') p++;
            memfree += get_num(p);
        }
        else if(strstr(buf, "SwapFree") == buf)
        {
            p = buf + 9;
            while(*p == ' ') p++;
            swapfree += get_num(p);
        }
        else if(strstr(buf, "Buffers") == buf)
        {
            p = buf + 8;
            while(*p == ' ') p++;
            buffers += get_num(p);
        }
        else if(strstr(buf, "Cached") == buf)
        {
            p = buf + 7;
            while(*p == ' ') p++;
            cached += get_num(p);
        }
    }

    fclose(f);

    // Calculate used memory
    memused = memtotal - memfree - buffers - cached;
    swapused = swaptotal - swapfree;

    // Memory reported in /proc/meminfo is in kilobytes. Convert to bytes and 
    // then convert to the requested units

    // Convert to bytes first
    memtotal  = (memtotal * 1024);
    memused   = (memused * 1024);
    memfree   = (memfree * 1024);
    swaptotal = (swaptotal * 1024);
    swapused  = (swapused * 1024);
    swapfree  = (swapfree * 1024);
    buffers   = (buffers * 1024);
    cached    = (cached * 1024);

    if(human_units)
    {
        unit = UNIT_BYTES;
        unit = largest_unit(memtotal, unit);
        unit = largest_unit(memused, unit);
        unit = largest_unit(memfree, unit);
        unit = largest_unit(swaptotal, unit);
        unit = largest_unit(swapused, unit);
        unit = largest_unit(swapfree, unit);
        unit = largest_unit(buffers, unit);
        unit = largest_unit(cached, unit);

        unit_str = si_units ? units_si_strs[unit] : units_normal_strs[unit];
        denom = si_units ? denom_si[unit] : denom_normal[unit];
    }
    else
    {
        if(si_units)
        {
            switch(unit)
            {
                case UNIT_KILOS:
                    denom = 1000;
                    break;

                case UNIT_MEGAS:
                    denom = 1000 * 1000;
                    break;

                case UNIT_GIGAS:
                    denom = 1000 * 1000 * 1000;
                    break;
            }
        }
        else
        {
            switch(unit)
            {
                case UNIT_KILOS:
                    denom = 1024;
                    break;

                case UNIT_MEGAS:
                    denom = 1024 * 1024;
                    break;

                case UNIT_GIGAS:
                    denom = 1024 * 1024 * 1024;
                    break;
            }
        }
    }

    memtotal  /= denom;
    memused   /= denom;
    memfree   /= denom;
    swaptotal /= denom;
    swapused  /= denom;
    swapfree  /= denom;
    buffers   /= denom;
    cached    /= denom;

    // print out the result
    if(unit_str)
    {
        if(unit_str[1] == '\0')
        {
            printf("             total         used         free   buff/cache\n");
        }
        else
        {
            printf("              total          used          free    buff/cache\n");
        }

        printf("Mem:   %10lu%s  %10lu%s  ", memtotal, unit_str, memused, unit_str);
        printf("%10lu%s  %10lu%s\n", memfree, unit_str, buffers + cached, unit_str);
        printf("Swap:  %10lu%s  %10lu%s  ", swaptotal, unit_str, swapused, unit_str);
        printf("%10lu%s\n", swapfree, unit_str);
    }
    else
    {
        printf("            total        used        free  buff/cache\n");
        printf("Mem:   %10lu  %10lu  %10lu  %10lu\n", 
                memtotal, memused, memfree, buffers + cached);
        printf("Swap:  %10lu  %10lu  %10lu\n", swaptotal, swapused, swapfree);
    }

    // print out the totals if wanted
    if(show_total)
    {
        if(unit_str)
        {
            printf("Total: %10lu%s  %10lu%s  ", memtotal + swaptotal, unit_str,
                                                memused + swapused, unit_str);
            printf("%10lu%s\n", memfree + swapfree, unit_str);
        }
        else
        {
            printf("Total: %10lu  %10lu  %10lu\n", memtotal + swaptotal,
                                                   memused + swapused,
                                                   memfree + swapfree);
        }
    }
}

