/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: client-window.c
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
 *  \file client-window.c
 *
 *  Functions to create, destroy, and work with client-side windows.
 *  These are the windows all programs (except the server) deal with.
 *  The server has its own implementation of windows.
 */

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include "../include/gui.h"
#include "../include/client/window.h"
#include "../include/client/statusbar.h"
#include "../include/event.h"
#include "../include/mutex.h"
#include "../include/menu.h"
#include "../include/directrw.h"

#include "inlines.c"


#define GLOB        __global_gui_data

static mutex_t __winlist_lock;

static volatile uint16_t next_winid = 0;
static volatile struct window_t **winlist = NULL;
static volatile int wincount = 0;
static volatile int winlistsz = 0;


void window_repaint_default_bg(struct window_t *window, int is_active_child)
{
    gc_fill_rect(window->gc, 0, 0, window->w, window->h, window->bgcolor);
}


struct window_t *__window_create(struct window_attribs_t *attribs, 
                                 int8_t type, winid_t owner)
{
    struct window_t *window;
    volatile struct window_t **tmplist;
    struct event_t ev, *ev2;
    uint32_t reqtype;
    
    if(type == WINDOW_TYPE_MENU_FRAME)
    {
        reqtype = REQUEST_MENU_FRAME_CREATE;
    }
    else if(type == WINDOW_TYPE_DIALOG)
    {
        reqtype = REQUEST_DIALOG_CREATE;
    }
    else
    {
        reqtype = REQUEST_WINDOW_CREATE;
    }

    if(!attribs)
    {
        __set_errno(EINVAL);
        return NULL;
    }
    
    if(!(window = malloc(sizeof(struct window_t))))
    {
        __set_errno(ENOMEM);
        return NULL;
    }

    A_memset(window, 0, sizeof(struct window_t));

    if(!(window->children = List_new()))
    {
        __set_errno(ENOMEM);
        free(window);
        return NULL;
    }

    mutex_lock(&__winlist_lock);

    // ensure we have space in the window array
    if(wincount < winlistsz)
    {
        winlist[wincount] = window;
        wincount++;
    }
    else
    {
        int i;

        if(!(tmplist = realloc(winlist, (wincount + 10) * sizeof(struct window_t *))))
        {
            mutex_unlock(&__winlist_lock);
            free(window);
            __set_errno(ENOMEM);
            return NULL;
        }

        for(i = wincount; i < wincount + 10; i++)
        {
            tmplist[i] = NULL;
        }

        winlist = tmplist;
        winlist[wincount] = window;
        wincount++;
        //winlistsz++;
        winlistsz += 10;
    }

    // now let's get down to business
    window->winid = TO_WINID(GLOB.mypid, next_winid);
    next_winid++;

    mutex_unlock(&__winlist_lock);    

    // Account for the menu height (if we want one).
    // We don't pass the HASMENU flag to the server.
    if(attribs->flags & WINDOW_HASMENU)
    {
        attribs->h += MENU_HEIGHT;
        attribs->flags &= ~WINDOW_HASMENU;
    }
    
    if(attribs->flags & WINDOW_HASSTATUSBAR)
    {
        attribs->h += STATUSBAR_HEIGHT;
        attribs->flags &= ~WINDOW_HASSTATUSBAR;
    }

    uint32_t seqid = __next_seqid();

    ev.type = reqtype;
    ev.seqid = seqid;
    ev.win.gravity = attribs->gravity;
    ev.win.x = attribs->x;
    ev.win.y = attribs->y;
    ev.win.w = attribs->w;
    ev.win.h = attribs->h;
    ev.win.flags = attribs->flags;
    ev.win.owner = owner;
    ev.src = window->winid;
    ev.dest = GLOB.server_winid;
    //write(GLOB.serverfd, (void *)ev, sizeof(struct event_t));
    direct_write(GLOB.serverfd, &ev, sizeof(struct event_t));

    if(!(ev2 = get_server_reply(seqid)))
    {
        //__set_errno(EINVAL);
        free(window);
        return NULL;
    }

    if(ev2->type == EVENT_ERROR)
    {
        //__set_errno(ev->err._errno);
        free(window);
        free(ev2);
        return NULL;
    }
    
    window->type = type;
    window->x = ev2->win.x;
    window->y = ev2->win.y;
    window->w = ev2->win.w;
    window->h = ev2->win.h;
    window->flags = ev2->win.flags;
    window->canvas_size = ev2->win.canvas_size;
    window->canvas_pitch = ev2->win.canvas_pitch;
    window->shmid = ev2->win.shmid;
    free(ev2);
    
    if((window->canvas = shmat(window->shmid, NULL, 0)) == (void *)-1)
    {
        List_free(window->children);
        free(window);
        return NULL;
    }

    window->visible = 1;
    window->bgcolor = WINDOW_BGCOLOR;
    window->repaint = window_repaint_default_bg;
    window->gc = gc_new(window->w, window->h,
                        GLOB.screen.pixel_width, window->canvas,
                        window->canvas_size, window->canvas_pitch,
                        &GLOB.screen);

    if(window->gc)
    {
        gc_set_font(window->gc, GLOB.sysfont.data ? &GLOB.sysfont :
                                                    &GLOB.mono);
    }

    return window;
}


