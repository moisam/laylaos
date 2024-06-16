/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: file-selector.c
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
 *  \file file-selector.c
 *
 *  The implementation of a file selector widget.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>         // PATH_MAX
#include <fnmatch.h>

#include "../include/gui.h"
#include "../include/resources.h"
#include "../include/client/file-selector.h"
#include "../include/client/listview.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"
#include "../include/mouse.h"
#include "../include/keys.h"
#include "../include/kbd.h"

#include "../desktop/desktop_entry_lines.c"

#include "inlines.c"


#ifndef MAX
#define MAX(a, b)               ((a) > (b) ? (a) : (b))
#endif

#define SELECTOR(w)             ((struct file_selector_t *)w)

#define BUFSZ                           0x1000

#define LISTVIEW_ENTRYHEIGHT            LISTVIEW_LINE_HEIGHT
#define LISTVIEW_LEFT_MARGIN            4
#define LISTVIEW_ICONWIDTH              20

#define ICONVIEW_ENTRYWIDTH             128
#define ICONVIEW_ENTRYHEIGHT            112
#define ICONVIEW_LEFT_MARGIN            32
#define ICONVIEW_ICONWIDTH              64

#define HIGHLIGHT_COLOR                 0x1F9EDEAA
#define BG_COLOR                        0xFFFFFFFF
#define TEXT_COLOR                      0x000000FF

#define GENERIC_FILE_ICON_PATH          DEFAULT_ICON_PATH "/file_generic.ico"
#define GENERIC_DIR_ICON_PATH           DEFAULT_ICON_PATH "/folder.ico"

#define GLOB                            __global_gui_data

#define GIGABYTE                        (1024 * 1024 * 1024)
#define MEGABYTE                        (1024 * 1024)
#define KILOBYTE                        (1024)

static char *weekdays[] =
{
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

static char *months[] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", 
    "Aug", "Sep", "Oct", "Nov", "Dec",
};

// generic icon for files
struct bitmap32_t default_file_icon =
{
    .width = ICONVIEW_ICONWIDTH,
    .height = ICONVIEW_ICONWIDTH,
    .data = NULL,
};

// generic icon for directories
struct bitmap32_t default_dir_icon =
{
    .width = ICONVIEW_ICONWIDTH,
    .height = ICONVIEW_ICONWIDTH,
    .data = NULL,
};

struct extension_t
{
    char *ext;
    char *icon_filename;
    struct bitmap32_t *icon64;
};

struct extension_t extensions[] =
{
    { ".bmp", "image.ico", NULL },
    { ".ico", "image.ico", NULL },
    { ".jpg", "image.ico", NULL },
    { ".jpeg", "image.ico", NULL },
    { ".png", "image.ico", NULL },
    { ".exe", "executable.ico", NULL },
    { ".html", "html.ico", NULL },
    { ".htm", "html.ico", NULL },
    { ".c", "code-csrc.ico", NULL },
    { ".h", "code-chdr.ico", NULL },
    { ".sh", "code-sh.ico", NULL },
    { NULL, NULL },
};

void file_selector_vscroll_callback(struct window_t *, struct scrollbar_t *);
void file_selector_hscroll_callback(struct window_t *, struct scrollbar_t *);


static inline void reset_backbuf_clipping(struct file_selector_t *selector)
{
    Rect *rect;

    // account for the border
    rect = selector->backbuf_gc.clipping.clip_rects->root;
    rect->top = 2;
    rect->left = 2;
    rect->bottom = selector->backbuf_gc.h - 3;
    rect->right = selector->backbuf_gc.w - 3;
}


/*
 * Use an inlined version for performance. We call this function 
 * a zillion times, at least.
 */
static inline void format_size(char *buf, off_t file_size)
{
    if(file_size >= GIGABYTE)
    {
        sprintf(buf, "%.1fGiB", (double)file_size / GIGABYTE);
    }
    else if(file_size >= MEGABYTE)
    {
        sprintf(buf, "%.1fMiB", (double)file_size / MEGABYTE);
    }
    else if(file_size >= KILOBYTE)
    {
        sprintf(buf, "%.1fKiB", (double)file_size / KILOBYTE);
    }
    else
    {
        sprintf(buf, "%ldb", file_size);
    }
}


static inline void format_mtime(char *buf, time_t mtime)
{
    time_t t = mtime;
    struct tm *tm;

    tm = gmtime(&t);
    sprintf(buf, "%s %d %s %d", weekdays[tm->tm_wday], tm->tm_mday,
                                months[tm->tm_mon], 1900 + tm->tm_year);
}


struct file_selector_t *file_selector_new(struct gc_t *gc, 
                                          struct window_t *parent,
                                          int x, int y, int w, int h,
                                          char *path)
{
    struct file_selector_t *selector;
    Rect *rect;

    if(!(selector = malloc(sizeof(struct file_selector_t))))
    {
        return NULL;
    }

    memset(selector, 0, sizeof(struct file_selector_t));


    if(gc_alloc_backbuf(gc, &selector->backbuf_gc, w, h) < 0)
    {
        free(selector);
        return NULL;
    }

    gc_set_font(&selector->backbuf_gc, GLOB.sysfont.data ? &GLOB.sysfont :
                                                           &GLOB.mono);

    // All subsequent drawing on the selector's canvas will be clipped to a
    // 1-pixel border. If we draw the border later (e.g. in selector_repaint())
    // we will fail, as the border will be clipped and will not be drawn.
    // A workaround would be to temporarily unclip the clipping and draw the
    // border, but this is complicated and messy. Instead, we draw the border
    // here, once and for all, and we never need to worry about it again.
    draw_inverted_3d_border(&selector->backbuf_gc, 0, 0, w, h);

    // account for the border
    reset_backbuf_clipping(selector);

    if(!(selector->window.clip_rects = RectList_new()))
    {
        free(selector->backbuf_gc.buffer);
        free(selector);
        return NULL;
    }

    if(parent->main_menu)
    {
        y += MENU_HEIGHT;
    }

    if(!(rect = Rect_new(y + 1, x + 1, y + h - 2,  x + w - 2)))
    {
        RectList_free(selector->window.clip_rects);
        free(selector->backbuf_gc.buffer);
        free(selector);
        return NULL;
    }

    RectList_add(selector->window.clip_rects, rect);
    selector->window.type = WINDOW_TYPE_FILE_SELECTOR;
    selector->window.x = x;
    selector->window.y = y;
    selector->window.w = w;
    selector->window.h = h;
    selector->window.gc = gc;
    selector->window.flags = WINDOW_NODECORATION | WINDOW_3D_WIDGET;
    selector->window.visible = 1;
    selector->window.bgcolor = BG_COLOR;
    selector->window.fgcolor = TEXT_COLOR;
    
    if(path)
    {
        __window_set_title(&selector->window, path, 0);
    }

    selector->cur_entry = -1;
    selector->selection_box_entry = -1;
    selector->flags = FILE_SELECTOR_FLAG_MULTISELECT;

    selector->window.repaint = file_selector_repaint;
    selector->window.mousedown = file_selector_mousedown;
    selector->window.mouseover = file_selector_mouseover;
    selector->window.mouseup = file_selector_mouseup;
    selector->window.mouseexit = file_selector_mouseexit;
    selector->window.unfocus = file_selector_unfocus;
    selector->window.focus = file_selector_focus;
    selector->window.destroy = file_selector_destroy;
    selector->window.keypress = file_selector_keypress;
    selector->window.keyrelease = file_selector_keyrelease;
    selector->window.size_changed = file_selector_size_changed;

    if(!(selector->vscroll = scrollbar_new(&selector->backbuf_gc,
                                           (struct window_t *)selector, 1)))
    {
        RectList_free(selector->window.clip_rects);
        free(selector->backbuf_gc.buffer);
        free(selector);
        return NULL;
    }

    if(!(selector->hscroll = scrollbar_new(&selector->backbuf_gc,
                                            (struct window_t *)selector, 0)))
    {
        RectList_free(selector->window.clip_rects);
        free(selector->backbuf_gc.buffer);
        free(selector);
        return NULL;
    }

    scrollbar_disable(selector->vscroll);
    selector->vscroll->value_change_callback = file_selector_vscroll_callback;

    scrollbar_disable(selector->hscroll);
    selector->hscroll->value_change_callback = file_selector_hscroll_callback;

    window_insert_child(parent, (struct window_t *)selector);

    return selector;
}


void file_selector_destroy(struct window_t *selector_window)
{
    struct file_selector_t *selector = SELECTOR(selector_window);

    if(selector->entries)
    {
        file_selector_free_list(selector->entries, selector->entry_count);
        selector->entries = NULL;
        selector->entry_count = 0;
    }

    file_selector_clear_filters(selector);

    widget_destroy(selector_window);
}


static inline int usable_width(struct file_selector_t *selector)
{
    // make sure not to paint over the right side vertical scrollbar!
    return selector->window.w - (selector->vscroll->window.visible ? 20 : 4);
}


