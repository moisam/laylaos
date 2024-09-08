/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: gui-global.h
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
 *  \file gui-global.h
 *
 *  Definition of the global gui data structure. This contains important global
 *  information like screen info, server pid, pointers to system fonts, etc.
 */

#ifndef GUI_GLOBAL_H
#define GUI_GLOBAL_H

#include "screen-struct.h"
#include "font-struct.h"

#include <ft2build.h>
#include FT_FREETYPE_H


struct __global_gui_data_t
{
    // current screen info
    struct screen_t screen;

    // system monotype font - user applications request this from the server 
    // on initialization
    struct font_t mono;

    // different file descriptors
    int fbfd, mousefd, serverfd;

    // the server and our own pids
    pid_t serverpid, mypid;

    // the server's window id
    winid_t server_winid;

    // set during exit
    int exit_cleanup_done;

    // the current cursor id
    curid_t curid;

    // used when talking to the server
    size_t evbufsz;
    /* volatile */ struct event_t *evbuf_internal;
    struct event_t *__cur_ev;
    struct queued_ev_t *first_queued_ev;
    struct queued_ev_t *last_queued_ev;

    // global instace of the FreeType library to load the default system font
    FT_Library ftlib;

    // system font - regular & bold
    struct font_t sysfont, sysfont_bold;

    // system color theme - not all of it is used (see theme.h)
    uint32_t themecolor[64];
};

extern struct __global_gui_data_t __global_gui_data;
extern uint32_t builtin_color_theme[];
extern uint32_t __seqid;

static inline uint32_t __next_seqid(void)
{
    uint32_t res = __sync_add_and_fetch(&__seqid, 1);

    // account for roundup
    if(res == 0)
    {
        res = __sync_add_and_fetch(&__seqid, 1);
    }

    return res;
}

#endif      /* GUI_GLOBAL_H */