struct window_t *window_create(struct window_attribs_t *attribs)
{
    uint32_t flags = attribs->flags;
    struct window_t *win;
    
    if(!(win = __window_create(attribs, WINDOW_TYPE_WINDOW, 0)))
    {
        return NULL;
    }

    if(flags & WINDOW_HASSTATUSBAR)
    {
        win->statusbar = statusbar_new(win->gc, win);
    }
    
    return win;
}


static inline void __window_destroy(struct window_t *window)
{
    winid_t winid;

    if(window->gc)
    {
        free(window->gc);
        window->gc = NULL;
    }
    
    if(window->canvas)
    {
        shmctl(window->shmid, IPC_RMID, NULL);
        shmdt(window->canvas);
        window->shmid = 0;
        window->canvas = NULL;
    }
    
    winid = window->winid;
    free(window);

    simple_request(REQUEST_WINDOW_DESTROY, GLOB.server_winid, winid);
}


void window_destroy(struct window_t *window)
{
    int i, found;

    mutex_lock(&__winlist_lock);

    if(!window || !winlist)
    {
        mutex_unlock(&__winlist_lock);
        return;
    }

    // find the window in our list
    for(found = 0, i = 0; i < wincount; i++)
    {
        if(winlist[i] && (winlist[i] == window))
        {
            winlist[i] = winlist[wincount - 1];
            winlist[wincount - 1] = NULL;
            wincount--;
            found = 1;
            break;
        }
    }

    mutex_unlock(&__winlist_lock);

    // not found in the list
    if(!found)
    {
        return;
    }
    
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    __window_destroy(window);
}


void window_destroy_children(struct window_t *window)
{
    struct window_t *current_child;
    ListNode *current_node, *next_node;

    if(!window)
    {
        return;
    }

    if(!window->children)
    {
        return;
    }

    current_node = window->children->root_node;
    
    while(current_node)
    {
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        current_child = (struct window_t *)current_node->payload;
        next_node = current_node->next;
        
        if(current_child->destroy)
        {
            current_child->destroy(current_child);
        }
        
        Listnode_free(current_node);
        current_node = next_node;
    }
    
    List_free(window->children);
    window->children = NULL;
}


/*
 * Destroy all non-destroyed windows. Called on exit to cleanup.
 */
void window_destroy_all(void)
{
    int i;

    mutex_lock(&__winlist_lock);

    if(!winlist)
    {
        mutex_unlock(&__winlist_lock);
        return;
    }

    /*
    while(wincount)
    {
        window_destroy(winlist[0]);
        winlist[0] = NULL;
    }
    */

    for(i = 0; i < wincount; i++)
    {
        if(winlist[i])
        {
            __window_destroy(winlist[i]);
        }

        winlist[i] = NULL;
    }

    mutex_unlock(&__winlist_lock);
}


struct window_t *win_for_winid(winid_t winid)
{
    int i;

    mutex_lock(&__winlist_lock);

    if(!winlist)
    {
        mutex_unlock(&__winlist_lock);
        return NULL;
    }
    
    for(i = 0; i < wincount; i++)
    {
        if(winlist[i] && winlist[i]->winid == winid)
        {
            mutex_unlock(&__winlist_lock);
            return winlist[i];
        }
    }

