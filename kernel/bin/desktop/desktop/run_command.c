
/*
 * Helper function. This file is not compiled separately, but included in
 * the desktop and top-panel source files.
 */

#ifndef GLOB
#define GLOB                        __global_gui_data
#endif

void run_command(char *__cmd)
{
    if(!fork())
    {
        // Copy the command string as strtok will modify it
        char *cmd = strdup(__cmd);
        int fd;

        if(!cmd)
        {
            fprintf(stderr, "run_command: insufficient memory\n");
            exit(EXIT_FAILURE);
        }

        // Release our controlling tty
        (void)ioctl(0, TIOCSCTTY, 0);

        fd = open("/dev/null", O_RDWR);
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        close(fd);
        close(GLOB.serverfd);

        // Now get our argument(s)
        char *tok = strtok(cmd, "\n\r\t ");
        char *argv[256];
        int argc = 0;

        while(tok)
        {
            argv[argc++] = tok;
            tok = strtok(NULL, "\n\r\t ");
        }

        argv[argc] = NULL;
        execvp(cmd, argv);
        exit(EXIT_FAILURE);
    }
}

