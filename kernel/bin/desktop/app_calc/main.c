/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: main.c
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
 *  \file main.c
 *
 *  A simple calculator program.
 */

#include <inttypes.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/shm.h>
#include "calculator.h"
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/keys.h"
#include "../include/menu.h"
#include "../include/clipboard.h"
#include "../include/client/dialog.h"


#define GLOB        __global_gui_data

#define ARGSZ       256

Calculator calculator;
struct window_t *main_window;
int last_op = 0;
int has_dot = 0;
int clear_next = 0;
int cur_arg = 0;
char arg[2][ARGSZ];
int arglen[2];


static void calculate(void)
{
    double arg0 = atof(arg[0]);
    double arg1 = atof(arg[1]);
    char buf[32];

    switch(last_op)
    {
        case '+':
            arg0 += arg1;
            break;

        case '-':
            arg0 -= arg1;
            break;

        case '*':
            arg0 *= arg1;
            break;

        case '/':
            arg0 /= arg1;
            break;

        case '%':
            arg0 = (long)arg0 % (long)arg1;
            break;
    }

    if(arg0 == (long)arg0)
    {
        sprintf(buf, "%ld", (long)arg0);
    }
    else
    {
        sprintf(buf, "%f", arg0);
    }

    strcpy(arg[0], buf);
    arglen[0] = strlen(arg[0]);
    textbox_set_text((struct window_t *)calculator.text_box, buf);
}


static void append(int op)
{
    char c[2];

    if(arglen[cur_arg] >= ARGSZ)
    {
        return;
    }

    c[0] = op & 0xff;
    c[1] = '\0';
    textbox_append_text((struct window_t *)calculator.text_box, c);
    arg[cur_arg][arglen[cur_arg]++] = c[0];
    arg[cur_arg][arglen[cur_arg]] = c[1];
}


static void append_digit(int op)
{
    if(clear_next || calculator.text_box->window.title[0] == '0')
    {
        textbox_set_text((struct window_t *)calculator.text_box, "");
        clear_next = 0;
        arglen[cur_arg] = 0;
        arg[cur_arg][0] = '\0';
    }

    append(op);
}


void calculator_button_handler(struct button_t *button, int x, int y)
{
    int op = (int)(uintptr_t)button->internal_data;

    switch(op)
    {
        case '.':
            if(has_dot)
            {
                return;
            }

            if(clear_next)
            {
                textbox_set_text((struct window_t *)calculator.text_box, "0");
                arg[cur_arg][arglen[cur_arg]++] = '0';
                clear_next = 0;
            }

            has_dot = 1;
            append(op);
            return;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            append_digit(op);
            return;

        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
            if(cur_arg == 0)
            {
                arg[++cur_arg][0] = '\0';
                arglen[cur_arg] = 0;
                clear_next++;
                last_op = op;
                has_dot = 0;
                return;
            }

            if(arg[cur_arg][0] == '\0')
            {
                last_op = op;
                return;
            }

            calculate();
            arg[cur_arg][0] = '\0';
            arglen[cur_arg] = 0;
            arglen[0] = strlen(arg[0]);
            clear_next++;
            last_op = op;
            has_dot = 0;
            return;

        case 'C':
            last_op = 0;
            has_dot = 0;
            cur_arg = 0;
            clear_next = 0;
            arg[0][0] = '\0';
            arg[1][0] = '\0';
            arglen[0] = 0;
            arglen[1] = 0;
            textbox_set_text((struct window_t *)calculator.text_box, "0");
            return;

        case '=':
            if(cur_arg == 0 || arglen[cur_arg] == 0)
            {
                return;
            }

            calculate();
            last_op = 0;
            has_dot = 0;
            cur_arg = 0;
            clear_next++;
            arg[1][0] = '\0';
            arglen[1] = 0;
            return;
    }
}

