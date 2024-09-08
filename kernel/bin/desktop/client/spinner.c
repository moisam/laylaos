/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: spinner.c
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
 *  \file spinner.c
 *
 *  The implementation of a spinner widget.
 */

#include <stdlib.h>
#include <string.h>
#include "../include/client/spinner.h"
#include "../include/client/inputbox.h"
#include "../include/clipboard.h"
#include "../include/cursor.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"
#include "../include/keys.h"

#include "inlines.c"

#define ARROW_WIDTH                     16
#define ARROW_HEIGHT                    13

#define RIGHT_INNER_MARGIN              (4 + ARROW_WIDTH)
#define LEFT_INNER_MARGIN               4
#define TOP_INNER_MARGIN                4

#define SPINNER_MIN_WIDTH               (LEFT_INNER_MARGIN + \
                                         RIGHT_INNER_MARGIN + 20)

#define GLOB                            __global_gui_data

#define TEMPLATE_BGCOLOR                0xCDCFD4FF
#define TEMPLATE_TEXTCOLOR              0x222226FF

#define _B                              TEMPLATE_BGCOLOR,
#define _T                              TEMPLATE_TEXTCOLOR,
#define _L                              GLOBAL_LIGHT_SIDE_COLOR,
#define _D                              GLOBAL_DARK_SIDE_COLOR,

static uint32_t arrow_up_img_template[] =
{
    _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D
    _B _L _L _L _L _L _L _L _L _L _L _L _L _L _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _T _T _B _B _B _B _B _D _D
    _B _L _B _B _B _B _T _T _T _T _B _B _B _B _D _D
    _B _L _B _B _B _T _T _T _T _T _T _B _B _B _D _D
    _B _L _B _B _T _T _T _T _T _T _T _T _B _B _D _D
    _B _L _B _T _T _T _T _T _T _T _T _T _T _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _D _D _D _D _D _D _D _D _D _D _D _D _D _D
    _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D
};

static uint32_t arrow_down_img_template[] =
{
    _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _D
    _B _L _L _L _L _L _L _L _L _L _L _L _L _L _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _T _T _T _T _T _T _T _T _T _T _B _D _D
    _B _L _B _B _T _T _T _T _T _T _T _T _B _B _D _D
    _B _L _B _B _B _T _T _T _T _T _T _B _B _B _D _D
    _B _L _B _B _B _B _T _T _T _T _B _B _B _B _D _D
    _B _L _B _B _B _B _B _T _T _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _B _B _B _B _B _B _B _B _B _B _B _B _D _D
    _B _L _D _D _D _D _D _D _D _D _D _D _D _D _D _D
    _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D _D
};

static uint32_t arrow_up_img[ARROW_WIDTH * ARROW_HEIGHT * 4];
static uint32_t arrow_down_img[ARROW_WIDTH * ARROW_HEIGHT * 4];

#undef _B
#undef _T
#undef _D
#undef _L

static struct bitmap32_t arrow_up =
{
    .width = ARROW_WIDTH, .height = ARROW_HEIGHT, .data = arrow_up_img,
};

static struct bitmap32_t arrow_down =
{
    .width = ARROW_WIDTH, .height = ARROW_HEIGHT, .data = arrow_down_img,
};


static void show_caret(struct window_t *spinner_window);
static void hide_caret(struct window_t *spinner_window);


static inline void reset_backbuf_clipping(struct spinner_t *spinner)
{
    Rect *rect;

    // account for the border
    rect = spinner->backbuf_gc.clipping.clip_rects->root;

    rect->top = 2;
    rect->left = 2;
    rect->bottom = spinner->backbuf_gc.h - 2;
    rect->right = spinner->backbuf_gc.w - 2;
}


