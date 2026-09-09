/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
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

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************
 * Global indices into the theme colors array
 *********************************************/
enum
{
    THEME_COLOR_WINDOW_BGCOLOR = 0,
    THEME_COLOR_WINDOW_TITLECOLOR_TOP,
    THEME_COLOR_WINDOW_TITLECOLOR_MID1,
    THEME_COLOR_WINDOW_TITLECOLOR_MID2,
    THEME_COLOR_WINDOW_TITLECOLOR_BOTTOM,
    THEME_COLOR_WINDOW_TITLECOLOR_INACTIVE_TOP,
    THEME_COLOR_WINDOW_TITLECOLOR_INACTIVE_MID1,
    THEME_COLOR_WINDOW_TITLECOLOR_INACTIVE_MID2,
    THEME_COLOR_WINDOW_TITLECOLOR_INACTIVE_BOTTOM,
    THEME_COLOR_WINDOW_TEXTCOLOR,
    THEME_COLOR_WINDOW_TEXTCOLOR_INACTIVE,

    THEME_COLOR_WINDOW_BORDERCOLOR_OUTER,
    THEME_COLOR_WINDOW_BORDERCOLOR_MID,
    THEME_COLOR_WINDOW_BORDERCOLOR_INNER,
    THEME_COLOR_WINDOW_BORDERCOLOR_TOP_HI,
    THEME_COLOR_WINDOW_BORDERCOLOR_INACTIVE_OUTER,
    THEME_COLOR_WINDOW_BORDERCOLOR_INACTIVE_MID,
    THEME_COLOR_WINDOW_BORDERCOLOR_INACTIVE_INNER,
    THEME_COLOR_WINDOW_BORDERCOLOR_INACTIVE_TOP_HI,

    THEME_COLOR_WINDOW_CONTROLBOX_BGCOLOR,
    THEME_COLOR_WINDOW_CONTROLBOX_BGCOLOR_HOVER,
    THEME_COLOR_WINDOW_CONTROLBOX_TEXT,
    THEME_COLOR_WINDOW_CONTROLBOX_INACTIVE_BGCOLOR,
    THEME_COLOR_WINDOW_CONTROLBOX_INACTIVE_BGCOLOR_HOVER,
    THEME_COLOR_WINDOW_CONTROLBOX_INACTIVE_TEXT,

    THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_BGCOLOR,
    THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_TEXT,
    THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_TEXT_SHADOW,
    THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_INACTIVE_BGCOLOR,
    THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_INACTIVE_TEXT,
    THEME_COLOR_WINDOW_CONTROLBOX_DISABLED_INACTIVE_TEXT_SHADOW,

    THEME_COLOR_WINDOW_CONTROLBOX_BORDER_HI,
    THEME_COLOR_WINDOW_CONTROLBOX_BORDER_LO,
    THEME_COLOR_WINDOW_CONTROLBOX_BORDER_HI_HOVER,
    THEME_COLOR_WINDOW_CONTROLBOX_BORDER_LO_HOVER,

    THEME_COLOR_BUTTON_BGCOLOR,
    THEME_COLOR_BUTTON_TEXTCOLOR,
    THEME_COLOR_BUTTON_BORDERCOLOR,

    THEME_COLOR_BUTTON_MOUSEOVER_BGCOLOR,
    THEME_COLOR_BUTTON_MOUSEOVER_TEXTCOLOR,
    THEME_COLOR_BUTTON_MOUSEOVER_BORDERCOLOR,

    THEME_COLOR_BUTTON_DOWN_BGCOLOR,
    THEME_COLOR_BUTTON_DOWN_TEXTCOLOR,
    THEME_COLOR_BUTTON_DOWN_BORDERCOLOR,

    THEME_COLOR_BUTTON_PUSH_BGCOLOR,
    THEME_COLOR_BUTTON_PUSH_TEXTCOLOR,
    THEME_COLOR_BUTTON_PUSH_BORDERCOLOR,

    THEME_COLOR_BUTTON_DISABLED_BGCOLOR,
    THEME_COLOR_BUTTON_DISABLED_TEXTCOLOR,
    THEME_COLOR_BUTTON_DISABLED_BORDERCOLOR,

