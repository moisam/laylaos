/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: main.c
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
 *  \file main.c
 *
 *  A graphical terminal program.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <gui/vbe.h>
#include <kernel/tty.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include "lterm.h"
#include "../include/client/window.h"
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/font.h"
#include "../include/directrw.h"

#define SHELL_EXE                   "/bin/bash"
#define GLOB                        __global_gui_data

struct window_t *main_window;


int get_pty(char *myname)
{
    char *slave_pty_name;

    if((fd_master = posix_openpt(O_RDWR | O_NOCTTY)) < 0)
    {
        fprintf(stderr, "%s: failed to open pty master: %s\n", 
                        myname, strerror(errno));
        return 0;
    }
    
    if(grantpt(fd_master) < 0)
    {
        fprintf(stderr, "%s: failed to access pty master: %s\n", 
                        myname, strerror(errno));
        return 0;
    }

    if(unlockpt(fd_master) < 0)
    {
        fprintf(stderr, "%s: failed to unlock pty master: %s\n", 
                        myname, strerror(errno));
        return 0;
    }
    
    if((slave_pty_name = ptsname(fd_master)) == NULL)
    {
        fprintf(stderr, "%s: failed to get pty slave name: %s\n", 
                        myname, strerror(errno));
        return 0;
    }

    if((fd_slave = open(slave_pty_name, O_RDWR | O_NOCTTY)) < 0)
    {
        fprintf(stderr, "%s: failed to open pty slave: %s\n", 
                        myname, strerror(errno));
        return 0;
    }

    return 1;
}


void repaint_terminal(struct window_t *window, int is_active_child)
{
    (void)is_active_child;

    window_invalidate(window);
}


pid_t child_pid;
int child_died = 0;
int child_exit_status = 0;

void sigchld_handler(int signum __attribute__((unused)))
{
    int        pid, st;
    int        saved_errno = errno;

    while((pid = waitpid(-1, &st, WNOHANG)) != 0)
    {
        if(pid == child_pid)
        {
            child_died = 1;
            child_exit_status = st;
        }

        if(errno == ECHILD)
        {
            break;
        }
        
        printf("server: unknown child exited (pid %d)\n", pid);
    }

    errno = saved_errno;
}


