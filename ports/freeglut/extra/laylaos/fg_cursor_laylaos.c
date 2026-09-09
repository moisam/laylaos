/*
 * fg_cursor_laylaos.c
 *
 * The LaylaOS-specific mouse cursor related stuff.
 *
 * Copyright (c) 2012 Stephen J. Baker. All Rights Reserved.
 * Written by John F. Fay, <fayjf@sourceforge.net>
 * Creation date: Thu Jan 19, 2012
 * Copyright (c) 2025 Mohammed Isam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PAWEL W. OLSZTA BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <GL/freeglut.h>
#include "../fg_internal.h"



void fgPlatformSetCursor ( SFG_Window *window, int cursorID )
{
    struct window_t *w = window->Window.Handle;

    if (w == NULL) return;

    switch( cursorID )
    {
        case GLUT_CURSOR_RIGHT_ARROW:
            cursor_show(w, CURSOR_NORMAL);
            break;
        case GLUT_CURSOR_LEFT_ARROW:                /* XXX ToDo */
            cursor_show(w, CURSOR_NORMAL);
            break;
        case GLUT_CURSOR_INFO:
            cursor_show(w, CURSOR_POINTING_HAND);
            break;
        case GLUT_CURSOR_DESTROY:
            cursor_show(w, CURSOR_X);
            break;
        case GLUT_CURSOR_HELP:
            cursor_show(w, CURSOR_HELP);
            break;
        case GLUT_CURSOR_CYCLE:                     /* XXX ToDo */
            cursor_show(w, CURSOR_NORMAL);
            break;
        case GLUT_CURSOR_SPRAY:
            cursor_show(w, CURSOR_CROSSHAIR);
            break;
        case GLUT_CURSOR_WAIT:
            cursor_show(w, CURSOR_WAITING);
            break;
        case GLUT_CURSOR_TEXT:
            cursor_show(w, CURSOR_IBEAM);
            break;
        case GLUT_CURSOR_CROSSHAIR:
            cursor_show(w, CURSOR_CROSSHAIR);
            break;
        case GLUT_CURSOR_UP_DOWN:
            cursor_show(w, CURSOR_NS);
            break;
        case GLUT_CURSOR_LEFT_RIGHT:
            cursor_show(w, CURSOR_WE);
            break;
        case GLUT_CURSOR_TOP_SIDE:
            cursor_show(w, CURSOR_N);
            break;
        case GLUT_CURSOR_BOTTOM_SIDE:
            cursor_show(w, CURSOR_S);
            break;
        case GLUT_CURSOR_LEFT_SIDE:
            cursor_show(w, CURSOR_W);
            break;
        case GLUT_CURSOR_RIGHT_SIDE:
            cursor_show(w, CURSOR_E);
            break;
        case GLUT_CURSOR_TOP_LEFT_CORNER:
            cursor_show(w, CURSOR_NW);
            break;
        case GLUT_CURSOR_TOP_RIGHT_CORNER:
            cursor_show(w, CURSOR_NE);
            break;
        case GLUT_CURSOR_BOTTOM_RIGHT_CORNER:
            cursor_show(w, CURSOR_SE);
            break;
        case GLUT_CURSOR_BOTTOM_LEFT_CORNER:
            cursor_show(w, CURSOR_SW);
            break;
        case GLUT_CURSOR_FULL_CROSSHAIR:
            cursor_show(w, CURSOR_FLEUR);
            break;

        case GLUT_CURSOR_NONE:
            cursor_hide(w);
            break;

        case GLUT_CURSOR_INHERIT:
        {
            SFG_Window *temp_window = window;
            while (temp_window->Parent)
            {
                temp_window = temp_window->Parent;
                if (temp_window->State.Cursor != GLUT_CURSOR_INHERIT)
                {
                    fgPlatformSetCursor(window, temp_window->State.Cursor);
                    return;
                }
            }
            /* No parent, or no parent with cursor type set. Fall back to default */
            fgPlatformSetCursor(window, GLUT_CURSOR_LEFT_ARROW);
        }
        break;

    default:
        fgError( "Unknown cursor type: %d", cursorID );
        break;
    }
}


void fgPlatformWarpPointer ( int x, int y )
{
    struct window_t *w;

    if (fgStructure.CurrentWindow == NULL) return;

    w = fgStructure.CurrentWindow->Window.Handle;
    x = to_child_x(w, x);
    y = to_child_y(w, y);
    cursor_set_pos(x, y);
}


void fghPlatformGetCursorPos(const SFG_Window *window, GLboolean client, SFG_XYUse *mouse_pos)
{
    /* Get current pointer location in screen coordinates (if client is false or window is NULL), else
     * Get current pointer location relative to top-left of client area of window (if client is true and window is not NULL)
     */
    struct cursor_info_t curinfo = { 0, 0, 0, 0, 0, };
    
    cursor_get_info(&curinfo);

    /* convert to client coords if wanted */
    if (client && window && window->Window.Handle)
    {
        struct window_t *w = window->Window.Handle;

        curinfo.x -= w->x;
        curinfo.y -= w->y;
    }

    mouse_pos->X = curinfo.x;
    mouse_pos->Y = curinfo.y;
    mouse_pos->Use = GL_TRUE;
}

