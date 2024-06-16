/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: login.c
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
 *  \file login.c
 *
 *  A simple login program.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>

#include "login-creds.c"

char *ver = "1.0";

char *username = NULL;


void print_help(char *myname)
{
    printf("login for LaylaOS, version %s\n\n", ver);

    printf("Usage: %s [-f username]\n\n", myname);
    printf("Begin a login session on the system.\n\n");
    printf("Options:\n");
    printf("  -h, --help            Show help (this page) and exit\n");
    printf("  -v, --version         Show version and exit\n");
    printf("  -f, --force-login user  \n"
           "                        Do not perform authentication, user is\n"
           "                          pre-authenticated\n");
    printf("Unknown options and/or arguments are ignored\n\n");
}


void parse_line_args(int argc, char **argv) 
{
    int c;
    static struct option long_options[] =
    {
        {"help",        no_argument      , 0, 'h'},
        {"force-login", required_argument, 0, 'f'},
        {"version",    no_argument       , 0, 'v'},
        {0, 0, 0, 0}
    };
  
    while((c = getopt_long(argc, argv, "hf:v", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 'f':
                username = optarg;
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
    
    //extra_argc = argc - optind;
    //extra_argv = argv + optind;
}


int main(int argc, char **argv)
{
    pid_t child_pid;
    int res, status;
    struct passwd *pwd = NULL;

    parse_line_args(argc, argv);

    if(username)
    {
        if(!(pwd = getpwnam(username)))
        {
            fprintf(stderr, "%s: cannot find user '%s' in database\n",
                            argv[0], username);
            exit(EXIT_FAILURE);
        }
    }

    if(!pwd)
    {
        while(1)
        {
            char name[1024];
            char pass[1024];
            char host[256];

            name[0] = '\0';
            pass[0] = '\0';
            
            gethostname(host, 255);
            
            fprintf(stdout, "%s login: ", host);
            fflush(stdout);
            
            if(fgets(name, 1024, stdin) == NULL)
            {
                clearerr(stdin);
                fprintf(stderr, "\nLogin failed.\n\n");
                continue;
            }
            
            name[1023] = '\0';
            
            /*
             * TODO: read and authenticate the password.
             */

            if(!(pwd = getpwnam(name)))
            {
                fprintf(stderr, "%s: cannot find user '%s' in database\n",
                                argv[0], name);
                continue;
            }
            
            break;
        }
    }

    if(!(child_pid = fork()))
    {
        char *shell_args[2];

        set_creds(pwd);
        setsid();

        shell_args[0] = getenv("SHELL");
        shell_args[1] = NULL;

        execvp(shell_args[0], shell_args);
        exit(EXIT_FAILURE);
    }
    else if(child_pid < 0)
    {
        fprintf(stderr, "%s: failed to fork: %s\n",
                        argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    do
    {
        res = waitpid(child_pid, &status, 0);
    } while(res < 0);
    
    __asm__ __volatile__("xchg %%bx, %%bx"::);
    if(res == child_pid && WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }

    return 0;
}

