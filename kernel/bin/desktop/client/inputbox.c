/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: inputbox.c
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
 *  \file inputbox.c
 *
 *  The implementation of an inputbox widget.
 */

#include <stdlib.h>
#include <string.h>
#include "../include/client/inputbox.h"
#include "../include/clipboard.h"
#include "../include/cursor.h"
#include "../include/rect.h"
#include "../include/font.h"
#include "../include/menu.h"
#include "../include/keys.h"

#include "inlines.c"

#define LEFT_INNER_MARGIN               4
#define TOP_INNER_MARGIN                4

#define GLOB                            __global_gui_data

static void show_caret(struct window_t *inputbox_window);
static void hide_caret(struct window_t *inputbox_window);


static inline void reset_backbuf_clipping(struct inputbox_t *inputbox)
{
    Rect *rect;

    // account for the border
    rect = inputbox->backbuf_gc.clipping.clip_rects->root;

    rect->top = TOP_INNER_MARGIN;
    rect->left = LEFT_INNER_MARGIN;
    rect->bottom = inputbox->backbuf_gc.h - TOP_INNER_MARGIN;
    rect->right = inputbox->backbuf_gc.w - LEFT_INNER_MARGIN;
}


struct inputbox_t *inputbox_new(struct gc_t *gc, struct window_t *parent,
                                                 int x, int y, int w,
                                                 char *title)
{
    struct inputbox_t *inputbox;
    Rect *rect;

    if(!(inputbox = malloc(sizeof(struct inputbox_t))))
    {
        return NULL;
    }

    memset(inputbox, 0, sizeof(struct inputbox_t));

    if(gc_alloc_backbuf(gc, &inputbox->backbuf_gc, w, INPUTBOX_HEIGHT) < 0)
    {
        free(inputbox);
        return NULL;
    }

    gc_set_font(&inputbox->backbuf_gc, GLOB.sysfont.data ? &GLOB.sysfont :
                                                           &GLOB.mono);

    // All subsequent drawing on the listview's canvas will be clipped to a
    // 1-pixel border. If we draw the border later (e.g. in listview_repaint())
    // we will fail, as the border will be clipped and will not be drawn.
    // A workaround would be to temporarily unclip the clipping and draw the
    // border, but this is complicated and messy. Instead, we draw the border
    // here, once and for all, and we never need to worry about it again.
    gc_fill_rect(&inputbox->backbuf_gc, 1, 1,
                         w - 2, INPUTBOX_HEIGHT - 2,
                         GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR]);
    draw_inverted_3d_border(&inputbox->backbuf_gc, 0, 0, w, INPUTBOX_HEIGHT);
    reset_backbuf_clipping(inputbox);

    if(!(inputbox->window.clip_rects = RectList_new()))
    {
        free(inputbox->backbuf_gc.buffer);
        free(inputbox);
        //free(backbuf);
        return NULL;
    }

    if(parent->main_menu)
    {
        y += MENU_HEIGHT;
    }

    if(!(rect = Rect_new(y, x, y + INPUTBOX_HEIGHT - 1,  x + w - 1)))
    {
        RectList_free(inputbox->window.clip_rects);
        free(inputbox->backbuf_gc.buffer);
        free(inputbox);
        //free(backbuf);
        return NULL;
    }

    RectList_add(inputbox->window.clip_rects, rect);
    inputbox->window.type = WINDOW_TYPE_INPUTBOX;
    inputbox->window.x = x;
    inputbox->window.y = y;
    inputbox->window.w = w;
    inputbox->window.h = INPUTBOX_HEIGHT /* h */;
    inputbox->window.gc = gc;
    inputbox->window.flags = WINDOW_NODECORATION | WINDOW_3D_WIDGET;
    inputbox->window.visible = 1;
    inputbox->window.bgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR];
    inputbox->window.fgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_TEXTCOLOR];
    
    if(title)
    {
        __window_set_title(&inputbox->window, title, 0);
        inputbox->vw = string_width_no_kerning(inputbox->window.gc->font,
                          inputbox->window.title);
    }
    
    inputbox->scrollx = LEFT_INNER_MARGIN;
    inputbox->caretx = LEFT_INNER_MARGIN;
    inputbox->carety = TOP_INNER_MARGIN;
    inputbox->careth = inputbox->window.h - (TOP_INNER_MARGIN * 2);

    //inputbox->window.paint_function = inputbox_paint;
    inputbox->window.repaint = inputbox_repaint;
    inputbox->window.mousedown = inputbox_mousedown;
    inputbox->window.mouseover = inputbox_mouseover;
    inputbox->window.mouseup = inputbox_mouseup;
    inputbox->window.mouseexit = inputbox_mouseexit;
    inputbox->window.unfocus = inputbox_unfocus;
    inputbox->window.focus = inputbox_focus;
    inputbox->window.destroy = inputbox_destroy;
    inputbox->window.keypress = inputbox_keypress;
    //inputbox->window.keyrelease = inputbox_keyrelease;
    inputbox->window.size_changed = inputbox_size_changed;
    inputbox->window.theme_changed = inputbox_theme_changed;

    window_insert_child(parent, (struct window_t *)inputbox);

    return inputbox;
}


