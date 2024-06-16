/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: mouse.c
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
 *  \file mouse.c
 *
 *  This file contains functions to handle the mouse in the minesweeper game.
 */

int last_lmouse_row = -1;
int last_lmouse_col = -1;
int last_rmouse_row = -1;
int last_rmouse_col = -1;

static void get_row_col(int x, int y, int *_row, int *_col)
{
    int row = (y - TOP_BORDER) / CELL_SIZE;
    int col = (x - LEFT_BORDER) / CELL_SIZE;

    if(row < 0 || row > rows || col < 0 || col > cols)
    {
        *_row = -1;
        *_col = -1;
    }
    else
    {
        *_row = row;
        *_col = col;
    }
}


void process_mousedown(struct window_t *window, struct mouse_state_t *mstate)
{
    int row, col;

    get_row_col(mstate->x, mstate->y, &row, &col);

    // mouse event outside the board
    if(row == -1 || col == -1)
    {
        return;
    }

    if(mstate->left_pressed)
    {
        last_lmouse_row = row;
        last_lmouse_col = col;
    }
    else if(mstate->right_pressed)
    {
        last_rmouse_row = row;
        last_rmouse_col = col;
    }
}


void process_mouseup(struct window_t *window, struct mouse_state_t *mstate)
{
    int row, col;

    get_row_col(mstate->x, mstate->y, &row, &col);

    // mouse event outside the board
    if(row == -1 || col == -1)
    {
        return;
    }

    if(mstate->left_released)
    {
        // check if it is in the same cell the mouse was pressed on
        if(row != last_lmouse_row || col != last_lmouse_col)
        {
            last_lmouse_row = -1;
            last_lmouse_col = -1;
            return;
        }

        last_lmouse_row = -1;
        last_lmouse_col = -1;

        if(board[row][col].is_revealed || board[row][col].is_flagged)
        {
            return;
        }

        if(board[row][col].is_mine)
        {
            show_mines();
            draw_smiley(bitmap_smiley_lose);
            game_over = 1;
        }
        else
        {
            check_mine(row, col);
            draw_top_banner(0);

            // check for win
            if(flagged + revealed == total_cells)
            {
                draw_smiley(bitmap_smiley_win);
                game_over = 1;
            }
        }

        window_invalidate(main_window);
    }
    else if(mstate->right_released)
    {
        // check if it is in the same cell the mouse was pressed on
        if(row != last_rmouse_row || col != last_rmouse_col)
        {
            last_rmouse_row = -1;
            last_rmouse_col = -1;
            return;
        }

        last_rmouse_row = -1;
        last_rmouse_col = -1;

        if(!board[row][col].is_revealed)
        {
            if(board[row][col].is_flagged)
            {
                board[row][col].is_flagged = 0;
                flagged--;
            }
            else
            {
                board[row][col].is_flagged = 1;
                flagged++;
            }

            draw_cell(row, col);
            draw_top_banner(0);

            // check for win
            if(flagged + revealed == total_cells)
            {
                draw_smiley(bitmap_smiley_win);
                game_over = 1;
            }

            window_invalidate(main_window);
        }
    }
}

