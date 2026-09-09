/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
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
#define CA      0x000000FF // Black
#define CB      0xFFFFFFFF // White
#define C_      0x00000000 // Clear

volatile curid_t old_cursor;
volatile curid_t cur_cursor;
volatile int cur_timer_set;

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
    C_, C_, C_, C_, C_, CA, C_, C_, C_, C_, CA, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CA, C_, C_, C_, C_, CA, CA, C_, C_, C_, C_,
    C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_,
    C_, C_, CA, CB, CB, CA, C_, C_, C_, C_, CA, CB, CB, CA, C_, C_,
    C_, CA, CB, CB, CB, CA, CA, CA, CA, CA, CA, CB, CB, CB, CA, C_,
    CA, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CB, CA,
    C_, CA, CB, CB, CB, CA, CA, CA, CA, CA, CA, CB, CB, CB, CA, C_,
    C_, C_, CA, CB, CB, CA, C_, C_, C_, C_, CA, CB, CB, CA, C_, C_,
    C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, CA, CA, C_, C_, C_, C_, CA, CA, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, C_, C_, C_, C_, CA, C_, C_, C_, C_, C_,
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
    C_, C_, C_, CA, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, CA, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_,
    C_, CA, CA, CA, CA, CA, CB, CA, CA, CA, CA, CA, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, CA, CA, CA, CA, CA, CB, CA, CA, CA, CA, CA, C_, C_, C_, C_,
    C_, C_, CA, CB, CB, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, CA, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
};

