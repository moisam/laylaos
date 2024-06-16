/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: init.c
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
 *  \file init.c
 *
 *  The init task. This is the initial user task that is run by the kernel
 *  after the system is finished booting, and it has the PID of 1. It is the
 *  parent (or grand-parent) of all the user tasks on the system. It finishes
 *  the boot process by mounting disks, initialising ttys, forking the 
 *  display manager thask that forks getty tasks to allow the user to log in.
 *  It then waits to reap zombie children. It also handles system shutdown.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <mntent.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <kernel/vfs.h>
#include <kernel/ttydefaults.h>
#include <gui/fb.h>
#include <kernel/tty.h>     // VT_SWITCH_TTY

#include <sys/syscall.h>
//#include <sys/syscall_macros.h>


#define INIT_VER                    "1.0"

#define TARGET_SINGLE_USER          1
#define TARGET_MULTI_USER           2
#define TARGET_DEFAULT              TARGET_MULTI_USER

int target = TARGET_DEFAULT;
char *my_name = "init";
char *condev = "/dev/tty0";
//char *condev = "/dev/console";
volatile int received_sighup = 0, received_sigint = 0, received_sigchld = 0;

struct child
{
    pid_t pid;
    int exit_status;
} child;

#define INIT_DEBUG          INIT_LOG

#define INIT_LOG(msg, ...)              \
{                                       \
    fprintf(stderr, "%s: ", my_name);   \
    fprintf(stderr, msg, __VA_ARGS__);  \
    fprintf(stderr, "\n");              \
}

#define INIT_WARN(msg, ...)                     \
{                                               \
    fprintf(stderr, "%s: warning: ", my_name);  \
    fprintf(stderr, msg, __VA_ARGS__);          \
    fprintf(stderr, "\n");                      \
}

#define INIT_MSG(msg)                   \
    { fprintf(stderr, "%s: %s\n", my_name, msg); }

#define INIT_LOG_UNKNOWN_OPTION(c)      \
    { fprintf(stderr, "%s: unknown option -- %c\n", my_name, c); }

#define INIT_LOG_UNKNOWN_OPTIONSTR(s)   \
    { fprintf(stderr, "%s: unknown option -- %s\n", my_name, s); }

#define INIT_LOG_UNKNOWN_ARGUMENT(a)    \
    { fprintf(stderr, "%s: unknown argument -- %s\n", my_name, a); }

#define INIT_LOG_UNKNOWN_TARGET(t)                              \
{                                                               \
    fprintf(stderr, "%s: unknown target -- %s\n", my_name, t);  \
    exit(1);                                                    \
}

#define INIT_LOG_MISSING_TARGET()       \
    { fprintf(stderr, "%s: missing target\n", my_name); }

#define __EXIT_ERR(code, fmt, ...)              \
{                                               \
    fprintf(stderr, "%s: fatal: ", my_name);    \
    fprintf(stderr, fmt, __VA_ARGS__);          \
    fprintf(stderr, "\n");                      \
    exit(code);                                 \
}

#define INIT_EXIT_ERR(fmt, ...)                 \
    __EXIT_ERR(2, fmt, __VA_ARGS__)

#define INIT_CHILD_EXIT_ERR(fmt, ...)           \
    __EXIT_ERR(255, fmt, __VA_ARGS__)


void shutdown(void)
{
    int status;
    int tries = 5, itries = 5;

    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    // ensure we are on the system console (i.e. tty0 == the 1st tty)
    ioctl(0, VT_SWITCH_TTY, 1);

    INIT_MSG("shutdown: sending SIGTERM to all processes");
    kill(-1, SIGTERM);
    sleep(5);

    INIT_MSG("shutdown: sending SIGKILL to all processes");
    kill(-1, SIGKILL);

    while(tries--)
    {
        INIT_LOG("waiting for child processes to exit (try %d/%d)",
                    (itries - tries), itries);
            
        if(waitpid(-1, &status, WNOHANG) < 0 && errno == ECHILD)
        {
            break;
        }
        
        sleep(5);
    }

    INIT_MSG("syncing filesystems");
    sync();
    
    if(received_sighup)
    {
        INIT_MSG("Restarting system.\n");
        exit(1);
    }
    
    INIT_MSG("Power down.\n");
    exit(2);
}