void calculator_init()
{
    // Set a default title
    window_set_title(main_window, "Calculator");

    // Create the buttons
    calculator.button_7 = button_new(main_window->gc, main_window,
                                        5, 30, 30, 30, "7");
    calculator.button_8 = button_new(main_window->gc, main_window,
                                        40, 30, 30, 30, "8");
    calculator.button_9 = button_new(main_window->gc, main_window,
                                        75, 30, 30, 30, "9");
    calculator.button_div = button_new(main_window->gc, main_window,
                                        110, 30, 30, 30, "/");
    calculator.button_c = button_new(main_window->gc, main_window,
                                        145, 30, 30, 30, "C");

    calculator.button_4 = button_new(main_window->gc, main_window,
                                        5, 65, 30, 30, "4");
    calculator.button_5 = button_new(main_window->gc, main_window,
                                        40, 65, 30, 30, "5");
    calculator.button_6 = button_new(main_window->gc, main_window,
                                        75, 65, 30, 30, "6");
    calculator.button_mul = button_new(main_window->gc, main_window,
                                        110, 65, 30, 30, "*");

    calculator.button_1 = button_new(main_window->gc, main_window,
                                        5, 100, 30, 30, "1");
    calculator.button_2 = button_new(main_window->gc, main_window,
                                        40, 100, 30, 30, "2");
    calculator.button_3 = button_new(main_window->gc, main_window,
                                        75, 100, 30, 30, "3");
    calculator.button_sub = button_new(main_window->gc, main_window,
                                        110, 100, 30, 30, "-");

    calculator.button_0 = button_new(main_window->gc, main_window,
                                        5, 135, 30, 30, "0");
    calculator.button_dot = button_new(main_window->gc, main_window,
                                        40, 135, 30, 30, ".");
    calculator.button_mod = button_new(main_window->gc, main_window,
                                        75, 135, 30, 30, "%");
    calculator.button_add = button_new(main_window->gc, main_window,
                                        110, 135, 30, 30, "+");

    calculator.button_ent = button_new(main_window->gc, main_window,
                                        145, 65, 30, 100, "=");

    calculator.button_0->internal_data = (void *)('0');
    calculator.button_1->internal_data = (void *)('1');
    calculator.button_2->internal_data = (void *)('2');
    calculator.button_3->internal_data = (void *)('3');
    calculator.button_4->internal_data = (void *)('4');
    calculator.button_5->internal_data = (void *)('5');
    calculator.button_6->internal_data = (void *)('6');
    calculator.button_7->internal_data = (void *)('7');
    calculator.button_8->internal_data = (void *)('8');
    calculator.button_9->internal_data = (void *)('9');
    calculator.button_div->internal_data = (void *)('/');
    calculator.button_mul->internal_data = (void *)('*');
    calculator.button_sub->internal_data = (void *)('-');
    calculator.button_dot->internal_data = (void *)('.');
    calculator.button_mod->internal_data = (void *)('%');
    calculator.button_add->internal_data = (void *)('+');
    calculator.button_ent->internal_data = (void *)('=');
    calculator.button_c->internal_data = (void *)('C');

    // We'll use the same handler to handle all of the buttons
    calculator.button_1->button_click_callback = calculator_button_handler;
    calculator.button_2->button_click_callback = calculator_button_handler;
    calculator.button_3->button_click_callback = calculator_button_handler;
    calculator.button_4->button_click_callback = calculator_button_handler;
    calculator.button_5->button_click_callback = calculator_button_handler;
    calculator.button_6->button_click_callback = calculator_button_handler;
    calculator.button_7->button_click_callback = calculator_button_handler;
    calculator.button_8->button_click_callback = calculator_button_handler;
    calculator.button_9->button_click_callback = calculator_button_handler;
    calculator.button_0->button_click_callback = calculator_button_handler;
    calculator.button_add->button_click_callback = calculator_button_handler;
    calculator.button_sub->button_click_callback = calculator_button_handler;
    calculator.button_mul->button_click_callback = calculator_button_handler;
    calculator.button_div->button_click_callback = calculator_button_handler;
    calculator.button_ent->button_click_callback = calculator_button_handler;
    calculator.button_c->button_click_callback = calculator_button_handler;
    calculator.button_dot->button_click_callback = calculator_button_handler;
    calculator.button_mod->button_click_callback = calculator_button_handler;

    // Create the textbox
    calculator.text_box = textbox_new(main_window->gc, main_window,
                                        5, 5, 170, 20, "0");
}


void menu_file_close_handler(winid_t winid)
{
    window_destroy(main_window);
    gui_exit(EXIT_SUCCESS);
}


void menu_file_copy_handler(winid_t winid)
{
    char *buf = calculator.text_box->window.title;
    size_t buflen = strlen(buf);

    if(*buf == '0')
    {
        return;
    }

    clipboard_set_data(CLIPBOARD_FORMAT_TEXT, buf, buflen + 1);
}


void menu_file_paste_handler(winid_t winid)
{
    size_t datasz;
    void *data;
    char *buf = calculator.text_box->window.title;
    
    if(!(datasz = clipboard_has_data(CLIPBOARD_FORMAT_TEXT)))
    {
        return;
    }
    
    if(!(data = clipboard_get_data(CLIPBOARD_FORMAT_TEXT)))
    {
        return;
    }

    // ensure pasted data is null-terminated
    if(((char *)data)[datasz - 1] != '\0')
    {
        char *tmp;

        if(!(tmp = realloc(data, datasz + 1)))
        {
            free(data);
            return;
        }

        data = tmp;
        datasz++;
    }

    if(*buf == '0')
    {
        if(datasz >= ARGSZ)
        {
            strncpy(arg[cur_arg], data, ARGSZ - 1);
            arg[cur_arg][ARGSZ - 1] = '\0';
            arglen[cur_arg] = ARGSZ - 1;
        }
        else
        {
            strcpy(arg[cur_arg], data);
            arglen[cur_arg] = datasz;
        }
    }
    else
    {
        if(datasz + arglen[cur_arg] >= ARGSZ)
        {
            strncat(arg[cur_arg] + arglen[cur_arg], data, ARGSZ - arglen[cur_arg] - 1);
            arg[cur_arg][ARGSZ - 1] = '\0';
            arglen[cur_arg] = ARGSZ - 1;
        }
        else
        {
            strcat(arg[cur_arg], data);
            arglen[cur_arg] += datasz;
        }
    }

    free(data);
    textbox_set_text((struct window_t *)calculator.text_box, arg[cur_arg]);
}


