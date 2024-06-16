/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: cat.c
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
 *  \file cat.c
 *
 *  A simple program to print the contents of text files.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <errno.h>

#define MAX_FILENAMES       1024

char buf[0x1000];
char cat_ver[] = "1.0";

/* output options */
char NUMBER_NONBLANK = 0;
char NUMBER_ALL = 0;
char SQUEEZE_BLANK = 0;
char SHOW_TABS = 0;
char SHOW_ENDS = 0;


void parse_line_args(int argc, char **argv) 
{
    int c;
    static struct option long_options[] =
    {
        {"help",            no_argument,         0,  'h'},
        {"number",          no_argument,         0,  'n'},
        {"number-nonblank", no_argument,         0,  'b'},
        {"show-ends",       no_argument,         0,  'E'},
        {"show-tabs",       no_argument,         0,  'T'},
        {"squeeze-blank",   no_argument,         0,  's'},
        {"version",         no_argument,         0,  'v'},
        {0, 0, 0, 0}
    };
    
    //optind = 1;
    
    while(1)
    {
        int option_index = 0;
    
        c = getopt_long(argc, argv, "hnbETsvu", long_options, &option_index);
        
        if(c == -1)
        {
            break;    //end of options
        }
    
        switch(c)
        {
            case 0:
            case 'u':     /* ignore */
                break;
            
            case 'n':
                NUMBER_ALL = 1;
                break;
      
            case 'b':
                NUMBER_NONBLANK = 1;
                break;
            
            case 'E':
                SHOW_ENDS = 1;
                break;
            
            case 'T':
                SHOW_TABS = 1;
                break;
            
            case 's':
                SQUEEZE_BLANK = 1;
                break;
            
            case 'v':
                printf("%s\n", cat_ver);
                exit(0);
            
            case 'h':    //show program help
                printf("File concatenation utility for Layla OS, ver %s\n\n", cat_ver);
                printf("Usage: %s [options] [file ...]\n\n", argv[0]);
                printf("Options:\n");
                printf("  -b, --number-nonblank   Number nonempty output lines\n");
                printf("  -E, --show-ends         Show $ at the end of each line\n");
                printf("  -n, --number            Number all output lines\n");
                printf("  -s, --squeeze-blank     Suppress repeated empty output lines\n");
                printf("  -T, --show-tabs         Display TABs as ^I\n");
                printf("  -h, --help              Show help (this) and exit\n");
                printf("  -u                      Unbuffer output (ignored)\n");
                printf("  -v, --version           Print version and exit\n");
                exit(0);

            case '?':
                break;
            
            default:
                fprintf(stderr, "cat: unknown option: %c\n", c);
                abort();
        }
    }
}


int cat(char *name)
{
    int line_count = 1;
    int char_count = 0;
    FILE *f = (name[0] == '-' && name[1] == '\0') ? stdin : fopen(name, "r");
    
    if(!f)
    {
        fprintf(stderr, "cat: error opening '%s': %s\n", name, strerror(errno));
        return -1;
    }

    while(!feof(f))
    {
        size_t i, res = fread((void *)buf, 1, 0x1000, f);

        if(res <= 0)
        {
            fprintf(stderr, "cat: error reading '%s': %s\n", name, strerror(errno));

            if(f != stdin)
            {
                fclose(f);
            }

            return -1;
        }

        for(i = 0; i < res; i++)
        {
            if(char_count == 0)
            {
                if(NUMBER_ALL)
                {
                    printf("%-6u ", line_count);
                }
                else if(buf[i] != '\n' && NUMBER_NONBLANK)
                {
                    printf("%-6u ", line_count);
                }
            }
            
            if(buf[i] == '\t')
            {
                if(SHOW_TABS)
                {
                    printf("^I");
                }
                else
                {
                    putchar('\t');
                }
                
                char_count++;
            }
            else if(buf[i] == '\n')
            {
                line_count++;
                char_count = 0;
    
                if(SHOW_ENDS)
                {
                    putchar('$');
                }
                
                putchar('\n');
            }
            else
            {
                putchar(buf[i]);
                char_count++;
            }
        }
    }
    
    if(f != stdin)
    {
        fclose(f);
    }
    
    return 0;
}


int main(int argc, char *argv[])
{
    parse_line_args(argc, argv);

    /*
     * No arguments. Read from STDIN.
     */
    if(optind >= argc)
    {
        return cat("-") ? 1 : 0;
    }
  
    int exit_res = 0;
    
    while(optind < argc)
    {
        if(cat(argv[optind++]) != 0)
        {
            exit_res = 1;
        }
    }
  
    return exit_res;
}