void inputbox_destroy(struct window_t *inputbox_window)
{
    // this will free the title, the clip_rects list, and the widget struct
    widget_destroy(inputbox_window);
}


void inputbox_repaint(struct window_t *inputbox_window, int is_active_child)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;
    int x = inputbox->scrollx;
    int xend = inputbox_window->w - LEFT_INNER_MARGIN;
    int selecting = inputbox->select_end != inputbox->select_start &&
                    is_active_child;
    size_t i, charw;
    char buf[2] = { 0, 0 };

    void (*func)(struct gc_t *, struct clipping_t *, char *, int, int,
                                uint32_t, char);

    if(inputbox_window->gc->font->flags & FONT_FLAG_TRUE_TYPE)
    {
        func = gc_draw_text_clipped_ttf;
    }
    else
    {
        func = gc_draw_text_clipped;
    }

    //White background
    gc_fill_rect(&inputbox->backbuf_gc, 2, 2,
                 inputbox_window->w - 4, inputbox_window->h - 4,
                 inputbox_window->bgcolor);

    //Draw the text within the box
    if(inputbox_window->title)
    {
        for(i = 0; i < inputbox_window->title_len; i++)
        {
            buf[0] = inputbox_window->title[i];
            //charw = string_width(inputbox_window->gc->font, buf);
            charw = char_width(inputbox_window->gc->font, buf[0]);

            if(x + charw >= LEFT_INNER_MARGIN)
            {
                if(selecting && i >= inputbox->select_start &&
                                i < inputbox->select_end)
                {
                    gc_fill_rect(&inputbox->backbuf_gc,
                                 x, TOP_INNER_MARGIN,
                                 charw,
                                 inputbox_window->h - (TOP_INNER_MARGIN * 2),
                                 GLOB.themecolor[THEME_COLOR_INPUTBOX_SELECT_BGCOLOR]);

                    func(&inputbox->backbuf_gc,
                         &inputbox->backbuf_gc.clipping, buf,
                         x, TOP_INNER_MARGIN,
                         GLOB.themecolor[THEME_COLOR_INPUTBOX_SELECT_TEXTCOLOR], 0);
                }
                else
                {
                    func(&inputbox->backbuf_gc,
                         &inputbox->backbuf_gc.clipping, buf,
                         x, TOP_INNER_MARGIN,
                         inputbox_window->fgcolor, 0);
                }
            }

            x += charw;

            if(x >= xend)
            {
                break;
            }
        }
    }

    gc_blit(inputbox_window->gc, &inputbox->backbuf_gc,
            inputbox_window->x, inputbox_window->y);

    if(is_active_child)
    //if(inputbox->flags & INPUTBOX_FLAG_CARET_SHOWN)
    {
        inputbox->flags |= INPUTBOX_FLAG_CARET_SHOWN;
        show_caret(inputbox_window);
    }
    else
    {
        inputbox->flags &= ~INPUTBOX_FLAG_CARET_SHOWN;
    }
}


