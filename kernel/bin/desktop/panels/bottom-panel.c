/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: bottom-panel.c
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
 *  \file bottom-panel.c
 *
 *  The desktop bottom panel. This is shown at the bottom of the screen
 *  and shows a list of the open windows.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "../include/client/window.h"
#include "../include/client/button.h"
#include "../include/gui.h"
#include "../include/rect.h"
#include "../include/event.h"
#include "../include/font.h"
#include "../include/panels/bottom-panel.h"

#include "../client/inlines.c"


#define GLOB                        __global_gui_data


struct button_color_t cell_colors[BUTTON_COLOR_ARRAY_LENGTH] =
{
    // { background, text, border } for normal, mouse-over and pressed states
    { CELL_BGCOLOR, CELL_TEXTCOLOR, CELL_BGCOLOR },
    { OVERCELL_BGCOLOR, OVERCELL_TEXTCOLOR, OVERCELL_TEXTCOLOR },
    { DOWNCELL_BGCOLOR, DOWNCELL_TEXTCOLOR, DOWNCELL_TEXTCOLOR }
};


// the width of a single cell on the bottom panel
int cellw = 0;

// our bottom panel window
struct window_t *main_window;


void repaint_bg(struct window_t *window, int is_active_child)
{
    (void)is_active_child;
    (void)window;
    gc_fill_rect(main_window->gc, 0, 0, main_window->w, main_window->h,
                                                     BOTTOMPANEL_BGCOLOR);
}


void cell_repaint(struct window_t *button_window, int is_active_child)
{
    int title_len;
    struct button_t *b = (struct button_t *)button_window;
    int j;
    uint32_t border_color, bg_color, text_color;

    j = (main_window->h / 2) - (__global_gui_data.mono.charh / 2);
    
    if(is_active_child)
    {
        bg_color = TOPCELL_BGCOLOR;
        text_color = TOPCELL_TEXTCOLOR;
        border_color = TOPCELL_BGCOLOR;
    }
    else
    {
        bg_color = b->colors[b->state].bg;
        text_color = b->colors[b->state].text;
        border_color = b->colors[b->state].border;
    }

    gc_fill_rect(button_window->gc,
                 to_child_x(button_window, 1), to_child_y(button_window, 1),
                 button_window->w - 1, button_window->h - 1,
                 bg_color);

    gc_draw_rect(button_window->gc,
                 to_child_x(button_window, 0), to_child_y(button_window, 0),
                 button_window->w, button_window->h,
                 border_color);

    if(button_window->title)
    {
        // Get the title length
        title_len = strlen(button_window->title);

        // Convert it into pixels
        title_len *= __global_gui_data.mono.charw;

        // Draw the title within the button
        button_window->gc->clipping.clip_rects = button_window->clip_rects;
        gc_draw_text(button_window->gc, button_window->title,
                             to_child_x(button_window, 4),
                             to_child_y(button_window, j),
                             text_color, 0);
        button_window->gc->clipping.clip_rects = NULL;
    }
}


void cell_handler(struct button_t *button, int x, int y)
{
    (void)x;
    (void)y;

    winid_t winid;
    
    winid = (winid_t)(uintptr_t)button->internal_data;
    
    simple_request(REQUEST_WINDOW_TOGGLE_STATE, GLOB.server_winid, winid);
}


struct window_t *window_for_winid(winid_t winid)
{
    volatile ListNode *cur_node;
    volatile struct button_t *b;

    for(cur_node = main_window->children->root_node;
        cur_node != NULL;
        cur_node = cur_node->next)
    {
        b = (struct button_t *)cur_node->payload;
        
        if((winid_t)(uintptr_t)b->internal_data == winid)
        {
            return (struct window_t *)b;
        }
    }
    
    return NULL;
}


void window_created(winid_t winid)
{
    struct button_t *b;
    struct window_t *oldactive = main_window->active_child;
    
    if(window_for_winid(winid))
    {
        // window already on the list, don't add it
        return;
    }
    
    // create a new cell to represent the window
    if(!(b = button_new(main_window->gc, main_window,
                                         main_window->children->count * cellw, 1,
                                         cellw - 2, main_window->h - 2,
                                         NULL)))
    {
        return;
    }

    main_window->active_child = oldactive;
    b->window.repaint = cell_repaint;
    b->window.visible = 0;
    b->internal_data = (void *)(uintptr_t)winid;
    b->button_click_callback = cell_handler;
    memcpy(b->colors, cell_colors, sizeof(cell_colors));
}


