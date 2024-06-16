/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
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
 *  Graphical library initialisation and shutdown functions.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "../include/gui.h"
#include "../include/window-defs.h"
#include "../include/screen.h"
#include "../include/event.h"
#include "../include/resources.h"
#include "../include/client/window.h"
#include "../include/cursor.h"

#define GLOB        __global_gui_data

void gui_atexit(void);


/* Code taken from GNU LibC manpage:
 *     https://www.gnu.org/software/libc/manual/html_node/Descriptor-Flags.html
 *
 * Set the FD_CLOEXEC flag of desc if value is nonzero, or clear the flag if 
 * value is 0.
 * Return 0 on success, or -1 on error with errno set.
 */
static void set_cloexec_flag(int desc, int value)
{
    int oldflags = fcntl (desc, F_GETFD, 0);

    /* If reading the flags failed, return error indication now. */
    if(oldflags < 0)
    {
        return;
    }
    
    /* Set just the flag we want to set. */
    if(value != 0)
    {
        oldflags |= FD_CLOEXEC;
    }
    else
    {
        oldflags &= ~FD_CLOEXEC;
    }
    
    /* Store modified flag word in the descriptor. */
    fcntl(desc, F_SETFD, oldflags);
}


static void load_sysfont(char *myname, char *fontname, struct font_t *font)
{
    if(font_load(fontname, font) == INVALID_RESID)
    {
        fprintf(stderr, "%s: failed to get %s font: %s\n", 
                        myname, fontname, strerror(errno));
        close(GLOB.serverfd);
        exit(EXIT_FAILURE);
    }

    if(font->flags & FONT_FLAG_TRUE_TYPE)
    {
        FT_Size ftsize;

        if(FT_New_Size(font->ft_face, &ftsize) == 0)
        {
            font->ptsz = 16;

            font->ftsize = ftsize;
            FT_Activate_Size(ftsize);

            // arguments to FT_Set_Char_Size():
            //   font face,
            //   0 => char width in 1/64 of points, 0 means same as height,
            //   10 * 64 => char height in 1/64 of points,
            //   0 => horizontal device resolution, 0 means same as vertical,
            //   0 => vertical device resolution, 0 means default (72 dpi),

            FT_Set_Char_Size(font->ft_face, 0, font->ptsz * 64, 0, 0);
        }
    }
}


static void __gui_init(int argc, char **argv, int load_fonts)
{
    //char *envserver;
    int retries, res;


    GLOB.evbufsz = 0x2000 /* 0x6000 */;
    GLOB.evbuf_internal = malloc(GLOB.evbufsz);

    //int server_sockfd;
    struct sockaddr_un server_addr;
    
    if((GLOB.serverfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
    {
        fprintf(stderr, "%s: failed to create socket: %s\n", 
                        argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, "/var/run/guiserver");
    
    res = 0;
    
    for(retries = 0; retries < 10; retries++)
    {
        if((res = connect(GLOB.serverfd, (struct sockaddr *)&server_addr, 
                                         sizeof(struct sockaddr_un))) == 0)
        {
            break;
        }
        
        sched_yield();
    }

    if(res != 0)
    {
        fprintf(stderr, "%s: failed to bind to socket: %s\n", 
                argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }

    GLOB.server_winid = TO_WINID(0, 0);
    GLOB.mypid = getpid();
    GLOB.curid = CURSOR_NORMAL;

    if(!get_screen_info(&GLOB.screen))
    {
        fprintf(stderr, "%s: failed to get screen info: %s\n", 
                argv[0], strerror(errno));
        close(GLOB.serverfd);
        exit(EXIT_FAILURE);
    }

    // if this is a palette-indexed mode, get the palette
    if(!GLOB.screen.rgb_mode)
    {
        if(!get_screen_palette(&GLOB.screen))
        {
            fprintf(stderr, "%s: failed to get color palette: %s\n", 
                    argv[0], strerror(errno));
            close(GLOB.serverfd);
            exit(EXIT_FAILURE);
        }
    }


    // load default system fonts
    if(load_fonts)
    {
        load_sysfont(argv[0], "font-monospace", &GLOB.mono);
        load_sysfont(argv[0], "font-system", &GLOB.sysfont);
    }

    // make sure the server connection is closed on EXEC
    set_cloexec_flag(GLOB.serverfd, FD_CLOEXEC);

    atexit(gui_atexit);
}


void gui_init(int argc, char **argv)
{
    return __gui_init(argc, argv, 1);
}


void gui_init_no_fonts(int argc, char **argv)
{
    return __gui_init(argc, argv, 0);
}


void gui_exit(int exit_code)
{
    // exit() will call gui_atexit() for us
    exit(exit_code);
}


void gui_atexit(void)
{
    if(!GLOB.exit_cleanup_done)
    {
        if(GLOB.ftlib)
        {
            font_unload(&GLOB.mono);
            font_unload(&GLOB.sysfont);
            FT_Done_FreeType(GLOB.ftlib);
            GLOB.ftlib = NULL;
        }

        window_destroy_all();
        //destroy_queue(TO_WINID(GLOB.mypid, 0));
        close(GLOB.serverfd);
        GLOB.serverfd = -1;
        GLOB.exit_cleanup_done = 1;
    }
}

