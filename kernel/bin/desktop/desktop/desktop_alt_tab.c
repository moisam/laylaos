/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: desktop_alt_tab.c
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
 *  \file desktop_alt_tab.c
 *
 *  The desktop ALT-TAB functionality that allows the user to switch between
 *  open windows.
 */

#include <string.h>
#include "desktop.h"
#include "../include/gui.h"
#include "../include/resources.h"
#include "../include/client/window.h"
#include "../include/keys.h"
#include "../include/font.h"

#include "../client/inlines.c"

#define GLOB                    __global_gui_data

#define SIDE_PADDING            16
#define TOP_PADDING             8
#define BOTTOM_PADDING          8
#define INNER_PADDING           8

#define ICONWIDTH               64
#define ENTRYWIDTH              (ICONWIDTH + (INNER_PADDING * 2))

#define WIN_BGCOLOR             0x2C3235FF
#define WIN_FGCOLOR             0xFFFFFFFF
#define WIN_HICOLOR             0x16A085FF

int alt_tabbing = 0;
int win_count;
int cur_focused;
int first_shown;
int shown_count;
int maxw;
struct bitmap32_t default_icon_bitmap;
struct window_t *alttab_win = NULL;


void desktop_init_alttab(void)
{
    //key_bind(KEYCODE_Z, 0, KEYBINDING_NOTIFY_ONCE);
    key_bind(KEYCODE_TAB, MODIFIER_MASK_ALT, KEYBINDING_NOTIFY_ONCE);
    key_bind(KEYCODE_LALT, 0, KEYBINDING_NOTIFY);
    key_bind(KEYCODE_RALT, 0, KEYBINDING_NOTIFY);

    default_icon_bitmap.width = ICONWIDTH;
    default_icon_bitmap.height = ICONWIDTH;
    default_icon_bitmap.data = NULL;

    image_load(DEFAULT_EXE_ICON_PATH, &default_icon_bitmap);
}


void desktop_prep_alttab(void)
{

#define winh    (ICONWIDTH + charh + (TOP_PADDING * 2) + (INNER_PADDING * 2))

    struct winent_t *winent;
    int calc_winw;
    int charh = GLOB.mono.charh;
    int winx;
    int winy = (GLOB.screen.h - winh) / 2;

    if(!alt_tabbing)
    {
        win_count = 0;
        cur_focused = 0;
        first_shown = 0;
        shown_count = 0;
        maxw = (GLOB.screen.w - ICONWIDTH);

        for(winent = winentries; winent != NULL; winent = winent->next)
        {
            if(!(winent->flags & WINDOW_SKIPTASKBAR))
            {
                win_count++;
            }
        }

        if(!win_count)
        {
            return;
        }

        alt_tabbing = 1;
        calc_winw = (win_count * ENTRYWIDTH) + (SIDE_PADDING * 2);
        shown_count = win_count;

        if(calc_winw > maxw)
        {
            shown_count = (maxw - (SIDE_PADDING * 2)) / ENTRYWIDTH;
            calc_winw = (shown_count * ENTRYWIDTH) + (SIDE_PADDING * 2);
        }

        if(alttab_win)
        {
            if(calc_winw != alttab_win->w)
            {
                if(!(alttab_win->flags & WINDOW_HIDDEN))
                {
                    window_hide(alttab_win);
                }

                winx = (GLOB.screen.w - calc_winw) / 2;
                window_set_size(alttab_win, winx, winy, calc_winw, winh);

                // wait for the window resize event
                return;
            }
        }
        else
        {
            struct window_attribs_t attribs;

            attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
            attribs.x = (GLOB.screen.w - calc_winw) / 2;
            attribs.y = winy;
            attribs.w = calc_winw;
            attribs.h = winh;
            attribs.flags = (WINDOW_ALWAYSONTOP | WINDOW_NODECORATION |
                             /* WINDOW_NORAISE | */ WINDOW_NOFOCUS | 
                             WINDOW_SKIPTASKBAR);

            if(!(alttab_win = window_create(&attribs)))
            {
                alt_tabbing = 0;
                return;
            }
        }
    }

    if(++cur_focused == win_count)
    {
        cur_focused = 0;
    }

    if(cur_focused == first_shown + shown_count)
    {
        first_shown++;
    }

    desktop_draw_alttab();
}