struct spinner_t *spinner_new(struct gc_t *gc, struct window_t *parent,
                                               int x, int y, int w)
{
    struct spinner_t *spinner;
    Rect *rect;

    if(!(spinner = malloc(sizeof(struct spinner_t))))
    {
        return NULL;
    }

    memset(spinner, 0, sizeof(struct spinner_t));

    if(w < SPINNER_MIN_WIDTH)
    {
        w = SPINNER_MIN_WIDTH;
    }

    if(gc_alloc_backbuf(gc, &spinner->backbuf_gc, w, INPUTBOX_HEIGHT) < 0)
    {
        free(spinner);
        return NULL;
    }

    gc_set_font(&spinner->backbuf_gc, GLOB.sysfont.data ? &GLOB.sysfont :
                                                          &GLOB.mono);

    // All subsequent drawing on the spinner's canvas will be clipped to a
    // 1-pixel border. If we draw the border later (e.g. in spinner_repaint())
    // we will fail, as the border will be clipped and will not be drawn.
    // A workaround would be to temporarily unclip the clipping and draw the
    // border, but this is complicated and messy. Instead, we draw the border
    // here, once and for all, and we never need to worry about it again.
    gc_fill_rect(&spinner->backbuf_gc, 1, 1,
                         w - 2, INPUTBOX_HEIGHT - 2, 
                         GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR]);
    draw_inverted_3d_border(&spinner->backbuf_gc, 0, 0, w, INPUTBOX_HEIGHT);
    reset_backbuf_clipping(spinner);

    if(!(spinner->window.clip_rects = RectList_new()))
    {
        free(spinner->backbuf_gc.buffer);
        free(spinner);
        return NULL;
    }

    if(parent->main_menu)
    {
        y += MENU_HEIGHT;
    }

    if(!(rect = Rect_new(y, x, y + INPUTBOX_HEIGHT - 1,  x + w - 1)))
    {
        RectList_free(spinner->window.clip_rects);
        free(spinner->backbuf_gc.buffer);
        free(spinner);
        return NULL;
    }

    RectList_add(spinner->window.clip_rects, rect);
    spinner->window.type = WINDOW_TYPE_SPINNER;
    spinner->window.x = x;
    spinner->window.y = y;
    spinner->window.w = w;
    spinner->window.h = INPUTBOX_HEIGHT;
    spinner->window.gc = gc;
    spinner->window.flags = WINDOW_NODECORATION;
    spinner->window.visible = 1;
    spinner->window.bgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR];
    spinner->window.fgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_TEXTCOLOR];

    spinner->val = 0;
    __window_set_title(&spinner->window, "0", 0);
    spinner->vw = string_width_no_kerning(spinner->window.gc->font,
                                          spinner->window.title);

    spinner->scrollx = LEFT_INNER_MARGIN;
    spinner->caretx = LEFT_INNER_MARGIN;
    spinner->carety = TOP_INNER_MARGIN;
    spinner->careth = spinner->window.h - (TOP_INNER_MARGIN * 2);

    spinner->window.repaint = spinner_repaint;
    spinner->window.mousedown = spinner_mousedown;
    spinner->window.mouseover = spinner_mouseover;
    spinner->window.mouseup = spinner_mouseup;
    spinner->window.mouseexit = spinner_mouseexit;
    spinner->window.unfocus = spinner_unfocus;
    spinner->window.focus = spinner_focus;
    spinner->window.destroy = spinner_destroy;
    spinner->window.keypress = spinner_keypress;
    //spinner->window.keyrelease = spinner_keyrelease;
    spinner->window.size_changed = spinner_size_changed;
    spinner->window.theme_changed = spinner_theme_changed;

    window_insert_child(parent, (struct window_t *)spinner);

    return spinner;
}


void spinner_destroy(struct window_t *spinner_window)
{
    // this will free the title, the clip_rects list, and the widget struct
    widget_destroy(spinner_window);
}