static inline int usable_height(struct file_selector_t *selector)
{
    // make sure not to paint over the bottom side horizontal scrollbar!
    return selector->window.h - (selector->hscroll->window.visible ? 20 : 4);
}


void paint_entry_iconview(struct window_t *selector_window, 
                          struct file_entry_t *entry,
                          int x, int y)
{
    struct file_selector_t *selector = SELECTOR(selector_window);
    struct font_t *font = selector->backbuf_gc.font;
    int charh = char_height(font, ' ');
    int highlighted = (entry->highlighted);
    int x1;
    size_t i, pixels;
    uint32_t text_color = highlighted ? 0xFFFFFFFF : selector_window->fgcolor;
    uint32_t bg_color = highlighted ? HIGHLIGHT_COLOR : BG_COLOR;
    /*
    int want_selection_box = (selector->cur_entry >= 0 &&
                            entry == &selector->entries[selector->cur_entry] &&
                            (selector->modifiers & MODIFIER_MASK_CTRL));
    */
    int want_selection_box =
            (selector->selection_box_entry >= 0 &&
                 entry == &selector->entries[selector->selection_box_entry]);

    // paint the icon (highlighted if necessary)
    gc_blit_bitmap_highlighted(&selector->backbuf_gc, entry->icon,
                               x + ICONVIEW_LEFT_MARGIN, y, 0, 0,
                               ICONVIEW_ICONWIDTH, ICONVIEW_ICONWIDTH,
                               highlighted ? HIGHLIGHT_COLOR : 0);

    y += ICONVIEW_ICONWIDTH;

    pixels = MAX(entry->name_line_pixels[0], entry->name_line_pixels[1]);

    // draw a background box (color depends on whether the entry is highlighted)
    gc_fill_rect(&selector->backbuf_gc,
                 x + ((ICONVIEW_ENTRYWIDTH - pixels) / 2), y,
                 pixels, charh, bg_color);

    // draw the first line
    x1 = x + ((ICONVIEW_ENTRYWIDTH - entry->name_line_pixels[0]) / 2);
    i = entry->name_line_end[0] - entry->name_line_start[0];

    {
        char buf[i + 1];

        memcpy(buf, entry->name + entry->name_line_start[0], i);
        buf[i] = '\0';

        gc_draw_text(&selector->backbuf_gc, buf, x1, y, text_color, 0);
    }

    // draw the second line, if there is one
    if(entry->name_line_pixels[1])
    {
        y += charh;
        x1 = x + ((ICONVIEW_ENTRYWIDTH - entry->name_line_pixels[1]) / 2);
        i = entry->name_line_end[1] - entry->name_line_start[1];

        char buf[i + 1];

        memcpy(buf, entry->name + entry->name_line_start[1], i);
        buf[i] = '\0';

        gc_fill_rect(&selector->backbuf_gc,
                     x + ((ICONVIEW_ENTRYWIDTH - pixels) / 2), y,
                     pixels, charh, bg_color);
        gc_draw_text(&selector->backbuf_gc, buf, x1, y, text_color, 0);
    }

    // draw selection box if CTRL is pressed
    if(want_selection_box)
    {
        gc_draw_rect(&selector->backbuf_gc,
                     x + ((ICONVIEW_ENTRYWIDTH - pixels) / 2), y,
                     pixels,
                     entry->name_line_pixels[1] ? charh * 2 : charh,
                     text_color);
    }
}


void paint_entry_listview(struct window_t *selector_window, 
                          struct file_entry_t *entry,
                          int y)
{
    struct file_selector_t *selector = SELECTOR(selector_window);
    struct font_t *font = selector->backbuf_gc.font;
    Rect *rect;
    char buf[32];
    int charh = char_height(font, ' ');
    int highlighted = (entry->highlighted);
    int w = usable_width(selector);
    int y1;
    uint32_t text_color = highlighted ? 0xFFFFFFFF : selector_window->fgcolor;
    uint32_t bg_color = highlighted ? HIGHLIGHT_COLOR : BG_COLOR;
    /*
    int want_selection_box = (selector->cur_entry >= 0 &&
                            entry == &selector->entries[selector->cur_entry] &&
                            (selector->modifiers & MODIFIER_MASK_CTRL));
    */
    int want_selection_box =
            (selector->selection_box_entry >= 0 &&
                 entry == &selector->entries[selector->selection_box_entry]);

    // draw a background box (color depends on whether the entry is highlighted)
    gc_fill_rect(&selector->backbuf_gc, 2, y, w, LISTVIEW_ENTRYHEIGHT, bg_color);

    // paint the icon (highlighted if necessary)
    gc_stretch_bitmap_highlighted(&selector->backbuf_gc, entry->icon,
                                  LISTVIEW_LEFT_MARGIN, y + 2,
                                  LISTVIEW_ICONWIDTH, LISTVIEW_ICONWIDTH,
                                  0, 0,
                                  ICONVIEW_ICONWIDTH, ICONVIEW_ICONWIDTH,
                                  highlighted ? HIGHLIGHT_COLOR : 0);

    y1 = y + ((LISTVIEW_ENTRYHEIGHT - charh) / 2);

    // clip column 0 drawing rect (entry name)
    rect = selector->backbuf_gc.clipping.clip_rects->root;
    rect->left = LISTVIEW_LEFT_MARGIN + LISTVIEW_ICONWIDTH + 4;

    if(w < 150)
    {
        // not enough space, just draw the name column
        rect->right = 2 + w;

        // draw the name column
        gc_draw_text(&selector->backbuf_gc, entry->name, 
                     rect->left, y1, text_color, 0);
    }
    else
    {
        if(w < 300)
        {
            // room enough only for name and size columns
            rect->right = 2 + w - 70;

            // draw the name column
            gc_draw_text(&selector->backbuf_gc, entry->name, 
                         rect->left, y1, text_color, 0);

            // clip column 1 drawing rect (entry size)
            rect->left = rect->right;
            rect->right = rect->left + 70;

            // draw the size column
            format_size(buf, entry->file_size);
            gc_draw_text(&selector->backbuf_gc, buf, 
                         rect->left, y1, text_color, 0);
        }
        else
        {
            rect->right = 2 + w - 130 - 70;

            // draw the name column
            gc_draw_text(&selector->backbuf_gc, entry->name, 
                         rect->left, y1, text_color, 0);

            // clip column 1 drawing rect (entry size)
            rect->left = rect->right;
            rect->right = rect->left + 70;

            // draw the size column
            format_size(buf, entry->file_size);
            gc_draw_text(&selector->backbuf_gc, buf, 
                         rect->left, y1, text_color, 0);

            // clip column 2 drawing rect (entry mtime)
            rect->left = rect->right;
            rect->right = rect->left + 130;

            // draw the mtime column
            format_mtime(buf, entry->mtime);
            gc_draw_text(&selector->backbuf_gc, buf, 
                         rect->left, y1, text_color, 0);
        }
    }

    // restore clipping
    rect->left = 2;
    rect->right = selector->backbuf_gc.w - 3;

    // draw selection box if CTRL is pressed
    if(want_selection_box)
    {
        gc_draw_rect(&selector->backbuf_gc, 2, y, w, 
                        LISTVIEW_ENTRYHEIGHT, text_color);
    }
}


void paint_entry_compactview(struct window_t *selector_window, 
                             struct file_entry_t *entry,
                             int x, int y, int w)
{
    struct file_selector_t *selector = SELECTOR(selector_window);
    struct font_t *font = selector->backbuf_gc.font;
    int charh = char_height(font, ' ');
    int highlighted = (entry->highlighted);
    int y1;
    uint32_t text_color = highlighted ? 0xFFFFFFFF : selector_window->fgcolor;
    uint32_t bg_color = highlighted ? HIGHLIGHT_COLOR : BG_COLOR;
    /*
    int want_selection_box = (selector->cur_entry >= 0 &&
                            entry == &selector->entries[selector->cur_entry] &&
                            (selector->modifiers & MODIFIER_MASK_CTRL));
    */
    int want_selection_box =
            (selector->selection_box_entry >= 0 &&
                 entry == &selector->entries[selector->selection_box_entry]);

    // draw a background box (color depends on whether the entry is highlighted)
    gc_fill_rect(&selector->backbuf_gc, x, y, w, LISTVIEW_ENTRYHEIGHT, bg_color);

    // paint the icon (highlighted if necessary)
    gc_stretch_bitmap_highlighted(&selector->backbuf_gc, entry->icon,
                                  x + 4, y + 2,
                                  LISTVIEW_ICONWIDTH, LISTVIEW_ICONWIDTH,
                                  0, 0,
                                  ICONVIEW_ICONWIDTH, ICONVIEW_ICONWIDTH,
                                  highlighted ? HIGHLIGHT_COLOR : 0);

    y1 = y + ((LISTVIEW_ENTRYHEIGHT - charh) / 2);

    // draw the name column
    gc_draw_text(&selector->backbuf_gc, entry->name, 
                 x + 4 + LISTVIEW_ICONWIDTH + 4, y1, text_color, 0);

    // draw selection box if CTRL is pressed
    if(want_selection_box)
    {
        gc_draw_rect(&selector->backbuf_gc, x, y, w, 
                        LISTVIEW_ENTRYHEIGHT, text_color);
    }
}