uint32_t cursor_nwse[MOUSE_BUFSZ] =
{
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CA, CA, CA, CA, CA, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CA, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, CA, CB, CA, C_, CA, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CA, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CB, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, CA, CB, CB, CB, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, CA, CA, CA, CA, CA, CA, CA, C_, C_, C_,
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
    C_, C_, C_, C_, C_, C_, CA, CA, CA, CA, CA, CA, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, CA, CB, CB, CB, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CB, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, C_, CA, CB, CA, CB, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, C_, CA, CB, CA, C_, CA, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, CA, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CA, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_, C_,
    CA, CA, CA, CA, CA, CA, CA, C_, C_, C_, C_, C_, C_, C_, C_, C_,
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
    C_, C_, C_, C_, C_, CA, CA, CA, C_, C_, C_, C_, C_, C_, C_, C_,
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

uint32_t cursor_empty[MOUSE_BUFSZ] =
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

struct cursor_t *cursor[CURSOR_COUNT] = { 0, };

#define DEFINE_CURSOR(index, bits, hx, hy)      \
    cursor[index]->bitmaps[0].data = bits;      \
    cursor[index]->bitmaps[0].hotx = hx;        \
    cursor[index]->bitmaps[0].hoty = hy;


void prep_mouse_cursor(struct gc_t *gc)
{
    int x;

    // set the current cursor
    cur_cursor = CURSOR_NORMAL;
    old_cursor = CURSOR_NORMAL;
    cur_timer_set = 0;

    // set the empty cursor
    cursor[CURSOR_NONE] = malloc(sizeof(struct cursor_t));
    cursor[CURSOR_NONE]->count = 1;
    cursor[CURSOR_NONE]->curframe = 0;
    cursor[CURSOR_NONE]->bitmaps[0].flags = 0;
    cursor[CURSOR_NONE]->bitmaps[0].w = MOUSE_WIDTH;
    cursor[CURSOR_NONE]->bitmaps[0].h = MOUSE_HEIGHT;
    DEFINE_CURSOR(CURSOR_NONE, cursor_empty, 0, 0);

    // try to load the default X11 style cursors
    if(prep_mouse_cursor_x11(24) == 0)
    {
        return;
    }

    // fallback to builtin cursors
    for(x = 0; x <= SYS_CURSOR_COUNT; x++)
    {
        cursor[x] = malloc(sizeof(struct cursor_t));
        cursor[x]->count = 1;
        cursor[x]->curframe = 0;
        cursor[x]->bitmaps[0].flags = 0;
        cursor[x]->bitmaps[0].w = MOUSE_WIDTH;
        cursor[x]->bitmaps[0].h = MOUSE_HEIGHT;
    }

    // set up buffer pointers and hotpoints
    DEFINE_CURSOR(CURSOR_NORMAL, cursor_normal, 0, 0);
    DEFINE_CURSOR(CURSOR_WE, cursor_we, 7, 11);
    DEFINE_CURSOR(CURSOR_E, cursor_we, 7, 11);
    DEFINE_CURSOR(CURSOR_W, cursor_we, 7, 11);
    DEFINE_CURSOR(CURSOR_COLRESIZE, cursor_we, 7, 11);
    DEFINE_CURSOR(CURSOR_NS, cursor_ns, 6, 11);
    DEFINE_CURSOR(CURSOR_N, cursor_ns, 6, 11);
    DEFINE_CURSOR(CURSOR_S, cursor_ns, 6, 11);
    DEFINE_CURSOR(CURSOR_ROWRESIZE, cursor_ns, 6, 11);
    DEFINE_CURSOR(CURSOR_NWSE, cursor_nwse, 6, 11);
    DEFINE_CURSOR(CURSOR_NW, cursor_nwse, 6, 11);
    DEFINE_CURSOR(CURSOR_SE, cursor_nwse, 6, 11);
    DEFINE_CURSOR(CURSOR_NESW, cursor_nesw, 6, 10);
    DEFINE_CURSOR(CURSOR_NE, cursor_nesw, 6, 10);
    DEFINE_CURSOR(CURSOR_SW, cursor_nesw, 6, 10);
    DEFINE_CURSOR(CURSOR_FLEUR, cursor_cross, 6, 10);
    DEFINE_CURSOR(CURSOR_CROSSHAIR, cursor_crosshair, 6, 10);
    DEFINE_CURSOR(CURSOR_CELL, cursor_crosshair, 6, 10);
    DEFINE_CURSOR(CURSOR_WAITING, cursor_waiting, 6, 10);
    DEFINE_CURSOR(CURSOR_IBEAM, cursor_ibeam, 6, 10);
    DEFINE_CURSOR(CURSOR_VIBEAM, cursor_ibeam, 6, 10);
    DEFINE_CURSOR(CURSOR_OPEN_HAND, cursor_hand, 5, 0);
    DEFINE_CURSOR(CURSOR_CLOSED_HAND, cursor_hand, 5, 0);
    DEFINE_CURSOR(CURSOR_POINTING_HAND, cursor_hand, 5, 0);
    DEFINE_CURSOR(CURSOR_X, cursor_x, 6, 11);
    DEFINE_CURSOR(CURSOR_FORBIDDEN, cursor_x, 6, 11);

    // fallback for cursors with no builtin sprites
    DEFINE_CURSOR(CURSOR_HELP, cursor_normal, 0, 0);
    DEFINE_CURSOR(CURSOR_ARROW_WAITING, cursor_normal, 0, 0);
    DEFINE_CURSOR(CURSOR_CONTEXT_MENU, cursor_normal, 0, 0);
    DEFINE_CURSOR(CURSOR_ZOOMIN, cursor_normal, 0, 0);
    DEFINE_CURSOR(CURSOR_ZOOMOUT, cursor_normal, 0, 0);
    DEFINE_CURSOR(CURSOR_DND_LINK, cursor_normal, 0, 0);
    DEFINE_CURSOR(CURSOR_DND_COPY, cursor_normal, 0, 0);
    DEFINE_CURSOR(CURSOR_DND_MOVE, cursor_normal, 0, 0);
    DEFINE_CURSOR(CURSOR_UP, cursor_normal, 0, 0);
}

#undef DEFINE_CURSOR


curid_t server_cursor_load(struct gc_t *gc, int w, int h,
                           int hotx, int hoty, uint32_t *data)
{
    curid_t curid;
    size_t datasz = w * 4 * h;
    
    if(datasz == 0 || data == NULL)
    {
        return 0;
    }
    
    // find an empty space in the cursor array
    for(curid = SYS_CURSOR_COUNT + 1; curid < CURSOR_COUNT; curid++)
    {
        if(cursor[curid] == NULL)
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
    if(!(cursor[curid] = malloc(sizeof(struct cursor_t))))
    {
        return 0;
    }

    if(!(cursor[curid]->bitmaps[0].data = malloc(datasz)))
    {
        free(cursor[curid]);
        cursor[curid] = NULL;
        return 0;
    }
    
    A_memcpy(cursor[curid]->bitmaps[0].data, data, datasz);

    cursor[curid]->count = 1;
    cursor[curid]->curframe = 0;
    cursor[curid]->bitmaps[0].hotx = hotx;
    cursor[curid]->bitmaps[0].hoty = hoty;
    cursor[curid]->bitmaps[0].w = w;
    cursor[curid]->bitmaps[0].h = h;
    cursor[curid]->bitmaps[0].flags = CURSOR_FLAG_MALLOCED;
    
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
    if(curid >= CURSOR_COUNT || cursor[curid] == NULL ||
       cursor[curid]->bitmaps[0].data == NULL ||
       !(cursor[curid]->bitmaps[0].flags & CURSOR_FLAG_MALLOCED))
    {
        return;
    }

    cursor_struct_free(cursor[curid]);
    cursor[curid] = NULL;
}

