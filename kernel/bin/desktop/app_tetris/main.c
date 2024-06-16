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
 *  The tetris game.
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "../include/client/window.h"
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/keys.h"
#include "../include/font.h"
#include "../include/menu.h"

#ifndef MAX
#define MAX(a, b)       ((a) < (b) ? (b) : (a))
#endif

int can_hold = 0;
int game_over = 0;
int board_left, board_top;
int next_block_left, next_block_top;
int hold_block_left, hold_block_top;
unsigned long long last_millis = 0, this_millis = 0;

int score = 0;
char score_text[64];
size_t score_len = 0, score_height = 0;

int delay_dec = 25;

struct window_t *main_window;
struct menu_item_t *pause_mi;

#include "defs.h"
#include "pieces.h"
#include "board.c"
#include "block.c"
#include "score.c"


static inline void draw(void)
{
    draw_board();
    draw_ghost_block();
    draw_block(cur_block);
    draw_next_block();
    draw_hold_block();
    draw_score();

    window_invalidate(main_window);
}


static inline void clear_window(void)
{
    int len, fontsz;

    gc_fill_rect(main_window->gc, 0, MENU_HEIGHT,
                 main_window->w, main_window->h - MENU_HEIGHT,
                 main_window->bgcolor);

    lock_font(main_window->gc->font);
    fontsz = gc_get_fontsize(main_window->gc);
    gc_set_fontsize(main_window->gc, 18);

    len = string_width(main_window->gc->font, "Next:");
    gc_draw_text(main_window->gc, "Next:",
                 next_block_left + ((CELL_SIZE * 4) - len) / 2,
                 next_block_top - 40,
                 0x000000ff, 0);

    len = string_width(main_window->gc->font, "Hold:");
    gc_draw_text(main_window->gc, "Hold:",
                 hold_block_left + ((CELL_SIZE * 4) - len) / 2,
                 hold_block_top - 40,
                 0x000000ff, 0);

    gc_set_fontsize(main_window->gc, fontsz);
    unlock_font(main_window->gc->font);
}


static inline void setup_new_game(void)
{
    last_millis = time_in_millis();

    game_over = 0;
    init_score();
    clear_window();
    init_board();
    init_blocks();
    draw();
}


void show_gameover_menu(void)
{
    int len, fontsz;
    char buf[64], *p;
    struct event_t *ev;

    // paint a semi-transparent black box over the window
    gc_fill_rect(main_window->gc, 0, MENU_HEIGHT,
                 main_window->w, main_window->h - MENU_HEIGHT, 0x000000aa);

    lock_font(main_window->gc->font);
    fontsz = gc_get_fontsize(main_window->gc);
    gc_set_fontsize(main_window->gc, 28);

    // print the Game Over message
    p = "Game Over";
    len = string_width(main_window->gc->font, p);
    gc_draw_text(main_window->gc, p,
                 (main_window->w - len) / 2, 140,
                 0xffffffff, 0);

    gc_set_fontsize(main_window->gc, 18);

    // print the current score
    sprintf(buf, "Score: %d", score);
    len = string_width(main_window->gc->font, buf);
    gc_draw_text(main_window->gc, buf,
                 (main_window->w - len) / 2, 180,
                 0xffffffff, 0);

    p = "Press Space to play again";
    len = string_width(main_window->gc->font, p);
    gc_draw_text(main_window->gc, p,
                 (main_window->w - len) / 2, 220,
                 0xffffffff, 0);

    gc_set_fontsize(main_window->gc, fontsz);
    unlock_font(main_window->gc->font);

    window_invalidate(main_window);

    while(1)
    {
        if(!(ev = next_event()))
        {
            continue;
        }

        switch(ev->type)
        {
            case EVENT_KEY_PRESS:
                switch(ev->key.code)
                {
                    case KEYCODE_ESC:
                        break;

                    case KEYCODE_SPACE:
                        setup_new_game();
                        return;

                    default:
                        break;
                }

                break;

            default:
                break;
        }

        free(ev);
    }
}


void process_key(char key, char modifiers)
{
    switch(key)
    {
        case KEYCODE_A:
        case KEYCODE_LEFT:
            move_block_left();
            break;

        case KEYCODE_D:
        case KEYCODE_RIGHT:
            move_block_right();
            break;

        case KEYCODE_S:
        case KEYCODE_DOWN:
            move_block_down();
            break;

        case KEYCODE_W:
        case KEYCODE_UP:
            rotate_block_clockwise();
            break;

        case KEYCODE_Z:
            rotate_block_counter_clockwise();
            break;

        case KEYCODE_C:
            hold_block();
            break;

        case KEYCODE_SPACE:
            drop_block();
            break;

        case KEYCODE_ESC:
            window_destroy(main_window);
            gui_exit(EXIT_SUCCESS);

        default: return;
    }

    draw();
}


void menu_file_close_handler(winid_t winid)
{
    window_destroy(main_window);
    gui_exit(EXIT_SUCCESS);
}


