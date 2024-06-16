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
 *  The minesweeper game.
 */

#include <errno.h>
#include "../include/gui.h"
#include "../include/gc.h"
#include "../include/menu.h"
#include "../include/keys.h"
#include "../include/client/window.h"
#include "defs.h"

#define WHITE           0xFFFFFFFF
#define BLACK           0x000000FF
#define GREY            0x444444FF
#define CELL_BG         0xCDCFD4FF

unsigned long long last_millis = 0, this_millis = 0;
int cols = 8;
int rows = 8;
int total_cells = 64;
int mines = 10;
int flagged = 0;
int revealed = 0;
int game_over = 0;
int ticks = 0;
int count_time = 1;
int force_newgame = 0;

struct menu_item_t *options_mi;
struct window_t *main_window;

void show_mines(void);
void check_mine(int row, int col);
void draw_top_banner(int invalidate);
void draw_smiley(uint32_t *bitmap_data);
extern void show_shortcuts_dialog(void);
extern void show_about_dialog(void);
extern void show_options_dialog(void);

struct cell_t
{
    int mines_around;
    int is_revealed;
    int is_mine;
    int is_flagged;
};

struct cell_t board[MAX_ROWS][MAX_COLS];

#include "bitmaps.c"
#include "cell.c"
#include "mouse.c"
#include "mine.c"


static void init_board(void)
{
    int row, col;
    int i;

    // clear the board
    A_memset(board, 0, sizeof(board));
    srand(time(NULL));
    flagged = 0;
    revealed = 0;
    game_over = 0;
    ticks = 0;
    total_cells = rows * cols;

    // make some mines
    for(i = 0; i < mines; )
    {
        row = rand() % rows;
        col = rand() % cols;

        // avoid duplicates
        if(!board[row][col].is_mine)
        {
            board[row][col].is_mine = 1;
            i++;
        }
    }
}


void draw_board(void)
{
    int row, col;

    for(row = 0; row < rows; row++)
    {
        for(col = 0; col < cols; col++)
        {
            draw_cell(row, col);
        }
    }
}


void draw_top_banner(int invalidate)
{
    struct bitmap32_t bitmap;
    static char buf[16];

    // draw flag and mine count
    bitmap.data = bitmap_flag;
    bitmap.width = 20;
    bitmap.height = 20;
    gc_blit_bitmap(main_window->gc, &bitmap, 15, MENU_HEIGHT + 4, 0, 0, 20, 20);

    sprintf(buf, "%d", flagged);
    gc_fill_rect(main_window->gc, 40, MENU_HEIGHT + 2, 20, 20, CELL_BG);
    gc_draw_text(main_window->gc, buf, 40, MENU_HEIGHT + 4, 0x000000ff, 0);

    // draw stopwatch and time spent
    bitmap.data = bitmap_stopwatch;
    bitmap.width = 24;
    bitmap.height = 28;
    gc_blit_bitmap(main_window->gc, &bitmap, main_window->w - 65, 
                                    MENU_HEIGHT, 0, 0, 24, 28);

    sprintf(buf, "%d", ticks);
    gc_fill_rect(main_window->gc, main_window->w - 35, 
                                  MENU_HEIGHT + 2, 35, 20, CELL_BG);
    gc_draw_text(main_window->gc, buf, main_window->w - 35, 
                                  MENU_HEIGHT + 4, 0x000000ff, 0);

    if(invalidate)
    {
        window_invalidate(main_window);
    }
}


void draw_smiley(uint32_t *bitmap_data)
{
    struct bitmap32_t bitmap;

    bitmap.data = bitmap_data;
    bitmap.width = 20;
    bitmap.height = 20;
    gc_blit_bitmap(main_window->gc, &bitmap,
                   (main_window->w - 20) / 2, MENU_HEIGHT + 2, 0, 0, 20, 20);
}


