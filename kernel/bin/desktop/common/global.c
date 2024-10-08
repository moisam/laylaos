/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: global.c
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
 *  \file global.c
 *
 *  This file contains the global data struct. This is intended for internal 
 *  library use, not for user programs.
 */

#include "../include/gui.h"

struct __global_gui_data_t __global_gui_data = { {0, }, {0, }, 0, };

uint32_t builtin_color_theme[] =
{
    0xCDCFD4FF, 0x3B4047FF, 0x3B4047FF,   // windows
    0xCDCFD4FF, 0x535E64FF, 0x2E3238FF,
    0x2E3238FF,
    0xCDCFD4FF, 0x222226FF, 0x222226FF,   // buttons
    0xB4B4B8FF, 0x222226FF, 0x222226FF,
    0xB4B4B8FF /* 0x535E64FF */, 0x222226FF /* 0xCDCFD4FF */, 0x222226FF,
    0xE0DFE3FF, 0x222226FF, 0x222226FF,
    0xCDCFD4FF, 0xBABDC4FF, 0x222226FF,
    0xCDCFD4FF, 0x222226FF,               // status bars
    0xCDCFD4FF, 0x222226FF,               // scroll bars
    0xFFFFFFFF, 0x000000FF,               // textboxes
    0xFFFFFFFF, 0x000000FF,               // inputboxes
    0x16A085FF, 0xFFFFFFFF,
    0xCDCFD4FF, 0xBABDC4FF,
    0x16A085FF, 0x333333FF, 0xDDDDDDFF,   // toggle buttons
};

