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
 *  The virtual keyboard program.
 */

#include <errno.h>
#include <stdlib.h>
#include <kernel/keycodes.h>
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/kbd.h"
#include "../include/keys.h"
#include "../include/client/window.h"
#include "../include/client/button.h"
#include "../include/client/label.h"

#define GLOB        __global_gui_data

#define INACTIVE_LABEL_COLOR        0x666666FF
#define ACTIVE_LABEL_COLOR          0x000000FF

int ctrl = 0, alt = 0, shift = 0;
int caps = 0, num = 0, scroll = 0;
//winid_t input_focus = 0;
struct window_t *main_window;

struct label_t *label_ctrl, *label_alt, *label_caps, *label_shift;

struct key_button_t
{
    int16_t x, y;
    uint16_t w, h;
    char text[2][8];
    int key;
};

// your usual US-keyboard layout
struct key_button_t buttons_us[] =
{
    { 10, 30, 30, 30, { "`", "~" }, KEYCODE_BACKTICK },
    { 50, 30, 30, 30, { "1", "!" }, KEYCODE_1 },
    { 90, 30, 30, 30, { "2", "@" }, KEYCODE_2 },
    { 130, 30, 30, 30, { "3", "#" }, KEYCODE_3 },
    { 170, 30, 30, 30, { "4", "$" }, KEYCODE_4 },
    { 210, 30, 30, 30, { "5", "%" }, KEYCODE_5 },
    { 250, 30, 30, 30, { "6", "^" }, KEYCODE_6 },
    { 290, 30, 30, 30, { "7", "&" }, KEYCODE_7 },
    { 330, 30, 30, 30, { "8", "*" }, KEYCODE_8 },
    { 370, 30, 30, 30, { "9", "(" }, KEYCODE_9 },
    { 410, 30, 30, 30, { "0", ")" }, KEYCODE_0 },
    { 450, 30, 30, 30, { "-", "_" }, KEYCODE_MINUS },
    { 490, 30, 30, 30, { "=", "+" }, KEYCODE_EQUAL },
    { 530, 30, 50, 30, { "BkSpc", "BkSpc" }, KEYCODE_BACKSPACE },

    { 10, 70, 50, 30, { "Tab", "Tab" }, KEYCODE_TAB },
    { 70, 70, 30, 30, { "q", "Q" }, KEYCODE_Q },
    { 110, 70, 30, 30, { "w", "W" }, KEYCODE_W },
    { 150, 70, 30, 30, { "e", "E" }, KEYCODE_E },
    { 190, 70, 30, 30, { "r", "R" }, KEYCODE_R },
    { 230, 70, 30, 30, { "t", "T" }, KEYCODE_T },
    { 270, 70, 30, 30, { "y", "Y" }, KEYCODE_Y },
    { 310, 70, 30, 30, { "u", "U" }, KEYCODE_U },
    { 350, 70, 30, 30, { "i", "I" }, KEYCODE_I },
    { 390, 70, 30, 30, { "o", "O" }, KEYCODE_O },
    { 430, 70, 30, 30, { "p", "P" }, KEYCODE_P },
    { 470, 70, 30, 30, { "[", "{" }, KEYCODE_LBRACKET },
    { 510, 70, 30, 30, { "]", "}" }, KEYCODE_RBRACKET },
    { 550, 70, 30, 30, { "\\", "|" }, KEYCODE_BACKSLASH },

    { 10, 110, 60, 30, { "Caps", "Caps" }, KEYCODE_CAPS },
    { 80, 110, 30, 30, { "a", "A" }, KEYCODE_A },
    { 120, 110, 30, 30, { "s", "S" }, KEYCODE_S },
    { 160, 110, 30, 30, { "d", "D" }, KEYCODE_D },
    { 200, 110, 30, 30, { "f", "F" }, KEYCODE_F },
    { 240, 110, 30, 30, { "g", "G" }, KEYCODE_G },
    { 280, 110, 30, 30, { "h", "H" }, KEYCODE_H },
    { 320, 110, 30, 30, { "j", "J" }, KEYCODE_J },
    { 360, 110, 30, 30, { "k", "K" }, KEYCODE_K },
    { 400, 110, 30, 30, { "l", "L" }, KEYCODE_L },
    { 440, 110, 30, 30, { ";", ":" }, KEYCODE_SEMICOLON },
    { 480, 110, 30, 30, { "'", "\"" }, KEYCODE_QUOTE },
    { 520, 110, 60, 30, { "Enter", "Enter" }, KEYCODE_ENTER },

    { 10, 150, 80, 30, { "Shift", "Shift" }, KEYCODE_LSHIFT },
    { 100, 150, 30, 30, { "z", "Z" }, KEYCODE_Z },
    { 140, 150, 30, 30, { "x", "X" }, KEYCODE_X },
    { 180, 150, 30, 30, { "c", "C" }, KEYCODE_C },
    { 220, 150, 30, 30, { "v", "V" }, KEYCODE_V },
    { 260, 150, 30, 30, { "b", "B" }, KEYCODE_B },
    { 300, 150, 30, 30, { "n", "N" }, KEYCODE_N },
    { 340, 150, 30, 30, { "m", "M" }, KEYCODE_M },
    { 380, 150, 30, 30, { ",", "<" }, KEYCODE_COMMA },
    { 420, 150, 30, 30, { ".", ">" }, KEYCODE_DOT },
    { 460, 150, 30, 30, { "/", "?" }, KEYCODE_SLASH },
    { 500, 150, 80, 30, { "Shift", "Shift" }, KEYCODE_RSHIFT },