static inline void set_sigaction(int signum, void (*handler)(int), int flags)
{
    struct sigaction act;

    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = handler;
    act.sa_flags = SA_RESTART;
    
    (void)sigaction(signum, &act, NULL);
}


void sig_handler(int signum __attribute__((unused)))
{
}


void init_sigint_handler(int signum __attribute__((unused)))
{
    /*
     * TODO: inplement ACPI system shutdown.
     *
     * For details, see: https://wiki.osdev.org/Shutdown
     */
    
    INIT_MSG("received SIGINT");
    received_sigint = 1;

    shutdown();
}


void init_sighup_handler(int signum __attribute__((unused)))
{
    INIT_MSG("received SIGHUP");
    received_sighup = 1;

    shutdown();
}


void init_sigchld_handler(int signum __attribute__((unused)))
{
    int        pid, st;
    int        saved_errno = errno;

    while((pid = waitpid(-1, &st, WNOHANG)) != 0)
    {
        if(errno == ECHILD)
        {
            break;
        }
        
        if(pid == child.pid)
        {
            //child.pid = 0;
            child.exit_status = st;
            received_sigchld = 1;
        }
        else
        {
            printf("%s: unknown child exited (pid %d)\n", my_name, pid);
        }
    }

    errno = saved_errno;
}


#define MNTALL_BUFSZ            0x1000

char *malloc_str(char *str, size_t len)
{
    char *s2 = (char *)malloc(len+1);
    
    if(!s2)
    {
        return NULL;
    }
    
    memcpy(s2, str, len);
    s2[len] = '\0';

    return s2;
}


/*
 * Check if a mount options string contains the given option.
 */
static int options_contains(char *options, char *op)
{
    size_t oplen = strlen(op);
    
    if(!options)
    {
        return 0;
    }
    
    while(*options)
    {
        if(*options != *op)
        {
            goto next;
        }
        
        if(strncmp(options, op, oplen) != 0)
        {
            goto next;
        }
        
        options += oplen;
        
        if(*options == '\0' || *options == ',')
        {
            return 1;
        }

next:
        // skip to the next option
        while(*options && *options != ',')
        {
            options++;
        }
            
        while(*options == ',')
        {
            options++;
        }
    }
    
    return 0;
}


