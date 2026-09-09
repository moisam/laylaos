/*
 * fg_window_laylaos.c
 *
 * The LaylaOS-specific mouse cursor related stuff.
 *
 * Copyright (c) 2012 Stephen J. Baker. All Rights Reserved.
 * Written by John F. Fay, <fayjf@sourceforge.net>
 * Creation date: Sun Jan 22, 2012
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

#define FREEGLUT_BUILDING_LIB
#include <GL/freeglut.h>
#include "../fg_internal.h"
#include "egl/fg_window_egl.h"
#include <gui/panels/top-panel.h>

#define NODECORATION_FLAGS  \
    (WINDOW_NODECORATION | WINDOW_SKIPTASKBAR | WINDOW_ALWAYSONTOP)


static void fghGetDefaultWindowStyle(uint32_t *flags)
{
    if ( fgState.DisplayMode & GLUT_BORDERLESS )
    {
        /* no window decorations needed */
        (*flags) |= NODECORATION_FLAGS;
    }
    else if ( fgState.DisplayMode & GLUT_CAPTIONLESS )
        /* XXX: only window decoration is a border, no title bar or buttons */
        (*flags) |= WINDOW_NORESIZE | WINDOW_NOCONTROLBOX;
    else
        /* window decoration are a border, title bar and buttons. */
        (*flags) |= 0;
}


/*
 * Opens a window. Requires a SFG_Window object created and attached
 * to the freeglut structure. OpenGL context is created here.
 */
void fgPlatformOpenWindow( SFG_Window* window, const char* title,
                           GLboolean positionUse, int x, int y,
                           GLboolean sizeUse, int w, int h,
                           GLboolean gameMode, GLboolean isSubWindow )
{
    uint32_t flags;
    struct window_attribs_t attribs;

    /* Determine window style flags */
    if( gameMode )
    {
        FREEGLUT_INTERNAL_ERROR_EXIT ( window->Parent == NULL,
                                       "Game mode being invoked on a subwindow",
                                       "fgOpenWindow" );

        /*
         * Set the window creation flags appropriately to make the window
         * entirely visible:
         */
        flags = NODECORATION_FLAGS;
    }
    else
    {
        flags = 0;

        /*
         * There's a small difference between creating the top, child and
         * menu windows
         */
        if ( window->IsMenu )
        {
            flags |= NODECORATION_FLAGS;
        }
        /* if this is not a subwindow (child), set its style based on the requested window decorations */
        else if( window->Parent == NULL )
            fghGetDefaultWindowStyle(&flags);
        else
            /* subwindows always have no decoration */
            flags |= WINDOW_NODECORATION | WINDOW_SKIPTASKBAR;
    }

    if( !positionUse )
    {
        x = 0;
        y = 0;
        attribs.gravity = WINDOW_ALIGN_CENTERH | WINDOW_ALIGN_CENTERV;
    }
    else
    {
        // ensure that window position 0, 0 does not fall behind the top panel
        if(y <= 0)
        {
            y = TOPPANEL_HEIGHT;
        }

        attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
    }

    if( !sizeUse )
    {
        w = 300;
        h = 300;
    }

    attribs.x = x;
    attribs.y = y;
    attribs.w = w;
    attribs.h = h;
    attribs.flags = flags;

    window->Window.Handle = window_create(&attribs);

    if( !( window->Window.Handle ) )
        fgError( "Failed to create a window (%s)!", title );

    if( title != NULL )
        window_set_title(window->Window.Handle, (char *)title);

    if( !window->IsMenu )    /* Don't show window after creation if its a menu */
    {
        window_show(window->Window.Handle);
		window->State.Visible = GL_TRUE;
    }

    /*
    if( gameMode )
    {
        window_enter_fullscreen(window->Window.Handle);
    }
    */

    /* Create context */
    fghChooseConfig(&window->Window.pContext.egl.Config);

    if( ! window->Window.pContext.egl.Config )
    {
        /*
         * The "fghChooseConfig" returned a null meaning that the visual
         * context is not available.
         * Try a couple of variations to see if they will work.
         */
        if( fgState.DisplayMode & GLUT_MULTISAMPLE )
        {
            fgState.DisplayMode &= ~GLUT_MULTISAMPLE ;
            fghChooseConfig( &window->Window.pContext.egl.Config );
            fgState.DisplayMode |= GLUT_MULTISAMPLE;
        }
    }

    FREEGLUT_INTERNAL_ERROR_EXIT( window->Window.pContext.egl.Config != NULL,
                                  "EGL configuration with necessary capabilities "
                                  "not found", "fgOpenWindow" );

	if(window->IsMenu) {
		/*
		 * If there isn't already an OpenGL rendering context for menu
		 * windows, make one
		 */
		if(!fgStructure.MenuContext) {
			fgStructure.MenuContext = malloc(sizeof *fgStructure.MenuContext);
			fgStructure.MenuContext->MContext = fghCreateNewContextEGL(window);
		}

		/* window->Window.Context = fgStructure.MenuContext->MContext; */
		window->Window.Context = fghCreateNewContextEGL(window);

	} else if(fgState.UseCurrentContext) {
		window->Window.Context = eglGetCurrentContext();

		if(!window->Window.Context) {
			window->Window.Context = fghCreateNewContextEGL(window);
		}
	} else {
		window->Window.Context = fghCreateNewContextEGL(window);
	}

    fghPlatformOpenWindowEGL(window);

    /* Bind context to the current thread if it's lost */
    if (eglGetCurrentContext() == EGL_NO_CONTEXT &&
        eglMakeCurrent(fgDisplay.pDisplay.egl.Display,
               window->Window.pContext.egl.Surface,
               window->Window.pContext.egl.Surface,
               window->Window.Context) == EGL_FALSE)
        fgError("eglMakeCurrent: err=%x\n", eglGetError());
}


