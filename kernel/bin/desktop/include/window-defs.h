/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: window-defs.h
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
 *  \file window-defs.h
 *
 *  General window-related macros used on both the client and server side.
 */

#ifndef WINDOW_DEFS_H
#define WINDOW_DEFS_H

#define WINDOW_TITLEHEIGHT          32 
#define WINDOW_BORDERWIDTH          2
#define WINDOW_ICONWIDTH            16

#define WINDOW_MIN_WIDTH            10
#define WINDOW_MIN_HEIGHT           10

// Window states
#define WINDOW_STATE_NORMAL         1
#define WINDOW_STATE_MAXIMIZED      2
#define WINDOW_STATE_MINIMIZED      3
#define WINDOW_STATE_FULLSCREEN     4

// Some flags to define our window behavior
#define WINDOW_NODECORATION         0x01
#define WINDOW_NOCONTROLBOX         0x02
#define WINDOW_NOICON               0x04
#define WINDOW_NORAISE              0x08
#define WINDOW_HIDDEN               0x10
#define WINDOW_NOFOCUS              0x20
#define WINDOW_NORESIZE             0x40
#define WINDOW_NOMINIMIZE           0x80
#define WINDOW_ALWAYSONTOP          0x100
#define WINDOW_SKIPTASKBAR          0x200

#ifdef GUI_SERVER
# define CONTROLBOX_FLAG_CLIP       0x01
# define CONTROLBOX_FLAG_INVALIDATE 0x02
#endif

// Flags for use by client applications
#ifndef GUI_SERVER
# define WINDOW_HASMENU             0x400
# define WINDOW_SHOWMENU            0x800
# define WINDOW_HASSTATUSBAR        0x1000
# define WINDOW_3D_WIDGET           0x2000  // 3d-looking widgets, not windows
#endif

// Window gravity types
#define WINDOW_ALIGN_ABSOLUTE       0x00
#define WINDOW_ALIGN_TOP            0x01
#define WINDOW_ALIGN_BOTTOM         0x02
#define WINDOW_ALIGN_LEFT           0x04
#define WINDOW_ALIGN_RIGHT          0x08
#define WINDOW_ALIGN_CENTERH        0x10
#define WINDOW_ALIGN_CENTERV        0x20
#define WINDOW_ALIGN_CENTERBOTH     0x30

// Widget alignment types
#define RESIZE_FILLW                0x1
#define RESIZE_FILLH                0x2
#define POSITION_BELOW              0x4
#define POSITION_ABOVE              0x8
#define POSITION_LEFTTO             0x10
#define POSITION_RIGHTTO            0x20
#define POSITION_CENTERH            0x40
#define POSITION_CENTERV            0x80
#define POSITION_ALIGN_LEFT         0x100
#define POSITION_ALIGN_RIGHT        0x200
#define RESIZE_FIXEDW               0x400
#define RESIZE_FIXEDH               0x800

// Text alignment for text-based controls (label, textbox, ...)
#define TEXT_ALIGN_TOP              0x1
#define TEXT_ALIGN_BOTTOM           0x2
#define TEXT_ALIGN_LEFT             0x4
#define TEXT_ALIGN_RIGHT            0x8
#define TEXT_ALIGN_CENTERV          0x10
#define TEXT_ALIGN_CENTERH          0x20

// Control buttons width/height
#define CONTROL_BUTTON_LENGTH       (WINDOW_TITLEHEIGHT - (2 * WINDOW_BORDERWIDTH))
#define CONTROL_BUTTON_LENGTH2      (CONTROL_BUTTON_LENGTH + CONTROL_BUTTON_LENGTH)
#define CONTROL_BUTTON_LENGTH3      (CONTROL_BUTTON_LENGTH * 3)

// Control buttons state
#define CLOSEBUTTON_OVER            0x01
#define MAXIMIZEBUTTON_OVER         0x02
#define MINIMIZEBUTTON_OVER         0x04
#define CLOSEBUTTON_DOWN            0x10
#define MAXIMIZEBUTTON_DOWN         0x20
#define MINIMIZEBUTTON_DOWN         0x40
/*
#define CLOSEBUTTON_DISABLED        0x100
#define MAXIMIZEBUTTON_DISABLED     0x200
#define MINIMIZEBUTTON_DISABLED     0x400
*/

// Widget types
#define WINDOW_TYPE_WINDOW          1
#define WINDOW_TYPE_DIALOG          2
#define WINDOW_TYPE_MENU_FRAME      3
#define WINDOW_TYPE_BUTTON          4
#define WINDOW_TYPE_TEXTBOX         5
#define WINDOW_TYPE_LABEL           6
#define WINDOW_TYPE_INPUTBOX        7
#define WINDOW_TYPE_FILE_SELECTOR   8
#define WINDOW_TYPE_STATUSBAR       9
#define WINDOW_TYPE_HSCROLL         10
#define WINDOW_TYPE_VSCROLL         11
#define WINDOW_TYPE_LISTVIEW        12
#define WINDOW_TYPE_GROUP_BORDER    13
#define WINDOW_TYPE_SPINNER         14
#define WINDOW_TYPE_TOGGLE          15
#define WINDOW_TYPE_PUSHBUTTON      16
#define WINDOW_TYPE_COMBOBOX        17

// macro to check if a widget is TAB-able
#define TABABLE(widget)             ((widget)->type != WINDOW_TYPE_LABEL && \
                                     (widget)->type != WINDOW_TYPE_STATUSBAR && \
                                     (widget)->type != WINDOW_TYPE_HSCROLL && \
                                     (widget)->type != WINDOW_TYPE_VSCROLL && \
                                     (widget)->type != WINDOW_TYPE_GROUP_BORDER)

// Window ID typedef
typedef uint64_t winid_t;

// Helper macros to work with Window IDs
#define TO_WINID(p, i)          ((uint64_t)(p) | ((uint64_t)(i) << 32))
#define PID_FOR_WINID(wid)      (pid_t)(wid & 0xffffffff)
#define WID_FOR_WINID(wid)      (uint16_t)(((wid) >> 32) & 0xffff)

#endif      /* WINDOW_DEFS_H */
