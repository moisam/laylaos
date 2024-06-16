/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: event.h
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
 *  \file event.h
 *
 *  Inlined functions for notifying client applications of different events.
 *
 *  The functions declared in this file are NOT intended for client
 *  application use.
 */

#ifndef GUI_SERVER_EVENT_H
#define GUI_SERVER_EVENT_H

#include "../event.h"
#include "../directrw.h"


#define GLOB        __global_gui_data

#include <errno.h>
#include <unistd.h>
#include <kernel/keycodes.h>

#define CHECK_DEAD_CLIENT(window)                               \
{                                                               \
    int err = errno;                                            \
    if(err == ENOTCONN || err == ECONNREFUSED || err == EINVAL) \
        server_window_dead(window);                             \
}


static inline int notify_simple_event(int fd, int event,
                                      winid_t dest, winid_t src,
                                      uint32_t seqid)
{
    struct event_t ev;
    
    if(!dest || !src)
    {
        return 0;
    }

    ev.type = event;
    ev.seqid = seqid;
    ev.src = src;
    ev.dest = dest;
    ev.valid_reply = 1;

    if(direct_write(fd, (void *)&ev, sizeof(struct event_t)) < 0)
    {
        int err = errno;

        if(err == ENOTCONN || err == ECONNREFUSED || err == EINVAL)
        {
            return -1;
        }
    }
    
    return 1;
}

static inline void send_key_event(struct server_window_t *window,
                                  char key, char flags, char modifiers)
{
    struct event_t ev;

    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = window->winid;
    ev.key.modifiers = modifiers;
    ev.valid_reply = 1;
    
    if((flags & KEYCODE_BREAK_MASK))
    {
        ev.type = EVENT_KEY_RELEASE;
    }
    else
    {
        ev.type = EVENT_KEY_PRESS;
    }
    
    ev.key.code = key;

    if(direct_write(window->clientfd->fd, (void *)&ev, 
                                    sizeof(struct event_t)) < 0)
    {
        CHECK_DEAD_CLIENT(window);
    }
}


static inline void __mouse_event(struct server_window_t *window,
                                 int mouse_x, int mouse_y,
                                 mouse_buttons_t mouse_buttons,
                                 uint32_t evtype)
{
    struct event_t ev;

    ev.type = evtype;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = window->winid;
    ev.valid_reply = 1;
    ev.mouse.x = mouse_x;
    ev.mouse.y = mouse_y;
    ev.mouse.buttons = mouse_buttons;

    if(direct_write(window->clientfd->fd, (void *)&ev, 
                                sizeof(struct event_t)) < 0)
    {
        CHECK_DEAD_CLIENT(window);
    }
}

static inline void send_mouse_event(struct server_window_t *window,
                      int mouse_x, int mouse_y,
                      mouse_buttons_t mouse_buttons)
{
    __mouse_event(window, mouse_x, mouse_y, mouse_buttons, EVENT_MOUSE);
}

static inline void send_mouse_exit_event(struct server_window_t *window,
                           int mouse_x, int mouse_y,
                           mouse_buttons_t mouse_buttons)
{
    __mouse_event(window, mouse_x, mouse_y, mouse_buttons, EVENT_MOUSE_EXIT);
}

static inline void send_mouse_enter_event(struct server_window_t *window,
                            int mouse_x, int mouse_y,
                            mouse_buttons_t mouse_buttons)
{
    __mouse_event(window, mouse_x, mouse_y, mouse_buttons, EVENT_MOUSE_ENTER);
}

static inline void send_resize_offer(struct server_window_t *window,
                                     int new_x, int new_y,
                                     int new_w, int new_h, uint32_t seqid)
{
    struct event_t ev;

    ev.type = EVENT_WINDOW_RESIZE_OFFER;
    ev.seqid = seqid;
    ev.win.x = new_x;
    ev.win.y = new_y;
    ev.win.w = new_w;
    ev.win.h = new_h;
    ev.win.flags = window->flags;
    //ev.win.shmid = window->shmid;
    ev.win.canvas_size = window->canvas_size;
    ev.win.canvas_pitch = window->canvas_pitch;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = window->winid;
    ev.valid_reply = 1;

    if(direct_write(window->clientfd->fd, (void *)&ev, 
                                sizeof(struct event_t)) < 0)
    {
        CHECK_DEAD_CLIENT(window);
    }
}

static inline void send_resize_confirmation(struct server_window_t *window, 
                                            uint32_t seqid)
{
    struct event_t ev;

    ev.type = EVENT_WINDOW_RESIZE_CONFIRM;
    ev.seqid = seqid;
    ev.win.x = window->resize.x;
    ev.win.y = window->resize.y;
    ev.win.w = window->resize.w;
    ev.win.h = window->resize.h;
    ev.win.flags = window->flags;
    ev.win.shmid = window->resize.shmid;
    ev.win.canvas_size = window->resize.canvas_size;
    ev.win.canvas_pitch = window->resize.canvas_pitch;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = window->winid;
    ev.valid_reply = 1;

    if(direct_write(window->clientfd->fd, (void *)&ev, 
                                    sizeof(struct event_t)) < 0)
    {
        CHECK_DEAD_CLIENT(window);
    }
}

