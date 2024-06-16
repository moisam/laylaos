/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: pieces.h
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
 *  \file pieces.h
 *
 *  Definitions of the game pieces in tetris.
 */

/*
 *
 * 1    ====   Teal           5    ==   Green
 *                                ==
 *
 * 2     =                    6   ===   Purple
 *       =     Blue                =
 *      ==
 *
 * 3      =    Orange         7   ==    Red
 *     ====                        ==
 *
 * 4    ==     Yellow         0   No tile
 *      ==
 *
 * Rotation states:
 * 0 - 0 degrees
 * 1 - 90 degrees
 * 2 - 180 degrees
 * 3 - 270 degrees
 *
 */

struct block_t blocks[BLOCK_TYPES + 1] =
{
    // 0 - No piece
    {
        0,
        0, 0,
        0x000000ff,
        {
        {
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        },
    },
    // 1 - I piece
    {
        1,
        0, 3,
        0x66ffffff,
        {
        {   // state 0 - no rotation
            { 0, 0, 0, 0, },
            { 1, 1, 1, 1, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 1 - 90 degrees rotation
            { 0, 0, 1, 0, },
            { 0, 0, 1, 0, },
            { 0, 0, 1, 0, },
            { 0, 0, 1, 0, },
        },
        {
            // state 2 - 180 degrees rotation
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
            { 1, 1, 1, 1, },
            { 0, 0, 0, 0, },
        },
        {
            // state 3 - 270 degrees rotation
            { 0, 1, 0, 0, },
            { 0, 1, 0, 0, },
            { 0, 1, 0, 0, },
            { 0, 1, 0, 0, },
        },
        },
    },
    // 2 - J piece
    {
        2,
        0, 3,
        0x0000ffff,
        {
        {   // state 0 - no rotation
            { 2, 0, 0, 0, },
            { 2, 2, 2, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 1 - 90 degrees rotation
            { 0, 2, 2, 0, },
            { 0, 2, 0, 0, },
            { 0, 2, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 2 - 180 degrees rotation
            { 0, 0, 0, 0, },
            { 2, 2, 2, 0, },
            { 0, 0, 2, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 3 - 270 degrees rotation
            { 0, 2, 0, 0, },
            { 0, 2, 0, 0, },
            { 2, 2, 0, 0, },
            { 0, 0, 0, 0, },
        },
        },
    },
    // 3 - L piece
    {
        3,
        0, 3,
        0xffcc00ff,
        {
        {   // state 0 - no rotation
            { 0, 0, 3, 0, },
            { 3, 3, 3, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 1 - 90 degrees rotation
            { 0, 3, 0, 0, },
            { 0, 3, 0, 0, },
            { 0, 3, 3, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 2 - 180 degrees rotation
            { 0, 0, 0, 0, },
            { 3, 3, 3, 0, },
            { 3, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 3 - 270 degrees rotation
            { 3, 3, 0, 0, },
            { 0, 3, 0, 0, },
            { 0, 3, 0, 0, },
            { 0, 0, 0, 0, },
        },
        },
    },
    // 4 - O piece
    {
        4,
        0, 4,
        0xffff00ff,
        {
        {   // state 0 - no rotation
            { 4, 4, 0, 0, },
            { 4, 4, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 1 - 90 degrees rotation
            { 4, 4, 0, 0, },
            { 4, 4, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 2 - 180 degrees rotation
            { 4, 4, 0, 0, },
            { 4, 4, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 3 - 270 degrees rotation
            { 4, 4, 0, 0, },
            { 4, 4, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        },
    },
    // 5 - S piece
    {
        5,
        0, 3,
        0x33cc33ff,
        {
        {   // state 0 - no rotation
            { 0, 5, 5, 0, },
            { 5, 5, 0, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 1 - 90 degrees rotation
            { 0, 5, 0, 0, },
            { 0, 5, 5, 0, },
            { 0, 0, 5, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 2 - 180 degrees rotation
            { 0, 0, 0, 0, },
            { 0, 5, 5, 0, },
            { 5, 5, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 3 - 270 degrees rotation
            { 5, 0, 0, 0, },
            { 5, 5, 0, 0, },
            { 0, 5, 0, 0, },
            { 0, 0, 0, 0, },
        },
        },
    },
    // 6 - T piece
    {
        6,
        0, 3,
        0x9900ffff,
        {
        {   // state 0 - no rotation
            { 0, 6, 0, 0, },
            { 6, 6, 6, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 1 - 90 degrees rotation
            { 0, 6, 0, 0, },
            { 0, 6, 6, 0, },
            { 0, 6, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 2 - 180 degrees rotation
            { 0, 0, 0, 0, },
            { 6, 6, 6, 0, },
            { 0, 6, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 3 - 270 degrees rotation
            { 0, 6, 0, 0, },
            { 6, 6, 0, 0, },
            { 0, 6, 0, 0, },
            { 0, 0, 0, 0, },
        },
        },
    },
    // 7 - Z piece
    {
        7,
        0, 3,
        0xff0000ff,
        {
        {   // state 0 - no rotation
            { 7, 7, 0, 0, },
            { 0, 7, 7, 0, },
            { 0, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 1 - 90 degrees rotation
            { 0, 0, 7, 0, },
            { 0, 7, 7, 0, },
            { 0, 7, 0, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 2 - 180 degrees rotation
            { 0, 0, 0, 0, },
            { 7, 7, 0, 0, },
            { 0, 7, 7, 0, },
            { 0, 0, 0, 0, },
        },
        {
            // state 3 - 270 degrees rotation
            { 0, 7, 0, 0, },
            { 7, 7, 0, 0, },
            { 7, 0, 0, 0, },
            { 0, 0, 0, 0, },
        },
        },
    },
};