void spinner_repaint(struct window_t *spinner_window, int is_active_child)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;
    int x = spinner->scrollx;
    int xend = spinner_window->w - RIGHT_INNER_MARGIN;
    int selecting = spinner->select_end != spinner->select_start &&
                    is_active_child;
    size_t i, charw;
    char buf[2] = { 0, 0 };

    void (*func)(struct gc_t *, struct clipping_t *, char *, int, int,
                                uint32_t, char);

    if(spinner_window->gc->font->flags & FONT_FLAG_TRUE_TYPE)
    {
        func = gc_draw_text_clipped_ttf;
    }
    else
    {
        func = gc_draw_text_clipped;
    }

    //White background
    gc_fill_rect(&spinner->backbuf_gc, 1, 1,
                 spinner_window->w - 2, spinner_window->h - 2,
                 spinner_window->bgcolor);

    //Draw the text within the box
    if(!spinner_window->title)
    {
        char val[32];

        sprintf(val, "%d", spinner->val);
        __window_set_title(&spinner->window, val, 0);
        spinner->vw = string_width_no_kerning(spinner->window.gc->font,
                                              spinner->window.title);
    }

    for(i = 0; i < spinner_window->title_len; i++)
    {
        buf[0] = spinner_window->title[i];
        charw = char_width(spinner_window->gc->font, buf[0]);

        if(x + charw >= LEFT_INNER_MARGIN)
        {
            if(selecting && i >= spinner->select_start &&
                            i < spinner->select_end)
            {
                gc_fill_rect(&spinner->backbuf_gc,
                             x, TOP_INNER_MARGIN,
                             charw,
                             spinner_window->h - (TOP_INNER_MARGIN * 2),
                             GLOB.themecolor[THEME_COLOR_INPUTBOX_SELECT_BGCOLOR]);

                func(&spinner->backbuf_gc,
                     &spinner->backbuf_gc.clipping, buf,
                     x, TOP_INNER_MARGIN,
                     GLOB.themecolor[THEME_COLOR_INPUTBOX_SELECT_TEXTCOLOR], 0);
            }
            else
            {
                func(&spinner->backbuf_gc,
                     &spinner->backbuf_gc.clipping, buf,
                     x, TOP_INNER_MARGIN,
                     spinner_window->fgcolor, 0);
            }
        }

        x += charw;

        if(x >= xend)
        {
            break;
        }
    }

    // draw the arrows on the right
    gc_blit_bitmap(&spinner->backbuf_gc, &arrow_up,
                   spinner_window->w - ARROW_WIDTH - 1, 1,
                   0, 0, 16, 13);

    gc_blit_bitmap(&spinner->backbuf_gc, &arrow_down,
                   spinner_window->w - ARROW_WIDTH - 1,
                   spinner_window->h - 14,
                   0, 0, 16, 13);

    gc_blit(spinner_window->gc, &spinner->backbuf_gc,
            spinner_window->x, spinner_window->y);

    if(is_active_child)
    {
        spinner->flags |= INPUTBOX_FLAG_CARET_SHOWN;
        show_caret(spinner_window);
    }
    else
    {
        spinner->flags &= ~INPUTBOX_FLAG_CARET_SHOWN;
    }
}


static inline int usable_width(struct window_t *spinner_window)
{
    return (spinner_window->w - LEFT_INNER_MARGIN - RIGHT_INNER_MARGIN);
}


static void scroll_to_start(struct window_t *spinner_window)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;

    spinner->scrollx = LEFT_INNER_MARGIN;
    spinner->caretx = LEFT_INNER_MARGIN;
}


static void scroll_to_end(struct window_t *spinner_window)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;
    int w = usable_width(spinner_window);

    if(spinner->vw <= w)
    {
        spinner->scrollx = LEFT_INNER_MARGIN;
        spinner->caretx = spinner->vw + LEFT_INNER_MARGIN;
    }
    else
    {
        spinner->scrollx = spinner_window->w - RIGHT_INNER_MARGIN - spinner->vw;
        spinner->caretx = spinner_window->w - RIGHT_INNER_MARGIN;
    }
}


static size_t caretx_to_charindex(struct spinner_t *spinner)
{
    struct window_t *spinner_window = (struct window_t *)spinner;
    size_t i;
    int x = spinner->scrollx;

    if(!spinner_window->title)
    {
        return 0;
    }

    for(i = 0; i < spinner_window->title_len; i++)
    {
        if(x >= spinner->caretx)
        {
            break;
        }

        x += char_width(spinner_window->gc->font, spinner_window->title[i]);
    }

    return i;
}


static size_t charindex_to_caretx(struct spinner_t *spinner, size_t charindex)
{
    struct window_t *spinner_window = (struct window_t *)spinner;
    size_t i;
    int x = spinner->scrollx;
    int xend = spinner_window->w - RIGHT_INNER_MARGIN;

    if(!spinner_window->title)
    {
        return 0;
    }

    for(i = 0; i < charindex; i++)
    {
        x += char_width(spinner_window->gc->font, spinner_window->title[i]);
    }

    if(x < LEFT_INNER_MARGIN)
    {
        spinner->scrollx = spinner->scrollx - x + LEFT_INNER_MARGIN;
    }
    else if(x > xend)
    {
        spinner->scrollx += (x - xend);
    }

    return x;
}


