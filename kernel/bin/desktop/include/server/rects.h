/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: rects.c
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
 *  \file rects.c
 *
 *  Functions to work with rectangles. These functions are similar and
 *  complementary to those in include/rect.h, except this file is only used
 *  by the server application and most of the functions avoid mutex locking
 *  to speed things up. This works on the server as we have only one thread
 *  working with rects and updating the screen, but this will not work on
 *  client applications that are multi-threaded.
 *
 *  This code is based on the "Windowing Systems by Example" blog series.
 *  The source code it released under the MIT license and can be found at:
 *  https://github.com/JMarlin/wsbe.
 */

#ifdef GUI_SERVER

#include "../rect.h"


// Update the clipping rectangles to only include those areas within both the
// existing clipping region AND the passed Rect
static inline
void intersect_clip_rect_unlocked(struct clipping_t *clipping, Rect *rect)
{
    RectList *output_rects;
    Rect *current_rect;
    Rect *intersect_rect;

    clipping->clipping_on = 1;

    if(!(output_rects = RectList_new_unlocked()))
    {
        return;
    }


    for(current_rect = clipping->clip_rects->root;
        current_rect != NULL;
        current_rect = current_rect->next)
    {
        intersect_rect = Rect_intersect(current_rect, rect);

        if(intersect_rect)
        {
            RectList_add(output_rects, intersect_rect);
        }
    }


    if(clipping->clip_rects->root)
    {
        clipping->clip_rects->last->next = (Rect *)rect_cache;
        rect_cache = clipping->clip_rects->root;
    }
    
    rect->next = (Rect *)rect_cache;
    rect_cache = rect;

    RectList_free_unlocked(clipping->clip_rects);

    // And re-point it to the new one we built above
    clipping->clip_rects = output_rects;
}


// Update the clipping rectangles to only include those areas within both the
// existing clipping region AND the passed Rect
static inline
void intersect_clip_rect(struct clipping_t *clipping, Rect *rect)
{
    RectList *output_rects;
    Rect *current_rect;
    Rect *intersect_rect;

    clipping->clipping_on = 1;

    if(!(output_rects = RectList_new()))
    {
        return;
    }

    for(current_rect = clipping->clip_rects->root;
        current_rect != NULL;
        current_rect = current_rect->next)
    {
        intersect_rect = Rect_intersect(current_rect, rect);

        if(intersect_rect)
        {
            RectList_add(output_rects, intersect_rect);
        }
    }


    if(clipping->clip_rects->root)
    {
        clipping->clip_rects->last->next = (Rect *)rect_cache;
        rect_cache = clipping->clip_rects->root;
    }
    
    rect->next = (Rect *)rect_cache;
    rect_cache = rect;

    RectList_free_unlocked(clipping->clip_rects);

    // And re-point it to the new one we built above
    clipping->clip_rects = output_rects;
}


// Split all existing clip rectangles against the passed rect
static inline void
subtract_clip_rect_unlocked(struct clipping_t *clipping, Rect *subtracted_rect)
{
    // Check each item already in the list to see if it overlaps with
    // the new rectangle
    Rect *cur_rect, *prev_rect = NULL;
    RectList *split_rects;

    clipping->clipping_on = 1;

    for(cur_rect = clipping->clip_rects->root; cur_rect != NULL; )
    {
        // Standard rect intersect test (if no intersect, skip to next)
        // See here for an example of why this works:
        // http://stackoverflow.com/questions/306316/determine-if-two-rectangles-overlap-each-other#tab-top
        if(!(cur_rect->left <= subtracted_rect->right &&
		     cur_rect->right >= subtracted_rect->left &&
		     cur_rect->top <= subtracted_rect->bottom &&
		     cur_rect->bottom >= subtracted_rect->top))
		{
		    prev_rect = cur_rect;
		    cur_rect = cur_rect->next;
            continue;
        }

        // If this rectangle does intersect with the new rectangle, 
        // we need to split it
        if(prev_rect)
        {
            prev_rect->next = cur_rect->next;
        }

        if(clipping->clip_rects->root == cur_rect)
        {
            clipping->clip_rects->root = cur_rect->next;
        }

        if(clipping->clip_rects->last == cur_rect)
        {
            if(!prev_rect)
            {
                prev_rect = clipping->clip_rects->root;
                
                while(prev_rect && prev_rect->next)
                {
                    prev_rect = prev_rect->next;
                }
            }

            clipping->clip_rects->last = prev_rect;
        }

        // Do the split
        split_rects = Rect_split(cur_rect, subtracted_rect);
        // We can throw this away now, we're done with it
        Rect_free_unlocked(cur_rect);

        // Copy the split, non-overlapping result rectangles into the list 
        if(split_rects->root)
        {
            if(!clipping->clip_rects->root)
            {
                clipping->clip_rects->root = split_rects->root;     
            }
            else
            {
                clipping->clip_rects->last->next = split_rects->root;
            }
        
            clipping->clip_rects->last = split_rects->last;


            if(clipping->clip_rects->root != NULL && clipping->clip_rects->last == NULL)
            {
                __asm__ __volatile__("xchg %%bx, %%bx"::);
                __asm__ __volatile__("xchg %%bx, %%bx"::);
            }
        }

        // Free the empty split_rect list 
        RectList_free_unlocked(split_rects);

        // Since we removed an item from the list, we need to start counting 
        // over again. In this way, we'll only exit this loop once nothing in
        // the list overlaps 
        cur_rect = clipping->clip_rects->root;
        prev_rect = NULL;
    }
}