    mutex_unlock(&__winlist_lock);
    
    return NULL;
}


#define window_request(w, req)      \
    if(w) simple_request(req, GLOB.server_winid, w->winid); \
    else return;

void window_show(struct window_t *window)
{
    if(window->type == WINDOW_TYPE_DIALOG)
    {
        window_request(window, REQUEST_DIALOG_SHOW);
    }
    else if(window->type == WINDOW_TYPE_MENU_FRAME)
    {
        window_request(window, REQUEST_MENU_FRAME_SHOW);
    }
    else
    {
        window_request(window, REQUEST_WINDOW_SHOW);
    }

    window->flags &= ~WINDOW_HIDDEN;
}


void window_hide(struct window_t *window)
{
    if(window->type == WINDOW_TYPE_DIALOG)
    {
        window_request(window, REQUEST_DIALOG_HIDE);
    }
    else if(window->type == WINDOW_TYPE_MENU_FRAME)
    {
        window_request(window, REQUEST_MENU_FRAME_HIDE);
    }
    else
    {
        window_request(window, REQUEST_WINDOW_HIDE);
    }

    window->flags |= WINDOW_HIDDEN;
}


void window_raise(struct window_t *window)
{
    if(!window)
    {
        return;
    }

    window_request(window, REQUEST_WINDOW_RAISE);
}


void window_maximize(struct window_t *window)
{
    if(!window)
    {
        return;
    }

    window_request(window, REQUEST_WINDOW_MAXIMIZE);
    window->flags &= ~WINDOW_HIDDEN;
}


void window_minimize(struct window_t *window)
{
    if(!window)
    {
        return;
    }

    window_request(window, REQUEST_WINDOW_MINIMIZE);
    window->flags |= WINDOW_HIDDEN;
}


void window_restore(struct window_t *window)
{
    if(!window)
    {
        return;
    }

    window_request(window, REQUEST_WINDOW_RESTORE);
    window->flags &= ~WINDOW_HIDDEN;
}


void window_enter_fullscreen(struct window_t *window)
{
    if(!window)
    {
        return;
    }

    window_request(window, REQUEST_WINDOW_ENTER_FULLSCREEN);
    window->flags &= ~WINDOW_HIDDEN;
}


void window_exit_fullscreen(struct window_t *window)
{
    if(!window)
    {
        return;
    }

    window_request(window, REQUEST_WINDOW_EXIT_FULLSCREEN);
    window->flags &= ~WINDOW_HIDDEN;
}


//Assign a string to the title of the window
void __window_set_title(struct window_t *window, char *new_title, 
                                                 int notify_parent)
{
    char *title_copy;
    size_t new_len = new_title ? strlen(new_title) + 1 : 0;
    
    if(!new_title || !new_len)
    {
        title_copy = NULL;
        window->title_len = 0;
    }
    else
    {
        if(!(title_copy = (char *)malloc((new_len) * sizeof(char))))
        {
            return;
        }
        
        memcpy(title_copy, new_title, new_len);
        window->title_len = new_len - 1;
    }
    
    //Make sure to free any preexisting title 
    if(window->title)
    {
        free(window->title);
        //window->title_alloced = 0;
    }

    window->title = title_copy;
    window->title_alloced = new_len;
    
    if(!notify_parent)
    {
        return;
    }

    size_t bufsz = sizeof(struct event_buf_t) + new_len;
    /* volatile */ struct event_buf_t *evbuf = malloc(bufsz + 1);

    if(!evbuf)
    {
        return;
    }

    A_memset((void *)evbuf, 0, bufsz);

    
    if(window->title)
    {
        memcpy((void *)evbuf->buf, window->title, new_len);
    }
    else
    {
        evbuf->buf[0] = '\0';
        bufsz++;
        new_len++;
    }

    evbuf->type = REQUEST_WINDOW_SET_TITLE;
    evbuf->seqid = __next_seqid();
    evbuf->bufsz = new_len;
    evbuf->src = window->winid;
    evbuf->dest = GLOB.server_winid;
    //write(GLOB.serverfd, (void *)evbuf, bufsz);
    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);

    free((void *)evbuf);
}


