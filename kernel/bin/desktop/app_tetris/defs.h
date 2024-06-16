/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: defs.h
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
 *  \file defs.h
 *
 *  Header file for the tetris game.
 */

#define CELL_SIZE       25

#define BOARD_ROWS      22
#define BOARD_COLS      10

#define BLOCK_TYPES     7
#define BLOCK_STATES    4

#define MAX_DELAY       1000
#define MIN_DELAY       75

#define GREY_BORDER     0x333333ff

struct block_t
{
    int id;
    int start_row, start_col;
    uint32_t color;
    int tiles[BLOCK_STATES][4][4];
    int row, col;
    int rotation;
};

#define is_game_over()      !(is_row_empty(0) && is_row_empty(1))

void show_shortcuts_dialog(void);
void show_about_dialog(void);

