/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: screenshot.c
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
 *  \file screenshot.c
 *
 *  Functions to work with screenshots on the client side.
 */

#include <unistd.h>
#include <fcntl.h>
#include "../include/gui.h"
#include "../include/event.h"

#define GLOB        __global_gui_data

// defined in common/next-event.c
extern mutex_t __global_evlock;


uint32_t *screenshot_get(winid_t winid, int x, int y, int w, int h)
{
    volatile struct event_t ev;
    struct event_t *ev2;
    struct event_buf_t *evbuf;
    size_t chunksz;
    size_t expected = w * h * 4;
    uint8_t *returnbuf;
    uint32_t seqid = __next_seqid();

    if(x < 0 || y < 0 || w <= 0 || h <= 0)
    {
        return NULL;
    }

    if(!(returnbuf = malloc(expected)))
    {
        return NULL;
    }

    // ensure no one modifies this while we read it
    mutex_lock(&__global_evlock);
    chunksz = GLOB.evbufsz - sizeof(struct event_buf_t);
    mutex_unlock(&__global_evlock);

    ev.seqid = seqid;
    ev.type = REQUEST_GET_SCREENSHOT;
    ev.screenshot.winid = winid;
    ev.screenshot.x = x;
    ev.screenshot.y = y;
    ev.screenshot.w = w;
    ev.screenshot.h = h;
    ev.screenshot.chunksz = chunksz;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));

    // the server will send a reply to the same sequence id
    // if the total size is less than our chunk size, it will be returned
    // in the reply, otherwise we will be passed a temp filename
    if(!(ev2 = get_server_reply(seqid)))
    {
        free(returnbuf);
        return NULL;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        free(returnbuf);
        return NULL;
    }

    evbuf = (struct event_buf_t *)ev2;

    //fprintf(stderr, "*** bufsz %ld\n", evbuf->bufsz);

    // the reply data contains 4-byte header
    // the reply buffer cannot be smaller than that
    if(evbuf->bufsz < 4)
    {
        free(ev2);
        free(returnbuf);
        return NULL;
    }

    if(evbuf->buf[0] == 'D' && evbuf->buf[1] == 'A' &&
       evbuf->buf[2] == 'T' && evbuf->buf[3] == 'A')
    {
        // inlined data
        A_memcpy(returnbuf, evbuf->buf + 4, evbuf->bufsz - 4);
    }
    else
    {
        // data stored in a temp file
        char path[evbuf->bufsz - 3];
        int fd;

        A_memcpy(path, evbuf->buf + 4, evbuf->bufsz - 4);
        path[evbuf->bufsz - 3] = '\0';

        if((fd = open(path, O_RDONLY, 0)) < 0)
        {
            free(ev2);
            free(returnbuf);
            return NULL;
        }

        if(read(fd, returnbuf, expected) < expected)
        {
            close(fd);
            free(ev2);
            free(returnbuf);
            unlink(path);
            return NULL;
        }

        close(fd);
        unlink(path);
    }

    free(ev2);

    return (uint32_t *)returnbuf;
}


void screenshot_free(uint32_t *screenshot)
{
    if(screenshot)
    {
        free(screenshot);
    }
}