    { 10, 190, 40, 30, { "Ctrl", "Ctrl" }, KEYCODE_LCTRL },
    { 60, 190, 40, 30, { "Alt", "Alt" }, KEYCODE_LALT },
    { 110, 190, 120, 30, { "Space", "Space" }, KEYCODE_SPACE },
    { 240, 190, 40, 30, { "Alt", "Alt" }, KEYCODE_RALT },
    { 290, 190, 40, 30, { "Ctrl", "Ctrl" }, KEYCODE_RCTRL },
    { 390, 190, 40, 30, { "Lt", "Lt" }, KEYCODE_LEFT },
    { 440, 190, 40, 30, { "Up", "Up" }, KEYCODE_UP },
    { 490, 190, 40, 30, { "Dn", "Dn" }, KEYCODE_DOWN },
    { 540, 190, 40, 30, { "Rt", "Rt" }, KEYCODE_RIGHT },

    { 0, 0, 0, 0, { "", "" }, 0 },
};


static inline void send_key_event(winid_t winid, char key, char modifiers)
{
    struct event_t ev;

    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = winid;
    ev.valid_reply = 1;
    ev.type = EVENT_KEY_PRESS;
    ev.key.code = key;
    ev.key.modifiers = modifiers;

    write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}


void handle_shift(void)
{
    int shifted = (shift != caps);
    ListNode *cur_node;
    struct button_t *b;
    struct key_button_t *k;

    for(cur_node = main_window->children->root_node;
        cur_node != NULL;
        cur_node = cur_node->next)
    {
        if(((struct window_t *)cur_node->payload)->type != WINDOW_TYPE_BUTTON)
        {
            continue;
        }

        b = (struct button_t *)cur_node->payload;
        k = b->internal_data;
        button_set_title(b, k->text[shifted]);
    }

    window_repaint(main_window);
    window_invalidate(main_window);
}


void button_handler(struct button_t *button, int x, int y)
{
    (void)x;
    (void)y;

    struct key_button_t *which = button->internal_data;
    int key = which->key;
    char modifiers = 0;
    
    switch(key)
    {
        case KEYCODE_LSHIFT:
        case KEYCODE_RSHIFT:
            shift = !shift;
            label_set_foreground(label_shift, shift ? ACTIVE_LABEL_COLOR :
                                                      INACTIVE_LABEL_COLOR);
            handle_shift();
            return;

        case KEYCODE_LCTRL:
        case KEYCODE_RCTRL:
            ctrl = !ctrl;
            label_set_foreground(label_ctrl, ctrl ? ACTIVE_LABEL_COLOR :
                                                    INACTIVE_LABEL_COLOR);
            label_repaint((struct window_t *)label_ctrl, 0);
            child_invalidate((struct window_t *)label_ctrl);
            return;

        case KEYCODE_LALT:
        case KEYCODE_RALT:
            alt = !alt;
            label_set_foreground(label_alt, alt ? ACTIVE_LABEL_COLOR :
                                                  INACTIVE_LABEL_COLOR);
            label_repaint((struct window_t *)label_alt, 0);
            child_invalidate((struct window_t *)label_alt);
            return;

        case KEYCODE_CAPS:
            caps = !caps;
            label_set_foreground(label_caps, caps ? ACTIVE_LABEL_COLOR :
                                                    INACTIVE_LABEL_COLOR);
            handle_shift();
            return;
    }

    if(shift != caps)
    {
        modifiers |= MODIFIER_MASK_SHIFT;
    }

    if(ctrl)
    {
        modifiers |= MODIFIER_MASK_CTRL;
    }

    if(alt)
    {
        modifiers |= MODIFIER_MASK_ALT;
    }

    //if(input_focus)
    {
        //send_key_event(input_focus, key, 0);
        send_key_event(get_input_focus(), key, modifiers);
    }
}


int main(int argc, char **argv)
{
    struct event_t *ev = NULL;
    struct window_attribs_t attribs;
    struct button_t *b;
    int i = 0;

    gui_init(argc, argv);

    //input_focus = get_input_focus();

    attribs.gravity = WINDOW_ALIGN_CENTERH | WINDOW_ALIGN_BOTTOM;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = 590;
    attribs.h = 230;
    attribs.flags = WINDOW_NORESIZE | WINDOW_ALWAYSONTOP | 
                    WINDOW_SKIPTASKBAR | WINDOW_NOFOCUS | WINDOW_NOMINIMIZE;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n",
                        argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    window_set_title(main_window, "Onscreen Keyboard");
    window_set_icon(main_window, "keyboard.ico");

    // add the buttons
    while(buttons_us[i].key)
    {
        b = button_new(main_window->gc, main_window, 
                       buttons_us[i].x, buttons_us[i].y, 
                       buttons_us[i].w, buttons_us[i].h, 
                       buttons_us[i].text[0]);
        b->internal_data = (void *)(uintptr_t)&buttons_us[i];
        b->button_click_callback = button_handler;
        i++;
    }

    // add the status labels
    label_caps  = label_new(main_window->gc, main_window,
                            390, 10, 40, 20, "CAPS");
    label_ctrl  = label_new(main_window->gc, main_window,
                            440, 10, 40, 20, "CTRL");
    label_alt   = label_new(main_window->gc, main_window,
                            500, 10, 30, 20, "ALT");
    label_shift = label_new(main_window->gc, main_window,
                            540, 10, 50, 20, "SHIFT");

    label_set_foreground(label_caps , INACTIVE_LABEL_COLOR);
    label_set_foreground(label_ctrl , INACTIVE_LABEL_COLOR);
    label_set_foreground(label_alt  , INACTIVE_LABEL_COLOR);
    label_set_foreground(label_shift, INACTIVE_LABEL_COLOR);

    // draw and show window
    window_repaint(main_window);
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
        }

        free(ev);
    }
}

