/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
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
    0xEFEFEFFF,                           // window bg
    0x737373FF,                           // window title bg top color
    0x5A5A5AFF,                           // window title bg mid1 color
    0x4A4A4AFF,                           // window title bg mid2 color
    0x3A3A3AFF,                           // window title bg bottom color
    0x555555FF,                           // window title bg top color (inactive)
    0x4D4D4DFF,                           // window title bg mid1 color (inactive)
    0x484848FF,                           // window title bg mid2 color (inactive)
    0x404040FF,                           // window title bg bottom color (inactive)
    0xFFFFFFFF, 0x888888FF,               // window text
    0x2A2A2AFF,                           // window border outer color (very dark grey)
    0x4A4A4AFF,                           // window border mid color (medium dark grey)
    0x737373FF,                           // window border inner color (light grey)
    0x9E9E9EFF,                           // window border top 1px highlight
    0x3A3A3AFF,                           // window border inactive outer color (very dark grey)
    0x454545FF,                           // window border inactive mid color (medium dark grey)
    0x505050FF,                           // window border inactive inner color (light grey)
    0x656565FF,                           // window border top 1px highlight (inactive)

    0x5A5A5AFF,                           // controlbox button background
    0x7D7D7DFF,                           // controlbox button background (hover)
    0xEBF0F5FF,                           // controlbox button text
    0x4D4D4DFF,                           // controlbox button inactive background
    0x585858FF,                           // controlbox button inactive background (hover)
    0x8A9095FF,                           // controlbox button inactive text

    0x5A5A5AFF,                           // controlbox button disabled background
    0x3A3A3AFF,                           // controlbox button disabled text
    0x7A7A7AFF,                           // controlbox button disabled text shadow
    0x4D4D4DFF,                           // controlbox button disabled inactive background
    0x353535FF,                           // controlbox button disabled inactive text
    0x606060FF,                           // controlbox button disabled inactive text shadow

    0x7F7F7FFF,                           // controlbox button top & left borders
    0x3A3A3AFF,                           // controlbox button bottom & right borders
    0x9E9E9EFF,                           // controlbox button hover top & left borders
    0x4A4A4AFF,                           // controlbox button hover bottom & right borders

    0xEFEFEFFF, 0x222226FF, 0x222226FF,   // buttons
    0xB4B4B8FF, 0x222226FF, 0x222226FF,
    0xB4B4B8FF, 0x222226FF, 0x222226FF,
    0xE0DFE3FF, 0x222226FF, 0x222226FF,
    0xEFEFEFFF, 0xBABDC4FF, 0x222226FF,
    0xEFEFEFFF, 0x222226FF,               // status bars
    0xEFEFEFFF, 0x222226FF,               // scroll bars
    0xFFFFFFFF, 0x000000FF,               // textboxes
    0xFFFFFFFF, 0x000000FF,               // inputboxes
    0x16A085FF, 0xFFFFFFFF,
    0xEFEFEFFF, 0xBABDC4FF,
    0x16A085FF, 0x333333FF, 0xDDDDDDFF,   // toggle buttons
};

