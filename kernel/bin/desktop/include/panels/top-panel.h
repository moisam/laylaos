/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: top-panel.h
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
 *  \file top-panel.h
 *
 *  Declarations and struct definitions for the desktop's top panel.
 */

#ifndef TOP_PANEL_H
#define TOP_PANEL_H

#include "../theme.h"

#define TOPPANEL_HEIGHT             25

// defined in top-panel.c
extern struct window_t *main_window;
extern struct gc_t backbuf_gc;

#endif      /* TOP_PANEL_H */
