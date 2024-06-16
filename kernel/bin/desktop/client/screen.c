/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: screen.c
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
 *  \file screen.c
 *
 *  Functions to get screen info and screen color palette.
 */

#include <errno.h>
#include <unistd.h>
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/screen.h"
#include "../include/directrw.h"


#define GLOB        __global_gui_data


int get_screen_info(struct screen_t *screen)
{
    struct event_t ev, *ev2;
    uint32_t seqid = __next_seqid();

    ev.type = REQUEST_SCREEN_INFO;
    ev.seqid = seqid;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;

    direct_write(GLOB.serverfd, &ev, sizeof(struct event_t));
    //write(GLOB.serverfd, (void *)ev, sizeof(struct event_t));

    if(!(ev2 = get_server_reply(seqid)))
    {
        //__set_errno(EINVAL);
        return 0;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        //__set_errno(ev->err._errno);
        return 0;
    }

    screen->w = ev2->screen.w;
    screen->h = ev2->screen.h;
    screen->rgb_mode = ev2->screen.rgb_mode;
    screen->pixel_width = ev2->screen.pixel_width;
    screen->red_pos = ev2->screen.red_pos;
    screen->green_pos = ev2->screen.green_pos;
    screen->blue_pos = ev2->screen.blue_pos;
    screen->red_mask_size = ev2->screen.red_mask_size;
    screen->green_mask_size = ev2->screen.green_mask_size;
    screen->blue_mask_size = ev2->screen.blue_mask_size;

    free(ev2);
    
    return 1;
}


int get_screen_palette(struct screen_t *screen)
{
    struct event_res_t *evbuf;
    struct event_t ev, *ev2;
    uint32_t seqid = __next_seqid();

    if(!screen)
    {
        __set_errno(EINVAL);
        return 0;
    }

    ev.type = REQUEST_COLOR_PALETTE;
    ev.seqid = seqid;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;

    direct_write(GLOB.serverfd, &ev, sizeof(struct event_t));
    //write(GLOB.serverfd, &ev, sizeof(struct event_t));

    if(!(ev2 = get_server_reply(seqid)))
    {
        return 0;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return 0;
    }

    evbuf = (struct event_res_t *)ev2;

    if(!(screen->palette = malloc(evbuf->datasz)))
    {
        free(ev2);
        __set_errno(ENOMEM);
        return 0;
    }

    A_memcpy(screen->palette, evbuf->data, evbuf->datasz);
    screen->color_count = evbuf->palette.color_count;

    free(ev2);

    return 1;
}