static inline int usable_width(struct window_t *inputbox_window)
{
    return (inputbox_window->w - (LEFT_INNER_MARGIN * 2));
}


static void scroll_to_start(struct window_t *inputbox_window)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;

    inputbox->scrollx = LEFT_INNER_MARGIN;
    inputbox->caretx = LEFT_INNER_MARGIN;
}


static void scroll_to_end(struct window_t *inputbox_window)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;
    int w = usable_width(inputbox_window);

    if(inputbox->vw <= w)
    {
        inputbox->scrollx = LEFT_INNER_MARGIN;
        inputbox->caretx = inputbox->vw + LEFT_INNER_MARGIN;
    }
    else
    {
        inputbox->scrollx = inputbox_window->w - LEFT_INNER_MARGIN - inputbox->vw;
        inputbox->caretx = inputbox_window->w - LEFT_INNER_MARGIN;
    }
}


void inputbox_append_text(struct window_t *inputbox_window, char *addstr)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;

    inputbox->vw += string_width_no_kerning(inputbox_window->gc->font,
                                            addstr);
    widget_append_text(inputbox_window, addstr);
    inputbox_window->title_len = strlen(inputbox_window->title);
    scroll_to_end(inputbox_window);

    inputbox_window->repaint(inputbox_window, IS_ACTIVE_CHILD(inputbox_window));
    child_invalidate(inputbox_window);
}


void inputbox_set_text(struct window_t *inputbox_window, char *new_title)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;

    __window_set_title(inputbox_window, new_title, 0);
    inputbox_window->title_len = strlen(inputbox_window->title);
    inputbox->vw = string_width_no_kerning(inputbox_window->gc->font,
                        inputbox_window->title);
    scroll_to_end(inputbox_window);

    //child_repaint(inputbox_window);
    inputbox_window->repaint(inputbox_window, IS_ACTIVE_CHILD(inputbox_window));
    child_invalidate(inputbox_window);
}


static size_t caretx_to_charindex(struct inputbox_t *inputbox)
{
    struct window_t *inputbox_window = (struct window_t *)inputbox;
    size_t i;
    int x = inputbox->scrollx;

    if(!inputbox_window->title)
    {
        return 0;
    }

    for(i = 0; i < inputbox_window->title_len; i++)
    {
        if(x >= inputbox->caretx)
        {
            break;
        }

        x += char_width(inputbox_window->gc->font, inputbox_window->title[i]);
    }

    return i;
}


static size_t charindex_to_caretx(struct inputbox_t *inputbox, 
                                  size_t charindex)
{
    struct window_t *inputbox_window = (struct window_t *)inputbox;
    size_t i;
    int x = inputbox->scrollx;
    int xend = inputbox_window->w - LEFT_INNER_MARGIN;

    if(!inputbox_window->title)
    {
        return 0;
    }

    for(i = 0; i < charindex; i++)
    {
        x += char_width(inputbox_window->gc->font, inputbox_window->title[i]);
    }

    if(x < LEFT_INNER_MARGIN)
    {
        inputbox->scrollx = inputbox->scrollx - x + LEFT_INNER_MARGIN;
    }
    else if(x > xend)
    {
        inputbox->scrollx += (x - xend);
    }

    return x;
}


static int mousex_to_caretx(struct inputbox_t *inputbox,
                            struct mouse_state_t *mstate, size_t *charindex)
{
    struct window_t *inputbox_window = (struct window_t *)inputbox;
    size_t i;
    int x = inputbox->scrollx;
    int xend = inputbox_window->w - LEFT_INNER_MARGIN;
    int mx = mstate->x;

    if(!inputbox_window->title)
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

