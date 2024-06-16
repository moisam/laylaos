/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: cursor.c
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
 *  \file cursor.c
 *
 *  Functions to work with the mouse cursor on the server side.
 */

#define GUI_SERVER
#include <pthread.h>
#include "../include/server/cursor.h"
#include "../include/rgb.h"

// Mouse image data
#define CA 0x000000FF //Black
#define CB 0xFFFFFFFF //White
#define C_ 0x00FF00FF //Clear (Green)

uint32_t transparent_color;
volatile curid_t old_cursor;
volatile curid_t cur_cursor;

uint32_t cursor_normal[MOUSE_BUFSZ] =
{
    CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_,
    CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_,
    CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_,
    CA, CB, CB, CB, CB, CB, CB, CA, CA, CA, CA, CA, CA, C_, C_, C_,
    CA, CB, CB, CB, CA, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CA, C_, CA, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CA, C_, C_, CA, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_,
    CA, CA, C_, C_, C_, C_, CA, CB, CB, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, CA, CB, CB, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, CA, CA, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
};

uint32_t cursor_we[MOUSE_BUFSZ] =
{
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, C_, C_, C_, C_, C_, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, CA, CA, C_, C_, C_, C_, C_, CA, CA, C_, C_, C_, C_, C_,
    C_, CA, CB, CA, CA, CA, CA, CA, CA, CA, CB, CA, C_, C_, C_, C_,
    CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_,
    C_, CA, CB, CA, CA, CA, CA, CA, CA, CA, CB, CA, C_, C_, C_, C_,
    C_, C_, CA, CA, C_, C_, C_, C_, C_, CA, CA, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, C_, C_, C_, C_, C_, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
};

uint32_t cursor_ns[MOUSE_BUFSZ] =
{
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CA, CB, CA, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CA, CB, CA, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
};

uint32_t cursor_nwse[MOUSE_BUFSZ] =
{
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CA, CA, CA, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CA, C_, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CA, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, CA, CA, CA, CA, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
};

uint32_t cursor_nesw[MOUSE_BUFSZ] =
{
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, CA, CA, CA, CA, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CA, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CA, C_, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CA, CA, CA, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
};

uint32_t cursor_cross[MOUSE_BUFSZ] =
{
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CA, CB, CA, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, C_, CA, CB, CA, C_, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, CA, CA, C_, CA, CB, CA, C_, CA, CA, C_, C_, C_, C_, C_,
    C_, CA, CB, CA, CA, CA, CB, CA, CA, CA, CB, CA, C_, C_, C_, C_,
    CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_,
    C_, CA, CB, CA, CA, CA, CB, CA, CA, CA, CB, CA, C_, C_, C_, C_,
    C_, C_, CA, CA, C_, CA, CB, CA, C_, CA, CA, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, C_, CA, CB, CA, C_, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CA, CB, CA, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
};

uint32_t cursor_crosshair[MOUSE_BUFSZ] =
{
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CA, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CA, CA, CA, CA, CA, CB, CA, CA, CA, CA, CA, CA, C_, C_, C_,
    CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_,
    CA, CA, CA, CA, CA, CA, CB, CA, CA, CA, CA, CA, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
};

