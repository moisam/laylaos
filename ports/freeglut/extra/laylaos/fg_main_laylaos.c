/*
 * fg_main_laylaos.c
 *
 * The LaylaOS-specific windows message processing methods.
 *
 * Copyright (c) 1999-2000 Pawel W. Olszta. All Rights Reserved.
 * Written by Pawel W. Olszta, <olszta@sourceforge.net>
 * Copied for Platform code by Evan Felix <karcaw at gmail.com>
 * Creation date: Thur Feb 2 2012
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
#include <errno.h>
#include <stdarg.h>
#include <kernel/keycodes.h>
#include <gui/keys.h>
#include <gui/kbd.h>


extern void fghOnReshapeNotify(SFG_Window *window, int width, int height, GLboolean forceNotify);
extern void fghOnPositionNotify(SFG_Window *window, int x, int y, GLboolean forceNotify);
extern void fghRedrawWindowAndChildren ( SFG_Window *window );
extern void fgPlatformFullScreenToggle( SFG_Window *win );
extern void fgPlatformPositionWindow( SFG_Window *window, int x, int y );
extern void fgPlatformReshapeWindow ( SFG_Window *window, int width, int height );
extern void fgPlatformPushWindow( SFG_Window *window );
extern void fgPlatformPopWindow( SFG_Window *window );
extern void fgPlatformHideWindow( SFG_Window *window );
extern void fgPlatformIconifyWindow( SFG_Window *window );
extern void fgPlatformShowWindow( SFG_Window *window );


fg_time_t fgPlatformSystemTime ( void )
{
#ifdef CLOCK_MONOTONIC
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_nsec/1000000 + now.tv_sec*1000;
#elif defined(HAVE_GETTIMEOFDAY)
    struct timeval now;
    gettimeofday( &now, NULL );
    return now.tv_usec/1000 + now.tv_sec*1000;
#endif
}


/*
 * Does the magic required to relinquish the CPU until something interesting
 * happens.
 */

void fgPlatformSleepForEvents( fg_time_t msec )
{
    while(1)
    {
        if(pending_events_timeout(msec * 1000))
        {
            break;
        }
    }
}


/*
 * Determine a GLUT modifier mask based on LaylaOS system info.
 */
static int fgPlatformGetModifiers (char modkeys)
{
    return ((modkeys & MODIFIER_MASK_SHIFT) ? GLUT_ACTIVE_SHIFT : 0) |
           ((modkeys & MODIFIER_MASK_ALT  ) ? GLUT_ACTIVE_ALT   : 0) |
           ((modkeys & MODIFIER_MASK_CTRL ) ? GLUT_ACTIVE_CTRL  : 0);
}


static void fghPlatformOnWindowStatusNotify(SFG_Window *window, GLboolean visState, GLboolean forceNotify)
{
    GLboolean notify = GL_FALSE;
    SFG_Window* child;

    if (window->State.Visible != visState)
    {
        window->State.Visible = visState;
        notify = GL_TRUE;
    }

    if (notify || forceNotify)
    {
        SFG_Window *saved_window = fgStructure.CurrentWindow;

        /* Here we only have two states, window displayed and window not displayed (iconified)
         * We map these to GLUT_FULLY_RETAINED and GLUT_HIDDEN respectively.
         */
        INVOKE_WCB( *window, WindowStatus, ( visState ? GLUT_FULLY_RETAINED:GLUT_HIDDEN ) );
        fgSetWindow( saved_window );
    }

    /* Also set windowStatus/visibility state for children */
    for( child = ( SFG_Window * )window->Children.First;
         child;
         child = ( SFG_Window * )child->Node.Next )
    {
        fghPlatformOnWindowStatusNotify(child, visState, GL_FALSE); /* No need to propagate forceNotify. Childs get this from their own INIT_WORK */
    }
}