        mx = -char_width(inputbox_window->gc->font, 'X');
    }
    else if(mx > xend)
    {
        mx = xend + char_width(inputbox_window->gc->font, 'X');
    }

    for(i = 0; i < inputbox_window->title_len; i++)
    {
        if(x >= mx)
        {
            break;
        }

        x += char_width(inputbox_window->gc->font, inputbox_window->title[i]);
    }

    if(x < LEFT_INNER_MARGIN)
    {
        inputbox->scrollx = inputbox->scrollx - x + LEFT_INNER_MARGIN;
    }
    else if(x > xend)
    {
        inputbox->scrollx += (x - xend);
    }

    *charindex = i;
    return x;
}


void inputbox_mouseover(struct window_t *inputbox_window, 
                        struct mouse_state_t *mstate)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;
    size_t i, oldi;

    if(GLOB.curid != CURSOR_IBEAM)
    {
        inputbox->global_curid = GLOB.curid;
        cursor_show(inputbox_window->parent, CURSOR_IBEAM);
    }

    if(!inputbox->selecting)
    {
        return;
    }

    oldi = caretx_to_charindex(inputbox);
    inputbox->caretx = mousex_to_caretx(inputbox, mstate, &i);
    inputbox->flags |= INPUTBOX_FLAG_CARET_SHOWN;

    if(inputbox->select_end == inputbox->select_start)
    {
        if(i >= oldi)
        {
            inputbox->select_end = i;
        }
        else
        {
            inputbox->select_start = i;
        }
    }
    else if(oldi == inputbox->select_end)
    {
        inputbox->select_end = i;
    }
    else
    {
        inputbox->select_start = i;
    }

    inputbox_repaint(inputbox_window, IS_ACTIVE_CHILD(inputbox_window));
    child_invalidate(inputbox_window);
}


void inputbox_mousedown(struct window_t *inputbox_window, 
                        struct mouse_state_t *mstate)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;

    if(!mstate->left_pressed)
    {
        return;
    }

    inputbox->caretx = mousex_to_caretx(inputbox, mstate, &inputbox->select_start);
    inputbox->selecting = 1;
    inputbox->flags |= INPUTBOX_FLAG_CARET_SHOWN;
    inputbox->select_end = inputbox->select_start;
    inputbox_repaint(inputbox_window, IS_ACTIVE_CHILD(inputbox_window));

    //show_caret(inputbox_window);
    child_invalidate(inputbox_window);
}


void inputbox_mouseexit(struct window_t *inputbox_window)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;

    cursor_show(inputbox_window->parent, inputbox->global_curid);
}


void inputbox_mouseup(struct window_t *inputbox_window, 
                      struct mouse_state_t *mstate)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;

    if(!mstate->left_released)
    {
        return;
    }

    inputbox->selecting = 0;
}


static void hide_caret(struct window_t *inputbox_window)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;

    RectList *saved_clip_rects = inputbox_window->gc->clipping.clip_rects;
    inputbox_window->gc->clipping.clip_rects = inputbox_window->clip_rects;

    gc_vertical_line(inputbox_window->gc,
                     to_child_x(inputbox_window, inputbox->caretx),
                     to_child_y(inputbox_window, inputbox->carety),
                     inputbox->careth, inputbox_window->bgcolor);

    inputbox_window->gc->clipping.clip_rects = saved_clip_rects;
}


static void show_caret(struct window_t *inputbox_window)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;

    RectList *saved_clip_rects = inputbox_window->gc->clipping.clip_rects;
    inputbox_window->gc->clipping.clip_rects = inputbox_window->clip_rects;

    gc_vertical_line(inputbox_window->gc,
                     to_child_x(inputbox_window, inputbox->caretx),
                     to_child_y(inputbox_window, inputbox->carety),
                     inputbox->careth, inputbox_window->fgcolor);

    inputbox_window->gc->clipping.clip_rects = saved_clip_rects;
}


void inputbox_unfocus(struct window_t *inputbox_window)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;

    if(inputbox->select_end != inputbox->select_start)
    {
        inputbox_repaint(inputbox_window, 0);
    }

    if(inputbox->flags & INPUTBOX_FLAG_CARET_SHOWN)
    {
        inputbox->flags &= ~INPUTBOX_FLAG_CARET_SHOWN;
        hide_caret(inputbox_window);
    }

    child_invalidate(inputbox_window);
}