static int mousex_to_caretx(struct spinner_t *spinner,
                            struct mouse_state_t *mstate, size_t *charindex)
{
    struct window_t *spinner_window = (struct window_t *)spinner;
    size_t i;
    int x = spinner->scrollx;
    int xend = spinner_window->w - RIGHT_INNER_MARGIN;
    int mx = mstate->x;

    if(!spinner_window->title)
    {
        *charindex = 0;
        return LEFT_INNER_MARGIN;
    }

    if(mx < LEFT_INNER_MARGIN)
    {
        if(x >= LEFT_INNER_MARGIN)
        {
            *charindex = 0;
            return LEFT_INNER_MARGIN;
        }

        mx = -char_width(spinner_window->gc->font, 'X');
    }
    else if(mx > xend)
    {
        mx = xend + char_width(spinner_window->gc->font, 'X');
    }

    for(i = 0; i < spinner_window->title_len; i++)
    {
        if(x >= mx)
        {
            break;
        }

        x += char_width(spinner_window->gc->font, spinner_window->title[i]);
    }

    if(x < LEFT_INNER_MARGIN)
    {
        spinner->scrollx = spinner->scrollx - x + LEFT_INNER_MARGIN;
    }
    else if(x > xend)
    {
        spinner->scrollx += (x - xend);
    }

    *charindex = i;
    return x;
}


void spinner_mouseover(struct window_t *spinner_window, 
                       struct mouse_state_t *mstate)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;
    size_t i, oldi;

    // check for mouse motion on the arrows on the right
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    if(mstate->x >= spinner_window->w - ARROW_WIDTH)
    {
        cursor_show(spinner_window->parent, CURSOR_NORMAL);
    }
    else if(GLOB.curid != CURSOR_IBEAM)
    {
        spinner->global_curid = GLOB.curid;
        cursor_show(spinner_window->parent, CURSOR_IBEAM);
    }

    if(!spinner->selecting)
    {
        return;
    }

    oldi = caretx_to_charindex(spinner);
    spinner->caretx = mousex_to_caretx(spinner, mstate, &i);
    spinner->flags |= INPUTBOX_FLAG_CARET_SHOWN;

    if(spinner->select_end == spinner->select_start)
    {
        if(i >= oldi)
        {
            spinner->select_end = i;
        }
        else
        {
            spinner->select_start = i;
        }
    }
    else if(oldi == spinner->select_end)
    {
        spinner->select_end = i;
    }
    else
    {
        spinner->select_start = i;
    }
    
    spinner_repaint(spinner_window, IS_ACTIVE_CHILD(spinner_window));
    child_invalidate(spinner_window);
}


void spinner_mousedown(struct window_t *spinner_window, 
                       struct mouse_state_t *mstate)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;

    if(!mstate->left_pressed)
    {
        return;
    }

    // check for mouse press on the arrows on the right
    if(mstate->x >= spinner_window->w - ARROW_WIDTH)
    {
        if(mstate->y <= ARROW_HEIGHT)
        {
            spinner_set_val(spinner, spinner->val + 1);
        }
        else
        {
            spinner_set_val(spinner, spinner->val - 1);
        }

        spinner_repaint(spinner_window, IS_ACTIVE_CHILD(spinner_window));
        child_invalidate(spinner_window);
        return;
    }

    /*
    if(spinner->flags & INPUTBOX_FLAG_CARET_SHOWN)
    {
        hide_caret(spinner_window);
    }
    */

    spinner->caretx = mousex_to_caretx(spinner, mstate, &spinner->select_start);
    spinner->selecting = 1;
    spinner->flags |= INPUTBOX_FLAG_CARET_SHOWN;
    spinner->select_end = spinner->select_start;
    spinner_repaint(spinner_window, IS_ACTIVE_CHILD(spinner_window));

    child_invalidate(spinner_window);
}


void spinner_mouseexit(struct window_t *spinner_window)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;

    cursor_show(spinner_window->parent, spinner->global_curid);
}


void spinner_mouseup(struct window_t *spinner_window, 
                     struct mouse_state_t *mstate)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;

    if(!mstate->left_released)
    {
        return;
    }

    spinner->selecting = 0;
}


