/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: theme.h
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
 *  \file theme.h
 *
 *  Declarations of the theme colors used by all widgets.
 */

#ifndef WINDOW_THEME_H
#define WINDOW_THEME_H

/******************************************
 * WINDOW theme colors
 ******************************************/

#define WINDOW_BGCOLOR                      0xCDCFD4FF
#define WINDOW_TITLECOLOR                   0x3B4047FF
#define WINDOW_TITLECOLOR_INACTIVE          0x3B4047FF
#define WINDOW_TEXTCOLOR                    0xCDCFD4FF
#define WINDOW_TEXTCOLOR_INACTIVE           0x535E64FF
#define WINDOW_BORDERCOLOR                  0x2E3238FF

/* For widgets that want to look "3D" */

#define GLOBAL_LIGHT_SIDE_COLOR             0xF2F2F3FF
#define GLOBAL_DARK_SIDE_COLOR              0x8B8A88FF

#define GLOBAL_BLACK_COLOR                  0x000000FF
#define GLOBAL_WHITE_COLOR                  0xFFFFFFFF


/******************************************
 * WINDOW CONTROLBOX buttons theme colors
 ******************************************/

/* Close button colors */

// disabled state
#define CLOSEBUTTON_TEXTCOLOR_DISABLED      0x5D6366FF

// normal state
#define CLOSEBUTTON_BGCOLOR                 0x3B4047FF
#define CLOSEBUTTON_TEXTCOLOR               0xCDCFD4FF
#define CLOSEBUTTON_BORDERCOLOR             0x3B4047FF

// mouse-over state
#define CLOSEBUTTON_MOUSEOVER_BGCOLOR       0xCDCFD4FF
#define CLOSEBUTTON_MOUSEOVER_TEXTCOLOR     0x0D6C60FF
#define CLOSEBUTTON_MOUSEOVER_BORDERCOLOR   0xCDCFD4FF

// depressed state
#define CLOSEBUTTON_DOWN_BGCOLOR            0xB4B4B8FF
#define CLOSEBUTTON_DOWN_TEXTCOLOR          0x0D6C60FF
#define CLOSEBUTTON_DOWN_BORDERCOLOR        0xB4B4B8FF

/* Maximize button colors */

// disabled state
#define MAXIMIZEBUTTON_TEXTCOLOR_DISABLED   0x5D6366FF

// normal state
#define MAXIMIZEBUTTON_BGCOLOR              0x3B4047FF
#define MAXIMIZEBUTTON_TEXTCOLOR            0xCDCFD4FF
#define MAXIMIZEBUTTON_BORDERCOLOR          0x3B4047FF

// mouse-over state
#define MAXIMIZEBUTTON_MOUSEOVER_BGCOLOR     0xCDCFD4FF
#define MAXIMIZEBUTTON_MOUSEOVER_TEXTCOLOR   0x0D6C60FF
#define MAXIMIZEBUTTON_MOUSEOVER_BORDERCOLOR 0xCDCFD4FF

// depressed state
#define MAXIMIZEBUTTON_DOWN_BGCOLOR         0xB4B4B8FF
#define MAXIMIZEBUTTON_DOWN_TEXTCOLOR       0x0D6C60FF
#define MAXIMIZEBUTTON_DOWN_BORDERCOLOR     0xB4B4B8FF

/* Minimize button colors */

// disabled state
#define MINIMIZEBUTTON_TEXTCOLOR_DISABLED   0x5D6366FF

// normal state
#define MINIMIZEBUTTON_BGCOLOR              0x3B4047FF
#define MINIMIZEBUTTON_TEXTCOLOR            0xCDCFD4FF
#define MINIMIZEBUTTON_BORDERCOLOR          0x3B4047FF

// mouse-over state
#define MINIMIZEBUTTON_MOUSEOVER_BGCOLOR     0xCDCFD4FF
#define MINIMIZEBUTTON_MOUSEOVER_TEXTCOLOR   0x0D6C60FF
#define MINIMIZEBUTTON_MOUSEOVER_BORDERCOLOR 0xCDCFD4FF

// depressed state
#define MINIMIZEBUTTON_DOWN_BGCOLOR         0xB4B4B8FF
#define MINIMIZEBUTTON_DOWN_TEXTCOLOR       0x0D6C60FF
#define MINIMIZEBUTTON_DOWN_BORDERCOLOR     0xB4B4B8FF