void inputbox_focus(struct window_t *inputbox_window)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;

    if(!(inputbox->flags & INPUTBOX_FLAG_CARET_SHOWN))
    {
        inputbox->flags |= INPUTBOX_FLAG_CARET_SHOWN;
        inputbox_repaint(inputbox_window, 1);
        //show_caret(inputbox_window);
        child_invalidate(inputbox_window);
    }
}


static inline int paint_after_keypress(struct window_t *inputbox_window)
{
    inputbox_window->repaint(inputbox_window, IS_ACTIVE_CHILD(inputbox_window));
    child_invalidate(inputbox_window);
    
    return 1;
}


static inline void select_char(struct inputbox_t *inputbox, 
                               size_t i, int goleft)
{
    // LEFT arrow pressed
    if(goleft)
    {
        if(inputbox->select_start == inputbox->select_end)
        {
            inputbox->select_start = i;
            inputbox->select_end = i + 1;
        }
        else
        {
            inputbox->select_start = i;
        }
    }
    // RIGHT arrow pressed
    else
    {
        if(inputbox->select_start == inputbox->select_end)
        {
            inputbox->select_start = i - 1;
            inputbox->select_end = i;
        }
        else
        {
            inputbox->select_end = i;
        }
    }
}


static inline void adjust_indices(struct inputbox_t *inputbox, size_t cur_char)
{
    struct window_t *inputbox_window = (struct window_t *)inputbox;
    int w = usable_width(inputbox_window);

    inputbox->vw = string_width_no_kerning(inputbox->window.gc->font,
                        inputbox->window.title);

    if(inputbox->vw <= w)
    {
        inputbox->scrollx = LEFT_INNER_MARGIN;
    }
    else
    {
        inputbox->scrollx = inputbox->window.w - LEFT_INNER_MARGIN - inputbox->vw;
    }

    inputbox->caretx = charindex_to_caretx(inputbox, cur_char);

    inputbox->select_start = 0;
    inputbox->select_end = 0;
}


static inline void delete_selection(struct inputbox_t *inputbox)
{
    char *s, *s2;

    if(inputbox->window.title_len == 0)
    {
        return;
    }

    // remove selection by moving all the chars after select_end to the left
    s = inputbox->window.title + inputbox->select_start;
    s2 = inputbox->window.title + inputbox->select_end;

    while(*s2)
    {
        *s++ = *s2++;
    }

    *s = '\0';

    // calculate the new title length
    inputbox->window.title_len = strlen(inputbox->window.title);

    // adjust display if needed
    adjust_indices(inputbox, (inputbox->select_start < inputbox->select_end) ?
                                inputbox->select_start : inputbox->select_end);
}


static inline void copy_selection(struct inputbox_t *inputbox, int cut)
{
    char *selection;
    size_t selection_len;

    if(inputbox->select_start == inputbox->select_end ||
       &inputbox->window.title_len == 0)
    {
        return;
    }

    selection_len = inputbox->select_end - inputbox->select_start;

    if(!(selection = malloc(selection_len + 1)))
    {
        return;
    }

    A_memcpy(selection, &inputbox->window.title[inputbox->select_start],
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
        delete_selection(inputbox);
    }
}


static inline void paste_selection(struct inputbox_t *inputbox, size_t cur_char)
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
    if(inputbox->select_start != inputbox->select_end)
    {
        cur_char = (inputbox->select_start < inputbox->select_end) ?
                        inputbox->select_start : inputbox->select_end;
        delete_selection(inputbox);
    }

    if(!widget_add_text_at(&inputbox->window, cur_char, data))
    {
        free(data);
        return;
    }

    free(data);

    // adjust display if needed
    adjust_indices(inputbox, cur_char + datasz - 1);
}