static void hide_caret(struct window_t *spinner_window)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;

    RectList *saved_clip_rects = spinner_window->gc->clipping.clip_rects;
    spinner_window->gc->clipping.clip_rects = spinner_window->clip_rects;

    gc_vertical_line(spinner_window->gc,
                             to_child_x(spinner_window, spinner->caretx),
                             to_child_y(spinner_window, spinner->carety),
                             spinner->careth, spinner_window->bgcolor);

    spinner_window->gc->clipping.clip_rects = saved_clip_rects;
}


static void show_caret(struct window_t *spinner_window)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;

    RectList *saved_clip_rects = spinner_window->gc->clipping.clip_rects;
    spinner_window->gc->clipping.clip_rects = spinner_window->clip_rects;

    gc_vertical_line(spinner_window->gc,
                             to_child_x(spinner_window, spinner->caretx),
                             to_child_y(spinner_window, spinner->carety),
                             spinner->careth, spinner_window->fgcolor);

    spinner_window->gc->clipping.clip_rects = saved_clip_rects;
}


static inline void title_to_val(struct spinner_t *spinner)
{
    spinner->val = atoi(spinner->window.title);

    if(spinner->value_change_callback)
    {
        spinner->value_change_callback(spinner->window.parent, spinner);
    }
}


static inline void adjust_indices(struct spinner_t *spinner, size_t cur_char)
{
    struct window_t *spinner_window = (struct window_t *)spinner;
    int w = usable_width(spinner_window);

    spinner->vw = string_width_no_kerning(spinner->window.gc->font,
                                          spinner->window.title);

    if(spinner->vw <= w)
    {
        spinner->scrollx = LEFT_INNER_MARGIN;
    }
    else
    {
        spinner->scrollx = spinner->window.w - RIGHT_INNER_MARGIN - spinner->vw;
    }

    spinner->caretx = charindex_to_caretx(spinner, cur_char);

    title_to_val(spinner);

    spinner->select_start = 0;
    spinner->select_end = 0;
}


static inline int validate_value(struct spinner_t *spinner)
{
    char buf[32];

    if(spinner->val < spinner->min)
    {
        spinner->val = spinner->min;
    }
    else if(spinner->val > spinner->max)
    {
        spinner->val = spinner->max;
    }

    sprintf(buf, "%d", spinner->val);

    if(strcmp(spinner->window.title, buf))
    {
        __window_set_title(&spinner->window, buf, 0);
        adjust_indices(spinner, 0);
        return 1;
    }

    return 0;
}


void spinner_unfocus(struct window_t *spinner_window)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;

    // validate value and fix the displayed number if needed
    if(validate_value(spinner))
    {
        spinner_repaint(spinner_window, 0);
    }

    if(spinner->select_end != spinner->select_start)
    {
        spinner_repaint(spinner_window, 0);
    }

    if(spinner->flags & INPUTBOX_FLAG_CARET_SHOWN)
    {
        spinner->flags &= ~INPUTBOX_FLAG_CARET_SHOWN;
        hide_caret(spinner_window);
    }

    child_invalidate(spinner_window);
}


void spinner_focus(struct window_t *spinner_window)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;

    if(!(spinner->flags & INPUTBOX_FLAG_CARET_SHOWN))
    {
        spinner->flags |= INPUTBOX_FLAG_CARET_SHOWN;
        spinner_repaint(spinner_window, 1);
        child_invalidate(spinner_window);
    }
}


static inline int paint_after_keypress(struct window_t *spinner_window)
{
    spinner_window->repaint(spinner_window, IS_ACTIVE_CHILD(spinner_window));
    child_invalidate(spinner_window);
    
    return 1;
}


static inline void select_char(struct spinner_t *spinner, size_t i, int goleft)
{
    // LEFT arrow pressed
    if(goleft)
    {
        if(spinner->select_start == spinner->select_end)
        {
            spinner->select_start = i;
            spinner->select_end = i + 1;
        }
        else
        {
            spinner->select_start = i;
        }
    }
    // RIGHT arrow pressed
    else
    {
        if(spinner->select_start == spinner->select_end)
        {
            spinner->select_start = i - 1;
            spinner->select_end = i;
        }
        else
        {
            spinner->select_end = i;
        }
    }
}