static inline void may_draw_scrolls(struct file_selector_t *selector)
{
    if(!(selector->vscroll->flags & SCROLLBAR_FLAG_DISABLED))
    {
        selector->vscroll->window.repaint((struct window_t *)selector->vscroll, 0);
    }

    if(!(selector->hscroll->flags & SCROLLBAR_FLAG_DISABLED))
    {
        selector->hscroll->window.repaint((struct window_t *)selector->hscroll, 0);
    }
}


void file_selector_repaint(struct window_t *selector_window, 
                           int is_active_child)
{
    struct file_entry_t *entry;
    struct file_selector_t *selector = SELECTOR(selector_window);
    int x = 0;
    int y = -selector->scrolly;
    int xend = 2 + usable_width(selector);
    int yend = 2 + usable_height(selector);

    //White background
    gc_fill_rect(&selector->backbuf_gc,
                 2, 2,
                 selector_window->w - 4, selector_window->h - 4,
                 selector_window->bgcolor /* 0xFFFFFFFF */);

    if(!selector->entries)
    {
        may_draw_scrolls(selector);
        gc_blit(selector_window->gc, &selector->backbuf_gc,
                selector_window->x, selector_window->y);
        return;
    }
    
    if(selector->viewmode == FILE_SELECTOR_LIST_VIEW)
    {
        for(entry = &selector->entries[0];
            entry < &selector->entries[selector->entry_count];
            entry++)
        {
            if((y + LISTVIEW_ENTRYHEIGHT) > 0)
            {
                paint_entry_listview(selector_window, entry, y);
            }

            y += LISTVIEW_ENTRYHEIGHT;

            if(y >= yend)
            {
                break;
            }
        }
    }
    else if(selector->viewmode == FILE_SELECTOR_COMPACT_VIEW)
    {
        int max_entryw = selector->longest_entry_width;

        y = 0;
        x = -selector->scrollx;

        for(entry = &selector->entries[0]; 
            entry < &selector->entries[selector->entry_count]; 
            entry++)
        {
            if((x + max_entryw) > 0)
            {
                paint_entry_compactview(selector_window, entry, x, y, max_entryw);
            }

            y += LISTVIEW_ENTRYHEIGHT;

            // make sure the is enough space for the next entry on the same col
            if((y + LISTVIEW_ENTRYHEIGHT) > yend)
            {
                y = 0;
                x += max_entryw;
            }

            if(x >= xend)
            {
                break;
            }
        }
    }
    else
    {
        for(entry = &selector->entries[0]; 
            entry < &selector->entries[selector->entry_count]; 
            entry++)
        {
            if((y + ICONVIEW_ENTRYHEIGHT) > 0)
            {
                paint_entry_iconview(selector_window, entry, x, y);
            }

            x += ICONVIEW_ENTRYWIDTH;

            // make sure the is enough space for the next entry on the same line
            if((x + ICONVIEW_ENTRYWIDTH) > xend)
            {
                x = 0;
                y += ICONVIEW_ENTRYHEIGHT;
            }

            if(y >= yend)
            {
                break;
            }
        }
    }

    may_draw_scrolls(selector);

    gc_blit(selector_window->gc, &selector->backbuf_gc,
            selector_window->x, selector_window->y);
}


void file_selector_mouseover(struct window_t *selector_window, 
                             struct mouse_state_t *mstate)
{
    int scrollx = 0;
    int scrolly = 0;
    int old_scrolly, old_scrollx;
    struct file_selector_t *selector = SELECTOR(selector_window);

    if(!selector->entries)
    {
        return;
    }

    if(selector->vh)
    {
        if(mstate->buttons & MOUSE_VSCROLL_DOWN)
        {
            scrolly += 16;
        }

        if(mstate->buttons & MOUSE_VSCROLL_UP)
        {
            scrolly -= 16;
        }

        if(scrolly == 0)
        {
            return;
        }
    
        old_scrolly = selector->scrolly;
        selector->scrolly += scrolly;
    
        if((selector->vh - selector->scrolly) < selector_window->h)
        {
            selector->scrolly = selector->vh - selector_window->h;
        }
    
        if(selector->scrolly < 0)
        {
            selector->scrolly = 0;
        }
    
        if(old_scrolly == selector->scrolly)
        {
            return;
        }
    
        scrollbar_set_val(selector->vscroll, selector->scrolly);
        file_selector_repaint(selector_window, 
                                IS_ACTIVE_CHILD(selector_window));
        child_invalidate(selector_window);
        return;
    }

    if(selector->vw)
    {
        if(mstate->buttons & MOUSE_HSCROLL_RIGHT)
        {
            scrollx += 16;
        }

        if(mstate->buttons & MOUSE_HSCROLL_LEFT)
        {
            scrollx -= 16;
        }

        if(scrollx == 0)
        {
            return;
        }
    
        old_scrollx = selector->scrollx;
        selector->scrollx += scrollx;
    
        if((selector->vw - selector->scrollx) < selector_window->w)
        {
            selector->scrollx = selector->vw - selector_window->w;
        }
    
        if(selector->scrollx < 0)
        {
            selector->scrollx = 0;
        }
    
        if(old_scrollx == selector->scrollx)
        {
            return;
        }
    
        scrollbar_set_val(selector->hscroll, selector->scrollx);
        file_selector_repaint(selector_window, 
                                IS_ACTIVE_CHILD(selector_window));
        child_invalidate(selector_window);
    }
}


void file_selector_mousedown(struct window_t *selector_window, 
                             struct mouse_state_t *mstate)
{
    struct file_entry_t *entry, *selection_box_entry = NULL;
    struct file_selector_t *selector = SELECTOR(selector_window);
    int x = 0;
    int y = -selector->scrolly;
    int xend = 2 + usable_width(selector);
    int yend = 2 + usable_height(selector);
    int mousex = mstate->x;
    int mousey = mstate->y;
    int found = 0;
    int scrolly = selector->scrolly;
    int scrollx = selector->scrollx;
    int ctrl_down = (selector->modifiers & MODIFIER_MASK_CTRL);
    int multiselect = (selector->flags & FILE_SELECTOR_FLAG_MULTISELECT);

    if(!selector->entries)
    {
        return;
    }

    if(!mstate->left_pressed)
    {
        return;
    }

    // When the user presses CTRL and moves around with the arrow keys, we
    // draw a black selection box to show where the cursor is at. If the user
    // then selects an entry using the mouse, we need to repaint that entry
    // to remove the selection box, otherwise it just stays there.
    if(selector->selection_box_entry >= 0)
    {
        selection_box_entry = &selector->entries[selector->selection_box_entry];
        selector->selection_box_entry = -1;
    }

