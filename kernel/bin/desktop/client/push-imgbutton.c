/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: push-imgbutton.c
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
 *  \file push-imgbutton.c
 *
 *  The implementation of a push image button widget. A push button maintains
 *  its pushed/released state until the mouse is clicked on it again.
 */

#include <string.h>
#include <stdlib.h>
#include "../include/client/imgbutton.h"
#include "../include/client/button.h"


struct imgbutton_t *push_imgbutton_new(struct gc_t *gc, 
                                       struct window_t *parent,
                                       int x, int y, int w, int h)
{
    struct imgbutton_t *button;

    if(!(button = imgbutton_new(gc, parent, x, y, w, h)))
    {
        return NULL;
    }

    button->window.type = WINDOW_TYPE_PUSHBUTTON;

    return button;
}

void imgbutton_set_push_state(struct imgbutton_t *button, int pushed)
{
    if(pushed)
    {
        button->push_state = 1;
        button->state = BUTTON_STATE_PUSHED;
    }
    else
    {
        button->push_state = 0;
        button->state = BUTTON_STATE_NORMAL;
    }
}

