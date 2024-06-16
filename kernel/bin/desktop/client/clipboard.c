/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: clipboard.c
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
 *  \file clipboard.c
 *
 *  Functions to get and set clipboard data.
 */

#include <errno.h>
#include "../include/gui.h"
#include "../include/clipboard.h"
#include "../include/event.h"
#include "../include/directrw.h"


#define GLOB        __global_gui_data


size_t clipboard_has_data(int format)
{
    struct event_t ev, *ev2;
    uint32_t seqid = __next_seqid();
    size_t sz;

    if(!format)
    {
        __set_errno(EINVAL);
        return 0;
    }

    ev.type = REQUEST_CLIPBOARD_QUERY;
    ev.seqid = seqid;
    ev.clipboard.fmt = format;
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
    
    if(ev2->clipboard.fmt != format || ev2->clipboard.sz == 0)
    {
        free(ev2);
        __set_errno(ENODATA);
        return 0;
    }

    sz = ev2->clipboard.sz;
    free(ev2);

    return sz;
}


void *clipboard_get_data(int format)
{
    struct event_t ev, *ev2;
    struct event_res_t *evbuf;
    void *data;
    uint32_t seqid = __next_seqid();

    if(!format)
    {
        __set_errno(EINVAL);
        return NULL;
    }

    ev.type = REQUEST_CLIPBOARD_GET;
    ev.seqid = seqid;
    ev.clipboard.fmt = format;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));

    if(!(ev2 = get_server_reply(seqid)))
    {
        return NULL;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return NULL;
    }
    
    evbuf = (struct event_res_t *)ev2;
    
    if(evbuf->clipboard.fmt != format || evbuf->datasz == 0)
    {
        free(ev2);
        __set_errno(ENODATA);
        return NULL;
    }

    if(!(data = malloc(evbuf->datasz)))
    {
        free(ev2);
        __set_errno(ENOMEM);
        return NULL;
    }

    A_memcpy(data, evbuf->data, evbuf->datasz);
    free(ev2);

    return data;
}


int clipboard_set_data(int format, void *data, size_t datasz)
{
    if(!format || !data || !datasz)
    {
        __set_errno(EINVAL);
        return 0;
    }

    size_t bufsz = sizeof(struct event_res_t) + datasz;
    struct event_res_t *evbuf = malloc(bufsz + 1);
    struct event_t *ev2;
    uint32_t seqid = __next_seqid();
    int res;

    if(!evbuf)
    {
        __set_errno(ENOMEM);
        return 0;
    }

    A_memset((void *)evbuf, 0, bufsz);
    memcpy((void *)evbuf->data, data, datasz);
    evbuf->type = REQUEST_CLIPBOARD_SET;
    evbuf->seqid = seqid;
    evbuf->datasz = datasz;
    evbuf->src = TO_WINID(GLOB.mypid, 0);
    evbuf->dest = GLOB.server_winid;
    //evbuf->restype = RESOURCE_TYPE_FONT;
    evbuf->clipboard.fmt = format;
    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);

    free((void *)evbuf);

    if(!(ev2 = get_server_reply(seqid)))
    {
        return 0;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return 0;
    }
    
    res = !!(ev2->clipboard.sz);
    free(ev2);

    return res;
}