void window_set_title(struct window_t *window, char *new_title)
{
    __window_set_title(window, new_title, 1);
}


void window_resize(struct window_t *window, int16_t x, int16_t y,
                                            uint16_t w, uint16_t h)
{
    struct event_t ev, *ev2;
    uint32_t seqid = __next_seqid();

    ev.type = REQUEST_WINDOW_RESIZE_ACCEPT;
    ev.seqid = seqid;
    ev.win.x = x;
    ev.win.y = y;
    ev.win.w = w;
    ev.win.h = h;
    ev.src = window->winid;
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));

    if(!(ev2 = get_server_reply(seqid)))
    {
        return;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return;
    }

    int shmid = ev2->win.shmid;
    uint32_t canvas_size = ev2->win.canvas_size;
    uint32_t canvas_pitch = ev2->win.canvas_pitch;
    uint8_t *new_canvas;

    x = ev2->win.x;
    y = ev2->win.y;
    w = ev2->win.w;
    h = ev2->win.h;
    free(ev2);

    if(shmid != window->shmid)
    {
        if((new_canvas = shmat(shmid, NULL, 0)) == (void *)-1)
        {
            return;
        }

        //shmctl(window->shmid, IPC_RMID, NULL);
        shmdt(window->canvas);
        window->canvas = new_canvas;
        window->shmid = shmid;
    }

    window->x = x;
    window->y = y;
    window->w = w;
    window->h = h;
    window->canvas_size = canvas_size;
    window->canvas_pitch = canvas_pitch;
    
    window->gc->w = w;
    window->gc->h = h;
    window->gc->buffer = window->canvas;
    window->gc->buffer_size = canvas_size;
    window->gc->pitch = canvas_pitch;
    
    window_resize_layout(window);
    window_repaint(window);

    ev.type = REQUEST_WINDOW_RESIZE_FINALIZE;
    ev.seqid = 0;
    ev.src = window->winid;
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}


void window_repaint(struct window_t *window)
{
    if(window->type == WINDOW_TYPE_WINDOW || 
       window->type == WINDOW_TYPE_DIALOG)
    {
        //Fill in the window background
        window->repaint(window, 0);

        if(window->main_menu)
        {
            draw_mainmenu(window);
        }
    }
    else if(window->parent)
    {
        window->repaint(window, (window == window->parent->active_child));
    }
    else
    {
        window->repaint(window, 0);
    }

    struct window_t *current_child;
    ListNode *current_node;

    if(!window->children)
    {
        /*
        if(window->type == WINDOW_TYPE_WINDOW)
        {
            window_invalidate(window, 0, 0, window->h - 1, window->w - 1);
        }
        */

        return;
    }

    for(current_node = window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        current_child = (struct window_t *)current_node->payload;
        
        if(current_child->visible)
        {
            window_repaint((struct window_t *)current_child);
        }
    }
}


void window_set_icon(struct window_t *window, char *name)
{
    size_t len;
    size_t bufsz;
    /* volatile */ struct event_buf_t *evbuf;

    if(!name || !*name)
    {
        return;
    }

    len = strlen(name) + 1;
    
    if(name[0] != '/')
    {
        len += strlen(DEFAULT_ICON_PATH) + 1;
    }
    
    bufsz = sizeof(struct event_buf_t) + len;

    if(!(evbuf = malloc(bufsz)))
    {
        return;
    }

    if(name[0] == '/')
    {
        sprintf((void *)evbuf->buf, "%s", name);
    }
    else
    {
        sprintf((void *)evbuf->buf, "%s/%s", DEFAULT_ICON_PATH, name);
    }

    evbuf->type = REQUEST_WINDOW_SET_ICON;
    evbuf->seqid = __next_seqid();
    evbuf->bufsz = len;
    evbuf->src = window->winid;
    evbuf->dest = GLOB.server_winid;
    //write(GLOB.serverfd, (void *)evbuf, bufsz);
    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);

    free((void *)evbuf);
}


