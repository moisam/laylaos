/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: widget.c
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
 *  \file widget.c
 *
 *  General functions common to all types of widgets.
 */

#include "../include/client/window.h"
#include "../include/rect.h"

void widget_destroy(struct window_t *widget)
{
    RectList *list;

    if(!widget)
    {
        return;
    }
    
    list = widget->clip_rects;
    
    if(list)
    {
        while(list->root)
        {
            Rect *next = list->root->next;
            Rect_free(list->root);
            list->root = next;
        }

        RectList_free(list);
    }
    
    if(widget->title)
    {
        free(widget->title);
    }
    
    free(widget);
}


int widget_append_text(struct window_t *widget, char *addstr)
{
    char *newstr;
    int olen, addlen;

    addlen = strlen(addstr);

    //Set the title if there isn't already one
    if(!widget->title)
    {
        if(!(newstr = malloc(addlen + 1)))
        {
            return 0;
        }

        strcpy(newstr, addstr);
        widget->title_alloced = addlen + 1;
        widget->title_len = addlen;
    }
    else
    {
        olen = strlen(widget->title);
        
        if((olen + addlen + 1) > widget->title_alloced)
        {
            if(!(newstr = realloc(widget->title, olen + addlen + 1)))
            {
                return 0;
            }
            
            widget->title_alloced = olen + addlen + 1;
        }
        else
        {
            newstr = widget->title;
        }

        strcpy(newstr + olen, addstr);
        widget->title_len = olen + addlen;
    }

    widget->title = newstr;

    return 1;
}


int widget_add_text_at(struct window_t *widget, size_t where, char *addstr)
{
    char *newstr, *tail;
    size_t olen, addlen;

    if(!widget->title)
    {
        return widget_append_text(widget, addstr);
    }

    addlen = strlen(addstr);
    olen = strlen(widget->title);
    
    // currently we don't support appending way after the string's end
    if(where > olen)
    {
        return 0;
    }
    
    // appending at the end of the string
    if(where == olen)
    {
        return widget_append_text(widget, addstr);
    }
    
    // appending in the middle of the string
    if((olen + addlen + 1) >= widget->title_alloced)
    {
        if(!(newstr = realloc(widget->title, olen + addlen + 1)))
        {
            return 0;
        }
        
        widget->title_alloced = olen + addlen + 1;
    }
    else
    {
        newstr = widget->title;
    }

    if(!(tail = strdup(newstr + where)))
    {
        return 0;
    }
    
    strcpy(newstr + where, addstr);
    strcpy(newstr + where + addlen, tail);
    free(tail);

    widget->title = newstr;
    widget->title_len = olen + addlen;

    return 1;
}


void widget_set_size_hints(struct window_t *widget, 
                           struct window_t *relativeto,
                           int hint, int x, int y, int w, int h)
{
    widget->relative_to = relativeto;
    widget->relative_x = x;
    widget->relative_y = y;
    widget->relative_w = w;
    widget->relative_h = h;
    widget->resize_hints = hint;
}


void widget_size_changed(struct window_t *widget)
{
    widget->clip_rects->root->top = widget->y;
    widget->clip_rects->root->left = widget->x;
    widget->clip_rects->root->bottom = widget->y + widget->h - 1;
    widget->clip_rects->root->right = widget->x + widget->w - 1;
}


void widget_set_tabindex(struct window_t *parent, struct window_t *widget)
{
    struct window_t *current_child;
    ListNode *current_node;
    int next_tab_index = 0;
    
    widget->tab_index = 0;
    
    if(!parent->children)
    {
        return;
    }

    for(current_node = parent->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        current_child = (struct window_t *)current_node->payload;
        
        if(current_child->tab_index >= next_tab_index)
        {
            next_tab_index = current_child->tab_index + 1;
        }
    }

    widget->tab_index = next_tab_index;
}


#define ISSCROLL(w)     (w->type == WINDOW_TYPE_VSCROLL || \
                         w->type == WINDOW_TYPE_HSCROLL)

void widget_next_tabstop(struct window_t *window)
{
    struct window_t *current_child, *old_active;
    ListNode *current_node;
    int cur_tab_index = 0;

    if(!window->children)
    {
        return;
    }
    
    old_active = window->active_child;

    if(old_active)
    {
        // if the active child is a scrollbar, use its parent as they
        // are not tabbable
        if(ISSCROLL(old_active))
        {
            old_active = old_active->parent;
        }

        cur_tab_index = old_active->tab_index;
    }

    // find the child widget with the highest tab index
    for(current_node = window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        current_child = (struct window_t *)current_node->payload;
        
        if(current_child->tab_index >= cur_tab_index &&
           current_child != old_active)
        {
            window->active_child = current_child;
            break;
        }
    }
    
    // if nothing was found, find the one with the lowest tab index
    if(window->active_child && window->active_child == old_active)
    {
        for(current_node = window->children->root_node;
            current_node != NULL;
            current_node = current_node->next)
        {
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            current_child = (struct window_t *)current_node->payload;
            
            if(current_child->tab_index >= 0 &&
               current_child->tab_index < cur_tab_index)
            {
                cur_tab_index = current_child->tab_index;
                window->active_child = current_child;
            }
        }
    }
    
    // now focus the new active child and unfocus the old one
    if(window->active_child && window->active_child != old_active)
    {
        if(old_active)
        {
            old_active->unfocus(old_active);
        }
        
        window->active_child->focus(window->active_child);
    }
}


void widget_set_text_alignment(struct window_t *widget, int alignment)
{
    if(!widget)
    {
        return;
    }

    widget->text_alignment = alignment;
}