int mountall(void)
{
    int res;
    char *buf = (char *)malloc(MNTALL_BUFSZ);
    volatile char *b;
    char *word = (char *)malloc(1024);
    volatile char *wordp;
    char *fields[6];
    ssize_t j;
    int n;
    int flags;
    FILE *f;
    
    if(!buf || !word)
    {
        return ENOMEM;
    }
    
    if(geteuid() != 0)
    {
        free(buf);
        free(word);
        return EPERM;
    }

    if((f = fopen(MNTTAB, "r")) == NULL)
    {
        free(buf);
        free(word);
        return errno;
    }
    
    wordp = word;
    n = 0;
    memset(fields, 0, sizeof(fields));

    while(fgets(buf, MNTALL_BUFSZ, f))
    {
        b = buf;
        
        while(*b)
        {
            // skip commented lines
            if(*b == '#')
            {
                while(*b && *b != '\n')
                {
                    b++;
                }
                
                if(n == 0)
                {
                    wordp = word;
                    break;
                }
            }

            if(*b == ' ' || *b == '\t' || *b == '\n' || *b == '\0')
            {
                if(!(fields[n] = malloc_str(word, (size_t)(wordp - word))))
                {
                    INIT_LOG("mountall: insufficient memory: %s", strerror(errno));
                    fclose(f);
                    free(buf);
                    free(word);
                    return ENOMEM;
                }
                
                if(*b == '\n' || *b == '\0')
                {
                    // no fields (i.e. empty line)?
                    if(n == 0)
                    {
                        break;
                    }
                    
                    if(fields[2] && strcmp(fields[2], "ignore") != 0)
                    {
                        flags = 0;
                        
                        /*
                        if(options_contains(fields[3], "force"))
                        {
                            flags |= MS_FORCE;
                        }
                        
                        if(options_contains(fields[3], "sysroot"))
                        {
                            flags |= MS_SYSROOT;
                        }
                        */

                        if(options_contains(fields[3], "remount"))
                        {
                            flags |= MS_REMOUNT;
                        }

                        if(options_contains(fields[3], "ro"))
                        {
                            flags |= MS_RDONLY;
                        }
                        
                        if((res = mount(fields[0], fields[1], fields[2], 
                                        flags, fields[3])) != 0)
                        {
                            INIT_LOG("mountall: failed to mount %s on %s: %s\n",
                                     fields[0], fields[1], strerror(errno));

                            if(!options_contains(fields[3], "skip-errors"))
                            {
                                fclose(f);
                                free(buf);
                                free(word);
                                return errno;
                            }
                        }
                        else
                        {
                            INIT_LOG("mountall: mounted %s on %s", 
                                        fields[0], fields[1]);
                        }
                    }
                    else
                    {
                        INIT_LOG("mountall: ignoring entry: %s", fields[0]);
                    }
                    
                    for(j = 0; j < 6; j++)
                    {
                        if(fields[j])
                        {
                            free(fields[j]);
                        }
                    }

                    memset(fields, 0, sizeof(fields));
                    n = 0;
                    wordp = word;
                    break;
                }
                else
                {
                    n++;
                }
                
                while(*b == ' ' || *b == '\t' || *b == '\n')
                {
                    b++;
                }

                wordp = word;
                continue;
            }
            
            *wordp++ = *b++;
        }
    }

    for(j = 0; j < 6; j++)
    {
        if(fields[j])
        {
            free(fields[j]);
        }
    }

    fclose(f);
    free(buf);
    free(word);
    return 0;
}


/*
 *    Set default terminal settings
 */
void reset_console(void)
{
    struct termios tty;
    int fd;

    if((fd = open(condev, O_RDONLY|O_NOCTTY|O_NONBLOCK)) < 0)
    {
        INIT_WARN("failed to open console dev: %s", strerror(errno));
        return;
    }

    (void) tcgetattr(fd, &tty);

    tty.c_cc[VINTR]     = CINTR;
    tty.c_cc[VQUIT]     = CQUIT;
    tty.c_cc[VERASE]    = CERASE; /* ASCII DEL (0177) */
    tty.c_cc[VKILL]     = CKILL;
    tty.c_cc[VEOF]      = CEOF;
    tty.c_cc[VTIME]     = CTIME;
    tty.c_cc[VMIN]      = CMIN;
    tty.c_cc[VSWTC]     = '\0';
    tty.c_cc[VSTART]    = CSTART;
    tty.c_cc[VSTOP]     = CSTOP;
    tty.c_cc[VSUSP]     = CSUSP;
    tty.c_cc[VEOL]      = CEOL;
    tty.c_cc[VREPRINT]  = CREPRINT;
    tty.c_cc[VDISCARD]  = CDISCARD;
    tty.c_cc[VWERASE]   = CWERASE;
    tty.c_cc[VLNEXT]    = CLNEXT;
    tty.c_cc[VEOL2]     = CEOL;

    /*
     *    Set pre and post processing
     */
    tty.c_iflag = TTYDEF_IFLAG;
    tty.c_oflag = TTYDEF_OFLAG;
    tty.c_lflag = TTYDEF_LFLAG;
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
    (void) close(fd);
}


void init_help(void)
{
    printf("Usage: %s [-OPTION...] [TARGET] [--target=TARGET]"
           " [--target TARGET]\n\n", my_name);
    printf("Initialise and manage userland.\n\n");
    printf("Options:\n");
    printf("  -D, --default         Init default target (multi-user)\n");
    printf("  -M, --multi-user      Init multi-user target\n");
    printf("  -S, --single-user     Init single-user target\n");
    printf("  -h, --help            Show help (this page) and exit\n");
    printf("  -v, --version         Show version and exit\n");
    printf("  --target=TARGET       Init the passed TARGET, which can be one of"
           "                        'default', 'multi-user', or 'signle-user'\n");
    printf("  --target TARGET       Same as above, except TARGET is passed in a"
           "                        separate argument\n");
    printf("\nTargets can also be passed with no leading '--'.\n");
    printf("Unknown options and/or arguments are ignored\n\n");
    exit(0);
}


