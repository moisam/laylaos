/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: board.c
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
 *  \file board.c
 *
 *  This file contains functions to work with the board in the tetris game.
 */

int board[BOARD_ROWS][BOARD_COLS];

static inline void init_board(void)
{
    int i, j;

    for(i = 0; i < BOARD_ROWS; i++)
    {
        for(j = 0; j < BOARD_COLS; j++)
        {
            board[i][j] = 0;
        }
    }
}

static inline int inside_board(int row, int col)
{
    return (row >= 0 && row < BOARD_ROWS &&
            col >= 0 && col < BOARD_COLS);
}


static inline int is_empty(int row, int col)
{
    return (row >= 0 && row < BOARD_ROWS &&
            col >= 0 && col < BOARD_COLS &&
            board[row][col] == 0);
}


static inline int is_row_full(int row)
{
    int col;

    for(col = 0; col < BOARD_COLS; col++)
    {
        if(board[row][col] == 0)
        {
            return 0;
        }
    }

    return 1;
}


static inline int is_row_empty(int row)
{
    int col;

    for(col = 0; col < BOARD_COLS; col++)
    {
        if(board[row][col] != 0)
        {
            return 0;
        }
    }

    return 1;
}


static inline void clear_row(int row)
{
    int col;

    for(col = 0; col < BOARD_COLS; col++)
    {
        board[row][col] = 0;
    }
}


static inline void move_row_down(int row, int n)
{
    int col;

    for(col = 0; col < BOARD_COLS; col++)
    {
        board[row + n][col] = board[row][col];
        board[row][col] = 0;
    }
}


static inline int clear_full_rows(void)
{
    int row, cleared = 0;

    for(row = BOARD_ROWS - 1; row >= 0; row--)
    {
        if(is_row_full(row))
        {
            clear_row(row);
            cleared++;
        }
        else if(cleared > 0)
        {
            move_row_down(row, cleared);
        }
    }

    return cleared;
}


static inline void draw_board(void)
{
    int i, j;

    // top 2 rows are hidden
    for(i = 2; i < BOARD_ROWS; i++)
    {
        for(j = 0; j < BOARD_COLS; j++)
        {
            gc_draw_rect(main_window->gc, board_left + (j * CELL_SIZE),
                                          board_top + (i * CELL_SIZE),
                                          CELL_SIZE, CELL_SIZE,
                                          GREY_BORDER);

            gc_fill_rect(main_window->gc, board_left + (j * CELL_SIZE) + 1,
                                          board_top + (i * CELL_SIZE) + 1,
                                          CELL_SIZE - 2, CELL_SIZE - 2,
                                          blocks[board[i][j]].color);
        }
    }
}

