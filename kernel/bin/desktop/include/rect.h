/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: rect.h
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
 *  \file rect.h
 *
 *  Functions to work with rectangles. The code is divided into 3 files:
 *  - common/rect.c: initialises the rect cache,
 *  - include/rect.h: inlined rect creation and manipulation functions,
 *  - include/rect-struct.h: rect struct definition.
 *
 *  This code is based on the "Windowing Systems by Example" blog series.
 *  The source code it released under the MIT license and can be found at:
 *  https://github.com/JMarlin/wsbe.
 */

#ifndef GUI_RECT_H
#define GUI_RECT_H

#include "list.h"
#include "rect-struct.h"
#include <stdlib.h>
#include "mutex.h"


#ifdef __cplusplus
extern "C" {
#endif

// defined in common/rect.c
void prep_rect_cache(void);

extern volatile Rect *rect_cache;
extern mutex_t cache_lock;


// Allocate a new rectangle object
static inline Rect *Rect_new(int top, int left, int bottom, int right)
{
    Rect *rect;

    if(rect_cache)
    {
        rect = (Rect *)rect_cache;
        rect_cache = rect_cache->next;
    }

    // Attempt to allocate the object
    else if(!(rect = (Rect*)malloc(sizeof(Rect))))
    {
        return rect;
    }

    // Assign intial values
    rect->top = top;
    rect->left = left;
    rect->bottom = bottom;
    rect->right = right;
    rect->next = NULL;

    return rect;
}

/*
 * MUST be called with rect_lock locked.
 */
static inline Rect *Rect_new_unlocked(int top, int left, int bottom, int right)
{
    Rect *rect;

    if(rect_cache)
    {
        rect = (Rect *)rect_cache;
        rect_cache = rect_cache->next;
    }

    // Attempt to allocate the object
    else
    {
        if(!(rect = (Rect*)malloc(sizeof(Rect))))
        {
            return rect;
        }
    }

    // Assign intial values
    rect->top = top;
    rect->left = left;
    rect->bottom = bottom;
    rect->right = right;
    rect->next = NULL;

    return rect;
}

static inline void Rect_free_unlocked(Rect *rect)
{
    rect->next = (Rect *)rect_cache;
    rect_cache = rect;
}

static inline void Rect_free(Rect *rect)
{
    rect->next = (Rect *)rect_cache;
    rect_cache = rect;
}

// Explode subject_rect into a list of contiguous rects which are
// not occluded by cutting_rect
// ________                ____ ___
//|s    ___|____          |o   |o__|
//|____|___|   c|   --->  |____|          
//     |________|              
//
// MUST be called with rect_lock locked.
//
static inline RectList *Rect_split(Rect *subject_rect, Rect *cutting_rect)
{
    // Allocate the list of result rectangles
    RectList* output_rects;

    if(!(output_rects = RectList_new_unlocked()))
    {
        return output_rects;
    }

    // We're going to modify the subject rect as we go,
    // so we'll clone it so as to not upset the object 
    // we were passed
    Rect subject_copy;
    subject_copy.top = subject_rect->top;
    subject_copy.left = subject_rect->left;
    subject_copy.bottom = subject_rect->bottom;
    subject_copy.right = subject_rect->right;

    // We need a rectangle to hold new rectangles before
    // they get pushed into the output list
    Rect *temp_rect, *next_rect;

    // Begin splitting
    // 1 - Split by left edge if that edge is between the subject's left
    // and right edges 
    if(cutting_rect->left > subject_copy.left && 
       cutting_rect->left <= subject_copy.right)
    {
        // Try to make a new rectangle spanning from the subject 
        // rectangle's left and stopping before the cutting rectangle's left
        if(!(temp_rect = Rect_new_unlocked(subject_copy.top, 
                                           subject_copy.left,
                                           subject_copy.bottom, 
                                           cutting_rect->left - 1)))
        {
            // If the object creation failed, we need to delete the list
            // and exit failed
            RectList_free_unlocked(output_rects);

            return (RectList*)0;
        }

        // Add the new rectangle to the output list
        RectList_add(output_rects, temp_rect);

        // Shrink the subject rectangle to exclude the split portion
        subject_copy.left = cutting_rect->left;
    }

    // 2 - Split by top edge if that edge is between the subject's top
    // and bottom edges 
    if(cutting_rect->top > subject_copy.top &&
       cutting_rect->top <= subject_copy.bottom)
    {
        // Try to make a new rectangle spanning from the subject rectangle's
        // top and stopping before the cutting rectangle's top
        if(!(temp_rect = Rect_new_unlocked(subject_copy.top, 
                                           subject_copy.left,
                                           cutting_rect->top - 1, 
                                           subject_copy.right)))
        {
            // If the object creation failed, we need to delete the list and
            // exit failed
            // This time, also delete any previously allocated rectangles
            for(temp_rect = output_rects->root; temp_rect != NULL; )
            {
                next_rect = temp_rect->next;
                Rect_free_unlocked(temp_rect);
                temp_rect = next_rect;
            }

            RectList_free_unlocked(output_rects);

            return (RectList*)0;
        }

        // Add the new rectangle to the output list
        RectList_add(output_rects, temp_rect);

        // Shrink the subject rectangle to exclude the split portion
        subject_copy.top = cutting_rect->top;
    }

    // 3 - Split by right edge if that edge is between the subject's
    // left and right edges 
    if(cutting_rect->right >= subject_copy.left &&
       cutting_rect->right < subject_copy.right)
    {
        // Try to make a new rectangle spanning from the subject rectangle's
        // right and stopping before the cutting rectangle's right
        if(!(temp_rect = Rect_new_unlocked(subject_copy.top, 
                                           cutting_rect->right + 1,
                                           subject_copy.bottom, 
                                           subject_copy.right)))
        {
            // Free on fail
            for(temp_rect = output_rects->root; temp_rect != NULL; )
            {
                next_rect = temp_rect->next;
                Rect_free_unlocked(temp_rect);
                temp_rect = next_rect;
            }

            RectList_free_unlocked(output_rects);

            return (RectList*)0;
        }

        // Add the new rectangle to the output list
        RectList_add(output_rects, temp_rect);

        // Shrink the subject rectangle to exclude the split portion
        subject_copy.right = cutting_rect->right;
    }

    // 4 - Split by bottom edge if that edge is between the subject's
    // top and bottom edges 
    if(cutting_rect->bottom >= subject_copy.top &&
       cutting_rect->bottom < subject_copy.bottom)
    {
        // Try to make a new rectangle spanning from the subject rectangle's
        // bottom and stopping before the cutting rectangle's bottom
        if(!(temp_rect = Rect_new_unlocked(cutting_rect->bottom + 1, 
                                           subject_copy.left,
                                           subject_copy.bottom, 
                                           subject_copy.right)))
        {
            // Free on fail
            for(temp_rect = output_rects->root; temp_rect != NULL; )
            {
                next_rect = temp_rect->next;
                Rect_free_unlocked(temp_rect);
                temp_rect = next_rect;
            }

            RectList_free_unlocked(output_rects);

            return (RectList*)0;
        }

        // Add the new rectangle to the output list
        RectList_add(output_rects, temp_rect);

        // Shrink the subject rectangle to exclude the split portion
        subject_copy.bottom = cutting_rect->bottom;
    }
 
    // Finally, after all that, we can return the output rectangles 
    return output_rects;
}

/*
 * MUST be called with rect_lock locked.
 */
static inline Rect *Rect_intersect(Rect *rect_a, Rect *rect_b)
{
    Rect* result_rect;

    if(!(rect_a->left <= rect_b->right &&
	   rect_a->right >= rect_b->left &&
	   rect_a->top <= rect_b->bottom &&
	   rect_a->bottom >= rect_b->top))
	{
        return (Rect*)0;
    }

    if(!(result_rect = Rect_new_unlocked(rect_a->top, rect_a->left,
                                         rect_a->bottom, rect_a->right)))
    {
        return (Rect*)0;
    }

    if(rect_b->left > result_rect->left && rect_b->left <= result_rect->right) 
    {
        result_rect->left = rect_b->left;
    }
 
    if(rect_b->top > result_rect->top && rect_b->top <= result_rect->bottom) 
    {
        result_rect->top = rect_b->top;
    }

    if(rect_b->right >= result_rect->left && rect_b->right < result_rect->right)
    {
        result_rect->right = rect_b->right;
    }

    if(rect_b->bottom >= result_rect->top && rect_b->bottom < result_rect->bottom)
    {
        result_rect->bottom = rect_b->bottom;
    }

    return result_rect;
}


#ifdef __cplusplus
}
#endif

#endif      /* GUI_RECT_H */