int inputbox_keypress(struct window_t *inputbox_window, 
                      char code, char modifiers)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;
    int key, charw, xend;
    char buf[4];
    char *s, *s2;
    size_t cur_char = caretx_to_charindex(inputbox);
    
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

            if(inputbox_window->title_len == 0)
            {
                return 1;
            }

            if(modifiers & MODIFIER_MASK_SHIFT)
            {
                if(inputbox->select_start == inputbox->select_end)
                {
                    inputbox->select_end = cur_char;
                }
            }
            else
            {
                inputbox->select_end = 0;
            }

            inputbox->select_start = 0;
            scroll_to_start(inputbox_window);
            
            return paint_after_keypress(inputbox_window);

        case KEYCODE_END:
            if(inputbox_window->title_len == 0)
            {
                return 1;
            }

            if(modifiers & MODIFIER_MASK_SHIFT)
            {
                if(inputbox->select_start == inputbox->select_end)
                {
                    inputbox->select_start = cur_char;
                }
            }
            else
            {
                inputbox->select_start = 0;
                inputbox->select_end = 0;
            }

            scroll_to_end(inputbox_window);

            if(modifiers & MODIFIER_MASK_SHIFT)
            {
                inputbox->select_end = caretx_to_charindex(inputbox);
            }

            return paint_after_keypress(inputbox_window);

        case KEYCODE_DELETE:
            // if there is any selected text, remove it
            if(inputbox->select_start != inputbox->select_end)
            {
                delete_selection(inputbox);
                return paint_after_keypress(inputbox_window);
            }

            if(cur_char >= inputbox_window->title_len)
            {
                return 1;
            }
            
            if(inputbox_window->title_len == 0)
            {
                return 1;
            }
            
            s = inputbox_window->title + cur_char;
            s2 = inputbox_window->title + inputbox_window->title_len;
            
            do
            {
                s[0] = s[1];
                s++;
            } while(s < s2);
            
            inputbox_window->title_len--;
            return paint_after_keypress(inputbox_window);

        case KEYCODE_BACKSPACE:
            // if there is any selected text, remove it
            if(inputbox->select_start != inputbox->select_end)
            {
                delete_selection(inputbox);
                return paint_after_keypress(inputbox_window);
            }

            if(cur_char == 0)
            {
                return 1;
            }
            
            if(inputbox_window->title_len == 0)
            {
                return 1;
            }
            
            charw = char_width(inputbox_window->gc->font,
                               inputbox_window->title[cur_char - 1]);

            s = inputbox_window->title + cur_char - 1;
            s2 = inputbox_window->title + inputbox_window->title_len;
            
            do
            {
                s[0] = s[1];
                s++;
            } while(s < s2);
            
            inputbox_window->title_len--;
            inputbox->caretx -= charw;

            if(inputbox->caretx < LEFT_INNER_MARGIN)
            {
                inputbox->scrollx += LEFT_INNER_MARGIN - inputbox->caretx;
                inputbox->caretx = LEFT_INNER_MARGIN;
            }

            inputbox->select_start = 0;
            inputbox->select_end = 0;

            return paint_after_keypress(inputbox_window);

        case KEYCODE_LEFT:
        case KEYCODE_UP:
            if(cur_char == 0)
            {
                return 1;
            }

            charw = char_width(inputbox_window->gc->font,
                               inputbox_window->title[cur_char - 1]);

            inputbox->caretx -= charw;

            if(inputbox->caretx < LEFT_INNER_MARGIN)
            {
                inputbox->scrollx += LEFT_INNER_MARGIN - inputbox->caretx;
                inputbox->caretx = LEFT_INNER_MARGIN;
            }

            if(modifiers & MODIFIER_MASK_SHIFT)
            {
                if(inputbox->select_start != inputbox->select_end &&
                   cur_char == inputbox->select_end)
                {
                    // deselect the last char on the right
                    inputbox->select_end--;
                }
                else
                {
                    select_char(inputbox, cur_char - 1, 1);
                }
            }
            else
            {
                inputbox->select_start = 0;
                inputbox->select_end = 0;
            }

            return paint_after_keypress(inputbox_window);

        case KEYCODE_RIGHT:
        case KEYCODE_DOWN:
            if(cur_char >= inputbox_window->title_len)
            {
                return 1;
            }

            charw = char_width(inputbox_window->gc->font,
                               inputbox_window->title[cur_char]);

            xend = inputbox_window->w - LEFT_INNER_MARGIN;

            inputbox->caretx += charw;

            if(inputbox->caretx > xend)
            {
                inputbox->scrollx -= inputbox->caretx - xend;
                inputbox->caretx = xend;
            }
            
            if(modifiers & MODIFIER_MASK_SHIFT)
            {
                if(inputbox->select_start != inputbox->select_end &&
                   cur_char == inputbox->select_start)
                {
                    // deselect the last char on the left
                    inputbox->select_start++;
                }
                else
                {
                    select_char(inputbox, cur_char + 1, 0);
                }
            }
            else
            {
                inputbox->select_start = 0;
                inputbox->select_end = 0;
            }

            return paint_after_keypress(inputbox_window);

        case KEYCODE_A:
            // CTRL-A - select all
            if(modifiers & MODIFIER_MASK_CTRL)
            {
                if(inputbox_window->title_len == 0)
                {
                    return 1;
                }

                scroll_to_end(inputbox_window);
                inputbox->select_end = inputbox_window->title_len;
                inputbox->select_start = 0;

                return paint_after_keypress(inputbox_window);
            }

            break;

        case KEYCODE_C:
            // CTRL-C - copy selected text (if any)
            if(modifiers & MODIFIER_MASK_CTRL)
            {
                copy_selection(inputbox, 0);
                return 1;
            }

            break;

        case KEYCODE_X:
            // CTRL-X - cut selected text (if any)
            if(modifiers & MODIFIER_MASK_CTRL)
            {
                copy_selection(inputbox, 1);
                return paint_after_keypress(inputbox_window);
            }

            break;

        case KEYCODE_V:
            // CTRL-V - paste copied text (if any)
            if(modifiers & MODIFIER_MASK_CTRL)
            {
                paste_selection(inputbox, cur_char);
                return paint_after_keypress(inputbox_window);
            }

            break;

        case KEYCODE_ENTER:
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

    // if there is any selected text, remove it
    if(inputbox->select_start != inputbox->select_end)
    {
        if(inputbox->select_start < inputbox->select_end)
        {
            cur_char = inputbox->select_start;
        }
        else
        {
            cur_char = inputbox->select_end;
        }

        delete_selection(inputbox);
    }
    
    buf[0] = (char)key;
    buf[1] = '\0';
    
    if(!widget_add_text_at(inputbox_window, cur_char, buf))
    {
        return 1;
    }

    // adjust the caret
    adjust_indices(inputbox, cur_char + 1);
    
    return paint_after_keypress(inputbox_window);
}