void menu_file_pause_handler(winid_t winid)
{
    int len, fontsz;
    char buf[64], *p;
    struct event_t *ev;

    // paint a semi-transparent black box over the window
    gc_fill_rect(main_window->gc, 0, MENU_HEIGHT,
                 main_window->w, main_window->h - MENU_HEIGHT, 0x000000aa);

    lock_font(main_window->gc->font);
    fontsz = gc_get_fontsize(main_window->gc);
    gc_set_fontsize(main_window->gc, 28);

    // print the Game Paused message
    p = "Game Paused";
    len = string_width(main_window->gc->font, p);
    gc_draw_text(main_window->gc, p,
                 (main_window->w - len) / 2, 140,
                 0xffffffff, 0);

    gc_set_fontsize(main_window->gc, 18);

    // print the current score
    sprintf(buf, "Score: %d", score);
    len = string_width(main_window->gc->font, buf);
    gc_draw_text(main_window->gc, buf,
                 (main_window->w - len) / 2, 180,
                 0xffffffff, 0);

    p = "Press any key to resume";
    len = string_width(main_window->gc->font, p);
    gc_draw_text(main_window->gc, p,
                 (main_window->w - len) / 2, 220,
                 0xffffffff, 0);

    gc_set_fontsize(main_window->gc, fontsz);
    unlock_font(main_window->gc->font);

    window_invalidate(main_window);

    while(1)
    {
        if(!(ev = next_event()))
        {
            continue;
        }

        switch(ev->type)
        {
            case EVENT_KEY_PRESS:
                free(ev);
                clear_window();
                draw();
                return;

            default:
                break;
        }

        free(ev);
    }
}


void menu_file_newgame_handler(winid_t winid)
{
    setup_new_game();
}


void menu_file_shortcuts_handler(winid_t winid)
{
    show_shortcuts_dialog();
}


void menu_file_about_handler(winid_t winid)
{
    show_about_dialog();
}


void create_main_menu(void)
{
    struct menu_item_t *file_menu = mainmenu_new_item(main_window, "&Game");
    struct menu_item_t *mi;
    
    /*
     * Create the File menu.
     */
    mi = menu_new_item(file_menu, "&New game");
    mi->handler = menu_file_newgame_handler;
    // assign the shortcut: CTRL + N
    menu_item_set_shortcut(main_window, mi, KEYCODE_N, MODIFIER_MASK_CTRL);

    pause_mi = menu_new_item(file_menu, "&Pause");
    pause_mi->handler = menu_file_pause_handler;
    // assign the shortcut: P
    menu_item_set_shortcut(main_window, pause_mi, KEYCODE_P, 0);

    menu_new_item(file_menu, "-");

    mi = menu_new_item(file_menu, "Keyboard shortcuts");
    mi->handler = menu_file_shortcuts_handler;
    // assign the shortcut: CTRL + F1
    menu_item_set_shortcut(main_window, mi, KEYCODE_F1, MODIFIER_MASK_CTRL);

    mi = menu_new_item(file_menu, "About");
    mi->handler = menu_file_about_handler;

    menu_new_item(file_menu, "-");

    mi = menu_new_icon_item(file_menu, "&Exit", NULL, MENU_FILE_EXIT);
    mi->handler = menu_file_close_handler;
    // assign the shortcut: CTRL + Q
    menu_item_set_shortcut(main_window, mi, KEYCODE_Q, MODIFIER_MASK_CTRL);

    finalize_menus(main_window);
}


int main(int argc, char **argv)
{
    int boardw, boardh;
    int delay;
    struct window_attribs_t attribs;
    struct event_t *ev;

    gui_init(argc, argv);

    board_left = (4 * CELL_SIZE) + 100;
    board_top = MENU_HEIGHT;
    boardw = CELL_SIZE * BOARD_COLS;
    boardh = BOARD_ROWS * CELL_SIZE;
    next_block_left = board_left + boardw + 50;
    next_block_top = board_top + (boardh / 2);
    hold_block_left = board_left - (4 * CELL_SIZE) - 50;
    hold_block_top = next_block_top;

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = (CELL_SIZE * BOARD_COLS) + (board_left * 2);
    attribs.h = CELL_SIZE * BOARD_ROWS;
    attribs.flags = WINDOW_NORESIZE | WINDOW_HASMENU;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n",
                        argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    create_main_menu();
    window_repaint(main_window);

    clear_window();
    srand(time(NULL));
    init_score();
    init_board();
    init_blocks();
    //draw_board();
    draw();

    window_set_title(main_window, "Tetris");
    window_set_icon(main_window, "terminal.ico");
    window_show(main_window);

loop:

    last_millis = time_in_millis();

    while(!game_over)
    {
        delay = MAX(MIN_DELAY, MAX_DELAY - (score * delay_dec));

        if(pending_events_utimeout(delay * 1000 /* 500000 */))
        {
            if((ev = next_event_for_seqid(NULL, 0, 0)))
            {
                if(!event_dispatch(ev))
                {
                    switch(ev->type)
                    {
                        case EVENT_WINDOW_CLOSING:
                            window_destroy(main_window);
                            gui_exit(EXIT_SUCCESS);
                            break;

                        case EVENT_KEY_PRESS:
                            process_key(ev->key.code, ev->key.modifiers);
                            break;

                        default:
                            break;
                    }
                }

                free(ev);
            }
        }

        this_millis = time_in_millis();

        if(this_millis >= last_millis + delay /* 500 */)
        {
            last_millis = this_millis;
            move_block_down();
            draw();
        }
    }

    show_gameover_menu();
    goto loop;
}