// Split all existing clip rectangles against the passed rect
static inline
void subtract_clip_rect(struct clipping_t *clipping, Rect *subtracted_rect)
{
    // Check each item already in the list to see if it overlaps with
    // the new rectangle
    Rect *cur_rect, *prev_rect = NULL;
    RectList *split_rects;

    clipping->clipping_on = 1;

    for(cur_rect = clipping->clip_rects->root; cur_rect != NULL; )
    {
        // Standard rect intersect test (if no intersect, skip to next)
        // See here for an example of why this works:
        // http://stackoverflow.com/questions/306316/determine-if-two-rectangles-overlap-each-other#tab-top
        if(!(cur_rect->left <= subtracted_rect->right &&
		     cur_rect->right >= subtracted_rect->left &&
		     cur_rect->top <= subtracted_rect->bottom &&
		     cur_rect->bottom >= subtracted_rect->top))
		{
		    prev_rect = cur_rect;
		    cur_rect = cur_rect->next;
            continue;
        }

        // If this rectangle does intersect with the new rectangle, 
        // we need to split it

        if(prev_rect)
        {
            prev_rect->next = cur_rect->next;
        }

        if(clipping->clip_rects->root == cur_rect)
        {
            clipping->clip_rects->root = cur_rect->next;
        }

        if(clipping->clip_rects->last == cur_rect)
        {
            if(!prev_rect)
            {
                prev_rect = clipping->clip_rects->root;
                
                while(prev_rect && prev_rect->next)
                {
                    prev_rect = prev_rect->next;
                }
            }

            clipping->clip_rects->last = prev_rect;
        }

        // Do the split
        split_rects = Rect_split(cur_rect, subtracted_rect);
        // We can throw this away now, we're done with it
        Rect_free_unlocked(cur_rect);

        if(split_rects->root)
        {
            if(!clipping->clip_rects->root)
            {
                clipping->clip_rects->root = split_rects->root;     
            }
            else
            {
                clipping->clip_rects->last->next = split_rects->root;
            }
        
            clipping->clip_rects->last = split_rects->last;

            if(clipping->clip_rects->root != NULL && clipping->clip_rects->last == NULL)
            {
                __asm__ __volatile__("xchg %%bx, %%bx"::);
                __asm__ __volatile__("xchg %%bx, %%bx"::);
            }
        }

        // Free the empty split_rect list 
        RectList_free_unlocked(split_rects);

        // Since we removed an item from the list, we need to start counting 
        // over again. In this way, we'll only exit this loop once nothing in
        // the list overlaps 
        cur_rect = clipping->clip_rects->root;
        prev_rect = NULL;
    }
}


static inline
void add_clip_rect_unlocked(struct clipping_t *clipping, Rect *added_rect)
{
    subtract_clip_rect_unlocked(clipping, added_rect);

    // Now that we have made sure none of the existing rectangles overlap
    // with the new rectangle, we can finally insert it 
    RectList_add(clipping->clip_rects, added_rect);
}


static inline void add_clip_rect(struct clipping_t *clipping, Rect *added_rect)
{
    subtract_clip_rect(clipping, added_rect);

    // Now that we have made sure none of the existing rectangles overlap
    // with the new rectangle, we can finally insert it 
    RectList_add(clipping->clip_rects, added_rect);
}


// Remove all of the clipping rects from the passed context object
static inline void clear_clip_rects(struct clipping_t *clipping)
{
    clipping->clipping_on = 0;

    if(!clipping->clip_rects->root)
    {
        return;
    }

    clipping->clip_rects->last->next = (Rect *)rect_cache;
    rect_cache = clipping->clip_rects->root;

    clipping->clip_rects->root = NULL;
    clipping->clip_rects->last = NULL;
}


static inline void clear_clip_rects_unlocked(struct clipping_t *clipping)
{
    clipping->clipping_on = 0;

    if(!clipping->clip_rects->root)
    {
        return;
    }

    clipping->clip_rects->last->next = (Rect *)rect_cache;
    rect_cache = clipping->clip_rects->root;
    clipping->clip_rects->root = NULL;
    clipping->clip_rects->last = NULL;
}

#endif      /* GUI_SERVER */