void window_load_icon(struct window_t *window,
                      unsigned int w, unsigned int h, uint32_t *data)
{
    size_t len = w * 4 * h;
    size_t bufsz;
    struct event_res_t *evbuf;

    // if we have data, we must have valid dimensions
    if(data && !len)
    {
        return;
    }

    bufsz = sizeof(struct event_cur_t) + (data ? len : 1);

    if(!(evbuf = malloc(bufsz)))
    {
        return;
    }
    
    if(data)
    {
        A_memcpy(evbuf->data, data, len);
    }
    else
    {
        evbuf->data[0] = '\0';
        w = 0;
        h = 0;
    }

    uint32_t seqid = __next_seqid();
    evbuf->seqid = seqid;
    evbuf->type = REQUEST_WINDOW_LOAD_ICON;
    evbuf->img.w = w;
    evbuf->img.h = h;
    evbuf->datasz = len;
    evbuf->src = window->winid;
    evbuf->dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);

    free((void *)evbuf);
}


void window_insert_child(struct window_t *window, struct window_t *child)
{
    child->parent = window;
    List_add(window->children, child);

    if(child->visible)
    {
        if(TABABLE(child))
        {
            widget_set_tabindex(child->parent, child);
            child->parent->active_child = child;
        }
        else
        {
            child->tab_index = -1;
        }
    }
}


void window_set_pos(struct window_t *window, int x, int y)
{
    /* volatile */ struct event_t ev;

    ev.type = REQUEST_WINDOW_SET_POS;
    ev.seqid = __next_seqid();
    ev.win.x = x;
    ev.win.y = y;
    ev.win.w = 0;
    ev.win.h = 0;
    ev.src = window->winid;
    ev.dest = GLOB.server_winid;
    //write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
    window->x = x;
    window->y = y;
}


void window_set_size(struct window_t *window, int x, int y, 
                                              uint16_t w, uint16_t h)
{
    struct event_t ev;
    uint32_t seqid = __next_seqid();

    ev.type = REQUEST_WINDOW_RESIZE;
    ev.seqid = seqid;
    ev.win.x = x;
    ev.win.y = y;
    ev.win.w = w;
    ev.win.h = h;
    ev.src = window->winid;
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}


void window_set_min_size(struct window_t *window, uint16_t w, uint16_t h)
{
    struct event_t ev;
    uint32_t seqid = __next_seqid();

    ev.type = REQUEST_WINDOW_SET_MIN_SIZE;
    ev.seqid = seqid;
    ev.win.x = 0;
    ev.win.y = 0;
    ev.win.w = w;
    ev.win.h = h;
    ev.src = window->winid;
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}


void window_set_attrib_xxx(struct window_t *window, int which, int unset)
{
    struct window_attribs_t attribs;
    /* volatile */ struct event_t ev;
    
    A_memset(&attribs, 0, sizeof(struct window_attribs_t));
    attribs.flags = window->flags;
    
    if(unset)
    {
        attribs.flags &= ~which;
    }
    else
    {
        attribs.flags |= which;
    }

    ev.type = REQUEST_WINDOW_SET_ATTRIBS;
    ev.seqid = __next_seqid();
    ev.winattr.winid = window->winid;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;
    //write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));

    window->flags = attribs.flags;
}


void window_set_bordered(struct window_t *window, int bordered)
{
    window_set_attrib_xxx(window, 
                          WINDOW_NODECORATION | WINDOW_NOCONTROLBOX, 
                          bordered);
}


void window_set_resizable(struct window_t *window, int resizable)
{
    window_set_attrib_xxx(window, WINDOW_NORESIZE, resizable);
}


void window_set_ontop(struct window_t *window, int ontop)
{
    window_set_attrib_xxx(window, WINDOW_ALWAYSONTOP, ontop);
}


void window_destroy_canvas(struct window_t *window)
{
    if(window->gc)
    {
        free(window->gc);
        window->gc = NULL;
    }
    
    if(window->canvas)
    {
        shmctl(window->shmid, IPC_RMID, NULL);
        shmdt(window->canvas);
        window->shmid = 0;
        window->canvas = NULL;
    }

    simple_request(REQUEST_WINDOW_DESTROY_CANVAS, 
                        GLOB.server_winid, window->winid);
}