static inline void delete_selection(struct spinner_t *spinner)
{
    char *s, *s2;

    if(spinner->window.title_len == 0)
    {
        return;
    }

    // remove selection by moving all the chars after select_end to the left
    s = spinner->window.title + spinner->select_start;
    s2 = spinner->window.title + spinner->select_end;

    while(*s2)
    {
        *s++ = *s2++;
    }

    *s = '\0';

    // calculate the new title length
    spinner->window.title_len = strlen(spinner->window.title);

    // adjust display if needed
    adjust_indices(spinner, (spinner->select_start < spinner->select_end) ?
                                spinner->select_start : spinner->select_end);
}


static inline void copy_selection(struct spinner_t *spinner, int cut)
{
    char *selection;
    size_t selection_len;

    if(spinner->select_start == spinner->select_end ||
       &spinner->window.title_len == 0)
    {
        return;
    }

    selection_len = spinner->select_end - spinner->select_start;

    if(!(selection = malloc(selection_len + 1)))
    {
        return;
    }

    A_memcpy(selection, &spinner->window.title[spinner->select_start],
                        selection_len);
    selection[selection_len] = '\0';

    if(!clipboard_set_data(CLIPBOARD_FORMAT_TEXT, selection, selection_len + 1))
    {
        free(selection);
        return;
    }

    free(selection);

    if(cut)
    {
        delete_selection(spinner);
    }
}


static inline void paste_selection(struct spinner_t *spinner, size_t cur_char)
{
    size_t datasz;
    char *data;
    
    if(!(datasz = clipboard_has_data(CLIPBOARD_FORMAT_TEXT)))
    {
        // clipboard is empty
        return;
    }
    
    if(!(data = clipboard_get_data(CLIPBOARD_FORMAT_TEXT)))
    {
        // out of memory
        return;
    }

    // ensure the data ends in a null char
    if(data[datasz - 1] != '\0')
    {
        char *dup;

        if(!(dup = malloc(datasz + 1)))
        {
            free(data);
            return;
        }

        A_memcpy(dup, data, datasz);
        dup[datasz] = '\0';
        datasz++;
        free(data);
        data = dup;
    }

    // remove any selected text then paste
    if(spinner->select_start != spinner->select_end)
    {
        cur_char = (spinner->select_start < spinner->select_end) ?
                        spinner->select_start : spinner->select_end;
        delete_selection(spinner);
    }

    if(!widget_add_text_at(&spinner->window, cur_char, data))
    {
        free(data);
        return;
    }

    free(data);

    // adjust display if needed
    adjust_indices(spinner, cur_char + datasz - 1);
}


