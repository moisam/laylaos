/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: desktop.c
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
 *  \file desktop.c
 *
 *  The desktop program core. Here we initialise the desktop and fork
 *  tasks for the top and bottom panels.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/hash.h>
#include <sys/wait.h>
#include "../include/client/window.h"
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/rect.h"
#include "../include/panels/bottom-panel.h"
#include "../include/panels/top-panel.h"
#include "../include/keys.h"
#include "../include/resources.h"
#include "desktop.h"
#include "desktop_entries.h"

#include "../client/inlines.c"

#define BOTTOM_PANEL_EXE        "/bin/desktop/desktop-bottom-panel"
#define TOP_PANEL_EXE           "/bin/desktop/desktop-top-panel"

#define GLOB                    __global_gui_data

pid_t bottom_panel_pid = 0;
pid_t top_panel_pid = 0;
winid_t bottom_panel_winid = 0;
winid_t top_panel_winid = 0;
winid_t mywinid = 0;
winid_t alttab_winid = 0;

#define INIT_HASHSZ             256

struct hashtab_t *loaded_icons = NULL;
struct window_t *desktop_window;
struct winent_t *winentries = NULL;


void remove_win_entry(winid_t winid)
{
    struct winent_t *prev = NULL, *ent = winentries;

    while(ent)
    {
        if(ent->winid == winid)
        {
            if(prev)
            {
                prev->next = ent->next;
            }
            else
            {
                winentries = ent->next;
            }
            
            free(ent);

            return;
        }
        
        prev = ent;
        ent = ent->next;
    }
}


void update_winent_icon(winid_t winid, resid_t resid)
{
    struct winent_t *ent;
    struct hashtab_item_t *hitem;
    struct bitmap32_t *bitmap = NULL, *tmp;

    // first check if the icon is already loaded
    if((hitem = hashtab_lookup(loaded_icons, (void *)(uintptr_t)resid)))
    {
        bitmap = (struct bitmap32_t *)hitem->val;
    }
    else
    {
        resid_t loaded_resid;
        
        if(!(bitmap = malloc(sizeof(struct bitmap32_t))))
        {
            return;
        }

        bitmap->width = 64;
        bitmap->height = 64;
        bitmap->data = NULL;

        // get the icon from the server
        if((loaded_resid = image_get(resid, bitmap)) == INVALID_RESID)
        {
            free(bitmap);
            return;
        }
        
        // ensure we got the right image
        if(loaded_resid != resid)
        {
            free(bitmap->data);
            free(bitmap);
            return;
        }

        // scale if needed
        if(bitmap->width != 64 || bitmap->height != 64)
        {
            tmp = image_resize(bitmap, 64, 64);
            free(bitmap->data);
            free(bitmap);
            bitmap = tmp;

            if(!bitmap)
            {
                return;
            }
        }
        
        hashtab_add(loaded_icons, (void *)(uintptr_t)resid, bitmap);
    }
    
    ent = winentries;
    
    // now find the window entry and set the icon
    while(ent)
    {
        if(ent->winid == winid)
        {
            if(ent->icon)
            {
                free(ent->icon);
            }
            
            ent->icon = bitmap;
            return;
        }
        
        ent = ent->next;
    }
}


void update_winent(winid_t winid, uint32_t evtype)
{
    struct winent_t *prev = NULL, *ent = winentries;
    
    while(ent)
    {
        if(ent->winid == winid)
        {
            switch(evtype)
            {
                /*
                case EVENT_CHILD_WINDOW_CREATED:
                case EVENT_CHILD_WINDOW_SHOWN:
                case EVENT_CHILD_WINDOW_HIDDEN:
                */
                
                case EVENT_CHILD_WINDOW_RAISED:
                    if(prev)
                    {
                        prev->next = ent->next;
                        ent->next = winentries;
                        winentries = ent;
                    }
                    
                    break;

                case EVENT_CHILD_WINDOW_DESTROYED:
                    //__asm__ __volatile__("xchg %%bx, %%bx"::);
                    remove_win_entry(winid);
                    break;
            }

            return;
        }
        
        prev = ent;
        ent = ent->next;
    }
}


struct winent_t *get_winent(winid_t winid)
{
    struct window_attribs_t attribs;
    struct winent_t *newent, *ent = winentries;
    
    while(ent)
    {
        if(ent->winid == winid)
        {
            return ent;
        }
        
        ent = ent->next;
    }

    // get the window's attributes
    if(!get_win_attribs(winid, &attribs))
    {
        return NULL;
    }
    
    // create a new entry
    if(!(newent = malloc(sizeof(struct winent_t))))
    {
        return NULL;
    }
    
    newent->winid = winid;
    newent->flags = attribs.flags;
    newent->title = NULL;
    newent->icon = NULL;
    newent->next = NULL;
    
    // now add it to our list
    if(!winentries)
    {
        winentries = newent;
    }
    else
    {
        ent = winentries;

        while(ent->next)
        {
            ent = ent->next;
        }
    
        ent->next = newent;
    }
    