void init_version(void)
{
    printf("%s %s\n", my_name, INIT_VER);
    exit(0);
}


int maybe_target_name(char *arg)
{
    INIT_LOG("checking argument: %s", arg);
    
    if(strcmp(arg, "single-user") == 0)
    {
        target = TARGET_SINGLE_USER;
        return 1;
    }

    if(strcmp(arg, "multi-user") == 0)
    {
        target = TARGET_MULTI_USER;
        return 1;
    }

    if(strcmp(arg, "default") == 0)
    {
        target = TARGET_DEFAULT;
        return 1;
    }
    
    return 0;
}


void parse_args(int argc, char **argv)
{
    int v;
    
    for(v = 1; v < argc; v++)
    {
        char *p = argv[v];
        
        /* options start with '-', others are arguments */
        if(*p != '-')
        {
            if(!maybe_target_name(p))
            {
                INIT_LOG_UNKNOWN_ARGUMENT(p);
            }

            INIT_LOG("found target: %s", p);
            continue;
        }
        
        /* stop parsing options when we hit '-' or '--' */
        if(strcmp(p, "-") == 0)
        {
            break;
        }

        if(p[0] == '-' && p[1] == '-')
        {
            p += 2;

            if(*p == '\0')          /* '--' */
            {
                v++;
                break;
            }

            if(maybe_target_name(p))
            {
                continue;
            }

            if(strncmp(p, "target", 6) == 0)
            {
                p += 6;
            
                if(*p == '\0')
                {
                    if(++v == argc)
                    {
                        INIT_LOG_MISSING_TARGET();
                    }
                    
                    p = argv[v];
                }
                else if(*p == '=')
                {
                    p++;
                }
                else
                {
                    INIT_LOG_UNKNOWN_OPTIONSTR(p);
                    continue;
                }
                    
                if(*p == '\0')
                {
                    INIT_LOG_MISSING_TARGET();
                }
                    
                if(!maybe_target_name(p))
                {
                    INIT_LOG_UNKNOWN_TARGET(p);
                }
                    
                continue;
            }

            if(strcmp(p, "help") == 0)
            {
                init_help();
            }

            if(strcmp(p, "version") == 0)
            {
                init_version();
            }
            
            INIT_LOG_UNKNOWN_OPTIONSTR(p);
            continue;
        }
        
        /* skip the '-' and parse the options string */
        while(*++p)
        {
            switch(*p)
            {
                case 'S':
                    target = TARGET_SINGLE_USER;
                    break;

                case 'M':
                    target = TARGET_MULTI_USER;
                    break;

                case 'D':
                    target = TARGET_DEFAULT;
                    break;

                case 'h':
                    init_help();
                    break;

                case 'v':
                    init_version();
                    break;
                    
                default:
                    INIT_LOG_UNKNOWN_OPTION(*p);
                    break;
            }
        }
    }

    for( ; v < argc; v++)
    {
        if(!maybe_target_name(argv[v]))
        {
            INIT_LOG_UNKNOWN_ARGUMENT(argv[v]);
        }
    }
}


