/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: getty.c
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
 *  \file getty.c
 *
 *  The getty (get tty or terminal) program. For each virtual terminal on the
 *  system, the display manager (dispman.c) forks a getty task that waits on
 *  the tty and then forks a login task for the user to login.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <kernel/tty.h>

#define LOGIN_CMD                   "/bin/login"
#define ISSUE_FILE                  "/etc/issue"

char *ver = "1.0";

int extra_argc = 0;
char **extra_argv = NULL;

char *auto_username = NULL;
char *initstr = NULL;
char *loginprog = LOGIN_CMD;
char *newroot = NULL;
char *newpwd = NULL;
int noreset = 0;
int noclear = 0;
int nonewline = 0;
int noissue = 0;

struct
{
    const char *name, *cmd;
} tty_colors[] =
{
    // basic colors
    { "black", "\033[30m" },
    { "blue", "\033[34m" },
    { "brown", "\033[33m" },
    { "cyan", "\033[36m" },
    { "darkgray", "\033[90m" },
    { "gray", "\033[37m" },
    { "green", "\033[32m" },
    { "magenta", "\033[35m" },
    { "red", "\033[31m" },
    { "white", "\033[37m" },

    // we don't have these -- use normal colors from above
    { "lightblue", "\033[34m" },
    { "lightcyan", "\033[36m" },
    { "lightgray", "\033[37m" },
    { "lightgreen", "\033[32m" },
    { "lightmagenta", "\033[35m" },
    { "lightred", "\033[31m" },

    // we don't have yellow -- use white instead
    { "yellow", "\033[37m" },

    // attributes
    { "bold", "\033[1m" },
    { "reset", "\033[0m" },
    { "halfbright", "\033[2m" },
    { "blink", "\033[5m" },
    { "reverse", "\033[7m" },
};

int tty_color_count = sizeof(tty_colors) / sizeof(tty_colors[0]);