uint32_t cursor_waiting[MOUSE_BUFSZ] =
{
    CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, C_, C_, C_,
    CA, CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA, CA, C_, C_, C_,
    CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, C_, C_, C_,
    C_, CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_,
    C_, CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_,
    C_, CA, CB, CB, CA, CB, CA, CB, CA, CB, CB, CA, C_, C_, C_, C_,
    C_, CA, CB, CB, CB, CA, CB, CA, CB, CB, CB, CA, C_, C_, C_, C_,
    C_, CA, CA, CB, CB, CB, CA, CB, CB, CB, CA, CA, C_, C_, C_, C_,
    C_, C_, CA, CA, CB, CB, CB, CB, CB, CA, CA, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CB, CA, CB, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CA, CB, CA, CA, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CA, CB, CA, CA, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CB, CB, CB, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, CA, CA, CB, CB, CA, CB, CB, CA, CA, C_, C_, C_, C_, C_,
    C_, CA, CA, CB, CB, CB, CB, CB, CB, CB, CA, CA, C_, C_, C_, C_,
    C_, CA, CB, CB, CB, CB, CA, CB, CB, CB, CB, CA, C_, C_, C_, C_,
    C_, CA, CB, CB, CB, CA, CB, CA, CB, CB, CB, CA, C_, C_, C_, C_,
    C_, CA, CB, CB, CA, CB, CA, CB, CA, CB, CB, CA, C_, C_, C_, C_,
    C_, CA, CB, CA, CB, CA, CB, CA, CB, CA, CB, CA, C_, C_, C_, C_,
    CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, C_, C_, C_,
    CA, CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA, CA, C_, C_, C_,
    CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
};

uint32_t cursor_ibeam[MOUSE_BUFSZ] =
{
    C_, C_, C_, CA, CA, CA, CA, CA, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CA, CB, CA, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CA, CB, CA, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CA, CA, CA, CA, CA, C_, C_, C_, C_, C_, C_,
};

uint32_t cursor_hand[MOUSE_BUFSZ] =
{
    C_, C_, C_, C_, C_, CA, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CB, CB, CA, CA, CA, CA, CA, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CB, CB, CA, CB, CB, CA, CB, CA, CA, C_, C_,
    C_, CA, CA, C_, CA, CB, CB, CA, CB, CB, CA, CB, CA, CB, CA, C_,
    CA, CB, CB, CA, CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA, C_,
    CA, CB, CB, CB, CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA, C_,
    C_, CA, CB, CB, CB, CB, CB, CA, CB, CA, CB, CA, CB, CB, CA, C_,
    C_, C_, CA, CB, CB, CB, CB, CA, CB, CA, CB, CA, CB, CB, CA, C_,
    C_, C_, CA, CB, CB, CB, CB, CA, CB, CA, CB, CA, CB, CA, C_, C_,
    C_, C_, C_, CA, CB, CB, CB, CA, CB, CA, CB, CA, CB, CA, C_, C_,
    C_, C_, C_, C_, CA, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CA, CA, CA, CA, CA, CA, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
};

uint32_t cursor_x[MOUSE_BUFSZ] =
{
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, CA, C_, C_, C_, C_,
    CA, CA, CA, C_, C_, C_, C_, C_, C_, C_, CA, CA, CA, C_, C_, C_,
    C_, CA, CA, CA, C_, C_, C_, C_, C_, CA, CA, CA, C_, C_, C_, C_,
    C_, C_, CA, CA, CA, C_, C_, C_, CA, CA, CA, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CA, C_, CA, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CA, CA, CA, CA, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CA, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CA, CA, CA, CA, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CA, C_, CA, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, CA, CA, CA, C_, C_, C_, CA, CA, CA, C_, C_, C_, C_, C_,
    C_, CA, CA, CA, C_, C_, C_, C_, C_, CA, CA, CA, C_, C_, C_, C_,
    CA, CA, CA, C_, C_, C_, C_, C_, C_, C_, CA, CA, CA, C_, C_, C_,
    C_, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, CA, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
};

/*
uint32_t cursor_waiting[MOUSE_BUFSZ] =
{
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
};
*/

struct cursor_t cursor[CURSOR_COUNT];