int spinner_keypress(struct window_t *spinner_window, 
                     char code, char modifiers)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;
    int key, charw, xend;
    char buf[4];
    char *s, *s2;
    size_t cur_char = caretx_to_charindex(spinner);
    
    /*
     * Handle cursor keys, etc.
     */
    switch(code)
    {
        case KEYCODE_HOME:
            if(cur_char == 0)
            {
                return 1;
            }

            if(spinner_window->title_len == 0)
            {
                return 1;
            }

            if(modifiers & MODIFIER_MASK_SHIFT)
            {
                if(spinner->select_start == spinner->select_end)
                {
                    spinner->select_end = cur_char;
                }
            }
            else
            {
                spinner->select_end = 0;
            }

            spinner->select_start = 0;
            scroll_to_start(spinner_window);
            
            return paint_after_keypress(spinner_window);

        case KEYCODE_END:
            if(spinner_window->title_len == 0)
            {
                return 1;
            }

            if(modifiers & MODIFIER_MASK_SHIFT)
            {
                if(spinner->select_start == spinner->select_end)
                {
                    spinner->select_start = cur_char;
                }
            }
            else
            {
                spinner->select_start = 0;
                spinner->select_end = 0;
            }

            scroll_to_end(spinner_window);

            if(modifiers & MODIFIER_MASK_SHIFT)
            {
                spinner->select_end = caretx_to_charindex(spinner);
            }

            return paint_after_keypress(spinner_window);

        case KEYCODE_UP:
            spinner_set_val(spinner, spinner->val + 1);
            return paint_after_keypress(spinner_window);

        case KEYCODE_DOWN:
            spinner_set_val(spinner, spinner->val - 1);
            return paint_after_keypress(spinner_window);

        case KEYCODE_DELETE:
            // if there is any selected text, remove it
            if(spinner->select_start != spinner->select_end)
            {
                delete_selection(spinner);
                return paint_after_keypress(spinner_window);
            }

            if(cur_char >= spinner_window->title_len)
            {
                return 1;
            }
            
            if(spinner_window->title_len == 0)
            {
                return 1;
            }
            
            s = spinner_window->title + cur_char;
            s2 = spinner_window->title + spinner_window->title_len;
            
            do
            {
                s[0] = s[1];
                s++;
            } while(s < s2);
            
            spinner_window->title_len--;
            title_to_val(spinner);

            return paint_after_keypress(spinner_window);

        case KEYCODE_BACKSPACE:
            // if there is any selected text, remove it
            if(spinner->select_start != spinner->select_end)
            {
                delete_selection(spinner);
                return paint_after_keypress(spinner_window);
            }

            if(cur_char == 0)
            {
                return 1;
            }
            
            if(spinner_window->title_len == 0)
            {
                return 1;
            }
            
            charw = char_width(spinner_window->gc->font,
                               spinner_window->title[cur_char - 1]);

            s = spinner_window->title + cur_char - 1;
            s2 = spinner_window->title + spinner_window->title_len;
            
            do
            {
                s[0] = s[1];
                s++;
            } while(s < s2);
            
            spinner_window->title_len--;
            title_to_val(spinner);

            spinner->caretx -= charw;

            if(spinner->caretx < LEFT_INNER_MARGIN)
            {
                spinner->scrollx += LEFT_INNER_MARGIN - spinner->caretx;
                spinner->caretx = LEFT_INNER_MARGIN;
            }

            spinner->select_start = 0;
            spinner->select_end = 0;

            return paint_after_keypress(spinner_window);

        case KEYCODE_LEFT:
            if(cur_char == 0)
            {
                return 1;
            }

            charw = char_width(spinner_window->gc->font,
                               spinner_window->title[cur_char - 1]);

            spinner->caretx -= charw;

            if(spinner->caretx < LEFT_INNER_MARGIN)
            {
                spinner->scrollx += LEFT_INNER_MARGIN - spinner->caretx;
                spinner->caretx = LEFT_INNER_MARGIN;
            }

            if(modifiers & MODIFIER_MASK_SHIFT)
            {
                if(spinner->select_start != spinner->select_end &&
                   cur_char == spinner->select_end)
                {
                    // deselect the last char on the right
                    spinner->select_end--;
                }
                else
                {
                    select_char(spinner, cur_char - 1, 1);
                }
            }
            else
            {
                spinner->select_start = 0;
                spinner->select_end = 0;
            }

            return paint_after_keypress(spinner_window);

        case KEYCODE_RIGHT:
            if(cur_char >= spinner_window->title_len)
            {
                return 1;
            }

            charw = char_width(spinner_window->gc->font,
                               spinner_window->title[cur_char]);

            xend = spinner_window->w - RIGHT_INNER_MARGIN;

            spinner->caretx += charw;

            if(spinner->caretx > xend)
            {
                spinner->scrollx -= spinner->caretx - xend;
                spinner->caretx = xend;
            }
            
            if(modifiers & MODIFIER_MASK_SHIFT)
            {
                if(spinner->select_start != spinner->select_end &&
                   cur_char == spinner->select_start)
                {
                    // deselect the last char on the left
                    spinner->select_start++;
                }
                else
                {
                    select_char(spinner, cur_char + 1, 0);
                }
            }
            else
            {
                spinner->select_start = 0;
                spinner->select_end = 0;
            }

            return paint_after_keypress(spinner_window);

        case KEYCODE_A:
            // CTRL-A - select all
            if(modifiers & MODIFIER_MASK_CTRL)
            {
                if(spinner_window->title_len == 0)
                {
                    return 1;
                }

                scroll_to_end(spinner_window);
                spinner->select_end = spinner_window->title_len;
                spinner->select_start = 0;

                return paint_after_keypress(spinner_window);
            }

            break;

        case KEYCODE_C:
            // CTRL-C - copy selected text (if any)
            if(modifiers & MODIFIER_MASK_CTRL)
            {
                copy_selection(spinner, 0);
                return 1;
            }

            break;

        case KEYCODE_X:
            // CTRL-X - cut selected text (if any)
            if(modifiers & MODIFIER_MASK_CTRL)
            {
                copy_selection(spinner, 1);
                return paint_after_keypress(spinner_window);
            }

            break;

        case KEYCODE_V:
            // CTRL-V - paste copied text (if any)
            if(modifiers & MODIFIER_MASK_CTRL)
            {
                paste_selection(spinner, cur_char);
                return paint_after_keypress(spinner_window);
            }

            break;

        case KEYCODE_ENTER:
            // validate value and fix the displayed number if needed
            if(validate_value(spinner))
            {
                paint_after_keypress(spinner_window);
            }

            return 1;

        case KEYCODE_TAB:
            // don't handle tab, return the key to parent to handle
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            return 0;
    }

    // Don't handle ALT-key combinations, as these are usually menu shortcuts.
    // Don't handle CTRL-key combinations we don't recognise, as these could
    // be things like CTRL-S (save), ...
    if(modifiers & (MODIFIER_MASK_ALT | MODIFIER_MASK_CTRL))
    {
        return 0;
    }
    
    if(!(key = get_printable_char(code, modifiers)))
    {
        return 0;
    }

    // don't show non-numeric input
    if(key < '0' || key > '9')
    {
        // but tell the caller we handled it
        return 1;
    }

    // if there is any selected text, remove it
    if(spinner->select_start != spinner->select_end)
    {
        if(spinner->select_start < spinner->select_end)
        {
            cur_char = spinner->select_start;
        }
        else
        {
            cur_char = spinner->select_end;
        }

        delete_selection(spinner);
    }
    
    buf[0] = (char)key;
    buf[1] = '\0';
    
    if(!widget_add_text_at(spinner_window, cur_char, buf))
    {
        return 1;
    }

    // adjust the caret
    adjust_indices(spinner, cur_char + 1);
    
    return paint_after_keypress(spinner_window);
}


