/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: mouse-state-struct.h
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
 *  \file mouse-state-struct.h
 *
 *  Definition of the mouse state structure.
 */

#ifndef MOUSE_STATE_STRUCT_H
#define MOUSE_STATE_STRUCT_H

#include <kernel/mouse.h>

struct mouse_state_t
{
    int x, y;
    mouse_buttons_t buttons;
    int left_pressed, left_released;
    int right_pressed, right_released;
};

#endif      /* MOUSE_STATE_STRUCT_H */