void prep_mouse_cursor(struct gc_t *gc)
{
    int x, y, z;
    
    A_memset(cursor, 0, sizeof(cursor));

    // set up buffer pointers and hotpoints
    cursor[CURSOR_NORMAL].data = cursor_normal;
    cursor[CURSOR_NORMAL].hotx = 0;
    cursor[CURSOR_NORMAL].hoty = 0;
    cursor[CURSOR_NORMAL].w = MOUSE_WIDTH;
    cursor[CURSOR_NORMAL].h = MOUSE_HEIGHT;

    cursor[CURSOR_WE].data = cursor_we;
    cursor[CURSOR_WE].hotx = 6;
    cursor[CURSOR_WE].hoty = 10;
    cursor[CURSOR_WE].w = MOUSE_WIDTH;
    cursor[CURSOR_WE].h = MOUSE_HEIGHT;

    cursor[CURSOR_NS].data = cursor_ns;
    cursor[CURSOR_NS].hotx = 6;
    cursor[CURSOR_NS].hoty = 10;
    cursor[CURSOR_NS].w = MOUSE_WIDTH;
    cursor[CURSOR_NS].h = MOUSE_HEIGHT;

    cursor[CURSOR_NWSE].data = cursor_nwse;
    cursor[CURSOR_NWSE].hotx = 6;
    cursor[CURSOR_NWSE].hoty = 10;
    cursor[CURSOR_NWSE].w = MOUSE_WIDTH;
    cursor[CURSOR_NWSE].h = MOUSE_HEIGHT;

    cursor[CURSOR_NESW].data = cursor_nesw;
    cursor[CURSOR_NESW].hotx = 6;
    cursor[CURSOR_NESW].hoty = 10;
    cursor[CURSOR_NESW].w = MOUSE_WIDTH;
    cursor[CURSOR_NESW].h = MOUSE_HEIGHT;

    cursor[CURSOR_CROSS].data = cursor_cross;
    cursor[CURSOR_CROSS].hotx = 6;
    cursor[CURSOR_CROSS].hoty = 10;
    cursor[CURSOR_CROSS].w = MOUSE_WIDTH;
    cursor[CURSOR_CROSS].h = MOUSE_HEIGHT;

    cursor[CURSOR_CROSSHAIR].data = cursor_crosshair;
    cursor[CURSOR_CROSSHAIR].hotx = 6;
    cursor[CURSOR_CROSSHAIR].hoty = 10;
    cursor[CURSOR_CROSSHAIR].w = MOUSE_WIDTH;
    cursor[CURSOR_CROSSHAIR].h = MOUSE_HEIGHT;

    cursor[CURSOR_WAITING].data = cursor_waiting;
    cursor[CURSOR_WAITING].hotx = 6;
    cursor[CURSOR_WAITING].hoty = 10;
    cursor[CURSOR_WAITING].w = MOUSE_WIDTH;
    cursor[CURSOR_WAITING].h = MOUSE_HEIGHT;

    cursor[CURSOR_IBEAM].data = cursor_ibeam;
    cursor[CURSOR_IBEAM].hotx = 6;
    cursor[CURSOR_IBEAM].hoty = 10;
    cursor[CURSOR_IBEAM].w = MOUSE_WIDTH;
    cursor[CURSOR_IBEAM].h = MOUSE_HEIGHT;

    cursor[CURSOR_HAND].data = cursor_hand;
    cursor[CURSOR_HAND].hotx = 5;
    cursor[CURSOR_HAND].hoty = 0;
    cursor[CURSOR_HAND].w = MOUSE_WIDTH;
    cursor[CURSOR_HAND].h = MOUSE_HEIGHT;

    cursor[CURSOR_X].data = cursor_x;
    cursor[CURSOR_X].hotx = 6;
    cursor[CURSOR_X].hoty = 11;
    cursor[CURSOR_X].w = MOUSE_WIDTH;
    cursor[CURSOR_X].h = MOUSE_HEIGHT;
    
    // set the current cursor
    cur_cursor = CURSOR_NORMAL;
    old_cursor = CURSOR_NORMAL;

    // convert all cursors to the current VGA mode format
    for(y = 0; y < MOUSE_HEIGHT; y++)
    {
        for(x = 0; x < MOUSE_WIDTH; x++)
        {
            int i = y * MOUSE_WIDTH + x;

            for(z = 1; z <= SYS_CURSOR_COUNT; z++)
            {
                if(gc->pixel_width == 1)
                {
                    cursor[z].data[i] = to_rgb8(gc, cursor[z].data[i]);
                }
                else if(gc->pixel_width == 2)
                {
                    cursor[z].data[i] = to_rgb16(gc, cursor[z].data[i]);
                }
                else if(gc->pixel_width == 3)
                {
                    cursor[z].data[i] = to_rgb24(gc, cursor[z].data[i]);
                }
                else
                {
                    cursor[z].data[i] = to_rgb32(gc, cursor[z].data[i]);
                }
            }
        }
    }
    
    if(gc->pixel_width == 1)
    {
        transparent_color = to_rgb8(gc, C_);
    }
    else if(gc->pixel_width == 2)
    {
        transparent_color = to_rgb16(gc, C_);
    }
    else if(gc->pixel_width == 3)
    {
        transparent_color = to_rgb24(gc, C_);
    }
    else
    {
        transparent_color = to_rgb32(gc, C_);
    }
}


