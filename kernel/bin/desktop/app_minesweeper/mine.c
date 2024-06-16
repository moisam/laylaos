/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: mine.c
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
 *  \file mine.c
 *
 *  This file contains functions to handle mines in the minesweeper game.
 */

void show_mines(void)
{
    int row, col;

    for(row = 0; row < rows; row++)
    {
        for(col = 0; col < cols; col++)
        {
            if(board[row][col].is_mine)
            {
                board[row][col].is_revealed = 1;
                draw_cell(row, col);
            }
        }
    }
}


static inline int count_mines(int row, int col)
{
    if(row < 0 || row >= rows || col < 0 || col >= cols)
    {
        return 0;
    }

    return !!(board[row][col].is_mine);
}


void check_mine(int row, int col)
{
    if(row < 0 || row >= rows || col < 0 || col >= cols)
    {
        return;
    }

    if(board[row][col].is_revealed)
    {
        return;
    }

    if(board[row][col].is_flagged)
    {
        board[row][col].is_flagged = 0;
        flagged--;
    }

    board[row][col].is_revealed = 1;
    board[row][col].mines_around = 0;
    revealed++;

    // top left
    board[row][col].mines_around += count_mines(row - 1, col - 1);
    // top
    board[row][col].mines_around += count_mines(row - 1, col);
    // top right
    board[row][col].mines_around += count_mines(row - 1, col + 1);

    // same row, left
    board[row][col].mines_around += count_mines(row, col - 1);
    // same row, right
    board[row][col].mines_around += count_mines(row, col + 1);

    // bottom left
    board[row][col].mines_around += count_mines(row + 1, col - 1);
    // bottom
    board[row][col].mines_around += count_mines(row + 1, col);
    // bottom right
    board[row][col].mines_around += count_mines(row + 1, col + 1);

    draw_cell(row, col);

    // if no mines found, check the neighboring cells
    if(board[row][col].mines_around == 0)
    {
        // top left
        check_mine(row - 1, col - 1);
        // top
        check_mine(row - 1, col);
        // top right
        check_mine(row - 1, col + 1);

        // same row, left
        check_mine(row, col - 1);
        // same row, right
        check_mine(row, col + 1);

        // bottom left
        check_mine(row + 1, col - 1);
        // bottom
        check_mine(row + 1, col);
        // bottom right
        check_mine(row + 1, col + 1);
    }
}

