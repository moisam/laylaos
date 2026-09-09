/*
 * fg_state_laylaos.c
 *
 * LaylaOS-specific freeglut state query methods.
 *
 * Copyright (c) 2012 Stephen J. Baker. All Rights Reserved.
 * Written by John F. Fay, <fayjf@sourceforge.net>
 * Creation date: Sat Feb 4 2012
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
#include "fg_internal.h"
#include "egl/fg_state_egl.h"

int fgPlatformGlutDeviceGet ( GLenum eWhat )
{
    switch( eWhat )
    {
    case GLUT_HAS_KEYBOARD:
        return 1;

    case GLUT_HAS_MOUSE:
        return 1;

    case GLUT_NUM_MOUSE_BUTTONS:
        /*
         * XXX: fix this
         */
        return 3;

    default:
        fgWarning( "glutDeviceGet(): missing enum handle %d", eWhat );
        break;
    }

    /* And now -- the failure. */
    return -1;
}


int fgPlatformGlutGet ( GLenum eWhat )
{
    switch( eWhat )
    {

    case GLUT_WINDOW_X:
    case GLUT_WINDOW_Y:
    {
        int x = 0, y = 0;
        struct window_t *p, *win;

        if (fgStructure.CurrentWindow == NULL)
            return 0;

        if (fgStructure.CurrentWindow->Parent)
        {
            /* For child window, we should return relative to upper-left
             * of parent's client area.
             */
            p = fgStructure.CurrentWindow->Parent->Window.Handle;
            if (p != NULL)
            {
                x = p->x;
                y = p->y;
            }
        }

        win = fgStructure.CurrentWindow->Window.Handle;
        x = win->x - x;
        y = win->y - y;

        if (!(win->flags & WINDOW_NODECORATION))
        {
            x += WINDOW_BORDERWIDTH;
            y += WINDOW_TITLEHEIGHT;
        }

        switch ( eWhat )
        {
        case GLUT_WINDOW_X: return x;
        case GLUT_WINDOW_Y: return y;
        }
    }
    
    case GLUT_WINDOW_BORDER_WIDTH:
    case GLUT_WINDOW_HEADER_HEIGHT:
    {
        int w = 0, h = 0;
        struct window_t *win;

        if (fgStructure.CurrentWindow == NULL)
            return 0;

        win = fgStructure.CurrentWindow->Window.Handle;

        if (!(win->flags & WINDOW_NODECORATION))
        {
            w = WINDOW_BORDERWIDTH;
            h = WINDOW_TITLEHEIGHT;
        }

        switch ( eWhat )
        {
        case GLUT_WINDOW_BORDER_WIDTH:  return w;
        case GLUT_WINDOW_HEADER_HEIGHT: return h;
        }
    }

    case GLUT_WINDOW_WIDTH:
    case GLUT_WINDOW_HEIGHT:
    {
        struct window_t *win;

        if (fgStructure.CurrentWindow == NULL)
            return 0;

        win = fgStructure.CurrentWindow->Window.Handle;

        switch ( eWhat )
        {
        case GLUT_WINDOW_WIDTH:            return win->w;
        case GLUT_WINDOW_HEIGHT:           return win->h;
        }
    }
    
    case GLUT_WINDOW_COLORMAP_SIZE:
		if(!fgStructure.CurrentWindow) {
			return 0;
		}
		return fgStructure.CurrentWindow->Window.cmap_size;

    default:
        return fghPlatformGlutGetEGL(eWhat);
    }
}