void inputbox_size_changed(struct window_t *inputbox_window)
{
    struct inputbox_t *inputbox = (struct inputbox_t *)inputbox_window;

    if(inputbox->backbuf_gc.w != inputbox_window->w ||
       inputbox->backbuf_gc.h != inputbox_window->h)
    {
        if(gc_realloc_backbuf(inputbox_window->gc, &inputbox->backbuf_gc,
                              inputbox_window->w, inputbox_window->h) < 0)
        {
            /*
             * NOTE: this should be a fatal error.
             */
            return;
        }

        gc_fill_rect(&inputbox->backbuf_gc, 1, 1,
                         inputbox_window->w - 2,
                         INPUTBOX_HEIGHT - 2, 
                         GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR]);
        draw_inverted_3d_border(&inputbox->backbuf_gc, 0, 0, 
                                inputbox_window->w, INPUTBOX_HEIGHT);
        reset_backbuf_clipping(inputbox);
    }

    widget_size_changed(inputbox_window);
}


/*
 * Called when the system color theme changes.
 * Updates the widget's colors.
 */
void inputbox_theme_changed(struct window_t *window)
{
    window->bgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_BGCOLOR];
    window->fgcolor = GLOB.themecolor[THEME_COLOR_INPUTBOX_TEXTCOLOR];
}