/******************************************
 * BUTTON theme colors
 ******************************************/

// normal state
#define BUTTON_BGCOLOR                      0xCDCFD4FF
#define BUTTON_TEXTCOLOR                    0x222226FF
#define BUTTON_BORDERCOLOR                  0x222226FF

// mouse-over state
#define BUTTON_MOUSEOVER_BGCOLOR            0xB4B4B8FF
#define BUTTON_MOUSEOVER_TEXTCOLOR          0x222226FF
#define BUTTON_MOUSEOVER_BORDERCOLOR        0x222226FF

// depressed state
#define BUTTON_DOWN_BGCOLOR                 0x535E64FF
#define BUTTON_DOWN_TEXTCOLOR               0xCDCFD4FF
#define BUTTON_DOWN_BORDERCOLOR             0x222226FF

// pushed state (for pushbuttons)
#define BUTTON_PUSH_BGCOLOR                 0xE0DFE3FF
#define BUTTON_PUSH_TEXTCOLOR               0x222226FF
#define BUTTON_PUSH_BORDERCOLOR             0x222226FF

// disabled state
#define BUTTON_DISABLED_BGCOLOR             0xCDCFD4FF
#define BUTTON_DISABLED_TEXTCOLOR           0xBABDC4FF
#define BUTTON_DISABLED_BORDERCOLOR         0x222226FF


/******************************************
 * MENU theme colors
 ******************************************/

#define MENU_BGCOLOR                        0xCDCFD4FF
#define MENU_TEXTCOLOR                      0x222226FF
#define MENU_DISABLED_TEXTCOLOR             0xB4B4B8FF
#define MENU_MOUSEOVER_BGCOLOR              0xB4B4B8FF
#define MENU_MOUSEOVER_TEXTCOLOR            0x222226FF


/******************************************
 * STATUSBAR theme colors
 ******************************************/

#define STATUSBAR_BGCOLOR                   0xCDCFD4FF
#define STATUSBAR_TEXTCOLOR                 0x222226FF


/******************************************
 * SCROLLBAR theme colors
 ******************************************/

#define SCROLLBAR_BGCOLOR                   0xCDCFD4FF
#define SCROLLBAR_TEXTCOLOR                 0x222226FF


/******************************************
 * GROUP-BORDER theme colors
 ******************************************/

#define GROUP_BORDER_BGCOLOR                0xCDCFD4FF
#define GROUP_BORDER_TEXTCOLOR              0x222226FF


/******************************************
 * TEXTBOX theme colors
 ******************************************/

#define TEXTBOX_BGCOLOR                     0xFFFFFFFF
#define TEXTBOX_TEXTCOLOR                   0x000000FF


/******************************************
 * INPUTBOX theme colors
 *
 * These are also used by other "editable"
 * controls like the listview, spinner and
 * dropdown box.
 ******************************************/

#define INPUTBOX_BGCOLOR                    0xFFFFFFFF
#define INPUTBOX_TEXTCOLOR                  0x000000FF
#define INPUTBOX_SELECT_BGCOLOR             0x16A085FF
#define INPUTBOX_SELECT_TEXTCOLOR           0xFFFFFFFF


/******************************************
 * TOGGLE theme colors
 ******************************************/

#define TOGGLE_BGCOLOR_ON                   0x16A085FF
#define TOGGLE_BGCOLOR_OFF                  0x333333FF
#define TOGGLE_BUTTON_COLOR                 0xDDDDDDFF


/******************************************
 * TOP PANEL theme colors
 ******************************************/

#define TOPPANEL_BGCOLOR                    0x2C3235FF
#define TOPPANEL_FGCOLOR                    0xFFFFFFFF
#define TOPPANEL_HICOLOR                    0x16A085FF

#define TOPPANEL_MOUSEOVER_BGCOLOR          0x919191FF
#define TOPPANEL_MOUSEOVER_FGCOLOR          0xFFFFFFFF

#define TOPPANEL_DOWN_BGCOLOR               0x919191FF
#define TOPPANEL_DOWN_FGCOLOR               0xFFFFFFFF


/******************************************
 * BOTTOM PANEL theme colors
 ******************************************/

#endif      /* WINDOW_THEME_H */
