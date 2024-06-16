/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: button-states.h
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
 *  \file button-states.h
 *
 *  Macro definitions of the different button states and definition of the
 *  button_color_t struct.
 */

#ifndef BUTTON_STATES_H
#define BUTTON_STATES_H

// Button states
#define BUTTON_STATE_NORMAL             0
#define BUTTON_STATE_MOUSEOVER          1
#define BUTTON_STATE_DOWN               2
#define BUTTON_STATE_PUSHED             3   /* for pushbuttons only */
#define BUTTON_STATE_DISABLED           4

// Button color array length
#define BUTTON_COLOR_ARRAY_LENGTH       5

// Button flags
#define BUTTON_FLAG_BORDERED            1
#define BUTTON_FLAG_BITMAP_MALLOCED     2   /* for imgbuttons only */
#define BUTTON_FLAG_FLATBORDER          4


struct button_color_t
{
    uint32_t bg, text, border;
};


#endif      /* BUTTON_STATES_H */