    if(selector->viewmode == FILE_SELECTOR_LIST_VIEW)
    {
        if(mousex >= 2 && mousex < selector_window->w - 2)
        {
            // find the entry in which the mouse was pressed
            for(entry = &selector->entries[0];
                entry < &selector->entries[selector->entry_count];
                entry++)
            {
                if(mousey >= y && mousey < y + LISTVIEW_ENTRYHEIGHT)
                {
                    selector->last_down = entry;
                    selector->cur_entry = entry - &selector->entries[0];
                    entry->highlighted = 1;

                    // scroll to selected item if needed
                    if(y < 0)
                    {
                        // entry is partially above our upper margin
                        scrolly += y;
                    }
                    else if(y + LISTVIEW_ENTRYHEIGHT > yend)
                    {
                        // entry is partially below our lower margin
                        scrolly += (y + LISTVIEW_ENTRYHEIGHT - yend);
                    }
                    else
                    {
                        // entry is within our viewable margins
                        paint_entry_listview(selector_window, entry, y);
                    }

                    found = 1;
                    //break;
                }
                else if(entry->highlighted)
                {
                    // single-selection if CTRL is not pressed or not in
                    // multi-select mode
                    if(!ctrl_down || !multiselect)
                    {
                        entry->highlighted = 0;

                        if(y < yend && (y + LISTVIEW_ENTRYHEIGHT) > 0)
                        {
                            paint_entry_listview(selector_window, entry, y);
                        }
                    }
                }
                else if(entry == selection_box_entry)
                {
                    if(y < yend && (y + LISTVIEW_ENTRYHEIGHT) > 0)
                    {
                        paint_entry_listview(selector_window, entry, y);
                    }
                }

                y += LISTVIEW_ENTRYHEIGHT;
            }
        }
    }
    else if(selector->viewmode == FILE_SELECTOR_COMPACT_VIEW)
    {
        int max_entryw = selector->longest_entry_width;

        y = 0;
        x = -selector->scrollx;

        // find the entry in which the mouse was pressed
        for(entry = &selector->entries[0];
            entry < &selector->entries[selector->entry_count];
            entry++)
        {
            if(mousex >= x && mousex < x + max_entryw &&
               mousey >= y && mousey < y + LISTVIEW_ENTRYHEIGHT)
            {
                selector->last_down = entry;
                selector->cur_entry = entry - &selector->entries[0];
                entry->highlighted = 1;

                // scroll to selected item if needed
                if(x < 0)
                {
                    // entry is partially before our left margin
                    scrollx += x;
                }
                else if(x + max_entryw > xend)
                {
                    // entry is partially after our right margin
                    scrolly += (x + max_entryw - xend);
                }
                else
                {
                    // entry is within our viewable margins
                    paint_entry_compactview(selector_window, entry, 
                                                x, y, max_entryw);
                }

                found = 1;
                //break;
            }
            else if(entry->highlighted)
            {
                // single-selection if CTRL is not pressed or not in
                // multi-select mode
                if(!ctrl_down || !multiselect)
                {
                    entry->highlighted = 0;

                    if(x < xend && (x + max_entryw) > 0)
                    {
                        paint_entry_compactview(selector_window, entry, 
                                                x, y, max_entryw);
                    }
                }
            }
            else if(entry == selection_box_entry)
            {
                if(x < xend && (x + max_entryw) > 0)
                {
                    paint_entry_compactview(selector_window, entry, 
                                            x, y, max_entryw);
                }
            }

            y += LISTVIEW_ENTRYHEIGHT;
        
            if((y + LISTVIEW_ENTRYHEIGHT) > yend)
            {
                y = 0;
                x += max_entryw;
            }
        }
    }
    else
    {
        // find the entry in which the mouse was pressed
        for(entry = &selector->entries[0];
            entry < &selector->entries[selector->entry_count];
            entry++)
        {
            if(mousex >= x && mousex < x + ICONVIEW_ENTRYWIDTH &&
               mousey >= y && mousey < y + ICONVIEW_ENTRYHEIGHT)
            {
                selector->last_down = entry;
                selector->cur_entry = entry - &selector->entries[0];
                entry->highlighted = 1;

                // scroll to selected item if needed
                if(y < 0)
                {
                    // entry is partially above our upper margin
                    scrolly += y;
                }
                else if(y + ICONVIEW_ENTRYHEIGHT > yend)
                {
                    // entry is partially below our lower margin
                    scrolly += (y + ICONVIEW_ENTRYHEIGHT - yend);
                }
                else
                {
                    // entry is within our viewable margins
                    paint_entry_iconview(selector_window, entry, x, y);
                }

                found = 1;
                //break;
            }
            else if(entry->highlighted)
            {
                // single-selection if CTRL is not pressed or not in
                // multi-select mode
                if(!ctrl_down || !multiselect)
                {
                    entry->highlighted = 0;

                    if(y < yend && (y + ICONVIEW_ENTRYHEIGHT) > 0)
                    {
                        paint_entry_iconview(selector_window, entry, x, y);
                    }
                }
            }
            else if(entry == selection_box_entry)
            {
                if(y < yend && (y + ICONVIEW_ENTRYHEIGHT) > 0)
                {
                    paint_entry_iconview(selector_window, entry, x, y);
                }
            }

            x += ICONVIEW_ENTRYWIDTH;
        
            if((x + ICONVIEW_ENTRYWIDTH) > xend)
            {
                x = 0;
                y += ICONVIEW_ENTRYHEIGHT;
            }
        }
    }
    
    if(!found)
    {
        selector->last_click_time = 0;
        selector->last_down = NULL;
        selector->last_clicked = NULL;
        selector->cur_entry = -1;
    }

    if(scrolly != selector->scrolly)
    {
        selector->scrolly = scrolly;
        scrollbar_set_val(selector->vscroll, selector->scrolly);
        file_selector_repaint(selector_window, 
                                IS_ACTIVE_CHILD(selector_window));
    }
    else if(scrollx != selector->scrollx)
    {
        selector->scrollx = scrollx;
        scrollbar_set_val(selector->hscroll, selector->scrollx);
        file_selector_repaint(selector_window, 
                                IS_ACTIVE_CHILD(selector_window));
    }
    else
    {
        gc_blit(selector_window->gc, &selector->backbuf_gc,
                selector_window->x, selector_window->y);
    }

    child_invalidate(selector_window);
}


void file_selector_mouseexit(struct window_t *selector_window)
{
}


static void process_click(struct file_selector_t *selector,
                          struct file_entry_t *entry,
                          unsigned long long click_time)
{
    // is this a click?
    if(selector->last_down == entry)
    {
        //selector->last_up = entry;
        //__asm__ __volatile__("xchg %%bx, %%bx"::);

        // is this a double-click?
        if(selector->last_clicked == entry &&
               (click_time - selector->last_click_time) < 
                                                DOUBLE_CLICK_THRESHOLD)
        {
            if(selector->entry_doubleclick_callback)
            {
                selector->entry_doubleclick_callback(selector, entry);
            }

            selector->last_click_time = 0;
            //selector->last_up = NULL;
            selector->last_down = NULL;
            selector->last_clicked = NULL;
            //selector->clicks = 0;
            return;
        }

        selector->last_click_time = click_time;
        selector->last_clicked = entry;
        //selector->clicks++;

        if(selector->entry_click_callback)
        {
            selector->entry_click_callback(selector, entry);
        }
    }
    else
    {
        selector->last_click_time = 0;
        //selector->last_up = NULL;
        selector->last_down = NULL;
        selector->last_clicked = NULL;
        //selector->clicks = 0;
    }
}


void file_selector_mouseup(struct window_t *selector_window, 
                           struct mouse_state_t *mstate)
{
    struct file_entry_t *entry;
    struct file_selector_t *selector = SELECTOR(selector_window);
    int x = 0;
    int y = -selector->scrolly;
    int xend = 2 + usable_width(selector);
    int yend = 2 + usable_height(selector);
    int mousex = mstate->x;
    int mousey = mstate->y;
    unsigned long long click_time;
    int found = 0;

    if(!selector->entries)
    {
        return;
    }

    if(!mstate->left_released)
    {
        return;
    }

    click_time = time_in_millis();
    
    if(selector->viewmode == FILE_SELECTOR_LIST_VIEW)
    {
        if(mousex >= 2 && mousex < selector_window->w - 2)
        {
            // find the entry in which the mouse was released
            for(entry = &selector->entries[0];
                entry < &selector->entries[selector->entry_count];
                entry++)
            {
                if(mousey >= y && mousey < y + LISTVIEW_ENTRYHEIGHT)
                {
                    found = 1;
                    process_click(selector, entry, click_time);
                    break;
                }

                y += LISTVIEW_ENTRYHEIGHT;
        
                if(y >= yend)
                {
                    break;
                }
            }
        }
    }
    else if(selector->viewmode == FILE_SELECTOR_COMPACT_VIEW)
    {
        int max_entryw = selector->longest_entry_width;

        y = 0;
        x = -selector->scrollx;

        // find the entry in which the mouse was released
        for(entry = &selector->entries[0];
            entry < &selector->entries[selector->entry_count];
            entry++)
        {
            if(mousex >= x && mousex < x + max_entryw &&
               mousey >= y && mousey < y + LISTVIEW_ENTRYHEIGHT)
            {
                found = 1;
                process_click(selector, entry, click_time);
                break;
            }

            y += LISTVIEW_ENTRYHEIGHT;
        
            if((y + LISTVIEW_ENTRYHEIGHT) > yend)
            {
                y = 0;
                x += max_entryw;
            }

            if(x >= xend)
            {
                break;
            }
        }
    }
    else
    {
        // find the entry in which the mouse was released
        for(entry = &selector->entries[0];
            entry < &selector->entries[selector->entry_count];
            entry++)
        {
            if(mousex >= x && mousex < x + ICONVIEW_ENTRYWIDTH &&
               mousey >= y && mousey < y + ICONVIEW_ENTRYHEIGHT)
            {
                found = 1;
                process_click(selector, entry, click_time);
                break;
            }

            x += ICONVIEW_ENTRYWIDTH;
        
            if((x + ICONVIEW_ENTRYWIDTH) > xend)
            {
                x = 0;
                y += ICONVIEW_ENTRYHEIGHT;
            }
        
            if(y >= yend)
            {
                break;
            }
        }
    }

    if(!found)
    {
        if(selector->entry_click_callback)
        {
            selector->entry_click_callback(selector, NULL);
        }
    }
}


void file_selector_unfocus(struct window_t *selector_window)
{
}


void file_selector_focus(struct window_t *selector_window)
{
    struct file_selector_t *selector = SELECTOR(selector_window);

    // update our view of the keyboard's modifier keys (used in multi-selection)
    selector->modifiers = get_modifier_keys();
}


