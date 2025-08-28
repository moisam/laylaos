#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "daemon.h"
#include "daemon-funcs.c"

static pid_t pid_for_daemon(struct daemon_t *d)
{
    DIR *procdir;
    struct dirent *dent;
    char buf[512];
    char buf2[PATH_MAX];
    size_t len;
    pid_t res;

    // Search /proc to find a task whose executable matches the daemon we are
    // looking for. We stop the search on the first hit.

    if(!(procdir = opendir("/proc")))
    {
        fprintf(stderr, "daemon: failed to read /proc: %s\n", strerror(errno));
        return 0;
    }
    
    while((dent = readdir(procdir)) != NULL)
    {
        if(dent->d_name[0] >= '0' && dent->d_name[0] <= '9')
        {
            sprintf(buf, "/proc/%s/exe", dent->d_name);

            if((len = readlink(buf, buf2, sizeof(buf2))) > 0)
            {
                buf2[len] = '\0';

                if(strcmp(buf2, d->cmd) == 0)
                {
                    res = atoi(dent->d_name);
                    closedir(procdir);
                    return res;
                }
            }
        }
    }

    closedir(procdir);

    return 0;
}


static int get_daemon_info(struct daemon_t *d)
{
    if(!d || !d->name || !d->name[0])
    {
        return 0;
    }

    char path[strlen(d->name) + 8];
    sprintf(path, "%s.daemon", d->name);

    if(!read_daemon_file("daemon", d, path))
    {
        return 0;
    }

    if(!d->cmd)
    {
        fprintf(stderr, "daemon: missing command name in %s\n", path);
        return 0;
    }

    d->pid = pid_for_daemon(d);

    if(d->pid)
    {
        if(kill(d->pid, 0) != 0 && errno == ESRCH)
        {
            d->pid = 0;
        }
    }

    return 1;
}


static int run_daemon(char *name)
{
    struct daemon_t d;

    d.name = name;

    if(!get_daemon_info(&d))
    {
        return 0;
    }

    if(d.pid)
    {
        return 1;
    }

    fork_daemon_task(&d);

    // Give the daemon a chance to run
    sched_yield();

    return !!pid_for_daemon(&d);
}


static int stop_daemon(char *name)
{
    struct daemon_t d;

    d.name = name;

    if(!get_daemon_info(&d))
    {
        return 0;
    }

    if(d.pid)
    {
        kill(d.pid, SIGKILL);
    }

    return 1;
}


static int stat_daemon(char *name, int print_status)
{
    struct daemon_t d;

    memset(&d, 0, sizeof(d));
    d.name = name;

    if(!get_daemon_info(&d))
    {
        if(print_status)
        {
            printf("daemon: could not stat daemon %s\n", name);
        }

        return 0;
    }

    if(print_status)
    {
        printf("         Name: %s\n", name);
        printf("  Description: %s\n", d.desc ? d.desc : "-");
        printf("      Command: %s\n", d.cmd ? d.cmd : "-");
        printf("  CommandArgs: %s\n", (d.cmdargs && d.cmdargs[0]) ? d.cmdargs : "None");
        printf("    Env $PATH: %s\n", (d.envpath && d.envpath[0]) ? d.envpath : "Standard $PATH");
        printf("          Pid: %d\n", d.pid);
        printf("       Status: %s\n", (d.pid) ? "running" : "stopped");
        printf("\n");
    }

    return !!(d.pid);
}


static void print_usage(char *myname)
{
    printf("daemon utility for LaylaOS\n\n"
           "Usage: %s cmd daemon\n\n"
           "Where:\n"
           "  cmd       start, restart, stop, status\n"
           "  daemon    name of the daemon to perform cmd on\n\n"
           , myname);
}


int main(int argc, char **argv)
{
    int start = 0, restart = 0, stop = 0, status = 0;
    int i, sum, cmd = 0;
    //char buf[512];

    // Parse the options
    for(i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "start") == 0)
        {
            start = 1;
            cmd = COMMAND_START;
        }
        else if(strcmp(argv[i], "restart") == 0)
        {
            restart = 1;
            cmd = COMMAND_RESTART;
        }
        else if(strcmp(argv[i], "stop") == 0)
        {
            stop = 1;
            cmd = COMMAND_STOP;
        }
        else if(strcmp(argv[i], "status") == 0)
        {
            status = 1;
            cmd = COMMAND_STATUS;
        }
        else if(strcmp(argv[i], "help") == 0 ||
                strcmp(argv[i], "--help") == 0 ||
                strcmp(argv[i], "-h") == 0)
        {
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
        else break;
    }

    // We must have exactly one option per invocation
    sum = start + restart + stop + status;

    if(sum == 0)
    {
        fprintf(stderr, "daemon: missing command.\n");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if(sum != 1)
    {
        fprintf(stderr, "daemon: you must pass only one command.\n");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if(i == argc)
    {
        fprintf(stderr, "daemon: missing daemon name.\n");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if(argc - i > 1)
    {
        fprintf(stderr, "daemon: too many arguments.\n");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    switch(cmd)
    {
        case COMMAND_START:
            printf("daemon: starting %s\n", argv[i]);
            run_daemon(argv[i]);
            break;

        case COMMAND_RESTART:
            printf("daemon: stopping %s\n", argv[i]);
            stop_daemon(argv[i]);

            printf("daemon: waiting for %s to stop\n", argv[i]);

            while(stat_daemon(argv[i], 0))
            {
                sched_yield();
            }

            printf("daemon: starting %s\n", argv[i]);
            run_daemon(argv[i]);
            break;

        case COMMAND_STOP:
            printf("daemon: stopping %s\n", argv[i]);
            stop_daemon(argv[i]);
            break;

        case COMMAND_STATUS:
            stat_daemon(argv[i], 1);
            break;
    }
}

