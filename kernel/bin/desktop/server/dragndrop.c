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
 *  Functions to handle drag and drop operations on the server side.
 */

#define GUI_SERVER
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/server/server.h"
#include "../include/server/event.h"
#include "../include/server/window.h"

#include "inlines.c"

#define GLOB                __global_gui_data

char *active_dnd_mimetypes = NULL;          // payload for EVENT_DRAG_ENTER
size_t active_dnd_mimetypes_alloced = 0;
size_t active_dnd_mimetypes_sz = 0;
int dnd_drop_localx = 0;                    // local mouse coordinates at drop site
int dnd_drop_localy = 0;
int dnd_accept_response = 0;
volatile int dnd_is_active = 0;             // indicates active DnD operation

struct server_window_t *dnd_current_target = NULL;
struct server_window_t *dnd_current_source = NULL;


void server_drag_start(struct event_t *ev)
{
    struct event_res_t *evres = (struct event_res_t *)ev;
    size_t bytes = evres->datasz;

    if(bytes == 0)
    {
        dnd_is_active = 0;
        return;
    }

    if(!active_dnd_mimetypes)
    {
        if(!(active_dnd_mimetypes = malloc(bytes)))
        {
            dnd_is_active = 0;
            return;
        }

        active_dnd_mimetypes_alloced = bytes;
    }
    else if(bytes > active_dnd_mimetypes_alloced)
    {
        char *tmp;

        if(!(tmp = realloc(active_dnd_mimetypes, bytes)))
        {
            dnd_is_active = 0;
            return;
        }

        active_dnd_mimetypes_alloced = bytes;
    }

    A_memcpy(active_dnd_mimetypes, evres->data, bytes);
    active_dnd_mimetypes_sz = bytes;
    dnd_current_source = server_window_by_winid(ev->src);
    dnd_is_active = 1;
}


void server_drag_move(struct event_t *ev)
{
    struct event_res_t *evres = (struct event_res_t *)ev;
    struct server_window_t *win = dnd_current_target;

    if(!dnd_is_active)
    {
        return;
    }

    dnd_current_target = get_window_under_mouse();

    // Drag is happening inside the same window. Update the target with
    // the new mouse coordinates
    if(win == dnd_current_target)
    {
        if(!win)
        {
            return;
        }

        size_t bufsz = sizeof(struct event_res_t) + active_dnd_mimetypes_sz;
        char cbuf[bufsz];
        struct event_res_t *evbuf = (struct event_res_t *)cbuf;

        A_memset((void *)evbuf, 0, bufsz);

        evbuf->type = EVENT_DRAG_MOVE;
        evbuf->seqid = ev->seqid;
        evbuf->src = ev->src;
        evbuf->dest = win->winid;
        evbuf->dnd.mousex = server_window_local_x(win, evres->dnd.mousex);
        evbuf->dnd.mousey = server_window_local_y(win, evres->dnd.mousey);
        evbuf->datasz = active_dnd_mimetypes_sz;
        evbuf->valid_reply = 1;
        A_memcpy(evbuf->data, active_dnd_mimetypes, active_dnd_mimetypes_sz);
        direct_write(win->clientfd->fd, evbuf, bufsz);

        return;
    }

    // Drag has entered a new window and possibly left an old one. Update the
    // new target with EVENT_DRAG_ENTER and the old one with EVENT_DRAG_LEAVE
    if(win)
    {
        struct event_res_t evbuf;

        evbuf.type = EVENT_DRAG_LEAVE;
        evbuf.seqid = ev->seqid;
        evbuf.src = ev->src;
        evbuf.dest = win->winid;
        evbuf.dnd.mousex = server_window_local_x(win, evres->dnd.mousex);
        evbuf.dnd.mousey = server_window_local_y(win, evres->dnd.mousey);
        evbuf.valid_reply = 1;
        direct_write(win->clientfd->fd, &evbuf, sizeof(struct event_res_t));
    }

    if(dnd_current_target && active_dnd_mimetypes_sz)
    {
        size_t bufsz = sizeof(struct event_res_t) + active_dnd_mimetypes_sz;
        char cbuf[bufsz];
        struct event_res_t *evbuf = (struct event_res_t *)cbuf;

        A_memset((void *)evbuf, 0, bufsz);

        evbuf->type = EVENT_DRAG_ENTER;
        evbuf->seqid = ev->seqid;
        evbuf->src = ev->src;
        evbuf->dest = dnd_current_target->winid;
        evbuf->dnd.mousex = server_window_local_x(dnd_current_target, evres->dnd.mousex);
        evbuf->dnd.mousey = server_window_local_y(dnd_current_target, evres->dnd.mousey);
        evbuf->datasz = active_dnd_mimetypes_sz;
        evbuf->valid_reply = 1;
        A_memcpy(evbuf->data, active_dnd_mimetypes, active_dnd_mimetypes_sz);
        direct_write(dnd_current_target->clientfd->fd, evbuf, bufsz);
    }
}


void server_drag_cancel(void)
{
    active_dnd_mimetypes_sz = 0;
    dnd_is_active = 0;
    dnd_current_target = NULL;
    dnd_current_source = NULL;
    dnd_accept_response = 0;
}


void server_drag_drop(struct event_t *ev)
{
    struct event_res_t *evres = (struct event_res_t *)ev;
    size_t bufsz = sizeof(struct event_res_t) + evres->datasz;

    dnd_current_target = get_window_under_mouse();

    if(!dnd_is_active || !dnd_current_target || dnd_accept_response == 0)
    {
        server_drag_cancel();
        return;
    }

    // fix some fields in the packet and send it to its destination
    evres->type = EVENT_DRAG_DROP;
    evres->dest = dnd_current_target->winid;
    evres->dnd.mousex = server_window_local_x(dnd_current_target, evres->dnd.mousex);
    evres->dnd.mousey = server_window_local_y(dnd_current_target, evres->dnd.mousey);
    evres->valid_reply = 1;

    direct_write(dnd_current_target->clientfd->fd, evres, bufsz);

    server_drag_cancel();
}


void server_drag_response(struct event_t *ev)
{
    /*
     * TODO: we should update the mouse pointer accordingly.
     */
    dnd_accept_response = ev->winst.state;

    if(dnd_current_source)
    {
        // fix some fields in the packet and send it to the drag source
        ev->type = EVENT_DRAG_RESPONSE;
        ev->dest = dnd_current_source->winid;
        ev->valid_reply = 1;

        direct_write(dnd_current_source->clientfd->fd, ev, sizeof(struct event_t));
    }
}

