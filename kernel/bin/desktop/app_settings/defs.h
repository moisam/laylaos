/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: defs.h
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
 *  \file defs.h
 *
 *  Header file for the settings application.
 */

#ifndef SETTINGSAPP_H
#define SETTINGSAPP_H

#include <stdint.h>
#include "../include/window-defs.h"

// main.c
extern struct window_t *main_window;

// show_sysinfo.c
winid_t show_window_sysinfo(void);

// show_theme.c
winid_t show_window_theme(void);

// show_background.c
extern uint32_t cur_bgcolor;

winid_t show_window_background(void);
void get_desktop_bg(void);

// show_display.c
winid_t show_window_display(void);

#endif      /* SETTINGSAPP_H */