void repaint_all(struct window_t *window, int is_active_child)
{
    (void)is_active_child;

    gc_fill_rect(window->gc, 0, 0, window->w, window->h, window->bgcolor);

    draw_board();
    draw_smiley(bitmap_smiley);
    draw_top_banner(0);
}


// Callback for when the window size changes
void size_changed(struct window_t *window)
{
    force_newgame = 1;
}


void menu_file_close_handler(winid_t winid)
{
    window_destroy(main_window);
    gui_exit(EXIT_SUCCESS);
}


void menu_file_newgame_handler(winid_t winid)
{
    init_board();
    window_repaint(main_window);
    window_invalidate(main_window);
}


void menu_file_options_handler(winid_t winid)
{
    // if the options dialog changes the window size, we will get an event
    // and then we can start a new game in the main loop below
    show_options_dialog();
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

    options_mi = menu_new_item(file_menu, "&Options");
    options_mi->handler = menu_file_options_handler;
    // assign the shortcut: CTRL + O
    menu_item_set_shortcut(main_window, options_mi, KEYCODE_O, MODIFIER_MASK_CTRL);

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
    struct window_attribs_t attribs;
    struct event_t *ev;

    gui_init(argc, argv);

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = (CELL_SIZE * cols) + (LEFT_BORDER * 2);
    attribs.h = (CELL_SIZE * rows) + (TOP_BORDER - MENU_HEIGHT) + BOTTOM_BORDER;
    attribs.flags = WINDOW_NORESIZE | WINDOW_HASMENU;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n",
                        argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    create_main_menu();
    init_board();

    main_window->repaint = repaint_all;
    main_window->size_changed = size_changed;
    window_repaint(main_window);

    window_set_title(main_window, "Minesweeper");
    window_set_icon(main_window, "minesweeper.ico");
    window_show(main_window);

loop:

    main_window->mousedown = process_mousedown;
    main_window->mouseup = process_mouseup;
    last_millis = time_in_millis();

    while(!game_over)
    {
        if(pending_events_utimeout(1000000))
        {
            if((ev = next_event_for_seqid(NULL, 0, 0)))
            {
                if(event_dispatch(ev))
                {
                    // if the window size changes, we set this flag so that
                    // we know we need to start a new game
                    if(force_newgame)
                    {
                        free(ev);
                        force_newgame = 0;
                        menu_file_newgame_handler(main_window->winid);
                        goto loop;
                    }
                }
                else
                {
                    switch(ev->type)
                    {
                        case EVENT_WINDOW_CLOSING:
                            window_destroy(main_window);
                            gui_exit(EXIT_SUCCESS);
                            break;

                        case EVENT_KEY_PRESS:
                            //process_key(ev->key.code, ev->key.modifiers);
                            break;

                        default:
                            break;
                    }
                }

                free(ev);
            }
        }

        this_millis = time_in_millis();

        if(this_millis >= last_millis + 1000)
        {
            last_millis = this_millis;

            if(count_time)
            {
                ticks++;
                draw_top_banner(1);
            }
        }
    }

    // game over

    // our mouse processing occurs in our callback functions -- disable them for 
    // now so the user cannot continue to interact with the cells while we loop
    main_window->mousedown = NULL;
    main_window->mouseup = NULL;

    // loop until the user exits or starts a new game
    while(game_over)
    {
        if((ev = next_event_for_seqid(NULL, 0, 1)))
        {
            if(event_dispatch(ev))
            {
                // if the window size changes, we set this flag so that
                // we know we need to start a new game
                if(force_newgame)
                {
                    free(ev);
                    force_newgame = 0;
                    menu_file_newgame_handler(main_window->winid);
                    goto loop;
                }
            }
            else
            {
                switch(ev->type)
                {
                    case EVENT_WINDOW_CLOSING:
                        window_destroy(main_window);
                        gui_exit(EXIT_SUCCESS);
                        break;

                    default:
                        break;
                }
            }

            free(ev);
        }
    }

    goto loop;
}

