/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: group-border.h
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
 *  \file group-border.h
 *
 *  Declarations and struct definitions for the group border widget.
 */

#ifndef GROUP_BORDER_H
#define GROUP_BORDER_H

#include "../client/window.h"

struct group_border_t
{
    struct window_t window;
};


struct group_border_t *group_border_new(struct gc_t *, struct window_t *,
                                        int x, int y, int w, int h, char *);
void group_border_destroy(struct window_t *gbw);

void group_border_mouseover(struct window_t *gbw, 
                            struct mouse_state_t *mstate);
void group_border_mouseup(struct window_t *gbw, 
                            struct mouse_state_t *mstate);
void group_border_mousedown(struct window_t *gbw, 
                            struct mouse_state_t *mstate);
void group_border_mouseexit(struct window_t *gbw);
void group_border_repaint(struct window_t *gbw, int is_active_child);
void group_border_unfocus(struct window_t *gbw);
void group_border_focus(struct window_t *gbw);

void group_border_set_title(struct group_border_t *gb, char *new_title);

#endif //GROUP_BORDER_H