    return newent;
}


/*
 * Returns 0 if the title is used so the caller won't free it.
 * Otherwise, returns -ENOENT and the caller can free the title string.
 */
int set_winent_title(winid_t winid, char *title)
{
    struct winent_t *winent;

    if(!(winent = get_winent(winid)))
    {
        return -ENOENT;
    }
    
    winent->title = title;
    
    return 0;
}


int should_notify_bottom_panel(winid_t winid)
{
    struct winent_t *winent;

    // do not notify about us or the top or bottom panels
    if(winid == mywinid || winid == top_panel_winid || 
                           winid == bottom_panel_winid)
    {
        return 0;
    }

    // do not notify about the ALT-TAB window    
    if(alttab_win && alttab_win->winid == winid)
    {
        return 0;
    }
    
    // do not notify if we cannot find the window in our list
    if(!(winent = get_winent(winid)))
    {
        return 0;
    }

    // only notify bottom panel if the window wants to be shown there
    return !(winent->flags & WINDOW_SKIPTASKBAR);
}


// Tell the bottom panel about all the windows that have been created
// before the bottom panel
void catch_up_with_bottom_panel(void)
{
    struct winent_t *ent = winentries;

    while(ent)
    {
        // do not notify about us or the top or bottom panels
        if(ent->winid == mywinid || ent->winid == top_panel_winid || 
                                    ent->winid == bottom_panel_winid)
        {
            ent = ent->next;
            continue;
        }
    
        // do not notify about the ALT-TAB window    
        if(alttab_win && alttab_win->winid == ent->winid)
        {
            ent = ent->next;
            continue;
        }

        // do not notify if the window does not want to be shown in the list
        if(ent->flags & WINDOW_SKIPTASKBAR)
        {
            ent = ent->next;
            continue;
        }

        // send a window creation followed by a window shown event
        simple_request(EVENT_CHILD_WINDOW_CREATED, bottom_panel_winid, ent->winid);
        simple_request(EVENT_CHILD_WINDOW_SHOWN, bottom_panel_winid, ent->winid);

        // if the window has a title, send a title set event
        if(ent->title)
        {
            notify_win_title_event(GLOB.serverfd, ent->title, 
                                   bottom_panel_winid, ent->winid);
        }

        // if the window has an icon, send an icon set event
        if(ent->icon)
        {
            simple_request(EVENT_CHILD_WINDOW_ICON_SET, 
                           bottom_panel_winid, ent->winid);
        }

        ent = ent->next;
    }
}


void sigchld_handler(int signum __attribute__((unused)))
{
    int        pid, st;
    int        saved_errno = errno;

    while((pid = waitpid(-1, &st, WNOHANG)) != 0)
    {
        if(errno == ECHILD)
        {
            break;
        }
    }

    errno = saved_errno;
}


/*
void sigsegv_handler(int signum __attribute__((unused)))
{
    __asm__ __volatile__("xchg %%bx, %%bx"::);
}
*/


void draw_desktop_background(void)
{
    gc_fill_rect(desktop_window->gc, 0, 0,
                 desktop_window->w, desktop_window->h, 0x16A085FF);
}


void redraw_desktop_background(int x, int y, int w, int h)
{
    gc_fill_rect(desktop_window->gc, x, y, w, h, 0x16A085FF);
}


