/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: echo.c
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
 *  \file echo.c
 *
 *  A simple implementation of the echo program.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

char DISABLE_NEW_LINE = 0;
char ENABLE_ESCAPE_CHARS = 0;
char echo_ver[] = "1.0";
char *arguments[1024];
int count = 0;


void parse_line_args(int argc, char **argv) 
{
    int c;

    static struct option long_options[] =
    {
        {"help",            no_argument,         0,  'h'},
        {"version",         no_argument,         0,  'v'},
        {0, 0, 0, 0}
    };

    //optind = 1;
  
    while(1)
    {
        int option_index = 0;

        c = getopt_long(argc, argv, "hvneE", long_options, &option_index);

        if(c == -1)
        {
            break;    //end of options
        }
    
        switch(c)
        {
            case 0:
	            break;
            
            case 'n':
	            DISABLE_NEW_LINE = 1;
	            break;
            
            case 'e':
	            ENABLE_ESCAPE_CHARS = 1;
	            break;
            
            case 'E':
	            ENABLE_ESCAPE_CHARS = 0;
	            break;
            
            case 'v':
	            printf("%s\n", echo_ver);
	            exit(0);

            case 'h':	//show program help
	            printf("echo utility for Layla OS, Version %s\n\n", echo_ver);
	            printf("Usage: %s [options] [file(s)]\n\n", argv[0]);
	            printf("Options:\n");
	            printf("  -e                      Enable interpretation of escaped characters\n");
	            printf("  -E                      Disable interpretation of escaped characters\n");
	            printf("  -h, --help              Show help (this) and exit\n");
	            printf("  -n                      Don't output trailing newline\n");
	            printf("  -v, --version           Print version and exit\n");
	            exit(0);

            case '?':
	            break;
            
            default:
                fprintf(stderr, "echo: unknown option: %c\n", c);
	            abort();
        }
    }
}


static char escape_chars[] = { '\\', '\a', '\b', '\0', '\e', '\f', '\n', '\r', '\t', '\v', ' ' };

static inline int is_escape_char(char c)
{
    switch(c)
    {
        case '\\': return 0;
        case 'a': return 1;
        case 'b': return 2;
        //case 'c': return 3; //produce no further output
        case 'e': return 4;
        case 'f': return 5;
        case 'n': return 6;
        case 'r': return 7;
        case 't': return 8;
        case 'v': return 9;
        case ' ': return 10;
    }
    
    return -1;
}


int main(int argc, char *argv[])
{
    //printf("echo: argc %d, argv 0x%x\n", argc, argv);
    parse_line_args(argc, argv);

    /*
     * No arguments.
     */
    if(argc == 1 || optind >= argc)
    {
        return 0;
    }

    while(optind < argc)
    {
        char *p = argv[optind++];

        while(*p)
        {
            if(*p == '\\' && ENABLE_ESCAPE_CHARS)
            {
                int k = is_escape_char(*++p);

                if(k >= 0)
                {
                    putchar(escape_chars[k]);
                    p++;
                    continue;
                }
            }
            
            putchar(*p++);
        }
        
        putchar(' ');
    }

    if(!DISABLE_NEW_LINE)
    {
        putchar('\n');
    }
    
    return 0;
}

