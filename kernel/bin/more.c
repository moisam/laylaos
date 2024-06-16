/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: more.c
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
 *  \file more.c
 *
 *  A program to print text files and view them in pages.
 */

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>

int fcount = 0;
char **fnames = NULL;
char *ver = "1.0";

char SQUEEZE_BLANKS = 0;

size_t screen_height = 0;
size_t screen_width = 0;

struct termios tty_attr;
struct termios tty_attr_old;


int get_screen_size(int fd)
{
    struct winsize w;

    //find the size of the view
    if(ioctl(fd, TIOCGWINSZ, &w) != 0)
    {
        return 0;
    }

    screen_height = w.ws_row;
    screen_width = w.ws_col;

    return 1;
}


void init_terminal(int fd)
{
    tcgetattr(fd, &tty_attr_old);
    /* turn off buffering, echo and key processing */
    tty_attr = tty_attr_old;
    tty_attr.c_lflag &= ~(ICANON | ECHO | ISIG);
    //tty_attr.c_iflag &= ~(ISTRIP | INLCR | ICRNL | IGNCR | IXON | IXOFF);
    tcsetattr(fd, TCSAFLUSH, &tty_attr);
}


void restore_terminal(int fd)
{
    tcsetattr(fd, TCSAFLUSH, &tty_attr_old);
}


void err_exit(int fd, int code)
{
    restore_terminal(fd);
    exit(code);
}


void parse_line_args(int argc, char **argv) 
{
    int c;
    static struct option long_options[] =
    {
        {"help",    no_argument, 0, 'h'},
        {"squeeze", no_argument, 0, 's'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
  
    while((c = getopt_long(argc, argv, "hsv", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 's':
                SQUEEZE_BLANKS = 1;
                break;

            case 'v':
                printf("%s\n", ver);
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                printf("more utility for LaylaOS, Version %s\n\n", ver);
                printf("Usage: %s [options] [file...]\n\n"
                       "Options:\n"
                       "  -h, --help        Show this help and exit\n"
                       "  -s, --squeeze     Squeeze multiple blank lines into one\n"
                       "  -v, --version     Print version and exit\n"
                       "\n", argv[0]);
                exit(EXIT_SUCCESS);
                break;

            case '?':
                break;

            default:
                abort();
        }
    }
    
    fcount = argc - optind;
    fnames = argv + optind;
}


int ttyin(int fd)
{
    int c = 0;

    if(read(fd, &c, 1) != 1)
    {
        err_exit(fd, EXIT_FAILURE);
    }

    return c;
}


void print_file(int fd, char *fname, int maxlines)
{
    char data[BUFSIZ];
    static int line_count = 0;
    size_t len = 0;
    int char_count = 0;
    char last_line_is_blank = 0;  
    size_t i;

    while(1)
    {
        i = 0;

        if((len = read(fd, data, BUFSIZ)) < 0)    // error
        {
            sprintf(data, "\033[7m`%s`: %s\033[0m\n", fname, strerror(errno));
            len = strlen(data);
        }
        else if(len == 0)   // EOF
        {
            break;
        }

        while(i < len)
        {
            int c = data[i++];

            if(c == '\n')
            {
                if(!(SQUEEZE_BLANKS && last_line_is_blank))
                {
                    putchar(c);
                }

                if(!char_count)
                {
                    last_line_is_blank = 1;
                }

                line_count++;
                char_count = 0;
            }
            else
            {
                char_count++;
                putchar(c);
            }

            if(char_count >= screen_width)
            {
                line_count++;
                char_count = 0;
            }

            /* Print output in pages */
            if(line_count == screen_height - 1)
            {
                /* Make sure we are at the bottom row */
                printf("\033[%ld;1H", screen_height);

                printf("--More--");
                line_count = 0;
                char_count = 0;
                fflush(stdout);

                while(1)
                {
                    c = ttyin(STDERR_FILENO);

                    if(c == ' ' || c == '\n' || c == '\r')
                    {
                        //printf("\033[2K");
                        printf("\n");
                        fflush(stdout);
                        break;
                    }

                    if(c == 'q' /* || c == '\033' */)
                    {
                        printf("\033[2K");
                        //printf("\n");
                        fflush(stdout);
                        restore_terminal(STDOUT_FILENO);
                        exit(EXIT_SUCCESS);
                    }
                }
            }
        }
    }
}


int main(int argc, char **argv)
{
    int i, fd;
    sigset_t set;

    parse_line_args(argc, argv);

    /* check for output redirection */
    if(!isatty(STDERR_FILENO))
    {
        freopen("/dev/tty", "w", stderr);
    }

    if(!isatty(STDOUT_FILENO))
    {
        fprintf(stderr, "%s: invalid output\n"
                        "See %s --help for syntax\n",
                argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    if(!get_screen_size(STDOUT_FILENO))
    {
        fprintf(stderr, "%s: failed to read terminal size\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* clear any inherited settings */
    signal(SIGCHLD, SIG_DFL);

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGTSTP);
    sigaddset(&set, SIGCONT);
    sigaddset(&set, SIGWINCH);
    sigprocmask(SIG_BLOCK, &set, NULL);
    
    init_terminal(STDOUT_FILENO);

    if(fcount)
    {
        for(i = 0; i < fcount; i++, fnames++)
        {
            fd = open(*fnames, O_RDONLY);
            print_file(fd, *fnames, screen_height - 1);
            close(fd);
        }
    }
    else
    {
        if(isatty(STDIN_FILENO))
        {
            fprintf(stderr, "%s: invalid input\n"
                            "See %s --help for syntax\n",
                    argv[0], argv[0]);
            err_exit(STDOUT_FILENO, EXIT_FAILURE);
        }

        print_file(STDIN_FILENO, "stdin", screen_height - 1);
    }
    
    //printf("more done 1!\n");

    restore_terminal(STDOUT_FILENO);

    //printf("more done 2!\n");

    exit(EXIT_SUCCESS);
}