int main(int argc, char **argv)
{
    /* volatile */ struct event_t *ev = NULL;
    Rect desktop_bounds;
    struct window_attribs_t attribs;
    struct sigaction act;

    gui_init(argc, argv);


    // if we launch an application (e.g. when the user double clicks an icon
    // on the desktop), we need to be ready to reap the zombie task when it
    // exits
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = sigchld_handler;
    act.sa_flags = SA_RESTART;
    (void)sigaction(SIGCHLD, &act, NULL);

    /*
    act.sa_handler = sigsegv_handler;
    (void)sigaction(SIGSEGV, &act, NULL);
    */

    mywinid = TO_WINID(GLOB.mypid, 0);
    attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = GLOB.screen.w;
    attribs.h = GLOB.screen.h;
    attribs.flags = WINDOW_NODECORATION | WINDOW_NORAISE | WINDOW_NOFOCUS;

    if(!(desktop_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n", 
                        argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    draw_desktop_background();

    loaded_icons = hashtab_create(INIT_HASHSZ, calc_hash_for_ptr, ptr_compare);
    load_desktop_entries();
    
    window_show(desktop_window);

    desktop_bounds.top = 0;
    desktop_bounds.left = 0;
    desktop_bounds.bottom = GLOB.screen.h - 1;
    desktop_bounds.right = GLOB.screen.w - 1;


    // start the bottom panel task
    if(!(bottom_panel_pid = fork()))
    {
        char *argv[] = { BOTTOM_PANEL_EXE, NULL };
        execvp(BOTTOM_PANEL_EXE, argv);
        exit(EXIT_FAILURE);
    }
    else if(bottom_panel_pid < 0)
    {
        fprintf(stderr, "%s: failed to fork bottom panel task: %s\n", 
                        argv[0], strerror(errno));
    }
    else
    {
        desktop_bounds.bottom -= BOTTOMPANEL_HEIGHT;
        bottom_panel_winid = TO_WINID(bottom_panel_pid, 0);
    }

    // start the top panel task
    if(!(top_panel_pid = fork()))
    {
        char *argv[] = { TOP_PANEL_EXE, NULL };
        execvp(TOP_PANEL_EXE, argv);
        exit(EXIT_FAILURE);
    }
    else if(top_panel_pid < 0)
    {
        fprintf(stderr, "%s: failed to fork top panel task: %s\n", 
                        argv[0], strerror(errno));
    }
    else
    {
        desktop_bounds.top = TOPPANEL_HEIGHT;
        top_panel_winid = TO_WINID(top_panel_pid, 0);
    }

    set_desktop_bounds(desktop_bounds.top, desktop_bounds.left,
                       desktop_bounds.bottom, desktop_bounds.right);

    desktop_init_alttab();
    
    while(1)
    {
        if(!(ev = next_event()))
        {
            continue;
        }

        switch(ev->type)
        {
            case EVENT_CHILD_WINDOW_CREATED:
            case EVENT_CHILD_WINDOW_SHOWN:
            case EVENT_CHILD_WINDOW_HIDDEN:
            case EVENT_CHILD_WINDOW_RAISED:
            case EVENT_CHILD_WINDOW_DESTROYED:
                if(bottom_panel_winid > 0)
                {
                    // keep a copy of these for now
                    uint32_t type = ev->type;
                    winid_t src = ev->src;
                    
                    // only notify bottom panel if the window wants to 
                    // be shown there
                    if(should_notify_bottom_panel(ev->src))
                    {
                        simple_request(type, bottom_panel_winid, src);
                    }
                    // if this is the bottom panel itself, catch up with
                    // the windows that were created before the bottom panel
                    // came to life
                    else if(src == bottom_panel_winid)
                    {
                        catch_up_with_bottom_panel();
                    }
                    
                    update_winent(src, type);

                    // Ignore window events from the ALT-TAB window
                    if(!alttab_win || alttab_win->winid != src)
                    {
                        desktop_cancel_alttab();
                    }
                }
                break;

            case EVENT_CHILD_WINDOW_TITLE_SET:
                if(bottom_panel_winid > 0)
                {
                    // keep a copy of these for now
                    winid_t src = ev->src;
                    char *copy = strdup((char *)((struct event_buf_t *)ev)->buf);
                    int dofree = 1;
                    
                    if(!copy)
                    {
                        break;
                    }
                    
                    dofree = set_winent_title(ev->src, copy);
                    
                    // only notify bottom panel if the window wants to 
                    // be shown there
                    if(should_notify_bottom_panel(ev->src))
                    {
                        notify_win_title_event(GLOB.serverfd, copy, 
                                               bottom_panel_winid, src);
                    }
                    
                    if(dofree)
                    {
                        free(copy);
                    }

                    desktop_cancel_alttab();
                }
                break;
            
            case EVENT_CHILD_WINDOW_ICON_SET:
                if(bottom_panel_winid > 0)
                {
                    struct event_res_t *evres = (struct event_res_t *)ev;

                    // keep a copy of these for now
                    uint32_t type = evres->type;
                    winid_t src = evres->src;
                    resid_t resid = evres->resid;

                    // only notify bottom panel if the window wants to 
                    // be shown there
                    if(should_notify_bottom_panel(ev->src))
                    {
                        simple_request(type, bottom_panel_winid, src);
                    }
                    
                    update_winent_icon(src, resid);
                }
                break;
            
            //case EVENT_WINDOW_LOST_FOCUS:
            case EVENT_WINDOW_LOWERED:
                if(alttab_win && alttab_win->winid == ev->dest)
                {
                    desktop_cancel_alttab();
                }
                break;

            case EVENT_WINDOW_RESIZE_OFFER:
                if(alttab_win && alttab_win->winid == ev->dest)
                {
                    window_resize(alttab_win, ev->win.x, ev->win.y,
                                              ev->win.w, ev->win.h);
                    desktop_draw_alttab();
                }
                break;

            case EVENT_MOUSE:
            {
                uint64_t millis = time_in_millis();
                desktop_mouseover(desktop_window, ev->mouse.x, ev->mouse.y,
                                                  ev->mouse.buttons, millis);
                break;
            }

            case EVENT_KEY_PRESS:
                if(ev->key.code == KEYCODE_TAB &&
                   ev->key.modifiers == MODIFIER_MASK_ALT)
                {
                    desktop_prep_alttab();
                }
                break;

            case EVENT_KEY_RELEASE:
                if(ev->key.code == KEYCODE_LALT ||
                   ev->key.code == KEYCODE_RALT /* ||
                   ev->key.code == KEYCODE_Z */)
                {
                    desktop_finish_alttab();
                }
                break;

            default:
                break;
        }

        free(ev);
    }
}

