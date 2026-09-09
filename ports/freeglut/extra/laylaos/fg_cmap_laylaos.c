/* fg_cmap_laylaos.c
 *
 * LaylaOS implementation of the colormap functions
 *
 * Copyright (C) 2022 John Tsiombikas <nuclear@member.fsf.org>
 * Creation date: Tue August 30 2022
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

void fgPlatformSetColor(int idx, float r, float g, float b)
{
	/* TODO */
	fgWarning("fgPlatformSetColor() not implemented yet on LaylaOS");
}

float fgPlatformGetColor(int idx, int comp)
{
	/* TODO */
	fgWarning("fgPlatformGetColor() not implemented yet on LaylaOS");
}

void fgPlatformCopyColormap(int win)
{
	/* TODO */
	fgWarning("fgPlatformCopyColormap() not implemented yet on LaylaOS");
}