static void set_entry_type(struct file_entry_t *entry)
{
    struct extension_t *extp;
    char *ext = file_extension(entry->name);
    
    if(!*ext)
    {
        if(S_ISDIR(entry->mode))
        {
            entry->icon = &default_dir_icon;
            return;
        }
        
        if(!(entry->mode & (S_IXUSR|S_IXGRP|S_IXOTH)))
        {
            entry->icon = &default_file_icon;
            return;
        }

        ext = ".exe";
    }
    
    for(extp = extensions; extp->ext; extp++)
    {
        if(strcasecmp(ext, extp->ext) == 0)
        {
            if(extp->icon64 == NULL)
            {
                resid_t loaded_resid;

                if(!(extp->icon64 = malloc(sizeof(struct bitmap32_t))))
                {
                    return;
                }

                extp->icon64->width = ICONVIEW_ICONWIDTH;
                extp->icon64->height = ICONVIEW_ICONWIDTH;
                extp->icon64->data = NULL;

                // get the icon from the server
                char path[128];
                
                sprintf(path, "%s/%s", DEFAULT_ICON_PATH, extp->icon_filename);
                
                if((loaded_resid = image_load(path, extp->icon64)) == INVALID_RESID)
                {
                    free(extp->icon64);
                    extp->icon64 = NULL;
                    break;
                }

                // ensure we got the right image
                if(extp->icon64->width != ICONVIEW_ICONWIDTH ||
                   extp->icon64->height != ICONVIEW_ICONWIDTH)
                {
                    free(extp->icon64);
                    extp->icon64 = NULL;
                    break;
                }
            }
            
            entry->icon = extp->icon64;
            return;
        }
    }
    
    entry->icon = S_ISDIR(entry->mode) ? &default_dir_icon : &default_file_icon;
}


/*
 * Sort directory entries before presenting them to the user.
 * The order of sorting is:
 *    - Directories come before files
 *    - Filenames are sorted alphabetically
 */
static int compare_func(const void *p1, const void *p2)
{
    struct file_entry_t *entry1 = (struct file_entry_t *)p1;
    struct file_entry_t *entry2 = (struct file_entry_t *)p2;
    
    if(S_ISDIR(entry1->mode) != S_ISDIR(entry2->mode))
    {
        // one is a dir, one is a file, return the dir
        return S_ISDIR(entry1->mode) ? -1 : 1;
    }
    
    // both are dirs or files, compare by name
    return strcmp(entry1->name, entry2->name);
}


/*
 * Match an entry name against the user-supplied filters.
 * If any filter matches, the entry is included in the final list we present
 * in the file selector. If no filter matches, the entry is excluded and not
 * presented.
 *
 * Returns 0 if the entry matches any filter, 1 if no matches.
 */
static inline int filter_out(char *name, char **filters)
{
    int i;

    for(i = 0; i < FILE_SELECTOR_MAX_FILTERS; i++)
    {
        if(filters[i] && fnmatch(filters[i], name, 0) == 0)
        {
            return 0;
        }
    }

    return 1;
}


/*
 * Check if the filters list includes at least one non-NULL entry. We only
 * check for the presence of a filter, we do not validate the contents of
 * the filter itself.
 *
 * Returns 1 if there is at least one filter, 0 otherwise.
 */
static inline int any_valid_filter(char **filters)
{
    int i;

    if(!filters)
    {
        return 0;
    }

    for(i = 0; i < FILE_SELECTOR_MAX_FILTERS; i++)
    {
        if(filters[i])
        {
            return 1;
        }
    }

    return 0;
}


static struct file_entry_t *ftree(char *path, int *entry_count,
                                  int *longestw, char **filters)
{
    char *p, *buf;
    DIR *dirp;
    struct dirent *ent;
    struct stat st;
    struct file_entry_t *entry;
    int count = 0, alloced = 0;
    int has_filters = any_valid_filter(filters);
    struct file_entry_t *entries = NULL;
    struct font_t *font = GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono;
    
    *entry_count = 0;
    *longestw = 0;
    
    if(!(buf = malloc(BUFSZ)))
    {
        return NULL;
    }

    if(!(p = malloc(PATH_MAX)))
    {
        free(buf);
        return NULL;
    }
    
    snprintf(p, PATH_MAX, "%s", path);
    
    if(stat(p, &st) == -1)
    {
        free(buf);
        free(p);
        return NULL;
    }
    
    if((dirp = opendir(p)) == NULL)
    {
        free(buf);
        free(p);
        return NULL;
    }
    
    for(;;)
    {
        errno = 0;
        
        if((ent = readdir(dirp)) == NULL)
        {
            if(errno)
            {
                closedir(dirp);
                free(buf);
                free(p);
                
                if(entries)
                {
                    free(entries);
                }

                return NULL;
            }
            
            break;
        }
        
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }
        
        snprintf(p, PATH_MAX, "%s/%s", path, ent->d_name);
        
        if(stat(p, &st) == -1)
        {
            continue;
        }

        // if there are filters, filter files and not directories
        if(!S_ISDIR(st.st_mode) &&
            has_filters && filter_out(ent->d_name, filters))
        {
            continue;
        }
        
        if(entries == NULL)
        {
            if(!(entries = malloc(32 * sizeof(struct file_entry_t))))
            {
                closedir(dirp);
                free(buf);
                free(p);
                return NULL;
            }
            
            alloced = 32;
        }
        else if(count >= alloced)
        {
            struct file_entry_t *new_entries;
            
            if(!(new_entries = realloc(entries, 
                            (alloced << 1) * sizeof(struct file_entry_t))))
            {
                /*
                 * TODO: should we return error here instead of a partial list?
                 */
                break;
            }
            
            entries = new_entries;
            alloced <<= 1;
        }
        
        entry = &entries[count++];

        A_memset(entry, 0, sizeof(struct file_entry_t));
        entry->name = strdup(ent->d_name);
        entry->mode = st.st_mode;
        entry->atime = st.st_atime;
        entry->ctime = st.st_ctime;
        entry->mtime = st.st_mtime;
        entry->file_size = st.st_size;
            
        if(entry->name)
        {
            set_entry_type(entry);
            split_two_lines(font, entry->name,
                                  entry->name_line_start,
                                  entry->name_line_end,
                                  entry->name_line_pixels,
                                  ICONVIEW_ENTRYWIDTH - 8);
        }
    }
    
    closedir(dirp);
    free(buf);
    free(p);

    // sort the names before returning them to the caller
    if(entries)
    {
        qsort(entries, count, sizeof(struct file_entry_t), compare_func);
    }

    // find the longest entry width so we do not have to go through this
    // again when painting and processing the mouse
    int pixels, longest_pixels = 0;

    for(entry = &entries[0]; entry < &entries[count]; entry++)
    {
        // we have to get the actual pixel width, as even if two entries
        // have the same number of characters, their pixel width might differ
        pixels = string_width(font, entry->name);

        if(pixels > longest_pixels)
        {
            longest_pixels = pixels;
        }
    }

    // add margins to ease calculations during painting
    *longestw = longest_pixels + LISTVIEW_ICONWIDTH + 4 + 4;
    
    *entry_count = count;

    return entries;
}


void file_selector_free_list(struct file_entry_t *entries, int entry_count)
{
    int i;
    
    for(i = 0; i < entry_count; i++)
    {
        if(entries[i].name)
        {
            free(entries[i].name);
        }
    }
    
    free(entries);
}


static inline void may_need_vscroll(struct file_selector_t *selector)
{
    scrollbar_parent_size_changed((struct window_t *)selector,
                                      (struct window_t *)selector->vscroll);

    // we may need a vertical scrollbar
    if(selector->vh > selector->window.h)
    {
        scrollbar_set_max(selector->vscroll, selector->vh - selector->window.h);
        scrollbar_set_val(selector->vscroll, selector->scrolly);
        scrollbar_set_step(selector->vscroll, 16);
        scrollbar_enable(selector->vscroll);
        selector->vscroll->window.visible = 1;
    }
    else
    {
        scrollbar_disable(selector->vscroll);
        selector->vscroll->window.visible = 0;
    }
}


static inline void may_need_hscroll(struct file_selector_t *selector)
{
    scrollbar_parent_size_changed((struct window_t *)selector,
                                      (struct window_t *)selector->hscroll);

    // we may need a horizontal scrollbar
    if(selector->vw > selector->window.w)
    {
        scrollbar_set_max(selector->hscroll, selector->vw - selector->window.w);
        scrollbar_set_val(selector->hscroll, selector->scrollx);
        scrollbar_set_step(selector->hscroll, 16);
        scrollbar_enable(selector->hscroll);
        selector->hscroll->window.visible = 1;
    }
    else
    {
        scrollbar_disable(selector->hscroll);
        selector->hscroll->window.visible = 0;
    }
}


static inline int get_entries_per_line(struct file_selector_t *selector)
{
    if(selector->viewmode != FILE_SELECTOR_ICON_VIEW)
    {
        return 1;
    }
    else
    {
        //return (selector->window.w - VSCROLLBAR_WIDTH) / ICONVIEW_ENTRYWIDTH;
        return usable_width(selector) / ICONVIEW_ENTRYWIDTH;
    }
}


