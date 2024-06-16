/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: desktop_entries.h
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
 *  \file desktop_entries.h
 *
 *  Declarations of functions to load desktop entries and to handle mouse
 *  events on the desktop window.
 */

#ifndef DESKTOP_ENTRIES_H
#define DESKTOP_ENTRIES_H

void load_desktop_entries(void);
void desktop_mouseover(struct window_t *window, int x, int y,
                       mouse_buttons_t buttons, unsigned long long ticks);

#endif      /* DESKTOP_ENTRIES_H */