    THEME_COLOR_STATUSBAR_BGCOLOR,
    THEME_COLOR_STATUSBAR_TEXTCOLOR,

    THEME_COLOR_SCROLLBAR_BGCOLOR,
    THEME_COLOR_SCROLLBAR_TEXTCOLOR,

    THEME_COLOR_TEXTBOX_BGCOLOR,
    THEME_COLOR_TEXTBOX_TEXTCOLOR,

    THEME_COLOR_INPUTBOX_BGCOLOR,
    THEME_COLOR_INPUTBOX_TEXTCOLOR,
    THEME_COLOR_INPUTBOX_SELECT_BGCOLOR,
    THEME_COLOR_INPUTBOX_SELECT_TEXTCOLOR,
    THEME_COLOR_INPUTBOX_DISABLED_BGCOLOR,
    THEME_COLOR_INPUTBOX_DISABLED_TEXTCOLOR,

    THEME_COLOR_TOGGLE_BGCOLOR_ON,
    THEME_COLOR_TOGGLE_BGCOLOR_OFF,
    THEME_COLOR_TOGGLE_BUTTON_COLOR,

    THEME_COLOR_LAST,
};


/******************************************
 * WINDOW theme colors
 ******************************************/

//#define WINDOW_BORDER_ALPHA                 0x000000AA

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
#define CLOSEBUTTON_TEXTCOLOR_DISABLED      0x919191FF /* 0x5D6366FF */

// normal state
#define CLOSEBUTTON_BGCOLOR                 0x3B4047FF
#define CLOSEBUTTON_TEXTCOLOR               0xCDCFD4FF

// mouse-over state
#define CLOSEBUTTON_MOUSEOVER_BGCOLOR       0xCDCFD4FF
#define CLOSEBUTTON_MOUSEOVER_TEXTCOLOR     0x0D6C60FF

/* Maximize button colors */

// disabled state
#define MAXIMIZEBUTTON_TEXTCOLOR_DISABLED   0x919191FF /* 0x5D6366FF */

// normal state
#define MAXIMIZEBUTTON_BGCOLOR              0x3B4047FF
#define MAXIMIZEBUTTON_TEXTCOLOR            0xCDCFD4FF

// mouse-over state
#define MAXIMIZEBUTTON_MOUSEOVER_BGCOLOR     0xCDCFD4FF
#define MAXIMIZEBUTTON_MOUSEOVER_TEXTCOLOR   0x0D6C60FF

/* Minimize button colors */

// disabled state
#define MINIMIZEBUTTON_TEXTCOLOR_DISABLED   0x919191FF /* 0x5D6366FF */

// normal state
#define MINIMIZEBUTTON_BGCOLOR              0x3B4047FF
#define MINIMIZEBUTTON_TEXTCOLOR            0xCDCFD4FF

// mouse-over state
#define MINIMIZEBUTTON_MOUSEOVER_BGCOLOR     0xCDCFD4FF
#define MINIMIZEBUTTON_MOUSEOVER_TEXTCOLOR   0x0D6C60FF


/******************************************
 * MENU theme colors
 ******************************************/

#define MENU_BGCOLOR                        0xCDCFD4FF
#define MENU_TEXTCOLOR                      0x222226FF
#define MENU_DISABLED_TEXTCOLOR             0xB4B4B8FF
#define MENU_MOUSEOVER_BGCOLOR              0xB4B4B8FF
#define MENU_MOUSEOVER_TEXTCOLOR            0x222226FF


/******************************************
 * GROUP-BORDER theme colors
 ******************************************/

#define GROUP_BORDER_BGCOLOR                0xCDCFD4FF
#define GROUP_BORDER_TEXTCOLOR              0x222226FF


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
 * Functions to work with themes
 ******************************************/

int get_color_theme(void);
void send_color_theme_to_server(void);
void set_color_theme(void *evbuf);
void set_dark_color_theme(void);

#ifdef __cplusplus
}
#endif

#endif      /* WINDOW_THEME_H */