int init(void)
{
    int i;
    
    INIT_MSG("mounting filesystems");

    if((i = mountall()) != 0)
    {
        INIT_EXIT_ERR("failed to mount filesystems: %s", strerror(i));
    }

    // now setup our SIGINT and SIGHUP signal handlers, which we use to handle
    // system shutdown and reboot
    set_sigaction(SIGINT, init_sigint_handler, SA_RESTART);
    set_sigaction(SIGHUP, init_sighup_handler, SA_RESTART);
    set_sigaction(SIGCHLD, init_sigchld_handler, SA_RESTART);

    set_sigaction(SIGALRM, sig_handler, 0);
    set_sigaction(SIGPWR, sig_handler, 0);
    set_sigaction(SIGWINCH, sig_handler, 0);
    set_sigaction(SIGUSR1, sig_handler, 0);
    set_sigaction(SIGUSR2, sig_handler, 0);
    set_sigaction(SIGSTOP, sig_handler, SA_RESTART);
    set_sigaction(SIGTSTP, sig_handler, SA_RESTART);
    set_sigaction(SIGCONT, sig_handler, SA_RESTART);
    set_sigaction(SIGSEGV, sig_handler, SA_RESTART);

    set_sigaction(SIGQUIT, sig_handler, 0);

    sigset_t nmask, omask;	/* For blocking SIGCHLD */
    struct termios tio;
    sigset_t oldset, sigttou;
    sigemptyset(&sigttou);
    sigaddset(&sigttou, SIGTTOU);
    
spawn:

    received_sigchld = 0;
    child.exit_status = 0;
        
    if(tcgetattr(0, &tio))
    {
        INIT_EXIT_ERR("failed to get terminal attribs: %s", strerror(errno));
    }

    // Block sigchild while forking
    sigemptyset(&nmask);
    sigaddset(&nmask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &nmask, &omask);

    if((child.pid = fork()) < 0)
    {
        INIT_EXIT_ERR("failed to fork: %s", strerror(errno));
    }
    else if(child.pid == 0)
    {
        pid_t pid = getpid();
        char *exe = "/bin/dispman";
        char *argv[] = { exe,
                         "--nogui",
                         "--target",
                         target == TARGET_SINGLE_USER ? 
                            "single-user" : "multi-user", NULL };

        sigprocmask(SIG_SETMASK, &omask, NULL);

        i = execve(exe, argv, environ);

        INIT_CHILD_EXIT_ERR("child %d: failed to exec %s: %s\n", 
                            pid, exe, strerror(errno));
    }

    sigprocmask(SIG_SETMASK, &omask, NULL);
        
    INIT_DEBUG("child.pid = %d", child.pid);

    while(1)
    {
        pause();

        if(received_sigchld && /* child.exit_status && */
           (WIFEXITED(child.exit_status) || WIFSIGNALED(child.exit_status)))
        {
            received_sigchld = 0;

            sigprocmask(SIG_BLOCK, &sigttou, &oldset);

            if(tcsetattr(0, TCSAFLUSH, &tio))
            {
                INIT_WARN("failed to flush console: %s", strerror(errno));
            }

            if(tcsetpgrp(0, getpgrp()) < 0)
            {
                INIT_WARN("failed to set console pgid: %s", strerror(errno));
            }

            (void)ioctl(0, TIOCSCTTY, 1);
            reset_console();

            sigprocmask(SIG_SETMASK, &oldset, NULL);

            if(WIFEXITED(child.exit_status))
            {
                //result = WEXITSTATUS(status);
                //break;
                INIT_LOG("child %d exited (status %d)",
                         child.pid, WEXITSTATUS(child.exit_status));
            }
            else if(WIFSIGNALED(child.exit_status))
            {
                INIT_LOG("child %d terminated by a signal (%s)",
                         child.pid, strsignal(WTERMSIG(child.exit_status)));
            }
            /*
            else
            {
                INIT_LOG("child %d died unexpectedly (status %d)",
                         child.pid, child.exit_status);
            }
            */

            /*
             * Do not respawn the child if there was an error prior to (or 
             * during) exec, such as setting environment or getting user
             * info, as these errors won't be fixed by respawning the child.
             */
            if(WEXITSTATUS(child.exit_status) != 255)
            {
                INIT_MSG("respawning child\n");
                //__asm__("xchg %%bx, %%bx"::);

                goto spawn;
            }
        }
    }
    
    // NOTREACHED
    return 0;
}


int main(int argc, char **argv)
{
    INIT_MSG("init started");

    if((my_name = strrchr(argv[0], '/')) != NULL)
    {
        my_name++;
    }
    else
    {
        my_name = argv[0];
    }

    if(geteuid() != 0)
    {
        INIT_MSG("must be root!");
        exit(1);
    }
    
    if(getpid() != 1)
    {
        INIT_MSG("init already running!");
        exit(1);
    }

    parse_args(argc, argv);
    umask(022);

    return init();
}