curid_t server_cursor_load(struct gc_t *gc, int w, int h,
                           int hotx, int hoty, uint32_t *data)
{
    curid_t curid;
    int x, y;
    //uint32_t *copy;
    size_t datasz = w * 4 * h;
    
    if(datasz == 0 || data == NULL)
    {
        return 0;
    }
    
    // find an empty space in the cursor array
    for(curid = SYS_CURSOR_COUNT + 1; curid < CURSOR_COUNT; curid++)
    {
        if(cursor[curid].data == NULL)
        {
            break;
        }
    }
    
    // array is full
    if(curid >= CURSOR_COUNT)
    {
        return 0;
    }
    
    // make a copy of the data
    if(!(cursor[curid].data = malloc(datasz)))
    {
        return 0;
    }
    
    //A_memcpy(copy, data, datasz);

    // convert all cursors to the current VGA mode format
    for(y = 0; y < h; y++)
    {
        for(x = 0; x < w; x++)
        {
            int i = y * w + x;
            
            /*
             * Mask out the transparent pixels.
             *
             * TODO: Instead of this, use the alpha channel to draw the
             *       mouse pixels properly.
             */
            if(!(data[i] & 0xff))
            {
                cursor[curid].data[i] = transparent_color;
            }
            else
            {
                if(gc->pixel_width == 1)
                {
                    cursor[curid].data[i] = to_rgb8(gc, data[i]);
                }
                else if(gc->pixel_width == 2)
                {
                    cursor[curid].data[i] = to_rgb16(gc, data[i]);
                }
                else if(gc->pixel_width == 3)
                {
                    cursor[curid].data[i] = to_rgb24(gc, data[i]);
                }
                else
                {
                    cursor[curid].data[i] = to_rgb32(gc, data[i]);
                }
            }
        }
    }

    //cursor[curid].data = copy;
    cursor[curid].hotx = hotx;
    cursor[curid].hoty = hoty;
    cursor[curid].w = w;
    cursor[curid].h = h;
    cursor[curid].flags = CURSOR_FLAG_MALLOCED;
    
    return curid;
}


void server_cursor_free(curid_t curid)
{
    // don't free system cursors
    if(curid <= SYS_CURSOR_COUNT)
    {
        return;
    }
    
    // and check the cursor id is valid
    if(curid >= CURSOR_COUNT || cursor[curid].data == NULL ||
       !(cursor[curid].flags & CURSOR_FLAG_MALLOCED))
    {
        return;
    }
    
    free(cursor[curid].data);
    cursor[curid].data = NULL;
    cursor[curid].hotx = 0;
    cursor[curid].hoty = 0;
    cursor[curid].w = 0;
    cursor[curid].h = 0;
    cursor[curid].flags = 0;
}

