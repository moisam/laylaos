/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: cursor.c
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
 *  \file cursor.c
 *
 *  Functions to work with the mouse cursor.
 */

#include "../include/gui.h"
#include "../include/event.h"
#include "../include/cursor.h"
#include "../include/directrw.h"
#include "inlines.c"

#define GLOB        __global_gui_data


curid_t cursor_load(int w, int h, int hotx, int hoty, uint32_t *data)
{
    size_t len = w * 4 * h;
    size_t bufsz;
    struct event_cur_t *evbuf;
    struct event_t *ev;

    if(!data)
    {
        return 0;
    }

    bufsz = sizeof(struct event_cur_t) + len;

    if(!(evbuf = malloc(bufsz)))
    {
        return 0;
    }
    
    A_memcpy(evbuf->data, data, len);

    uint32_t seqid = __next_seqid();
    evbuf->seqid = seqid;
    evbuf->type = REQUEST_CURSOR_LOAD;
    evbuf->w = w;
    evbuf->h = h;
    evbuf->hotx = hotx;
    evbuf->hoty = hoty;
    evbuf->datasz = len;
    evbuf->src = TO_WINID(GLOB.mypid, 0);
    evbuf->dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);

    free((void *)evbuf);

    if(!(ev = get_server_reply(seqid)))
    {
        return 0;
    }

    if(ev->type == EVENT_ERROR)
    {
        free(ev);
        return 0;
    }

    GLOB.curid = ev->cur.curid;
    free(ev);

    return GLOB.curid;
}


void cursor_free(curid_t curid)
{
    struct event_t ev;

    ev.type = REQUEST_CURSOR_FREE;
    ev.seqid = __next_seqid();
    ev.cur.curid = curid;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}


void cursor_show(struct window_t *win, curid_t curid)
{
    struct event_t ev;

    ev.type = REQUEST_CURSOR_SHOW;
    ev.seqid = __next_seqid();
    ev.cur.curid = curid;
    ev.src = win->winid;
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));

    GLOB.curid = curid;
}


void cursor_hide(struct window_t *win)
{
    simple_request(REQUEST_CURSOR_HIDE, GLOB.server_winid, win->winid);

    GLOB.curid = 0;
}


void cursor_set_pos(int x, int y)
{
    struct event_t ev;

    ev.type = REQUEST_CURSOR_SET_POS;
    ev.seqid = __next_seqid();
    ev.cur.x = x;
    ev.cur.y = y;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}


void cursor_get_info(struct cursor_info_t *curinfo)
{
    struct event_t *ev;
    uint32_t seqid;

    if(!curinfo)
    {
        return;
    }

    seqid = simple_request(REQUEST_CURSOR_GET_INFO,
                           GLOB.server_winid,
                           TO_WINID(GLOB.mypid, 0));

    if(!(ev = get_server_reply(seqid)))
    {
        return;
    }

    if(ev->type == EVENT_ERROR)
    {
        free(ev);
        return;
    }
    
    A_memcpy(curinfo, &ev->cur, sizeof(struct cursor_info_t));
    free(ev);
}

