/*
 * fg_internal_laylaos.h
 *
 * The freeglut library private include file.
 *
 * Copyright (c) 2025 Mohammed Isam
 * Copyright (c) 2012 Stephen J. Baker. All Rights Reserved.
 * Written by Diederick C. Niehorster, <dcnieho@gmail.com>
 * Creation date: Fri Jan 20, 2012
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

#ifndef  FREEGLUT_INTERNAL_LAYLAOS_H
#define  FREEGLUT_INTERNAL_LAYLAOS_H


/* -- PLATFORM-SPECIFIC INCLUDES ------------------------------------------- */
#include "egl/fg_internal_egl.h"
#include <gui/client/window.h>
#include <gui/mouse.h>
#include <gui/cursor.h>
#include <gui/event.h>


/* -- GLOBAL TYPE DEFINITIONS ---------------------------------------------- */
/* The structure used by display initialization in fg_init.c */
typedef struct tagSFG_PlatformDisplay SFG_PlatformDisplay;
struct tagSFG_PlatformDisplay
{
    struct tagSFG_PlatformDisplayEGL egl;
};

/* The structure used by window creation in fg_window.c */
typedef struct tagSFG_PlatformContext SFG_PlatformContext;
struct tagSFG_PlatformContext
{
    struct tagSFG_PlatformContextEGL egl;
};

/* Window's state description. This structure should be kept portable. */
typedef struct tagSFG_PlatformWindowState SFG_PlatformWindowState;
struct tagSFG_PlatformWindowState
{
    int             OldWidth;           /* Window width from before a resize */
    int             OldHeight;          /*   "    height  "    "    "   "    */
};


/* -- JOYSTICK-SPECIFIC STRUCTURES AND TYPES ------------------------------- */
#define _JS_MAX_AXES  4
#define MAX_NUM_JOYSTICKS  8

typedef struct tagSFG_PlatformJoystick SFG_PlatformJoystick;
struct tagSFG_PlatformJoystick
{
    int unused;
};


/* Menu font and color definitions */
#define  FREEGLUT_MENU_FONT    GLUT_BITMAP_HELVETICA_18

#define  FREEGLUT_MENU_PEN_FORE_COLORS   {0.0f,  0.0f,  0.0f,  1.0f}
#define  FREEGLUT_MENU_PEN_BACK_COLORS   {0.70f, 0.70f, 0.70f, 1.0f}
#define  FREEGLUT_MENU_PEN_HFORE_COLORS  {0.0f,  0.0f,  0.0f,  1.0f}
#define  FREEGLUT_MENU_PEN_HBACK_COLORS  {1.0f,  1.0f,  1.0f,  1.0f}


/* -- PRIVATE FUNCTION DECLARATIONS ---------------------------------------- */

#endif  /* FREEGLUT_INTERNAL_LAYLAOS_H */