/*
 * Request a window resize
 */
void fgPlatformReshapeWindow ( SFG_Window *window, int width, int height )
{
    struct window_t *w = window->Window.Handle;

    if( w == NULL ) return;

    window_set_size(w, w->x, w->y, width, height);
    w->w = width;
    w->h = height;
}


/*
 * Closes a window, destroying the frame and OpenGL context
 */
void fgPlatformCloseWindow( SFG_Window* window )
{
    fghPlatformCloseWindowEGL(window);

    if( window->Window.Handle ) {
        window_destroy( window->Window.Handle );
        window->Window.Handle = NULL;
    }
}


/*
 * This function makes the specified window visible
 */
void fgPlatformShowWindow( SFG_Window *window )
{
    if( window->Window.Handle == NULL ) return;

    window_show(window->Window.Handle);
}


/*
 * This function hides the specified window
 */
void fgPlatformHideWindow( SFG_Window *window )
{
    if( window->Window.Handle == NULL ) return;

    window_hide(window->Window.Handle);
}


/*
 * Iconify the specified window (top-level windows only)
 */
void fgPlatformIconifyWindow( SFG_Window *window )
{
    if( window->Window.Handle == NULL ) return;

    window_minimize(window->Window.Handle);
    fgStructure.CurrentWindow->State.Visible   = GL_FALSE;
}


/*
 * Set the current window's title
 */
void fgPlatformGlutSetWindowTitle( const char* title )
{
    if( fgStructure.CurrentWindow->Window.Handle == NULL ) return;

    window_set_title(fgStructure.CurrentWindow->Window.Handle, (char *)title);
}

/*
 * Set the current window's iconified title
 */
void fgPlatformGlutSetIconTitle( const char* title )
{
    // XXX: ToDo
    fprintf(stderr, "fgPlatformGlutSetIconTitle(): STUB\n");
}

/*
 * Change the specified window's position
 */
void fgPlatformPositionWindow( SFG_Window *window, int x, int y )
{
    if( window->Window.Handle == NULL ) return;

    window_set_pos(window->Window.Handle, x, y);
}

/*
 * Lowers the specified window (by Z order change)
 */
void fgPlatformPushWindow( SFG_Window *window )
{
    if( window->Window.Handle == NULL ) return;

    // XXX: this should lower, not minimize, the window
    window_minimize(window->Window.Handle);
}

/*
 * Raises the specified window (by Z order change)
 */
void fgPlatformPopWindow( SFG_Window *window )
{
    if( window->Window.Handle == NULL ) return;

    window_raise(window->Window.Handle);
}

/*
 * Toggle the window's full screen state.
 */
void fgPlatformFullScreenToggle( SFG_Window *window )
{
    struct window_t *w = window->Window.Handle;

    if( w == NULL ) return;

    // make sure it is resizable or the window manager won't do it
    if(w->flags & WINDOW_NORESIZE) {
        window_set_resizable(w, 1);
    }

    if(glutGet(GLUT_FULL_SCREEN)) {
        /* restore original window size */
        fgStructure.CurrentWindow->State.WorkMask = GLUT_SIZE_WORK;
        fgStructure.CurrentWindow->State.DesiredWidth  = window->State.pWState.OldWidth;
        fgStructure.CurrentWindow->State.DesiredHeight = window->State.pWState.OldHeight;

        window_exit_fullscreen(w);
    } else {
        fgStructure.CurrentWindow->State.pWState.OldWidth  = window->State.Width;
        fgStructure.CurrentWindow->State.pWState.OldHeight = window->State.Height;

        /* resize the window to cover the entire screen */
        window_enter_fullscreen(w);
    }

    window->State.IsFullscreen = !window->State.IsFullscreen;
}