static inline int get_entries_per_col(struct file_selector_t *selector)
{
    //return (selector->window.h - HSCROLLBAR_HEIGHT) / LISTVIEW_ENTRYHEIGHT;
    return usable_height(selector) / LISTVIEW_ENTRYHEIGHT;
}


static inline void reset_vh(struct file_selector_t *selector,
                            int entry_count, int entries_per_line)
{
    if(selector->viewmode == FILE_SELECTOR_LIST_VIEW)
    {
        selector->vh = entry_count * LISTVIEW_ENTRYHEIGHT;
    }
    else if(selector->viewmode == FILE_SELECTOR_COMPACT_VIEW)
    {
        selector->vh = 0;
    }
    else
    {
        selector->vh = ICONVIEW_ENTRYHEIGHT * 
                ((entry_count + entries_per_line - 1) / entries_per_line);
    }
}


static inline void reset_vw(struct file_selector_t *selector,
                            int entry_count, int entries_per_col)
{
    if(selector->viewmode != FILE_SELECTOR_COMPACT_VIEW)
    {
        selector->vw = 0;
    }
    else
    {
        selector->vw = selector->longest_entry_width * 
                ((entry_count + entries_per_col - 1) / entries_per_col);
    }
}


int file_selector_set_path(struct file_selector_t *selector, char *new_path)
{
    int entry_count, longestw;
    struct file_entry_t *entries;
    struct window_t *selector_window = (struct window_t *)selector;
    //int entries_per_line = selector_window->w / ENTRYWIDTH;
    int entries_per_line = get_entries_per_line(selector);

    if(!new_path || !*new_path)
    {
        errno = EINVAL;
        return -1;
    }

    errno = 0;
    
    if(!(entries = ftree(new_path, &entry_count, &longestw, selector->filters)))
    {
        if(errno)
        {
            return -1;
        }
        
        // dir is empty, continue ...
    }

    if(selector->entries)
    {
        file_selector_free_list(selector->entries, selector->entry_count);
    }
    
    __window_set_title(selector_window, new_path, 0);
    selector->entries = entries;
    selector->entry_count = entry_count;
    selector->longest_entry_width = longestw;

    reset_vh(selector, entry_count, entries_per_line);
    reset_vw(selector, entry_count, get_entries_per_col(selector));

    selector->scrolly = 0;
    may_need_vscroll(selector);
    selector->scrollx = 0;
    may_need_hscroll(selector);

    selector->last_click_time = 0;
    selector->last_down = NULL;
    selector->last_clicked = NULL;
    selector->cur_entry = -1;
    selector->selection_box_entry = -1;

    if(default_file_icon.data == NULL)
    {
        image_load(GENERIC_FILE_ICON_PATH, &default_file_icon);
    }

    if(default_dir_icon.data == NULL)
    {
        image_load(GENERIC_DIR_ICON_PATH, &default_dir_icon);
    }
    
    return 0;
}


char *file_selector_get_path(struct file_selector_t *selector)
{
    return selector->window.title;
}


/*
 * Get the list of selected items. If res is NULL, the count is returned
 * (useful in querying the number of items without getting the actual items).
 *
 * Returns:
 *    0 if no items are selected
 *    -1 if an error occurred
 *    otherwise the count of selected items
 *
 * The returned list should be freed by calling file_selector_free_list().
 */
int file_selector_get_selected(struct file_selector_t *selector, 
                               struct file_entry_t **res)
{
    struct file_entry_t *entry, *newentries;
    int i, count = 0;

    for(entry = &selector->entries[0];
        entry < &selector->entries[selector->entry_count];
        entry++)
    {
        if(entry->highlighted)
        {
            count++;
        }
    }
    
    if(count == 0)
    {
        return 0;
    }
    
    // only want the count
    if(res == NULL)
    {
        return count;
    }
    
    if(!(newentries = malloc(count * sizeof(struct file_entry_t))))
    {
        return -1;
    }
    
    i = 0;

    for(entry = &selector->entries[0];
        entry < &selector->entries[selector->entry_count];
        entry++)
    {
        if(entry->highlighted)
        {
            A_memset(&newentries[i], 0, sizeof(struct file_entry_t));
            newentries[i].name = strdup(entry->name);
            //newentries[i].namelen = entry->namelen;
            newentries[i].mode = entry->mode;
            newentries[i].atime = entry->atime;
            newentries[i].ctime = entry->ctime;
            newentries[i].mtime = entry->mtime;
            newentries[i].file_size = entry->file_size;
            i++;
        }
    }
    
    *res = newentries;
    return count;
}


static void unselect_all(struct file_selector_t *selector)
{
    struct file_entry_t *entry;

    for(entry = &selector->entries[0];
        entry < &selector->entries[selector->entry_count];
        entry++)
    {
        entry->highlighted = 0;
    }

    selector->selection_box_entry = -1;
}


void file_selector_select_all(struct file_selector_t *selector)
{
    struct file_entry_t *entry;

    if(!selector || !selector->entries)
    {
        return;
    }

    for(entry = &selector->entries[0];
        entry < &selector->entries[selector->entry_count];
        entry++)
    {
        entry->highlighted = 1;
    }

    selector->selection_box_entry = -1;

    if(selector->selection_change_callback)
    {
        selector->selection_change_callback(selector);
    }
}


void file_selector_unselect_all(struct file_selector_t *selector)
{
    if(!selector || !selector->entries)
    {
        return;
    }

    unselect_all(selector);

    if(selector->selection_change_callback)
    {
        selector->selection_change_callback(selector);
    }
}


static void scroll_to_cur(struct window_t *selector_window)
{
    struct file_selector_t *selector = SELECTOR(selector_window);
    struct file_entry_t *entry;
    struct file_entry_t *cur_entry = &selector->entries[selector->cur_entry];
    int x = 0;
    int y = 0;
    int xend = 2 + usable_width(selector);

    if(!selector->entries || selector->cur_entry < 0)
    {
        return;
    }
    
    if(selector->viewmode == FILE_SELECTOR_LIST_VIEW)
    {
        // find the entry in which the mouse was pressed
        for(entry = &selector->entries[0];
            entry < &selector->entries[selector->entry_count];
            entry++)
        {
            if(cur_entry == entry)
            {
                if(y < selector->scrolly)
                {
                    selector->scrolly = y;
                    break;
                }

                if(y + LISTVIEW_ENTRYHEIGHT >= 
                        selector->scrolly + selector_window->h)
                {
                    selector->scrolly = (y + LISTVIEW_ENTRYHEIGHT) - 
                                                    selector_window->h;
                    break;
                }
            }

            y += LISTVIEW_ENTRYHEIGHT;
        }
    }
    else if(selector->viewmode == FILE_SELECTOR_COMPACT_VIEW)
    {
        int max_entryw = selector->longest_entry_width;
        int yend = 2 + usable_height(selector);

        // find the entry in which the mouse was pressed
        for(entry = &selector->entries[0];
            entry < &selector->entries[selector->entry_count];
            entry++)
        {
            if(cur_entry == entry)
            {
                if(x < selector->scrollx)
                {
                    selector->scrollx = x;
                    break;
                }

                if(x + max_entryw >= 
                        selector->scrollx + selector_window->w)
                {
                    selector->scrollx = (x + max_entryw) - 
                                                selector_window->w;
                    break;
                }
            }

            y += LISTVIEW_ENTRYHEIGHT;
        
            if((y + LISTVIEW_ENTRYHEIGHT) > yend)
            {
                y = 0;
                x += max_entryw;
            }
        }
    }
    else
    {
        // find the entry in which the mouse was pressed
        for(entry = &selector->entries[0];
            entry < &selector->entries[selector->entry_count];
            entry++)
        {
            if(cur_entry == entry)
            {
                if(y < selector->scrolly)
                {
                    selector->scrolly = y;
                    break;
                }

                if(y + ICONVIEW_ENTRYHEIGHT >= 
                        selector->scrolly + selector_window->h)
                {
                    selector->scrolly = (y + ICONVIEW_ENTRYHEIGHT) - 
                                                    selector_window->h;
                    break;
                }
            }

            x += ICONVIEW_ENTRYWIDTH;
        
            if((x + ICONVIEW_ENTRYWIDTH) > xend)
            {
                x = 0;
                y += ICONVIEW_ENTRYHEIGHT;
            }
        }
    }
}


#define SCROLL_AND_REPAINT(selector_window) \
    scroll_to_cur(selector_window); \
    scrollbar_set_val(selector->vscroll, selector->scrolly); \
    scrollbar_set_val(selector->hscroll, selector->scrollx); \
    file_selector_repaint(selector_window, IS_ACTIVE_CHILD(selector_window)); \
    child_invalidate(selector_window);


