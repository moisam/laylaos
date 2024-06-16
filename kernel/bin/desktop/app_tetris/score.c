/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: score.c
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
 *  \file score.c
 *
 *  Initialise and show score in the tetris game.
 */

static inline void init_score(void)
{
    int fontsz;

    score = 0;
    sprintf(score_text, "Score: 0");
    score_len = string_width(main_window->gc->font, score_text);

    lock_font(main_window->gc->font);
    fontsz = gc_get_fontsize(main_window->gc);
    gc_set_fontsize(main_window->gc, 24);

    score_height = char_height(main_window->gc->font, ' ');

    gc_set_fontsize(main_window->gc, fontsz);
    unlock_font(main_window->gc->font);
}


void draw_score(void)
{
    int fontsz;

    gc_fill_rect(main_window->gc,
                 (main_window->w - score_len) / 2,
                 MENU_HEIGHT + (score_height / 2),
                 score_len, score_height, main_window->bgcolor);

    lock_font(main_window->gc->font);
    fontsz = gc_get_fontsize(main_window->gc);
    gc_set_fontsize(main_window->gc, 24);

    sprintf(score_text, "Score: %d", score);
    score_len = string_width(main_window->gc->font, score_text);

    gc_draw_text(main_window->gc, score_text,
                 (main_window->w - score_len) / 2,
                 MENU_HEIGHT + (score_height / 2),
                 0x000000ff, 0);

    gc_set_fontsize(main_window->gc, fontsz);
    unlock_font(main_window->gc->font);
}