void menu_file_about_handler(winid_t winid)
{
    show_about_dialog();
}


int main(int argc, char **argv)
{
    /* volatile */ struct event_t *ev = NULL;
    volatile char key;
    struct window_attribs_t attribs;

    gui_init(argc, argv);
    memset(&calculator, 0, sizeof(Calculator));

    attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
    attribs.x = 30;
    attribs.y = 30;
    attribs.w = 180;
    attribs.h = 170;
    attribs.flags = WINDOW_HASMENU | WINDOW_NORESIZE;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n", 
                        argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    calculator.window = main_window;


    struct menu_item_t *file_menu = mainmenu_new_item(main_window, "&File");
    struct menu_item_t *view_menu = mainmenu_new_item(main_window, "&View");
    struct menu_item_t *mi;
    
    mi = menu_new_icon_item(file_menu, "Copy", NULL, MENU_EDIT_COPY);
    mi->handler = menu_file_copy_handler;
    // assign the shortcut: CTRL + C
    menu_item_set_shortcut(main_window, mi, KEYCODE_C, MODIFIER_MASK_CTRL);

    mi = menu_new_icon_item(file_menu, "Paste", NULL, MENU_EDIT_PASTE);
    mi->handler = menu_file_paste_handler;
    // assign the shortcut: CTRL + V
    menu_item_set_shortcut(main_window, mi, KEYCODE_V, MODIFIER_MASK_CTRL);

    menu_new_item(file_menu, "-");

    mi = menu_new_item(file_menu, "About");
    mi->handler = menu_file_about_handler;

    menu_new_item(file_menu, "-");

    mi = menu_new_icon_item(file_menu, "Exit", NULL, MENU_FILE_EXIT);
    mi->handler = menu_file_close_handler;
    // assign the shortcut: CTRL + Q
    menu_item_set_shortcut(main_window, mi, KEYCODE_Q, MODIFIER_MASK_CTRL);

    menu_new_item(view_menu, "Normal");
    menu_new_item(view_menu, "Extended");

    finalize_menus(main_window);

    calculator_init();
    window_repaint(main_window);
    
    window_set_icon(main_window, "calculator.ico");
    
    window_show(main_window);
    
    while(1)
    {
        if(!(ev = next_event()))
        {
            continue;
        }
        
        if(event_dispatch(ev))
        {
            free(ev);
            continue;
        }
    
        switch(ev->type)
        {
            case EVENT_WINDOW_CLOSING:
                window_destroy(main_window);

                gui_exit(EXIT_SUCCESS);
                break;

            case EVENT_KEY_PRESS:
                //__asm__ __volatile__("xchg %%bx, %%bx"::);
                //key = get_printable_char((struct event_t *)ev);
                key = get_printable_char(ev->key.code, ev->key.modifiers);

#define BUTTON_CLICK(which)     \
    calculator_button_handler(calculator.button_ ## which, 0, 0);

                switch(key)
                {
                    case '.': BUTTON_CLICK(dot); break;
                    case '+': BUTTON_CLICK(add); break;
                    case '-': BUTTON_CLICK(sub); break;
                    case '*': BUTTON_CLICK(mul); break;
                    case '/': BUTTON_CLICK(div); break;
                    case '%': BUTTON_CLICK(mod); break;
                    case '0': BUTTON_CLICK(0); break;
                    case '1': BUTTON_CLICK(1); break;
                    case '2': BUTTON_CLICK(2); break;
                    case '3': BUTTON_CLICK(3); break;
                    case '4': BUTTON_CLICK(4); break;
                    case '5': BUTTON_CLICK(5); break;
                    case '6': BUTTON_CLICK(6); break;
                    case '7': BUTTON_CLICK(7); break;
                    case '8': BUTTON_CLICK(8); break;
                    case '9': BUTTON_CLICK(9); break;
                    case '\n': BUTTON_CLICK(ent); break;
                }

                //__asm__ __volatile__("xchg %%bx, %%bx"::);
                break;

#undef BUTTON_CLICK

            default:
                break;
        }

        free(ev);
    }
}

