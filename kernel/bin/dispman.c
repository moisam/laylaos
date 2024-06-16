/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: dispman.c
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
 *  \file dispman.c
 *
 *  The main display manager. It is started by init after the system has
 *  finished starting up. It spawns a getty task for each virtual terminal
 *  on the system.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <kernel/tty.h>     // VT_SWITCH_TTY & NTTYS

#define DESKTOP_CMD                 "/bin/desktop/guiserver"
#define GETTY_CMD                   "/bin/getty"
#define DEFAULT_TTY                 "tty2"

#define TARGET_SINGLE_USER          1
#define TARGET_MULTI_USER           2
#define TARGET_DEFAULT              TARGET_MULTI_USER

pid_t child_pid[NTTYS] = { 0, };

char *ver = "1.0";

int target = TARGET_DEFAULT;
int force_login = 0;
int nogui = 0;
char *username = "root";
char *switchtty = DEFAULT_TTY;


void print_help(char *myname)
{
    printf("dispman (display manager) for LaylaOS, version %s\n\n", ver);

    printf("Usage: %s [--target=TARGET] [--switch-tty=TTY]\n\n", myname);
    printf("Initialise ttys and call getty on each virtual tty.\n\n");
    printf("Options:\n");
    printf("  -h, --help            Show help (this page) and exit\n");
    printf("  -n, --nogui           Do not start the graphical interface\n");
    printf("  -s, --switch-tty=TTY  Switch to the given tty, which must be"
           "                          one of the special device files listed "
                                      "under /dev\n");
    printf("  -s, --switch-tty TTY  Same as above, except TTY is passed in a"
           "                          separate argument\n");
    printf("  -t, --target=TARGET   Use the passed TARGET, which can be one of"
           "                          'default', 'multi-user', or 'signle-user'\n");
    printf("  -t, --target TARGET   Same as above, except TARGET is passed in a"
           "                          separate argument\n");
    printf("  -v, --version         Show version and exit\n");
    printf("\nTARGETs are passed to dispman from init on system startup.\n");
    printf("If no TARGET is passed, the buildin DEFAULT target is used.\n");
    printf("Unknown options and/or arguments are ignored\n\n");
}


void parse_line_args(int argc, char **argv) 
{
    int c;
    static struct option long_options[] =
    {
        {"help",       no_argument      , 0, 'h'},
        {"nogui",      no_argument      , 0, 'n'},
        {"switch-tty", required_argument, 0, 's'},
        {"target",     required_argument, 0, 't'},
        {"version",    no_argument      , 0, 'v'},
        {0, 0, 0, 0}
    };
  
    while((c = getopt_long(argc, argv, "hns:t:v", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 'n':
                nogui = 1;
                break;

            case 's':
                if(strlen(optarg) > 16)
                {
                    fprintf(stderr, "%s: ignoring long tty name: %s\n",
                                    argv[0], optarg);
                }
                else
                {
                    switchtty = optarg;
                }

                break;

            case 't':
                if(strcmp(optarg, "single-user") == 0)
                {
                    target = TARGET_SINGLE_USER;
                }
                else if(strcmp(optarg, "multi-user") == 0)
                {
                    target = TARGET_MULTI_USER;
                }
                else if(strcmp(optarg, "default") == 0)
                {
                    target = TARGET_DEFAULT;
                }
                else
                {
                    fprintf(stderr, "%s: ignoring unknown target: %s\n",
                                    argv[0], optarg);
                }

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


void fork_getty(char *myname, int i)
{
    char *args[7];
    char ttyname[8];
    int j = 2;

    sprintf(ttyname, "tty%d", i);
    args[0] = GETTY_CMD;
    args[1] = ttyname;
    
    if(force_login)
    {
        args[j++] = "-a";
        args[j++] = username;
    }

    // use tty2 exclusively for the gui desktop
    if(!nogui && i == 2)
    {
        args[j++] = "-l";
        args[j++] = DESKTOP_CMD;
    }

    args[j] = NULL;
    
    if(!(child_pid[i] = fork()))
    {
        execvp(args[0], args);
        exit(EXIT_FAILURE);
    }
    else if(child_pid[i] < 0)
    {
        fprintf(stderr, "%s: failed to fork\n", myname);
    }
}


int main(int argc, char **argv)
{
    int i, res, status;
    char ttypath[32];

    parse_line_args(argc, argv);
    
    if(target == TARGET_SINGLE_USER)
    {
        struct passwd *pwd = getpwuid(getuid());
        
        if(pwd)
        {
            username = pwd->pw_name;
            force_login = 1;
        }
    }

    fprintf(stderr, "%s: forking getty\n", argv[0]);

    for(i = 2; i < NTTYS; i++)
    {
        fork_getty(argv[0], i);
    }

    sprintf(ttypath, "/dev/%s", switchtty);

    if((i = open(ttypath, O_RDONLY|O_NOCTTY|O_NONBLOCK)) < 0)
    {
        fprintf(stderr, "%s: failed to open tty: %s\n",
                        argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // if 0 is passed as arg, use the tty device referenced by
    // the given file descriptor
    ioctl(i, VT_SWITCH_TTY, 0);

    close(i);

    fprintf(stderr, "%s: waiting for children\n", argv[0]);
    
    while(1)
    {
        do
        {
            res = waitpid(-1, &status, 0);
        } while(res < 0);
        
        for(i = 2; i < NTTYS; i++)
        {
            if(child_pid[i] == res /* && WIFSIGNALED(status) */)
            {
                fprintf(stderr, "%s: respawning getty\n", argv[0]);
                fork_getty(argv[0], i);
                break;
            }
        }
    }
    
    return 0;
}