static char *weekdays[] =
{
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

static char *months[] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

void parse_issue_file(void);


void print_help(char *myname)
{
    printf("getty for LaylaOS, version %s\n\n", ver);

    printf("Usage: %s [options] port [term]\n\n", myname);
    printf("Open a tty name and invoke the login program.\n\n");
    printf("Arguments:\n");
    printf("  port                  The name of a tty device under /dev\n");
    printf("  term                  The value to use for the $TERM env var\n");
    printf("Options:\n");
    printf("  -a, --autologin user  Automatically log the given user in [this\n"
           "                          adds an -f user option the the login\n"
           "                          program command-line]\n");
    printf("  -c, --noreset         Do not reset tty's cflags\n");
    printf("  -d, --chdir dir       Change directory before login\n");
    printf("  -h, --help            Show help (this page) and exit\n");
    printf("  -i, --noissue         Do not display the contents of /etc/issue\n");
    printf("  -l, --login-program program  \n"
           "                        Invoke the given program instead of /bin/login\n");
    printf("  -r, --chroot dir      Change root directory before login\n");
    printf("  -v, --version         Show version and exit\n");
    printf("  -I, --init-string string \n"
           "                        Send the given string to tty before anything else\n"
           "                          [e.g. to initialise the tty device]\n");
    printf("  -J, --noclear         Do not clear the screen\n");
    printf("  -N, --nonewline       Do not print a newline before displaying the\n"
           "                          contents of /etc/issue\n");
    printf("  -S, --show-issue      Display the contents of /etc/issue\n");
    printf("Unknown options and/or arguments are ignored\n\n");
}


void parse_line_args(int argc, char **argv) 
{
    int c;
    static struct option long_options[] =
    {
        {"help",        no_argument        , 0, 'h'},
        {"autologin",   required_argument  , 0, 'a'},
        {"noreset",     no_argument        , 0, 'c'},
        {"chdir",       required_argument  , 0, 'd'},
        {"noissue",     no_argument        , 0, 'i'},
        {"login-program", required_argument, 0, 'l'},
        {"chroot",      required_argument  , 0, 'r'},
        {"version",     no_argument        , 0, 'v'},
        {"init-string", required_argument  , 0, 'I'},
        {"noclear",     no_argument        , 0, 'J'},
        {"nonewline",   no_argument        , 0, 'N'},
        {"show-issue",  no_argument        , 0, 'S'},
        {0, 0, 0, 0}
    };
  
    while((c = getopt_long(argc, argv, "ha:cd:il:r:vI:JNS", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 'a':
                auto_username = optarg;
                break;

            case 'c':
                noreset = 1;
                break;

            case 'd':
                newpwd = optarg;
                break;

            case 'i':
                noissue = 1;
                break;

            case 'l':
                loginprog = optarg;
                break;

            case 'r':
                newroot = optarg;
                break;

            case 'v':
                printf("%s\n", ver);
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
                break;

            case 'I':
                initstr = optarg;
                break;

            case 'J':
                noclear = 1;
                break;

            case 'N':
                nonewline = 1;
                break;

            case 'S':
                parse_issue_file();
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


/*
 *    Set default terminal settings
 */
void reset_console(int fd)
{
    struct termios tty;

    if(!noreset)
    {
        return;
    }

    (void) tcgetattr(fd, &tty);

    //tty.c_cflag &= CBAUD|CBAUDEX|CSIZE|CSTOPB|PARENB|PARODD;
    //tty.c_cflag |= HUPCL|CLOCAL|CREAD;

    tty.c_cc[VINTR   ]  = CINTR;
    tty.c_cc[VQUIT   ]  = CQUIT;
    tty.c_cc[VERASE  ]  = CERASE; /* ASCII DEL (0177) */
    tty.c_cc[VKILL   ]  = CKILL;
    tty.c_cc[VEOF    ]  = CEOF;
    tty.c_cc[VTIME   ]  = CTIME;
    tty.c_cc[VMIN    ]  = CMIN;
    tty.c_cc[VSWTC   ]  = '\0';
    tty.c_cc[VSTART  ]  = CSTART;
    tty.c_cc[VSTOP   ]  = CSTOP;
    tty.c_cc[VSUSP   ]  = CSUSP;
    tty.c_cc[VEOL    ]  = CEOL;
    tty.c_cc[VREPRINT]  = CREPRINT;
    tty.c_cc[VDISCARD]  = CDISCARD;
    tty.c_cc[VWERASE ]  = CWERASE;
    tty.c_cc[VLNEXT  ]  = CLNEXT;
    tty.c_cc[VEOL2   ]  = CEOL;

    /*
     *    Set pre and post processing
     */
    tty.c_iflag = TTYDEF_IFLAG; // IGNPAR|ICRNL|IXON|IXANY;
    tty.c_oflag = TTYDEF_OFLAG; // OPOST|ONLCR;
    tty.c_lflag = TTYDEF_LFLAG; // ISIG|ICANON|ECHO|ECHOCTL|ECHOE|ECHOKE;
    tty.c_cflag = TTYDEF_CFLAG;

    /*
     *    Disable flow control (-ixon), ignore break (ignbrk),
     *    and make nl/cr more usable (sane).
     */
    tty.c_iflag |=  IGNBRK;
    //tty.c_iflag &= ~(BRKINT|INLCR|IGNCR|IXON);
    //tty.c_oflag &= ~(OCRNL|ONLRET);

    /*
     *    Now set the terminal line.
     *    We don't care about non-transmitted output data
     *    and non-read input data.
     */
    (void) tcsetattr(fd, TCSANOW, &tty);
    (void) tcflush(fd, TCIOFLUSH);
}


char *getarg(FILE *f)
{
    static char arg[32];
    int c = fgetc(f);
    int i = 0;
    
    if(c == '{')
    {
        do
        {
            c = fgetc(f);
            
            if(c <= 0 || c == '}')
            {
                break;
            }
            
            arg[i++] = c;
            arg[i] = '\0';
        } while(i < 31);
        
        if(i)
        {
            return arg;
        }
    }
    
    return NULL;
}


void parse_issue_file(void)
{
    FILE *f;
    struct utsname uts;
    time_t t = 0;
    struct tm *tm = NULL;
    char *arg;
    int c;

    if(noissue)
    {
        return;
    }

    if(!nonewline)
    {
        printf("\n");
    }

    if(!(f = fopen(ISSUE_FILE, "r")))
    {
        return;
    }

    uname(&uts);
    time(&t);
    tm = gmtime(&t);

    while(!feof(f))
    {
        c = fgetc(f);
        
        // EOF or err
        if(c <= 0)
        {
            break;
        }
        
        // parse escaped chars
        if(c == '\\')
        {
            c = fgetc(f);
            
            /*
             * See `man 8 getty` for the details of these escape sequences.
             */
            switch(c)
            {
                case '\n':
                    break;

                case '\\':
                    printf("\\");
                    break;

                case 'd':
                    printf("%s %d %s", weekdays[tm->tm_wday], tm->tm_mday,
                                       months[tm->tm_mon]);
                    break;

                case 'l':
                    printf("%s", ttyname(0));
                    break;

                case 'm':
                    printf("%s", uts.machine);
                    break;

                case 'n':
                    printf("%s", uts.nodename);
                    break;

                case 'o':
                case 'O':
                    printf("%s", uts.domainname);
                    break;

                case 'r':
                    printf("%s", uts.release);
                    break;

                case 's':
                    printf("%s", uts.sysname);
                    break;

                case 't':
                    printf("%02u:%02u:%02u", tm->tm_hour, tm->tm_min, tm->tm_sec);
                    break;

                case 'v':
                    printf("%s", uts.version);
                    break;

                case 'e':
                    if((arg = getarg(f)))
                    {
                        for(c = 0; c < tty_color_count; c++)
                        {
                            if(strcmp(arg, tty_colors[c].name) == 0)
                            {
                                printf("%s", tty_colors[c].cmd);
                                break;
                            }
                        }
                    }
                    else
                    {
                        printf("\033");
                    }

                    break;
            }
        }
        else
        {
            printf("%c", c);
        }
    }

    fclose(f);
}


int main(int argc, char **argv)
{
    int fd;
    char ttypath[32];
    char *login_args[4];

    parse_line_args(argc, argv);

    if(geteuid() != 0)
    {
        fprintf(stderr, "%s: you must be root!\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if(!extra_argc)
    {
        fprintf(stderr, "%s: missing tty name\n", argv[0]);
        fprintf(stderr, "Type `%s --help` for usage\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    extra_argc--;
    sprintf(ttypath, "/dev/%s", *extra_argv++);

    if((fd = open(ttypath, O_RDWR)) < 0)
    {
        fprintf(stderr, "%s: failed to open %s: %s\n",
                        argv[0], ttypath, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    setpgid(0, 0);
    setsid();
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);

    reset_console(fd);
    close(fd);

    /* Take over controlling tty by force */
    (void)ioctl(0, TIOCSCTTY, 1);

    int flags = fcntl(0, F_GETFL);
    //__asm__("xchg %%bx, %%bx"::);
    (void)fcntl(0, F_SETFL, flags | O_NOATIME);

    if(newroot)
    {
        chroot(newroot);
    }

    if(newpwd)
    {
        chdir(newpwd);
    }

    if(!noclear)
    {
        // move the cursor to the top-left and clear the screen
        write(1, "\033[1;1H", 6);
        write(1, "\033[2J", 4);
    }

    parse_issue_file();

    if(extra_argc)
    {
        setenv("TERM", *extra_argv, 1);
    }
    else
    {
        setenv("TERM", "vt100", 1);
    }

    login_args[0] = loginprog;
    login_args[3] = NULL;

    if(auto_username)
    {
        login_args[1] = "-f";
        login_args[2] = auto_username;
    }
    else
    {
        login_args[1] = NULL;
        login_args[2] = NULL;
    }
    
    while(1)
    {
        //fprintf(stderr, "%s: 9\n", argv[0]);
        pid_t child_pid;
        int res, status;
        
        if(!(child_pid = fork()))
        {
            execvp(login_args[0], login_args);
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
        
        /*
        if(WIFSIGNALED(status))
        {
            exit(255);
        }
        */
    }
}

