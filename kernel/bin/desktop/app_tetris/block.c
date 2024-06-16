/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: block.c
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
 *  \file block.c
 *
 *  This file contains functions to work with blocks in the tetris game.
 */

#ifndef MIN
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#endif

struct block_t *next_block = NULL;
struct block_t *cur_block = NULL;
struct block_t *held_block = NULL;


static inline struct block_t *random_block(void)
{
    return &blocks[rand() % BLOCK_TYPES + 1];
}


static inline void reset_block(struct block_t *b)
{
    b->rotation = 0;
    b->row = b->start_row;
    b->col = b->start_col;
}


static inline void move_block(struct block_t *b, int rows, int cols)
{
    b->row += rows;
    b->col += cols;
}


static inline int block_fits(void)
{
    int i, j;

    for(i = 0; i < 4; i++)
    {
        for(j = 0; j < 4; j++)
        {
            if(cur_block->tiles[cur_block->rotation][i][j] != 0)
            {
                if(!is_empty(cur_block->row + i, cur_block->col + j))
                {
                    return 0;
                }
            }
        }
    }

    return 1;
}


static inline void set_cur_block(struct block_t *b)
{
    int i;

    cur_block = b;
    reset_block(b);

    for(i = 0; i < 2; i++)
    {
        move_block(cur_block, 1, 0);
    
        if(!block_fits())
        {
            move_block(cur_block, -1, 0);
            game_over = 1;
        }
    }
}


static inline struct block_t *get_and_update(void)
{
    struct block_t *b = next_block;

    do
    {
        next_block = random_block();
    } while(b->id == next_block->id);
    
    return b;
}


static inline void init_blocks(void)
{
    int i;

    for(i = 0; i < BLOCK_TYPES; i++)
    {
        blocks[i].row = 0;
        blocks[i].col = 0;
        blocks[i].rotation = 0;
    }

    can_hold = 1;
    held_block = NULL;
    next_block = random_block();
    reset_block(next_block);
    set_cur_block(get_and_update());
}


static inline void rotate_clockwise(struct block_t *b)
{
    b->rotation = (b->rotation + 1) % 4;
}


static inline void rotate_counter_clockwise(struct block_t *b)
{
    if(b->rotation == 0)
    {
        b->rotation = 3;
    }
    else
    {
        b->rotation--;
    }
}


static inline void rotate_block_clockwise(void)
{
    rotate_clockwise(cur_block);
    
    if(!block_fits())
    {
        rotate_counter_clockwise(cur_block);
    }
}


static inline void rotate_block_counter_clockwise(void)
{
    rotate_counter_clockwise(cur_block);
    
    if(!block_fits())
    {
        rotate_clockwise(cur_block);
    }
}


static inline void move_block_left(void)
{
    move_block(cur_block, 0, -1);
    
    if(!block_fits())
    {
        move_block(cur_block, 0, 1);
    }
}


static inline void move_block_right(void)
{
    move_block(cur_block, 0, 1);
    
    if(!block_fits())
    {
        move_block(cur_block, 0, -1);
    }
}


static inline void place_block(void)
{
    int i, j;
    char c;

    for(i = 0; i < 4; i++)
    {
        if(cur_block->row + i <= 2)
        {
            continue;
        }

        for(j = 0; j < 4; j++)
        {
            c = cur_block->tiles[cur_block->rotation][i][j];

            if(c != 0)
            {
                board[cur_block->row + i][cur_block->col + j] = c;
            }
        }
    }

    score += clear_full_rows();

    if(is_game_over())
    {
        game_over = 1;
    }
    else
    {
        set_cur_block(get_and_update());
        can_hold = 1;
    }
}


static inline void move_block_down(void)
{
    move_block(cur_block, 1, 0);
    
    if(!block_fits())
    {
        move_block(cur_block, -1, 0);
        place_block();
    }
}