int window_new_canvas(struct window_t *window)
{
    /* volatile */ struct event_t *ev;
    uint32_t seqid;

    if(!window)
    {
        return 0;
    }
    
    seqid = simple_request(REQUEST_WINDOW_NEW_CANVAS, 
                                GLOB.server_winid, window->winid);

    if(!(ev = get_server_reply(seqid)))
    {
        return 0;
    }

    if(ev->type == EVENT_ERROR)
    {
        free(ev);
        return 0;
    }

    window->canvas_size = ev->win.canvas_size;
    window->canvas_pitch = ev->win.canvas_pitch;
    window->shmid = ev->win.shmid;
    free(ev);
    
    if((window->canvas = shmat(window->shmid, NULL, 0)) == (void *)-1)
    {
        return 0;
    }

    window->gc = gc_new(window->w, window->h,
                        GLOB.screen.pixel_width, window->canvas,
                        window->canvas_size, window->canvas_pitch,
                        &GLOB.screen);
    return 1;
}


void window_set_focus_child(struct window_t *window, struct window_t *child)
{
    struct window_t *old_active;

    if(!window->children)
    {
        return;
    }
    
    old_active = window->active_child;
    window->active_child = child;

    if(old_active)
    {
        old_active->unfocus(old_active);
    }

    child->focus(child);
}


void window_resize_layout(struct window_t *window)
{
    struct window_t *current_child;
    ListNode *current_node;
    int y, h, sbarh;

    if(!window->children)
    {
        // tell the window its size changed
        if(window->size_changed)
        {
            window->size_changed(window);
        }

        return;
    }

    h = window->h - (window->main_menu ? MENU_HEIGHT : 0);
    y = (window->main_menu) ? MENU_HEIGHT : 0;
    sbarh = 0;
    
    // first round, do the fixed resizings
    for(current_node = window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        current_child = (struct window_t *)current_node->payload;
        
        if(current_child->visible)
        {
            if(current_child->type == WINDOW_TYPE_STATUSBAR)
            {
                current_child->y = window->h - current_child->h;
                sbarh = current_child->h;
                continue;
            }
            
            if(!current_child->resize_hints)
            {
                continue;
            }
            
            if(current_child->resize_hints & POSITION_LEFTTO)
            {
                if(!current_child->relative_to)
                {
                    continue;
                }
                
                current_child->x = current_child->relative_to->x -
                                   current_child->w -
                                   current_child->relative_x;
            }

            if(current_child->resize_hints & POSITION_RIGHTTO)
            {
                if(!current_child->relative_to)
                {
                    continue;
                }
                
                current_child->x = current_child->relative_to->x +
                                   current_child->relative_to->w +
                                   current_child->relative_x;
            }

            if(current_child->resize_hints & POSITION_ABOVE)
            {
                if(!current_child->relative_to)
                {
                    continue;
                }
                
                current_child->y = current_child->relative_to->y -
                                   current_child->h -
                                   current_child->relative_y;
            }

            if(current_child->resize_hints & POSITION_BELOW)
            {
                if(!current_child->relative_to)
                {
                    continue;
                }
                
                current_child->y = current_child->relative_to->y +
                                   current_child->relative_to->h +
                                   current_child->relative_y;
            }

            if(current_child->resize_hints & POSITION_CENTERH)
            {
                current_child->x = (window->w - current_child->w) / 2;
            }

            if(current_child->resize_hints & POSITION_CENTERV)
            {
                current_child->y = y + (h - current_child->h) / 2;
            }
        }
    }
    
    // second round, do the fillouts
    for(current_node = window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        current_child = (struct window_t *)current_node->payload;
        
        if(current_child->visible)
        {
            if(!current_child->resize_hints)
            {
                continue;
            }

            if((current_child->resize_hints & RESIZE_FILLW) &&
               !(current_child->resize_hints & RESIZE_FIXEDW))
            {
                current_child->w = window->w - current_child->x;
            }

            if((current_child->resize_hints & RESIZE_FILLH) &&
               !(current_child->resize_hints & RESIZE_FIXEDH))
            {
                current_child->h = window->h - current_child->y - sbarh;
            }
        }
    }

    // third round, notify child widgets
    for(current_node = window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        current_child = (struct window_t *)current_node->payload;
        
        if(current_child->visible && current_child->size_changed)
        {
            current_child->size_changed(current_child);
        }
    }

    // last, tell the window its size changed
    if(window->size_changed)
    {
        window->size_changed(window);
    }
}

