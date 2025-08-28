#include <fcntl.h>

/*
 * Common functions used by the daemon utility and the init program.
 */

static int read_daemon_file(char *utility, struct daemon_t *d, char *filename)
{
    FILE *f;
    char path[strlen(filename) + 16];
    char buf[512];

    sprintf(path, "%s%s", DAEMON_DATADIR, filename);

    if(!(f = fopen(path, "r")))
    {
        fprintf(stderr, "%s: failed to read: %s\n", utility, path);
        return 0;
    }

    while((fgets(buf, sizeof(buf), f)))
    {
        if(buf[strlen(buf) - 1] == '\n')
        {
            buf[strlen(buf) - 1] = '\0';
        }

        if(strstr(buf, "NAME=") == buf)
        {
            if(!d->name)
            {
                d->name = strdup(buf + 5);
            }
        }
        else if(strstr(buf, "DESC=") == buf)
        {
            d->desc = strdup(buf + 5);
        }
        else if(strstr(buf, "DAEMON=") == buf)
        {
            d->cmd = strdup(buf + 7);
        }
        else if(strstr(buf, "DAEMON_OPTS=") == buf)
        {
            d->cmdargs = strdup(buf + 12);
        }
        else if(strstr(buf, "PATH=") == buf)
        {
            d->envpath = strdup(buf + 5);
        }
    }

    fclose(f);

    return 1;
}


static void fork_daemon_task(struct daemon_t *d)
{
    if(!(d->pid = fork()))
    {
        char *argv[256];
        int argc = 1;
        char *argstr = NULL;
        char buf[128];

        // Copy the arguments string as strtok will modify it
        if(d->cmdargs && d->cmdargs[0])
        {
            if(!(argstr = strdup(d->cmdargs)))
            {
                fprintf(stderr, "daemon: insufficient memory\n");
                exit(EXIT_FAILURE);
            }
        }

        // Close file descriptors and open output for logging
        close(0);
        close(1);
        close(2);
        sprintf(buf, "/var/log/%s.log", d->name);
        open("/dev/null", O_RDONLY);
        open(buf, O_RDWR | O_CREAT | O_APPEND, 0644);
        dup2(1, 2);

        // Set env $PATH if the daemon wants it
        if(d->envpath)
        {
            setenv("PATH", d->envpath, 1);
        }

        // Now get our argument(s)
        if(argstr)
        {
            char *tok = strtok(argstr, "\n\r\t ");

            while(tok)
            {
                argv[argc++] = tok;
                tok = strtok(NULL, "\n\r\t ");
            }
        }

        argv[0] = d->cmd;
        argv[argc] = NULL;
        execvp(d->cmd, argv);
        //fprintf(stderr, "daemon: failed to run '%s': %s\n", d.cmd, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

