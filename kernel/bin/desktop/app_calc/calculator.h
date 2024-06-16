/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: calculator.h
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
 *  \file calculator.h
 *
 *  Header file for the calculator application.
 */

#ifndef CALCULATOR_H
#define CALCULATOR_H

#include "../include/client/window.h"
#include "../include/client/button.h"
#include "../include/client/textbox.h"

//Another special case of Window
typedef struct Calculator_struct
{
    //struct window_t window; //'inherit' Window
    struct window_t *window;
    struct textbox_t *text_box;
    struct button_t *button_1;
    struct button_t *button_2;
    struct button_t *button_3;
    struct button_t *button_4;
    struct button_t *button_5;
    struct button_t *button_6;
    struct button_t *button_7;
    struct button_t *button_8;
    struct button_t *button_9;
    struct button_t *button_0;
    struct button_t *button_add;
    struct button_t *button_sub;
    struct button_t *button_div;
    struct button_t *button_mul;
    struct button_t *button_ent;
    struct button_t *button_dot;
    struct button_t *button_mod;
    struct button_t *button_c;
} Calculator;

Calculator *Calculator_new(void);

void show_about_dialog(void);

#endif //CALCULATOR_H