void repaint_cells(struct window_t *win, struct window_t *oldactive)
{
    volatile ListNode *cur_node;
    Rect *rect;
    int new_cellw, x;

    if(main_window->children->count == 0)
    {
        // no kids? no problem!
        repaint_bg(main_window, 0);
        window_invalidate(main_window);
        return;
    }

    new_cellw = main_window->w / main_window->children->count;

    if(new_cellw > 250)
    {
        new_cellw = 250;
    }
    
    // new cell width - repaint everybody
    if(new_cellw != cellw)
    {
        cellw = new_cellw;

        for(cur_node = main_window->children->root_node, x = 0;
            cur_node != NULL;
            cur_node = cur_node->next, x += cellw)
        {
            win = (struct window_t *)cur_node->payload;
            win->x = x;
            win->w = cellw - 2;

            rect = win->clip_rects->root;
            rect->top = win->y;
            rect->left = win->x;
            rect->bottom = win->y + win->h - 1;
            rect->right = win->x + win->w - 1;
        }
        
        window_repaint(main_window);
        window_invalidate(main_window);
    }
    else
    {
        if(oldactive)
        {
            oldactive->repaint(oldactive, 0);
            child_invalidate(oldactive);
        }
        
        if(win)
        {
            win->repaint(win, 1);
            child_invalidate(win);
        }
    }
}


void window_destroyed(winid_t winid)
{
    volatile ListNode *cur_node;
    volatile struct button_t *b;
    volatile int i;

    for(cur_node = main_window->children->root_node, i = 0;
        cur_node != NULL;
        cur_node = cur_node->next, i++)
    {
        b = (struct button_t *)cur_node->payload;
        
        if((winid_t)(uintptr_t)b->internal_data == winid)
        {
            if(main_window->active_child == (struct window_t *)b)
            {
                main_window->active_child = NULL;
            }

            b->window.visible = 0;
            List_remove_at(main_window->children, i);
            button_destroy((struct window_t *)b);

            // force repaint of everything
            cellw = 0;
            repaint_cells(main_window->active_child, NULL);

            return;
        }
    }
}


void window_shown(winid_t winid)
{
    struct window_t *win, *oldactive;
    
    if(!(win = window_for_winid(winid)))
    {
        return;
    }
    
    oldactive = main_window->active_child;
    main_window->active_child = win;
    win->visible = 1;
    
    repaint_cells(win, oldactive);
}


void window_hidden(winid_t winid)
{
    struct window_t *win;
    
    if(!(win = window_for_winid(winid)))
    {
        return;
    }

    main_window->active_child = NULL;
    win->repaint(win, 0);
    child_invalidate(win);
}


void window_raised(winid_t winid)
{
    window_shown(winid);
}


void window_title_set(winid_t winid, char *title)
{
    struct window_t *win;
    char *new_title;

    if(!(win = window_for_winid(winid)))
    {
        return;
    }
    
    if(!(new_title = strdup(title)))
    {
        return;
    }
    
    if(win->title)
    {
        free(win->title);
    }
    
    win->title = new_title;
    
    if(win->visible)
    {
        win->repaint(win, (main_window->active_child == win));
        child_invalidate(win);
    }
}


int main(int argc, char **argv)
{
    /* volatile */ struct event_t *ev = NULL;
    struct window_attribs_t attribs;

    gui_init(argc, argv);

    attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
    attribs.x = 0;
    attribs.y = GLOB.screen.h - BOTTOMPANEL_HEIGHT;
    attribs.w = GLOB.screen.w;
    attribs.h = BOTTOMPANEL_HEIGHT;
    attribs.flags = WINDOW_NODECORATION | WINDOW_NORAISE | 
                    WINDOW_NOFOCUS | WINDOW_ALWAYSONTOP;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n", 
                        argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    main_window->repaint = repaint_bg;
    repaint_bg(main_window, 0);
    
    window_show(main_window);

    while(1)
    {
        if(!(ev = next_event()))
        {
            continue;
        }

        switch(ev->type)
        {
            case EVENT_CHILD_WINDOW_CREATED:
                window_created(ev->src);
                break;

            case EVENT_CHILD_WINDOW_DESTROYED:
                window_destroyed(ev->src);
                break;

            case EVENT_CHILD_WINDOW_SHOWN:
                window_shown(ev->src);
                break;

            case EVENT_CHILD_WINDOW_HIDDEN:
                window_hidden(ev->src);
                break;

            case EVENT_CHILD_WINDOW_RAISED:
                window_raised(ev->src);
                break;

            case EVENT_CHILD_WINDOW_TITLE_SET:
                window_title_set(ev->src, (char *)((struct event_buf_t *)ev)->buf);
                break;

            case EVENT_MOUSE:
                window_mouseover(main_window, ev->mouse.x, ev->mouse.y, 
                                              ev->mouse.buttons);
                break;

            case EVENT_MOUSE_EXIT:
                window_mouseexit(main_window, ev->mouse.buttons);
                break;

            default:
                break;
        }

        free(ev);
    }
}

