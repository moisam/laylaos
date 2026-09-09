/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: systray.c
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
 *  \file systray.c
 *
 *  Functions to work with the system tray on the client side.
 */

#include "../include/gui.h"
#include "../include/event.h"
#include "inlines.c"

#define GLOB        __global_gui_data

winid_t systray_manager_winid = 0;


void register_systray_manager(winid_t winid)
{
    simple_request(REQUEST_REGISTER_SYSTRAY_MANAGER, GLOB.server_winid, winid);
}


winid_t systray_get_manager_winid(void)
{
    if(systray_manager_winid != 0)
    {
        return systray_manager_winid;
    }

    struct event_t ev, *ev2;
    uint32_t seqid = __next_seqid();

    ev.type = REQUEST_SYSTRAY_MANAGER_WINID;
    ev.seqid = seqid;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));

    if(!(ev2 = get_server_reply(seqid)))
    {
        return 0;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return 0;
    }

    systray_manager_winid = ev2->winattr.winid;
    free(ev2);

    return systray_manager_winid;
}


int systray_add(winid_t src)
{
    struct event_t ev;

    if(!(ev.dest = systray_get_manager_winid()))
    {
        // systray manager not available
        return 0;
    }

    ev.type = REQUEST_SYSTRAY_ADD;
    ev.seqid = __next_seqid();
    ev.src = src;
    ev.valid_reply = 1;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));

    return 1;
}


int systray_remove(winid_t src)
{
    struct event_t ev;

    if(!(ev.dest = systray_get_manager_winid()))
    {
        // systray manager not available
        return 0;
    }

    ev.type = REQUEST_SYSTRAY_REMOVE;
    ev.seqid = __next_seqid();
    ev.src = src;
    ev.valid_reply = 1;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));

    return 1;
}


int systray_set_icon_visibility(winid_t src, int visible)
{
    struct event_t ev;

    if(!(ev.dest = systray_get_manager_winid()))
    {
        // systray manager not available
        return 0;
    }

    ev.type = visible ? REQUEST_SYSTRAY_SHOW : REQUEST_SYSTRAY_HIDE;
    ev.seqid = __next_seqid();
    ev.src = src;
    ev.valid_reply = 1;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));

    return 1;
}


int systray_set_icon(winid_t src, const char *data, size_t datasz)
{
    size_t bufsz;
    struct event_buf_t *evbuf;

    if(!systray_get_manager_winid())
    {
        // systray manager not available
        return 0;
    }

    // if we have data, we must have valid size
    if(data && !datasz)
    {
        return 0;
    }

    bufsz = sizeof(struct event_buf_t) + (data ? datasz : 1);

    if(!(evbuf = malloc(bufsz)))
    {
        return 0;
    }
    
    if(data)
    {
        A_memcpy(evbuf->buf, data, datasz);
    }
    else
    {
        evbuf->buf[0] = '\0';
    }

    evbuf->seqid = __next_seqid();
    evbuf->type = REQUEST_SYSTRAY_SET_ICON;
    evbuf->bufsz = datasz;
    evbuf->src = src;
    evbuf->dest = systray_manager_winid;
    evbuf->valid_reply = 1;
    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);

    free((void *)evbuf);

    return 1;
}


int systray_set_tooltip(winid_t src, const char *tooltip)
{
    if(!systray_get_manager_winid())
    {
        // systray manager not available
        return 0;
    }

    size_t len = tooltip ? strlen(tooltip) + 1 : 0;
    size_t bufsz = sizeof(struct event_buf_t) + len;
    struct event_buf_t *evbuf = malloc(bufsz + 1);

    if(!evbuf)
    {
        return 0;
    }

    A_memset((void *)evbuf, 0, bufsz);

    if(tooltip)
    {
        A_memcpy((void *)evbuf->buf, tooltip, len);
    }
    else
    {
        evbuf->buf[0] = '\0';
        bufsz++;
        len++;
    }

    evbuf->type = REQUEST_SYSTRAY_SET_TOOLTIP;
    evbuf->seqid = __next_seqid();
    evbuf->bufsz = len;
    evbuf->src = src;
    evbuf->dest = systray_manager_winid;
    evbuf->valid_reply = 1;
    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);

    free((void *)evbuf);

    return 1;
}


int systray_get_bounds(winid_t src, Rect *r)
{
    if(!systray_get_manager_winid())
    {
        // systray manager not available
        return 0;
    }

    struct event_t ev, *ev2;
    uint32_t seqid = __next_seqid();

    if(!r)
    {
        return 0;
    }

    ev.type = REQUEST_SYSTRAY_GET_BOUNDS;
    ev.seqid = seqid;
    ev.src = src;
    ev.dest = systray_manager_winid;
    ev.valid_reply = 1;

    direct_write(GLOB.serverfd, &ev, sizeof(struct event_t));

    if(!(ev2 = get_server_reply(seqid)))
    {
        return 0;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return 0;
    }

    r->top = ev2->rect.top;
    r->left = ev2->rect.left;
    r->bottom = ev2->rect.bottom;
    r->right = ev2->rect.right;

    free(ev2);
    
    return 1;
}


int systray_show_message(winid_t src, const char *title, const char *msg,
                         const void *icon, size_t iconsz, int msgtype, int msgduration)
{
    if(!systray_get_manager_winid())
    {
        // systray manager not available
        return 0;
    }

    if(!title || !msg)
    {
        return 0;
    }

    size_t titlelen = strlen(title) + 1;
    size_t msglen = strlen(msg) + 1;
    size_t bufsz = titlelen + msglen + iconsz;
    size_t totalsz = bufsz + sizeof(struct event_traymsg_t);
    struct event_traymsg_t *evbuf;

    if(!(evbuf = malloc(totalsz)))
    {
        return 0;
    }

    A_memset((void *)evbuf, 0, totalsz);

    A_memcpy(evbuf->data, title, titlelen);
    A_memcpy(evbuf->data + titlelen, msg, msglen);

    if(icon && iconsz)
    {
        A_memcpy(evbuf->data + titlelen + msglen, icon, iconsz);
    }

    evbuf->type = REQUEST_SYSTRAY_SHOW_MESSAGE;
    evbuf->seqid = __next_seqid();
    evbuf->datasz = bufsz;
    evbuf->titlelen = titlelen;
    evbuf->msglen = msglen;
    evbuf->msgtype = msgtype;
    evbuf->msgduration = msgduration;
    evbuf->src = src;
    evbuf->dest = systray_manager_winid;
    evbuf->valid_reply = 1;
    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);

    free((void *)evbuf);

    return 1;
}