static inline void draw_block(struct block_t *b)
{
    int i, j;

    for(i = 0; i < 4; i++)
    {
        if(b->row + i < 2)
        {
            continue;
        }

        for(j = 0; j < 4; j++)
        {
            if(b->tiles[b->rotation][i][j] != 0)
            {
                // top 2 rows are hidden
                gc_fill_rect(main_window->gc, 
                             board_left + ((b->col + j) * CELL_SIZE) + 1,
                             board_top + ((b->row + i) * CELL_SIZE) + 1,
                             CELL_SIZE - 2, CELL_SIZE - 2, b->color);
            }
        }
    }
}


// draw the next block in the preview box on the right
static inline void draw_next_block(void)
{
    int i, j;
    int color;

    for(i = 0; i < 4; i++)
    {
        for(j = 0; j < 4; j++)
        {
            gc_draw_rect(main_window->gc, next_block_left + (j * CELL_SIZE),
                                          next_block_top + (i * CELL_SIZE),
                                          CELL_SIZE, CELL_SIZE,
                                          GREY_BORDER);

            color = next_block->tiles[next_block->rotation][i][j] ?
                                next_block->color : 0x000000ff;

            gc_fill_rect(main_window->gc, next_block_left + (j * CELL_SIZE) + 1,
                                          next_block_top + (i * CELL_SIZE) + 1,
                                          CELL_SIZE - 2, CELL_SIZE - 2, color);
        }
    }
}


// draw the held block in the preview box on the left
static inline void draw_hold_block(void)
{
    int i, j;
    int color;

    for(i = 0; i < 4; i++)
    {
        for(j = 0; j < 4; j++)
        {
            gc_draw_rect(main_window->gc, hold_block_left + (j * CELL_SIZE),
                                          hold_block_top + (i * CELL_SIZE),
                                          CELL_SIZE, CELL_SIZE,
                                          GREY_BORDER);

            color = (held_block &&
                        held_block->tiles[0][i][j]) ?
                                held_block->color : 0x000000ff;

            gc_fill_rect(main_window->gc, hold_block_left + (j * CELL_SIZE) + 1,
                                          hold_block_top + (i * CELL_SIZE) + 1,
                                          CELL_SIZE - 2, CELL_SIZE - 2, color);
        }
    }
}


static inline void hold_block(void)
{
    if(!can_hold)
    {
        return;
    }

    if(!held_block)
    {
        held_block = cur_block;
        set_cur_block(get_and_update());
    }
    else
    {
        struct block_t *tmp = cur_block;

        set_cur_block(held_block);
        //cur_block = held_block;
        //reset_block(cur_block);
        held_block = tmp;
    }

    can_hold = 0;
}


static inline int tile_drop_dist(int row, int col)
{
    int drop = 0;

    while(is_empty(row + drop + 1, col))
    {
        drop++;
    }

    return drop;
}


static inline int block_drop_dist(void)
{
    int drop = BOARD_ROWS;
    int i, j;

    for(i = 0; i < 4; i++)
    {
        for(j = 0; j < 4; j++)
        {
            if(cur_block->tiles[cur_block->rotation][i][j] != 0)
            {
                drop = MIN(drop, tile_drop_dist(cur_block->row + i,
                                                cur_block->col + j));
            }
        }
    }

    return drop;
}


static inline void drop_block(void)
{
    move_block(cur_block, block_drop_dist(), 0);
    place_block();
}


// draw a ghost block to show where the current block will end
static inline void draw_ghost_block(void)
{
    int i, j;
    int drop = block_drop_dist();
    uint32_t color = (cur_block->color & ~0xff) | 0x44;

    for(i = 0; i < 4; i++)
    {
        if(cur_block->row + i < 2)
        {
            continue;
        }

        for(j = 0; j < 4; j++)
        {
            if(cur_block->tiles[cur_block->rotation][i][j] != 0)
            {
                gc_fill_rect(main_window->gc,
                             board_left + ((cur_block->col + j) * CELL_SIZE) + 1,
                             board_top + ((cur_block->row + i + drop) * CELL_SIZE) + 1,
                             CELL_SIZE - 2, CELL_SIZE - 2, color);
            }
        }
    }
}