int file_selector_keypress(struct window_t *selector_window, 
                           char code, char modifiers)
{
    struct file_selector_t *selector = SELECTOR(selector_window);
    int entries_per_line;
    int ctrl_down = (selector->modifiers & MODIFIER_MASK_CTRL);
    int multiselect = (selector->flags & FILE_SELECTOR_FLAG_MULTISELECT);

    // Don't handle ALT-key combinations, as these are usually menu shortcuts.
    if(modifiers & MODIFIER_MASK_ALT)
    {
        return 0;
    }

    switch(code)
    {
        case KEYCODE_LCTRL:
        case KEYCODE_RCTRL:
            selector->modifiers |= MODIFIER_MASK_CTRL;
            return 1;

        case KEYCODE_LSHIFT:
        case KEYCODE_RSHIFT:
            selector->modifiers |= MODIFIER_MASK_SHIFT;
            return 1;

        case KEYCODE_LALT:
        case KEYCODE_RALT:
            selector->modifiers |= MODIFIER_MASK_ALT;
            return 1;

        case KEYCODE_HOME:
            if(selector->cur_entry <= 0 || !selector->entries)
            {
                return 1;
            }
            
            // multi-selection if SHIFT is pressed
            if((selector->modifiers & MODIFIER_MASK_SHIFT) && multiselect)
            {
                while(selector->cur_entry > 0)
                {
                    selector->entries[--selector->cur_entry].highlighted = 1;
                }

                //selector->cur_entry = 0;
            }
            // single-selection if CTRL is not pressed or not in
            // multi-select mode
            else if(!ctrl_down || !multiselect)
            {
                unselect_all(selector);
                selector->cur_entry = 0;
                selector->entries[selector->cur_entry].highlighted = 1;
            }

            selector->selection_box_entry = -1;
            SCROLL_AND_REPAINT(selector_window);

            if(selector->selection_change_callback)
            {
                selector->selection_change_callback(selector);
            }

            return 1;

        case KEYCODE_END:
            if(!selector->entries)
            {
                return 1;
            }
            
            // multi-selection if SHIFT is pressed
            if((selector->modifiers & MODIFIER_MASK_SHIFT) && multiselect)
            {
                if(selector->cur_entry < 0)
                {
                    selector->cur_entry = 0;
                    selector->entries[0].highlighted = 1;
                }

                while(selector->cur_entry < selector->entry_count)
                {
                    selector->entries[++selector->cur_entry].highlighted = 1;
                }

                //selector->cur_entry = selector->entry_count - 1;
            }
            // single-selection if CTRL is not pressed or not in
            // multi-select mode
            else if(!ctrl_down || !multiselect)
            {
                unselect_all(selector);
                selector->cur_entry = selector->entry_count - 1;
                selector->entries[selector->cur_entry].highlighted = 1;
            }

            selector->selection_box_entry = -1;
            SCROLL_AND_REPAINT(selector_window);

            if(selector->selection_change_callback)
            {
                selector->selection_change_callback(selector);
            }

            return 1;

        case KEYCODE_UP:
            // we handle UP with SHIFT but no other modifier keys
            /*
            if(selector->modifiers & ~MODIFIER_MASK_SHIFT)
            {
                return 0;
            }
            */
            
            if(selector->cur_entry <= 0 || !selector->entries)
            {
                return 1;
            }

            //entries_per_line = selector_window->w / ENTRYWIDTH;
            entries_per_line = get_entries_per_line(selector);
            
            if(selector->cur_entry - entries_per_line < 0)
            {
                return 1;
            }

            // multi-selection if SHIFT is pressed
            if((selector->modifiers & MODIFIER_MASK_SHIFT) && multiselect)
            {
                while(entries_per_line--)
                {
                    selector->entries[--selector->cur_entry].highlighted = 1;
                }

                selector->selection_box_entry = -1;
            }
            else
            {
                selector->cur_entry -= entries_per_line;

                // single-selection if CTRL is not pressed or not in
                // multi-select mode
                if(!ctrl_down || !multiselect)
                {
                    unselect_all(selector);
                    selector->entries[selector->cur_entry].highlighted = 1;
                    selector->selection_box_entry = -1;
                }
                else
                {
                    selector->selection_box_entry = selector->cur_entry;
                }
            }

            SCROLL_AND_REPAINT(selector_window);
            
            if(selector->selection_change_callback)
            {
                selector->selection_change_callback(selector);
            }

            return 1;

        case KEYCODE_DOWN:
            // we handle DOWN with no modifier keys
            /*
            if(selector->modifiers)
            {
                return 0;
            }
            */
            
            if(!selector->entries)
            {
                return 1;
            }

            //entries_per_line = selector_window->w / ENTRYWIDTH;
            entries_per_line = get_entries_per_line(selector);

            if(selector->cur_entry == -1)
            {
                selector->cur_entry = 0;
                /*
                if((selector->entry_count < entries_per_line) ||
                   (selector->viewmode != FILE_SELECTOR_ICON_VIEW &&
                    selector->entry_count == 1))
                {
                    selector->cur_entry = 0;
                }
                else
                {
                    selector->cur_entry = entries_per_line;
                }
                */
                
                selector->entries[selector->cur_entry].highlighted = 1;
                selector->selection_box_entry = -1;
            }
            else
            {
                if((selector->cur_entry + entries_per_line) >= 
                                                    selector->entry_count)
                {
                    if(selector->viewmode != FILE_SELECTOR_ICON_VIEW)
                    {
                        entries_per_line = 0;
                    }
                    else
                    {
                        entries_per_line = selector->entry_count - 
                                           selector->cur_entry;
                    }

                    selector->cur_entry = selector->entry_count - 1;
                }
                else
                {
                    selector->cur_entry += entries_per_line;
                }

                // multi-selection if SHIFT is pressed
                if((selector->modifiers & MODIFIER_MASK_SHIFT) && multiselect)
                {
                    while(entries_per_line--)
                    {
                        selector->entries[selector->cur_entry - 
                                           entries_per_line].highlighted = 1;
                    }

                    selector->selection_box_entry = -1;
                }
                else
                {
                    // single-selection if CTRL is not pressed or not in
                    // multi-select mode
                    if(!ctrl_down || !multiselect)
                    {
                        unselect_all(selector);
                        selector->entries[selector->cur_entry].highlighted = 1;
                        selector->selection_box_entry = -1;
                    }
                    else
                    {
                        selector->selection_box_entry = selector->cur_entry;
                    }
                }
            }

            SCROLL_AND_REPAINT(selector_window);

            if(selector->selection_change_callback)
            {
                selector->selection_change_callback(selector);
            }

            return 1;

        case KEYCODE_RIGHT:
            // we handle RIGHT with no modifier keys
            /*
            if(selector->modifiers)
            {
                return 0;
            }
            */

            // right arrow does nothing in list mode
            if(selector->viewmode == FILE_SELECTOR_LIST_VIEW)
            {
                return 1;
            }
            
            if(selector->cur_entry == -1)
            {
                if(!selector->entries)
                {
                    return 1;
                }

                selector->cur_entry = 0;
                selector->entries[selector->cur_entry].highlighted = 1;
                selector->selection_box_entry = -1;
            }
            else
            {
                if(selector->cur_entry == selector->entry_count - 1)
                {
                    return 1;
                }
                
                // compact view is handled differently to icon view
                if(selector->viewmode == FILE_SELECTOR_COMPACT_VIEW)
                {
                    int entries_per_col = get_entries_per_col(selector);

                    if((selector->cur_entry + entries_per_col) >= 
                                                    selector->entry_count)
                    {
                        entries_per_col = selector->entry_count - 
                                                selector->cur_entry - 1;
                        selector->cur_entry = selector->entry_count - 1;
                    }
                    else
                    {
                        selector->cur_entry += entries_per_col;
                    }

                    // multi-selection if SHIFT is pressed
                    if((selector->modifiers & MODIFIER_MASK_SHIFT) && multiselect)
                    {
                        while(entries_per_col--)
                        {
                            selector->entries[selector->cur_entry - 
                                               entries_per_col].highlighted = 1;
                        }

                        selector->selection_box_entry = -1;
                    }
                    else
                    {
                        // single-selection if CTRL is not pressed or not in
                        // multi-select mode
                        if(!ctrl_down || !multiselect)
                        {
                            unselect_all(selector);
                            selector->entries[selector->cur_entry].highlighted = 1;
                            selector->selection_box_entry = -1;
                        }
                        else
                        {
                            selector->selection_box_entry = selector->cur_entry;
                        }
                    }
                }
                else
                {
                    // multi-selection if SHIFT is pressed
                    if((selector->modifiers & MODIFIER_MASK_SHIFT) && multiselect)
                    {
                        selector->entries[selector->cur_entry + 1].highlighted = 1;
                        selector->selection_box_entry = -1;
                    }
                    // single-selection if CTRL is not pressed or not in
                    // multi-select mode
                    else if(!ctrl_down || !multiselect)
                    {
                        unselect_all(selector);
                        //selector->cur_entry[0].highlighted = 0;
                        selector->entries[selector->cur_entry + 1].highlighted = 1;
                        selector->selection_box_entry = -1;
                    }
                    else
                    {
                        selector->selection_box_entry = selector->cur_entry + 1;
                    }

                    selector->cur_entry++;
                }
            }

            SCROLL_AND_REPAINT(selector_window);

            if(selector->selection_change_callback)
            {
                selector->selection_change_callback(selector);
            }

            return 1;

        case KEYCODE_LEFT:
            // we handle LEFT with no modifier keys
            /*
            if(selector->modifiers)
            {
                return 0;
            }
            */

            if(selector->viewmode == FILE_SELECTOR_LIST_VIEW)
            {
                return 1;
            }
            
            if(selector->cur_entry <= 0 || !selector->entries)
            {
                return 1;
            }

            // compact view is handled differently to icon view
            if(selector->viewmode == FILE_SELECTOR_COMPACT_VIEW)
            {
                int entries_per_col = get_entries_per_col(selector);

                if(selector->cur_entry - entries_per_col < 0)
                {
                    return 1;
                }

                // multi-selection if SHIFT is pressed
                if((selector->modifiers & MODIFIER_MASK_SHIFT) && multiselect)
                {
                    while(entries_per_col--)
                    {
                        selector->entries[--selector->cur_entry].highlighted = 1;
                    }

                    selector->selection_box_entry = -1;
                }
                else
                {
                    selector->cur_entry -= entries_per_col;

                    // single-selection if CTRL is not pressed or not in
                    // multi-select mode
                    if(!ctrl_down || !multiselect)
                    {
                        unselect_all(selector);
                        selector->entries[selector->cur_entry].highlighted = 1;
                        selector->selection_box_entry = -1;
                    }
                    else
                    {
                        selector->selection_box_entry = selector->cur_entry;
                    }
                }
            }
            else
            {
                selector->cur_entry--;
            
                // multi-selection if SHIFT is pressed
                if((selector->modifiers & MODIFIER_MASK_SHIFT) && multiselect)
                {
                    selector->entries[selector->cur_entry].highlighted = 1;
                    selector->selection_box_entry = -1;
                }
                // single-selection if CTRL is not pressed or not in
                // multi-select mode
                else if(!ctrl_down || !multiselect)
                {
                    unselect_all(selector);
                    selector->entries[selector->cur_entry].highlighted = 1;
                    //selector->cur_entry[0].highlighted = 1;
                    ////selector->cur_entry[1].highlighted = 0;
                    selector->selection_box_entry = -1;
                }
                else
                {
                    selector->selection_box_entry = selector->cur_entry;
                }
            }

            SCROLL_AND_REPAINT(selector_window);

            if(selector->selection_change_callback)
            {
                selector->selection_change_callback(selector);
            }

            return 1;

        case KEYCODE_SPACE:
            if(selector->cur_entry <= 0 || !selector->entries)
            {
                return 1;
            }

            // toggle multi-selection if CTRL is pressed and multiselecting
            if(ctrl_down && multiselect)
            //if(selector->modifiers & MODIFIER_MASK_CTRL)
            {
                selector->entries[selector->cur_entry].highlighted =
                          !selector->entries[selector->cur_entry].highlighted;
                SCROLL_AND_REPAINT(selector_window);

                if(selector->selection_change_callback)
                {
                    selector->selection_change_callback(selector);
                }
            }

            return 1;

        case KEYCODE_ENTER:
            // we handle ENTER with no modifier keys
            if(selector->modifiers)
            {
                return 0;
            }

            if(selector->cur_entry >= 0 &&
               selector->entry_doubleclick_callback &&
               file_selector_get_selected(selector, NULL) == 1)
            {
                selector->entry_doubleclick_callback(selector,
                                    &selector->entries[selector->cur_entry]);
            }
            
            return 1;
    }

    return 0;
}