static void updateWindowState(struct event_t *ev)
{
    struct window_t *win;
    SFG_Window *window;

    win = win_for_winid(ev->dest);
    if (win == NULL) return;

    window = fgWindowByHandle(win);
    if (window == NULL) return;

    /* Update visibility state of the window */
    if (ev->winst.state == WINDOW_STATE_MINIMIZED) {
        fghPlatformOnWindowStatusNotify(window, GL_FALSE, GL_FALSE);
        window->State.WorkMask &= ~GLUT_DISPLAY_WORK;
    } else {
        fghPlatformOnWindowStatusNotify(window, GL_TRUE, GL_FALSE);
        window->State.WorkMask |= GLUT_DISPLAY_WORK;
    }
}


#define GETWINDOW(ev)                           \
    win = win_for_winid(ev->dest);              \
    if(win == NULL) break;                      \
    window = fgWindowByHandle(win);             \
    if(window == NULL) break;

#define GETMOUSE(ev)                            \
    window->State.MouseX = ev->mouse.x;         \
    window->State.MouseY = ev->mouse.y;


void fgPlatformProcessSingleEvent ( void )
{
    struct event_t *ev = NULL;
    struct window_t *win;
    SFG_Window *window;

    while ((ev = next_event_for_seqid(NULL, 0, 0))) {

        switch (ev->type) {
                case EVENT_WINDOW_POS_CHANGED:
                {
                    int x, y;

                    GETWINDOW(ev);

                    win->x = ev->win.x;
                    win->y = ev->win.y;
                    x = win->x;
                    y = win->y;

                    if (window->Parent)
                    {
                        /* For child window, we should return relative to upper-left
                         * of parent's client area.
                         */
                        struct window_t *p = window->Parent->Window.Handle;

                        x -= p->x;
                        y -= p->y;
                    }

                    /* Update state and call callback, if there was a change */
                    fghOnPositionNotify(window, x, y, GL_FALSE);
                    break;
                }

                case EVENT_WINDOW_RESIZE_OFFER:
                {
                    int x, y;
                    unsigned int w, h;

                    GETWINDOW(ev);

                    x = ev->win.x;
                    y = ev->win.y;
                    w = ev->win.w;
                    h = ev->win.h;

                    window_resize(win, x, y, w, h);

                    if (window->Parent)
                    {
                        /* For child window, we should return relative to upper-left
                         * of parent's client area.
                         */
                        struct window_t *p = window->Parent->Window.Handle;

                        x -= p->x;
                        y -= p->y;
                    }

                    /* Update state and call callback, if there was a change */
                    fghOnPositionNotify(window, x, y, GL_FALSE);

                    /* Update state and call callback, if there was a change */
                    fghOnReshapeNotify(window, w, h, GL_FALSE);

                    /* Now directly call the drawing function to update
                     * window and window's children.
                     */
                    fghRedrawWindowAndChildren(window);

                    window_invalidate(win);
                    break;
                }

                case EVENT_WINDOW_STATE:
                    updateWindowState(ev);
                    break;

                case EVENT_WINDOW_GAINED_FOCUS:
                    GETWINDOW(ev);
                    window_invalidate(win);
                    break;

                case EVENT_WINDOW_LOST_FOCUS:
                    break;

                case EVENT_WINDOW_LOWERED:
                case EVENT_WINDOW_RAISED:
                case EVENT_WINDOW_SHOWN:
                case EVENT_WINDOW_HIDDEN:
                    updateWindowState(ev);
                    break;

                case EVENT_WINDOW_CLOSING:
                    GETWINDOW(ev);
                    fgDestroyWindow ( window );

                    if( fgState.ActionOnWindowClose == GLUT_ACTION_EXIT )
                    {
                        fgDeinitialize( );
                        exit( 0 );
                    }
                    else if( fgState.ActionOnWindowClose == GLUT_ACTION_GLUTMAINLOOP_RETURNS )
                        fgState.ExecState = GLUT_EXEC_STATE_STOP;

                    break;

                case EVENT_MOUSE_ENTER:
                    GETWINDOW(ev);
                    GETMOUSE(ev);
                    win->last_button_state = ev->mouse.buttons;
                    INVOKE_WCB( *window, Entry, (GLUT_ENTERED));
                    break;

                case EVENT_MOUSE_EXIT:
                    GETWINDOW(ev);
                    GETMOUSE(ev);

                    if( window->IsMenu && window->ActiveMenu && window->ActiveMenu->IsActive )
                        fgUpdateMenuHighlight( window->ActiveMenu );

                    INVOKE_WCB( *window, Entry, (GLUT_LEFT));
                    break;

#define BUTTON_PRESSED(which)   \
    (!(obuttons & MOUSE_ ## which ## _DOWN) &&  \
      (nbuttons & MOUSE_ ## which ## _DOWN))

#define BUTTON_RELEASED(which)  \
    ((obuttons & MOUSE_ ## which ## _DOWN) &&  \
     !(nbuttons & MOUSE_ ## which ## _DOWN))

                case EVENT_MOUSE:
                {
                    GETWINDOW(ev);
                    GETMOUSE(ev);

                    mouse_buttons_t obuttons = win->last_button_state;
                    mouse_buttons_t nbuttons = ev->mouse.buttons;
                    int pressed, released;

                    win->last_button_state = nbuttons;

                    pressed  = (BUTTON_PRESSED(LBUTTON) ? 0 :
                                (BUTTON_PRESSED(RBUTTON) ? 1 :
                                 (BUTTON_PRESSED(MBUTTON) ? 2 : -1)));

                    released = (BUTTON_RELEASED(LBUTTON) ? 0 :
                                (BUTTON_RELEASED(RBUTTON) ? 1 :
                                 (BUTTON_RELEASED(MBUTTON) ? 2 : -1)));

                    if(ev->mouse.buttons & MOUSE_VSCROLL_DOWN) pressed = 3;
                    if(ev->mouse.buttons & MOUSE_VSCROLL_UP) pressed = 4;

                    fgState.Modifiers = fgPlatformGetModifiers( get_modifier_keys() );

                    if( pressed >= 0 || released >= 0 )
                    {
                        /*
                         * Do not execute the application's mouse callback if a menu
                         * is hooked to this button.  In that case an appropriate
                         * private call should be generated.
                         */
                        if(fgCheckActiveMenu( window, (pressed >= 0) ? pressed : released, 
                                                      (pressed >= 0) ? GL_TRUE : GL_FALSE, 
                                                      ev->mouse.x, ev->mouse.y))
                            break;

                        /*
                         * Check if there is a mouse or mouse wheel callback hooked to the
                         * window
                         */
                        if(!FETCH_WCB(*window, Mouse) && !FETCH_WCB(*window, MouseWheel))
                            break;

                        fgSetWindow( window );

                        /* Finally execute the mouse or mouse wheel callback.
                         * If a wheel callback hasn't been registered, we simply treat them
                         * as button presses and pass them to the mouse handler. This is
                         * important for compatibility with the original GLUT.
                         */
                        int button = (pressed >= 0) ? pressed : released;

                        if(button < 3 || button > 4 || !FETCH_WCB(*window, MouseWheel)) {
                            INVOKE_WCB(*window, Mouse, 
                                        (button, (pressed >= 0) ? GLUT_DOWN : GLUT_UP, 
                                        ev->mouse.x, ev->mouse.y));
                        } else {
                            if(pressed >= 0) {
                                int dir = button & 1 ? 1 : -1;
                                /* there's no way to know if X buttons after 5 are more
                                 * wheels/wheel axes, or regular buttons. So we'll only
                                 * ever invoke the wheel CB for wheel 0.
                                 */
                                INVOKE_WCB(*window, MouseWheel, (0, dir, ev->mouse.x, ev->mouse.y));
                            }
                        }
                    }

                    if( window->ActiveMenu ) {
                        fgUpdateMenuHighlight( window->ActiveMenu );
                        break;
                    }

                    if( ( ev->mouse.buttons & MOUSE_LBUTTON_DOWN ) ||
                        ( ev->mouse.buttons & MOUSE_MBUTTON_DOWN ) ||
                        ( ev->mouse.buttons & MOUSE_RBUTTON_DOWN ) )
                        INVOKE_WCB( *window, Motion, ( window->State.MouseX,
                                                       window->State.MouseY ) );
                    else
                        INVOKE_WCB( *window, Passive, ( window->State.MouseX,
                                                        window->State.MouseY ) );

                    fgState.Modifiers = INVALID_MODIFIERS;
                    break;
                }

                case EVENT_KEY_PRESS:
                case EVENT_KEY_RELEASE:
                {
                    FGCBKeyboardUC keyboard_cb;
                    FGCBSpecialUC special_cb;
                    FGCBUserData keyboard_ud;
                    FGCBUserData special_ud;
                    char printable;

                    GETWINDOW(ev);

                    if( ev->type == EVENT_KEY_PRESS ) {
                        keyboard_cb = (FGCBKeyboardUC)( FETCH_WCB( *window, Keyboard ));
                        special_cb  = (FGCBSpecialUC) ( FETCH_WCB( *window, Special  ));
                        keyboard_ud = FETCH_USER_DATA_WCB( *window, Keyboard );
                        special_ud  = FETCH_USER_DATA_WCB( *window, Special  );
                    } else {
                        keyboard_cb = (FGCBKeyboardUC)( FETCH_WCB( *window, KeyboardUp ));
                        special_cb  = (FGCBSpecialUC) ( FETCH_WCB( *window, SpecialUp  ));
                        keyboard_ud = FETCH_USER_DATA_WCB( *window, KeyboardUp );
                        special_ud  = FETCH_USER_DATA_WCB( *window, SpecialUp  );
                    }

                    /* Is there a keyboard/special callback hooked for this window? */
                    if( keyboard_cb || special_cb )
                    {
                        /* GLUT API tells us to have two separate callbacks... */
                        if( ev->key.code == KEYCODE_ESC || 
                            (printable = get_printable_char(ev->key.code, ev->key.modifiers)) ) {
                            /* ...one for the ASCII translateable keypresses... */
                            if( keyboard_cb ) {
                                if( ev->key.code == KEYCODE_ESC ) printable = '\033';
                                fgSetWindow( window );
                                fgState.Modifiers = fgPlatformGetModifiers( get_modifier_keys() );
                                keyboard_cb( printable,
                                             window->State.MouseX, window->State.MouseY,
                                             keyboard_ud);
                                fgState.Modifiers = INVALID_MODIFIERS;
                            }
                        } else {
                            int special = -1;

                            /*
                             * ...and one for all the others, which need to be
                             * translated to GLUT_KEY_Xs...
                             */
                            switch( ev->key.code )
                            {
                            case KEYCODE_F1:     special = GLUT_KEY_F1;     break;
                            case KEYCODE_F2:     special = GLUT_KEY_F2;     break;
                            case KEYCODE_F3:     special = GLUT_KEY_F3;     break;
                            case KEYCODE_F4:     special = GLUT_KEY_F4;     break;
                            case KEYCODE_F5:     special = GLUT_KEY_F5;     break;
                            case KEYCODE_F6:     special = GLUT_KEY_F6;     break;
                            case KEYCODE_F7:     special = GLUT_KEY_F7;     break;
                            case KEYCODE_F8:     special = GLUT_KEY_F8;     break;
                            case KEYCODE_F9:     special = GLUT_KEY_F9;     break;
                            case KEYCODE_F10:    special = GLUT_KEY_F10;    break;
                            case KEYCODE_F11:    special = GLUT_KEY_F11;    break;
                            case KEYCODE_F12:    special = GLUT_KEY_F12;    break;

                            case KEYCODE_KP_4:
                            case KEYCODE_LEFT:   special = GLUT_KEY_LEFT;   break;
                            case KEYCODE_KP_6:
                            case KEYCODE_RIGHT:  special = GLUT_KEY_RIGHT;  break;
                            case KEYCODE_KP_8:
                            case KEYCODE_UP:     special = GLUT_KEY_UP;     break;
                            case KEYCODE_KP_2:
                            case KEYCODE_DOWN:   special = GLUT_KEY_DOWN;   break;

                            case KEYCODE_KP_9:
                            case KEYCODE_PGUP:   special = GLUT_KEY_PAGE_UP; break;
                            case KEYCODE_KP_3:
                            case KEYCODE_PGDN:   special = GLUT_KEY_PAGE_DOWN; break;
                            case KEYCODE_KP_7:
                            case KEYCODE_HOME:   special = GLUT_KEY_HOME;   break;
                            case KEYCODE_KP_1:
                            case KEYCODE_END:    special = GLUT_KEY_END;    break;
                            case KEYCODE_KP_0:
                            case KEYCODE_INSERT: special = GLUT_KEY_INSERT; break;

                            case KEYCODE_NUM :  special = GLUT_KEY_NUM_LOCK;  break;
                            //case XK_KP_Begin :  special = GLUT_KEY_BEGIN;     break;
                            case KEYCODE_DELETE:special = GLUT_KEY_DELETE;    break;

                            case KEYCODE_LSHIFT: special = GLUT_KEY_SHIFT_L;    break;
                            case KEYCODE_RSHIFT: special = GLUT_KEY_SHIFT_R;    break;
                            case KEYCODE_LCTRL : special = GLUT_KEY_CTRL_L;     break;
                            case KEYCODE_RCTRL : special = GLUT_KEY_CTRL_R;     break;
                            case KEYCODE_LALT  : special = GLUT_KEY_ALT_L;      break;
                            case KEYCODE_RALT  : special = GLUT_KEY_ALT_R;      break;
                            case KEYCODE_LGUI  : special = GLUT_KEY_SUPER_L;    break;
                            case KEYCODE_RGUI  : special = GLUT_KEY_SUPER_R;    break;
                            }

                            /*
                             * Execute the callback (if one has been specified),
                             * given that the special code seems to be valid...
                             */
                            if( special_cb && (special != -1) ) {
                                fgSetWindow( window );
                                fgState.Modifiers = fgPlatformGetModifiers( get_modifier_keys() );
                                special_cb( special, 
                                            window->State.MouseX, window->State.MouseY, 
                                            special_ud );
                                fgState.Modifiers = INVALID_MODIFIERS;
                            }
                        }
                    }

                    break;
                }
        }

        free(ev);
    }
}


void fgPlatformPosResZordWork(SFG_Window* window, unsigned int workMask)
{
    if (workMask & GLUT_FULL_SCREEN_WORK)
        fgPlatformFullScreenToggle( window );
    if (workMask & GLUT_POSITION_WORK)
        fgPlatformPositionWindow( window, window->State.DesiredXpos, window->State.DesiredYpos );
    if (workMask & GLUT_SIZE_WORK)
        fgPlatformReshapeWindow ( window, window->State.DesiredWidth, window->State.DesiredHeight );
    if (workMask & GLUT_ZORDER_WORK)
    {
        if (window->State.DesiredZOrder < 0)
            fgPlatformPushWindow( window );
        else
            fgPlatformPopWindow( window );
    }
}


void fgPlatformVisibilityWork(SFG_Window* window)
{
    /* Visibility status of window gets updated in the window message handlers above 
     * XXX: is this really the case? check
     */
    SFG_Window *win = window;
    switch (window->State.DesiredVisibility)
    {
    case DesireHiddenState:
        fgPlatformHideWindow( window );
        break;
    case DesireIconicState:
        /* Call on top-level window */
        while (win->Parent)
            win = win->Parent;
        fgPlatformIconifyWindow( win );
        break;
    case DesireNormalState:
        fgPlatformShowWindow( window );
        break;
    }
}


/* deal with work list items */
void fgPlatformInitWork(SFG_Window* window)
{
    struct window_t *w = window->Window.Handle;

    /* Notify windowStatus/visibility */
    fghPlatformOnWindowStatusNotify(window, window->State.Visible, GL_TRUE);

    if( w != NULL ) {
        /* get and notify window's position */
        fghOnPositionNotify(window, w->x, w->y, GL_TRUE);

        /* get and notify window's size */
        fghOnReshapeNotify(window, w->w, w->h, GL_TRUE);
    }
}


void fgPlatformMainLoopPreliminaryWork ( void )
{
    /* no-op */
}

