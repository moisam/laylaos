/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: gui-open.c
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
 *  \file gui-open.c
 *
 *  A utility program to open different file types, similar to e.g. xdg-open.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include "desktop/include/gui.h"

struct app_entry_t *first_entry = NULL;
struct app_entry_t *last_entry = NULL;

#include "desktop/desktop/desktop_entry_lister.c"

char *ver = "1.0";

int extra_argc = 0;
char **extra_argv = NULL;

#define MIME_BUFFER_SIZE 128
#define CMD_BUFFER_SIZE  512


char *get_mime_from_file_utility(char *myname, const char *filepath)
{
    char command[CMD_BUFFER_SIZE];
    static char mime_out[MIME_BUFFER_SIZE];
    volatile int tries = 0;
    
    snprintf(command, sizeof(command), "file --mime-type -b \"%s\"", filepath);
    
    FILE *fp = popen(command, "r");

    if(fp == NULL)
    {
        printf("%s: Failed to execute the 'file' utility: %s\n", myname, strerror(errno));
        return NULL;
    }
    
    // Read the single-line string containing the exact MIME type
    while(1)
    {
        if(fgets(mime_out, sizeof(mime_out), fp) != NULL)
        {
            break;
        }

        if(errno != EINTR)
        {
            printf("%s: Failed to read 'file' utility output: %s\n", myname, strerror(errno));
            pclose(fp);
            return NULL;
        }

        if(++tries >= 10)
        {
            printf("%s: Failed to read 'file' utility output: timeout\n", myname);
            pclose(fp);
            return NULL;
        }
    }

    pclose(fp);
    
    // Strip trailing newlines or whitespaces returned by terminal output
    mime_out[strcspn(mime_out, "\r\n")] = '\0';
    
    return mime_out;
}


void print_help(char *myname)
{
    printf("gui-open for LaylaOS, version %s\n\n", ver);

    printf("Usage: %s [options] filepath [args]\n\n", myname);
    printf("Open the given filepath using its MIME type.\n\n");
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
        {"help",       no_argument       , 0, 'h'},
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
    char *p, *mime, *pathname;
    char *newargv[512];
    int newargc = 0;
    struct app_entry_t *desktop_entry;

    parse_line_args(argc, argv);

    if(!extra_argc)
    {
        fprintf(stderr, "%s: missing filepath\n", argv[0]);
        fprintf(stderr, "Type `%s --help` for usage\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    pathname = *extra_argv;

    if(strncmp(pathname, "http://", 7) == 0 ||
       strncmp(pathname, "https://", 8) == 0)
    {
        mime = "text/html";
    }
    else
    {
        // if a local file is given, strip the leading file://
        if(strncmp(pathname, "file://", 7) == 0)
        {
            pathname += 7;
        }

        mime = get_mime_from_file_utility(argv[0], pathname);
    }

    if(!mime || strlen(mime) == 0)
    {
        printf("%s: Could not resolve file type\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    extra_argv++;
    extra_argc--;

    // if this is an executable program or a shell script, skip the code below
    if(strcmp(mime, "application/x-executable") == 0 ||
       strcmp(mime, "application/x-pie-executable") == 0)
    {
        // construct the new argv from the command and any args passed by the user
        newargv[newargc++] = pathname;

        while(extra_argc--)
        {
            newargv[newargc++] = *extra_argv;
            extra_argv++;
        }

        newargv[newargc] = NULL;
        execvp(newargv[0], newargv);
        exit(EXIT_FAILURE);
    }
    else if(strcmp(mime, "text/x-shellscript") == 0)
    {
        // construct the new argv from the command and any args passed by the user
        newargv[newargc++] = "/bin/sh";
        newargv[newargc++] = pathname;

        while(extra_argc--)
        {
            newargv[newargc++] = *extra_argv;
            extra_argv++;
        }

        newargv[newargc] = NULL;
        execvp(newargv[0], newargv);
        exit(EXIT_FAILURE);
    }

    // load default application categories and the applications list
    if(!(p = malloc(PATH_MAX)))
    {
        printf("%s: Insufficient memory\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    else
    {
        ftree(DEFAULT_DESKTOP_PATH, p);
        free(p);
    }

    for(desktop_entry = first_entry;
        desktop_entry != NULL;
        desktop_entry = desktop_entry->next)
    {
        if(!desktop_entry->mimetypes || !desktop_entry->mimetypes[0])
        {
            continue;
        }

        //printf("%s: %s\n", desktop_entry->name, desktop_entry->mimetypes);

        if(strstr(desktop_entry->mimetypes, mime))
        {
            // construct the new argv from the command (which may include
            // multiple words), the filepath, and any args passed by the user
            char *tok = strtok(desktop_entry->command, "\n\r\t ");

            while(tok)
            {
                newargv[newargc++] = tok;
                tok = strtok(NULL, "\n\r\t ");
            }

            newargv[newargc++] = pathname;

            while(extra_argc--)
            {
                newargv[newargc++] = *extra_argv;
                extra_argv++;
            }

            newargv[newargc] = NULL;
            execvp(newargv[0], newargv);
            exit(EXIT_FAILURE);
        }
    }

    printf("%s: Could not find a program to open file: %s\n", argv[0], pathname);
    printf("%s: Detected file type: %s\n", argv[0], mime);

    exit(EXIT_FAILURE);
}