void spinner_size_changed(struct window_t *spinner_window)
{
    struct spinner_t *spinner = (struct spinner_t *)spinner_window;

    if(spinner->backbuf_gc.w != spinner_window->w ||
       spinner->backbuf_gc.h != spinner_window->h)
    {
        if(gc_realloc_backbuf(spinner_window->gc, &spinner->backbuf_gc,
                              spinner_window->w, spinner_window->h) < 0)
        {
            /*
             * NOTE: this should be a fatal error.
             */
            return;
        }

        gc_fill_rect(&spinner->backbuf_gc, 1, 1,
                         spinner_window->w - 2,
                         INPUTBOX_HEIGHT - 2, 
                         GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR]);
        draw_inverted_3d_border(&spinner->backbuf_gc, 0, 0, 
                                spinner_window->w, INPUTBOX_HEIGHT);
        reset_backbuf_clipping(spinner);
    }

    widget_size_changed(spinner_window);
}


void spinner_set_max(struct spinner_t *spinner, int max)
{
    spinner->max = max;
}


void spinner_set_min(struct spinner_t *spinner, int min)
{
    spinner->min = min;
}


void spinner_set_val(struct spinner_t *spinner, int val)
{
    if(val >= spinner->min && val <= spinner->max)
    {
        char buf[32];

        spinner->val = val;
        sprintf(buf, "%d", spinner->val);

        __window_set_title(&spinner->window, buf, 0);
        adjust_indices(spinner, 0);
    }
}


static inline
void color_from_template(uint32_t *array, uint32_t *template, int index)
{
    if(template[index] == TEMPLATE_BGCOLOR)
    {
        array[index] = GLOB.themecolor[THEME_COLOR_SCROLLBAR_BGCOLOR];
    }
    else if(template[index] == TEMPLATE_TEXTCOLOR)
    {
        array[index] = GLOB.themecolor[THEME_COLOR_SCROLLBAR_TEXTCOLOR];
    }
    else
    {
        array[index] = template[index];
    }
}


/*
 * Called on startup and when the system color theme changes.
 * Updates the global arrow bitmaps.
 */
void spinner_theme_changed_global(void)
{
    int i, j, k;

    for(i = 0, k = 0; i < 16; i++)
    {
        for(j = 0; j < 16; j++, k++)
        {
            color_from_template(arrow_up_img, arrow_up_img_template, k);
            color_from_template(arrow_down_img, arrow_down_img_template, k);
        }
    }
}


/*
 * Called when the system color theme changes.
 * Updates the widget's colors.
 */
void spinner_theme_changed(struct window_t *window)
{
    window->bgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR];
    window->fgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_TEXTCOLOR];
}