void desktop_cancel_alttab(void)
{
    if(alttab_win && !(alttab_win->flags & WINDOW_HIDDEN))
    {
        window_hide(alttab_win);
    }

    alt_tabbing = 0;
}


void desktop_finish_alttab(void)
{
    int i;
    struct winent_t *winent;

    if(!alt_tabbing || !alttab_win)
    {
        return;
    }

    desktop_cancel_alttab();

    i = cur_focused;
    winent = winentries;

    while(winent)
    {
        if(!(winent->flags & WINDOW_SKIPTASKBAR))
        {
            if(i-- <= 0)
            {
                break;
            }
        }

        winent = winent->next;
    }
    
    if(!winent)
    {
        return;
    }
    
    simple_request(REQUEST_WINDOW_RAISE, GLOB.server_winid, winent->winid);
}


char *ellipsify(char *str)
{
    int charw = GLOB.mono.charw;
    size_t maxlen = (ENTRYWIDTH / charw) - 1;
    size_t len;
    char *copy;

    if(!str)
    {
        return NULL;
    }

    if((len = strlen(str)) <= maxlen)
    {
        return strdup(str);
    }

    if(!(copy = malloc(maxlen + 1)))
    {
        return NULL;
    }

    strncpy(copy, str, maxlen - 3);
    copy[maxlen - 3] = '.';
    copy[maxlen - 2] = '.';
    copy[maxlen - 1] = '.';
    copy[maxlen] = '\0';

    return copy;
}


void desktop_draw_alttab(void)
{
    int i, curx, lastx;
    int texty;
    char *str;
    struct winent_t *winent;
    struct bitmap32_t *bitmap;
    int charh = GLOB.mono.charh;

    if(!alt_tabbing || !alttab_win)
    {
        return;
    }

    i = first_shown;
    winent = winentries;

    while(winent)
    {
        if(!(winent->flags & WINDOW_SKIPTASKBAR))
        {
            if(i-- <= 0)
            {
                break;
            }
        }

        winent = winent->next;
    }

    if(!winent)
    {
        return;
    }

    gc_fill_rect(alttab_win->gc,
                         0, 0, alttab_win->w, alttab_win->h, WIN_BGCOLOR);
    curx = SIDE_PADDING;
    lastx = alttab_win->w - SIDE_PADDING;
    texty = alttab_win->h - BOTTOM_PADDING - charh;

    for(i = first_shown; i < win_count; i++, curx += ENTRYWIDTH)
    {
        if(curx >= lastx || !winent)
        {
            break;
        }

        if(i == cur_focused)
        {
            gc_fill_rect(alttab_win->gc,
                                 curx, TOP_PADDING,
                                 ENTRYWIDTH, ENTRYWIDTH,
                                 WIN_HICOLOR);
        }

        if(winent->icon)
        {
            bitmap = winent->icon;
        }
        else
        {
            bitmap = &default_icon_bitmap;
        }


        gc_blit_bitmap_highlighted(alttab_win->gc, bitmap,
                                   curx + INNER_PADDING,
                                   TOP_PADDING + INNER_PADDING,
                                   0, 0, ICONWIDTH, ICONWIDTH, 0);

        if((str = ellipsify(winent->title)))
        {
            gc_draw_text(alttab_win->gc, str,
                                 curx + ((ENTRYWIDTH -
                                    string_width(&GLOB.mono, str)) / 2),
                                 texty,
                                 WIN_FGCOLOR, 0);
            free(str);
        }

        winent = winent->next;

        while(winent && (winent->flags & WINDOW_SKIPTASKBAR))
        {
            winent = winent->next;
            
            if(!winent)
            {
                break;
            }
        }
    }

    if((alttab_win->flags & WINDOW_HIDDEN))
    {
        window_show(alttab_win);
    }
    else
    {
        window_invalidate(alttab_win);
    }
}

