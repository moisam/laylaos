/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: file-selector-inlines.c
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
 *  \file file-selector-inlines.c
 *
 *  Inlined functions used by the file selector and gallery view widgets.
 */


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

static inline void file_selector_reset_scrolls(struct file_selector_t *selector)
{
    int entry_count = selector->entry_count;

    reset_vh(selector, entry_count, get_entries_per_line(selector));
    reset_vw(selector, entry_count, get_entries_per_col(selector));
    may_need_vscroll(selector);
    may_need_hscroll(selector);
}

static inline void file_selector_reset_click_count(struct file_selector_t *selector)
{
    selector->last_click_time = 0;
    selector->last_down = NULL;
    selector->last_clicked = NULL;
    selector->cur_entry = -1;
}

