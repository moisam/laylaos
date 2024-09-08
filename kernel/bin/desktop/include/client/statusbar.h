/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: statusbar.h
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
 *  \file statusbar.h
 *
 *  Declarations and struct definitions for the statusbar widget.
 */

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include "../client/window.h"
#include "../mouse-state-struct.h"
#include "../font.h"

// standard inputbox height (depends on our fixed font height)
#define STATUSBAR_HEIGHT            (__global_gui_data.mono.charh + 8)

struct statusbar_t
{
    struct window_t window;
};

struct statusbar_t *statusbar_new(struct gc_t *, struct window_t *);
void statusbar_destroy(struct window_t *statusbar_window);

void statusbar_mouseover(struct window_t *statusbar_window, 
                            struct mouse_state_t *mstate);
void statusbar_mousedown(struct window_t *statusbar_window, 
                            struct mouse_state_t *mstate);
void statusbar_mouseup(struct window_t *statusbar_window, 
                            struct mouse_state_t *mstate);
void statusbar_mouseexit(struct window_t *statusbar_window);
void statusbar_unfocus(struct window_t *statusbar_window);
void statusbar_focus(struct window_t *statusbar_window);
void statusbar_repaint(struct window_t *statusbar_window, int is_active_child);
//void statusbar_size_changed(struct window_t *window);

void statusbar_append_text(struct window_t *statusbar_window, char *addstr);
void statusbar_set_text(struct window_t *statusbar_window, char *addstr);
void statusbar_size_changed(struct window_t *statusbar_window);

void statusbar_theme_changed(struct window_t *window);

#endif //STATUSBAR_H
