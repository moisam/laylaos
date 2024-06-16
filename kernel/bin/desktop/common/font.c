/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: font.c
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
 *  \file font.c
 *
 *  Functions to query the width and height of characters and strings
 *  using a given font.
 */

#include "../include/font.h"

#define SCREEN_RESOLUTION       72      /* dpi */
#define PIXEL_SIZE(fsz)         ((fsz) * SCREEN_RESOLUTION / 72)


int char_max_width(struct font_t *font, char c)
{
    if(font->flags & FONT_FLAG_TRUE_TYPE)
    {
        // FreeType font
        if(!font->ft_face)
        {
            return 0;
        }

        return ((font->ft_face->bbox.xMax - font->ft_face->bbox.xMin) *
                    PIXEL_SIZE(font->ptsz) /
                        font->ft_face->units_per_EM);
    }
    else
    {
        // Monospace font
        return font->charw;
    }
}

int char_width(struct font_t *font, char c)
{
    if(font->flags & FONT_FLAG_TRUE_TYPE)
    {
        // FreeType font
        if(!font->ft_face)
        {
            return 0;
        }

        return char_width_ttf(font, c);
    }
    else
    {
        // Monospace font
        return font->charw;
    }
}

int char_height(struct font_t *font, char c)
{
    if(font->flags & FONT_FLAG_TRUE_TYPE)
    {
        // FreeType font
        if(!font->ft_face)
        {
            return 0;
        }

        return ((font->ft_face->bbox.yMax - font->ft_face->bbox.yMin) *
                    PIXEL_SIZE(font->ptsz) /
                        font->ft_face->units_per_EM);
    }
    else
    {
        // Monospace font
        return font->charh;
    }
}

int char_ascender(struct font_t *font, char c)
{
    if(font->flags & FONT_FLAG_TRUE_TYPE)
    {
        // FreeType font
        if(!font->ft_face)
        {
            return 0;
        }

        return (font->ft_face->bbox.yMax *
        //return (font->ft_face->ascender *
                    PIXEL_SIZE(font->ptsz) /
                        font->ft_face->units_per_EM);
    }
    else
    {
        // Monospace font - ascender is a quality that monospace fonts do
        // not possess, and this call should have never happened, so we ..
        return 0;
    }
}

int string_width(struct font_t *font, char *str)
{
    if(font->flags & FONT_FLAG_TRUE_TYPE)
    {
        // FreeType font
        if(!font->ft_face)
        {
            return 0;
        }

        /*
         * NOTE: This is just a very broad approximation, that will very
         *       likely be broader than the actual width. It is simple enough
         *       though to allow us to center text and so on.
         */
        //return strlen(str) * char_width(font, ' ');
        return string_width_ttf(font, str);
    }
    else
    {
        // Monospace font
        return strlen(str) * font->charw;
    }
}

int string_width_no_kerning(struct font_t *font, char *str)
{
    if(font->flags & FONT_FLAG_TRUE_TYPE)
    {
        // FreeType font
        if(!font->ft_face)
        {
            return 0;
        }

        return string_width_ttf_no_kerning(font, str);
    }
    else
    {
        // Monospace font
        return strlen(str) * font->charw;
    }
}