int file_selector_keyrelease(struct window_t *selector_window, 
                             char code, char modifiers)
{
    struct file_selector_t *selector = SELECTOR(selector_window);

    switch(code)
    {
        case KEYCODE_LCTRL:
        case KEYCODE_RCTRL:
            selector->modifiers &= ~MODIFIER_MASK_CTRL;
            return 1;

        case KEYCODE_LSHIFT:
        case KEYCODE_RSHIFT:
            selector->modifiers &= ~MODIFIER_MASK_SHIFT;
            return 1;

        case KEYCODE_LALT:
        case KEYCODE_RALT:
            selector->modifiers &= ~MODIFIER_MASK_ALT;
            return 1;
    }

    return 0;
}


void file_selector_size_changed(struct window_t *window)
{
    struct file_selector_t *selector = SELECTOR(window);
    //int entries_per_line = window->w / ENTRYWIDTH;
    int entries_per_line = get_entries_per_line(selector);

    if(selector->backbuf_gc.w != window->w ||
       selector->backbuf_gc.h != window->h)
    {
        if(gc_realloc_backbuf(window->gc, &selector->backbuf_gc,
                              window->w, window->h) < 0)
        {
            /*
             * NOTE: this should be a fatal error.
             */
            return;
        }

        draw_inverted_3d_border(&selector->backbuf_gc, 0, 0, window->w, window->h);
        reset_backbuf_clipping(selector);
    }

    window->clip_rects->root->top = window->y + 1;
    window->clip_rects->root->left = window->x + 1;
    window->clip_rects->root->bottom = window->y + window->h - 2;
    window->clip_rects->root->right = window->x + window->w - 2;

    reset_vh(selector, selector->entry_count, entries_per_line);
    reset_vw(selector, selector->entry_count, get_entries_per_col(selector));

    may_need_vscroll(selector);
    may_need_hscroll(selector);
}


void file_selector_vscroll_callback(struct window_t *parent, 
                                    struct scrollbar_t *sbar)
{
    struct file_selector_t *selector = SELECTOR(parent);

    if(sbar->val != selector->scrolly)
    {
        selector->scrolly = sbar->val;
        file_selector_repaint(parent, IS_ACTIVE_CHILD(parent));
        child_invalidate(parent);
    }
}


void file_selector_hscroll_callback(struct window_t *parent, 
                                    struct scrollbar_t *sbar)
{
    struct file_selector_t *selector = SELECTOR(parent);

    if(sbar->val != selector->scrollx)
    {
        selector->scrollx = sbar->val;
        file_selector_repaint(parent, IS_ACTIVE_CHILD(parent));
        child_invalidate(parent);
    }
}


void file_selector_set_viewmode(struct file_selector_t *selector, int mode)
{
    if(mode < FILE_SELECTOR_ICON_VIEW || mode > FILE_SELECTOR_COMPACT_VIEW)
    {
        return;
    }

    if(mode == selector->viewmode)
    {
        return;
    }

    selector->viewmode = mode;

    if(selector->entries)
    {
        unselect_all(selector);
    }

    reset_vh(selector, selector->entry_count, get_entries_per_line(selector));
    selector->scrolly = 0;
    may_need_vscroll(selector);

    reset_vw(selector, selector->entry_count, get_entries_per_col(selector));
    selector->scrollx = 0;
    may_need_hscroll(selector);

    selector->last_click_time = 0;
    selector->last_down = NULL;
    selector->last_clicked = NULL;
    selector->cur_entry = -1;
}


void file_selector_reload_entries(struct file_selector_t *selector)
{
    int entry_count, longestw;
    struct file_entry_t *entries;
    int entries_per_line = get_entries_per_line(selector);

    errno = 0;
    
    if(!(entries = ftree(selector->window.title, &entry_count, 
                         &longestw, selector->filters)))
    {
        if(errno)
        {
            return;
        }
        
        // dir is empty, continue ...
    }

    if(selector->entries)
    {
        file_selector_free_list(selector->entries, selector->entry_count);
    }
    
    selector->entries = entries;
    selector->entry_count = entry_count;
    selector->longest_entry_width = longestw;

    reset_vh(selector, entry_count, entries_per_line);
    reset_vw(selector, entry_count, get_entries_per_col(selector));

    selector->scrolly = 0;
    may_need_vscroll(selector);
    selector->scrollx = 0;
    may_need_hscroll(selector);

    selector->last_click_time = 0;
    selector->last_down = NULL;
    selector->last_clicked = NULL;
    selector->cur_entry = -1;
    selector->selection_box_entry = -1;
}


void file_selector_clear_filters(struct file_selector_t *selector)
{
    int i;

    for(i = 0; i < FILE_SELECTOR_MAX_FILTERS; i++)
    {
        if(selector->filters[i])
        {
            free(selector->filters[i]);
            selector->filters[i] = NULL;
        }
    }
}


void file_selector_add_filter(struct file_selector_t *selector, char *filter)
{
    int i;

    for(i = 0; i < FILE_SELECTOR_MAX_FILTERS; i++)
    {
        if(selector->filters[i] == NULL)
        {
            selector->filters[i] = strdup(filter);
            break;
        }
    }
}