static inline void send_pos_changed_event(struct server_window_t *window)
{
    struct event_t ev;

    ev.type = EVENT_WINDOW_POS_CHANGED;
    ev.win.x = window->x;
    ev.win.y = window->y;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = window->winid;
    ev.valid_reply = 1;

    if(direct_write(window->clientfd->fd, (void *)&ev, 
                                    sizeof(struct event_t)) < 0)
    {
        CHECK_DEAD_CLIENT(window);
    }
}

static inline void send_canvas_event(struct server_window_t *window, 
                                     uint32_t seqid)
{
    struct event_t ev;

    ev.type = EVENT_WINDOW_NEW_CANVAS;
    ev.seqid = seqid;
    ev.win.x = window->x;
    ev.win.y = window->y;
    ev.win.w = window->client_w;
    ev.win.h = window->client_h;
    ev.win.flags = window->flags;
    ev.win.shmid = window->shmid;
    ev.win.canvas_size = window->canvas_size;
    ev.win.canvas_pitch = window->canvas_pitch;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = window->winid;
    ev.valid_reply = 1;

    if(direct_write(window->clientfd->fd, (void *)&ev, 
                                    sizeof(struct event_t)) < 0)
    {
        CHECK_DEAD_CLIENT(window);
    }
}


static inline void send_err_event(int fd, winid_t dest, uint32_t evtype,
                                  int error, uint32_t seqid)
{
    struct event_t ev;

    ev.type = evtype;
    ev.seqid = seqid;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = dest;
    ev.err._errno = error;
    ev.valid_reply = 0;

    if(direct_write(fd, (void *)&ev, sizeof(struct event_t)) < 0)
    {
    }
}


static inline void notify_child(struct server_window_t *window,
                                uint32_t evtype, uint32_t seqid)
{
    struct event_t ev;

    ev.type = evtype;
    ev.seqid = seqid;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = window->winid;
    ev.winst.state = window->state;
    ev.valid_reply = 1;

    if(direct_write(window->clientfd->fd, (void *)&ev, 
                                    sizeof(struct event_t)) < 0)
    {
        CHECK_DEAD_CLIENT(window);
    }
}

#define notify_parent(win, evtype)                                  \
    if(!win->parent) return;                                        \
    if(notify_simple_event(win->parent->clientfd->fd, evtype,       \
                           win->parent->winid, win->winid, 0) == -1)\
        server_window_dead(win->parent);


static inline void notify_parent_win_created(struct server_window_t *window)
{
    notify_parent(window, EVENT_CHILD_WINDOW_CREATED);
}


static inline void notify_parent_win_destroyed(struct server_window_t *window)
{
    notify_parent(window, EVENT_CHILD_WINDOW_DESTROYED);
}

static inline void notify_win_shown(struct server_window_t *window)
{
    notify_child(window, EVENT_WINDOW_SHOWN, 0);
    notify_parent(window, EVENT_CHILD_WINDOW_SHOWN);
}

static inline void notify_win_hidden(struct server_window_t *window)
{
    notify_child(window, EVENT_WINDOW_HIDDEN, 0);
    notify_parent(window, EVENT_CHILD_WINDOW_HIDDEN);
}

static inline void notify_win_raised(struct server_window_t *window)
{
    notify_child(window, EVENT_WINDOW_RAISED, 0);
    notify_parent(window, EVENT_CHILD_WINDOW_RAISED);
}

static inline void notify_win_lowered(struct server_window_t *window)
{
    notify_child(window, EVENT_WINDOW_LOWERED, 0);
    notify_parent(window, EVENT_CHILD_WINDOW_LOWERED);
}

static inline void notify_win_lost_focus(struct server_window_t *window)
{
    notify_child(window, EVENT_WINDOW_LOST_FOCUS, 0);
}

static inline void notify_win_gained_focus(struct server_window_t *window)
{
    notify_child(window, EVENT_WINDOW_GAINED_FOCUS, 0);
}

static inline void notify_mouse_grab(struct server_window_t *window,
                                     int grabbed, uint32_t seqid)
{
    (void)grabbed;
    notify_child(window, EVENT_MOUSE_GRABBED, seqid);
}

static inline void notify_keyboard_grab(struct server_window_t *window,
                                        int grabbed, uint32_t seqid)
{
    (void)grabbed;
    notify_child(window, EVENT_KEYBOARD_GRABBED, seqid);
}

#endif      /* GUI_SERVER_EVENT_H */