int main(int argc, char **argv)
{
    int maxfd;
    volatile char buf[1];
    /* volatile */ fd_set rdfs;
    /* volatile */ int i;
    volatile struct event_t *ev;

    gui_init(argc, argv);

    ev = GLOB.evbuf_internal;


    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = sigchld_handler;
    act.sa_flags = SA_RESTART;
    (void)sigaction(SIGCHLD, &act, NULL);

    struct window_attribs_t attribs;

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = STANDARD_VGA_WIDTH * __global_gui_data.mono.charw;
    attribs.h = STANDARD_VGA_HEIGHT * __global_gui_data.mono.charh;
    attribs.flags = WINDOW_NORESIZE;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n", 
                        argv[0], strerror(errno));
        close(fd_master);
        close(fd_slave);
        gui_exit(EXIT_FAILURE);
    }

    window_set_title(main_window, "lterm");

    if(!init_terminal(argv[0], STANDARD_VGA_WIDTH, STANDARD_VGA_HEIGHT))
    {
        window_destroy(main_window);
        gui_exit(EXIT_FAILURE);
    }

    if(!get_pty(argv[0]))
    {
        window_destroy(main_window);
        gui_exit(EXIT_FAILURE);
    }

    if(ioctl(fd_master, TIOCSWINSZ, &windowsz) < 0)
    {
        fprintf(stderr, "%s: failed to set pty size: %s\n", 
                        argv[0], strerror(errno));
        close(fd_master);
        close(fd_slave);
        window_destroy(main_window);
        gui_exit(EXIT_FAILURE);
    }
    
    if((child_pid = fork()) == 0)
    {
        char *child_argv[] = { SHELL_EXE, NULL };
        sigset_t oldset, sigttou;

        sigemptyset(&sigttou);
        sigaddset(&sigttou, SIGTTOU);

        /* Release our controlling tty */
        (void)ioctl(0, TIOCSCTTY, 0);

        close(fd_master);
        setpgid(0, 0);
        setsid();
        
        if(ioctl(fd_slave, TIOCSCTTY, 1) < 0)
        {
            fprintf(stderr, "%s: failed to set controlling pty: %s\n", 
                            argv[0], strerror(errno));
            close(fd_slave);
            exit(EXIT_FAILURE);
        }
        
        close(GLOB.serverfd);
        
        dup2(fd_slave, 0);
        dup2(fd_slave, 1);
        dup2(fd_slave, 2);
        close(fd_slave);

        sigprocmask(SIG_BLOCK, &sigttou, &oldset);

        if(tcsetpgrp(0, getpid()) < 0)
        {
            fprintf(stderr, "%s: failed to set pty pgid: %s\n", 
                            argv[0], strerror(errno));
            close(0);
            close(1);
            close(2);
            exit(EXIT_FAILURE);
        }

        sigprocmask(SIG_SETMASK, &oldset, NULL);
        
        execvp(SHELL_EXE, child_argv);
        exit(EXIT_FAILURE);
    }
    else if(child_pid > 0)
    {
        close(fd_slave);
    }
    else
    {
        fprintf(stderr, "%s: failed to fork shell: %s\n", 
                        argv[0], strerror(errno));
        close(fd_master);
        close(fd_slave);
        window_destroy(main_window);
        gui_exit(EXIT_FAILURE);
    }

    main_window->repaint = repaint_terminal;

    erase_display(terminal_width, terminal_height, 2);
    repaint_cursor();

    window_set_icon(main_window, "terminal.ico");
    
    window_show(main_window);
    
    maxfd = (fd_master > GLOB.serverfd) ? fd_master : GLOB.serverfd;

    while(1)
    {
        FD_ZERO(&rdfs);
        FD_SET(fd_master, &rdfs);
        FD_SET(GLOB.serverfd, &rdfs);

        if(pending_refresh)
        {
            static struct timeval tv;

            tv.tv_sec = 0;
            tv.tv_usec = 1000;
            i = select(maxfd + 1, &rdfs, NULL, NULL, &tv);
        }
        else
        {
            i = select(maxfd + 1, &rdfs, NULL, NULL, NULL);
        }

        if(i > 0)
        {
            /*
             * Output from child.
             */
            if(FD_ISSET(fd_master, &rdfs))
            {
                if(direct_read(fd_master, (void *)buf, 1) <= 0)
                {
                    fprintf(stderr, "%s: child exited\n", argv[0]);
                    close(fd_master);
                    window_destroy(main_window);
                    gui_exit(EXIT_FAILURE);
                }
                
                console_write(buf[0]);
            }

            /*
             * Input from keyboard & system messages.
             */
            if(FD_ISSET(GLOB.serverfd, &rdfs))
            {
                __get_event(GLOB.serverfd, GLOB.evbuf_internal, GLOB.evbufsz, 0);

                switch(ev->type)
                {
                    case EVENT_KEY_PRESS:
                        process_key(ev->key.code, ev->key.modifiers);
                        break;

                    case EVENT_MOUSE:
                        process_mouse(ev->mouse.x, ev->mouse.y, ev->mouse.buttons);
                        break;

                    case EVENT_WINDOW_CLOSING:
                        // Closing the master pseudoterminal device will send
                        // SIGHUP to the processes whose controlling terminal
                        // is this device
                        close(fd_master);
                        window_destroy(main_window);
                        gui_exit(EXIT_SUCCESS);
                        break;

                    default:
                        break;
                }
            }
        }
        else
        {
            if(child_died)
            {
                close(fd_master);
                window_destroy(main_window);
                gui_exit(child_exit_status);
            }
            
            if(pending_refresh)
            {
                repaint_dirty();
            }
        }
    }
}

