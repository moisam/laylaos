/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: dragndrop.c
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
 *  \file dragndrop.c
 *
 *  Functions to work with drag and drop source and destination.
 */

#include "../include/gui.h"
#include "../include/event.h"
#include "../include/client/dragndrop.h"
#include "inlines.c"

#define GLOB            __global_gui_data


void drag_start(char **mimetypes, int count)
{
    int i;
    size_t datasz = 0;
    size_t sizes[count];
    char *p;

    if(!mimetypes || !count)
    {
        return;
    }

    // count string lengths and add space for NULL terminators
    for(i = 0; i < count; i++)
    {
        sizes[i] = strlen(mimetypes[i]) + 1;
        datasz += sizes[i];
    }

    // account for the final NULL string
    datasz++;

    size_t bufsz = sizeof(struct event_res_t) + datasz;
    char cbuf[bufsz];
    struct event_res_t *evbuf = (struct event_res_t *)cbuf;

    A_memset((void *)evbuf, 0, bufsz);
    p = evbuf->data;

    for(i = 0; i < count; i++)
    {
        A_memcpy(p, mimetypes[i], sizes[i]);
        p += sizes[i];
    }

    evbuf->type = REQUEST_DRAG_START;
    evbuf->seqid = __next_seqid();
    evbuf->datasz = datasz;
    evbuf->src = TO_WINID(GLOB.mypid, 0);
    evbuf->dest = GLOB.server_winid;

    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);
}


void drag_move(int mousex, int mousey, mouse_buttons_t b, char keymods)
{
    struct event_res_t ev;

    ev.type = REQUEST_DRAG_MOVE;
    ev.seqid = __next_seqid();
    ev.dnd.mousex = mousex;
    ev.dnd.mousey = mousey;
    ev.dnd.buttons = b;
    ev.dnd.modifiers = keymods;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}


void drag_drop(int mousex, int mousey, mouse_buttons_t b, char keymods, const char *pathname)
{
    size_t datasz;

    if(!pathname)
    {
        return;
    }

    datasz = strlen(pathname) + 1;

    size_t bufsz = sizeof(struct event_res_t) + datasz;
    char cbuf[bufsz];
    struct event_res_t *evbuf = (struct event_res_t *)cbuf;

    A_memset((void *)evbuf, 0, bufsz);
    A_memcpy(evbuf->data, pathname, datasz);

    evbuf->type = REQUEST_DRAG_DROP;
    evbuf->seqid = __next_seqid();
    evbuf->datasz = datasz;
    evbuf->src = TO_WINID(GLOB.mypid, 0);
    evbuf->dest = GLOB.server_winid;
    evbuf->dnd.mousex = mousex;
    evbuf->dnd.mousey = mousey;
    evbuf->dnd.buttons = b;
    evbuf->dnd.modifiers = keymods;

    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);
}


void drag_cancel(void)
{
    simple_request(REQUEST_DRAG_CANCEL, 
                        GLOB.server_winid, TO_WINID(GLOB.mypid, 0));
}


void drag_response(winid_t winid, int response)
{
    struct event_t ev;

    ev.type = REQUEST_DRAG_RESPONSE;
    ev.seqid = __next_seqid();
    ev.winst.state = response;
    ev.src = winid;
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}

