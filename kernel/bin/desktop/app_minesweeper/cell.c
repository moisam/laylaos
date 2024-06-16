/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: cell.c
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
 *  \file cell.c
 *
 *  This file contains the function to draw a cell in the minesweeper game.
 */

void draw_cell(int row, int col)
{
    struct bitmap32_t bitmap;
    int y = TOP_BORDER + (row * CELL_SIZE);
    int x = LEFT_BORDER + (col * CELL_SIZE);
    static int border = (CELL_SIZE - 20) / 2;

    // draw background
    gc_fill_rect(main_window->gc, x, y, CELL_SIZE, CELL_SIZE, CELL_BG);

    // draw border
    if(board[row][col].is_revealed)
    {
        gc_fill_rect(main_window->gc, x, y, CELL_SIZE, 1, GREY);
        gc_fill_rect(main_window->gc, x, y, 1, CELL_SIZE, GREY);
        gc_fill_rect(main_window->gc, x + CELL_SIZE - 1, y, 1, CELL_SIZE, WHITE);
        gc_fill_rect(main_window->gc, x + 1, y + CELL_SIZE - 1, CELL_SIZE - 1, 1, WHITE);
    }
    else
    {
        gc_fill_rect(main_window->gc, x, y, CELL_SIZE, 2, WHITE);
        gc_fill_rect(main_window->gc, x, y, 2, CELL_SIZE, WHITE);
        gc_fill_rect(main_window->gc, x + CELL_SIZE - 2, y + 1, 1, CELL_SIZE - 1, GREY);
        gc_fill_rect(main_window->gc, x + CELL_SIZE - 1, y, 1, CELL_SIZE, GREY);
        gc_fill_rect(main_window->gc, x + 1, y + CELL_SIZE - 2, CELL_SIZE - 1, 1, GREY);
        gc_fill_rect(main_window->gc, x, y + CELL_SIZE - 1, CELL_SIZE, 1, GREY);
    }

    // draw content
    if(board[row][col].is_mine && board[row][col].is_revealed)
    {
        bitmap.data = bitmap_mine;
    }
    else if(board[row][col].is_flagged)
    {
        bitmap.data = bitmap_flag;
    }
    else
    {
        bitmap.data = bitmap_numbers[board[row][col].mines_around];
    }

    bitmap.width = 20;
    bitmap.height = 20;
    gc_blit_bitmap(main_window->gc, &bitmap, x + border, y + border, 0, 0, 20, 20);
}

