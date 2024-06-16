/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: font.h
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
 *  \file font.h
 *
 *  Declarations and struct definitions for working with font resources.
 */

#ifndef GUI_FONT_H
#define GUI_FONT_H

#include <inttypes.h>
#include <string.h>

#include "mutex.h"
#include "font-struct.h"


// these are defined in common/gc-ttf.c
void free_tglyph_cache(struct font_t *font);
int char_width_ttf(struct font_t *font, char c);
int string_width_ttf(struct font_t *font, char *string);
int string_width_ttf_no_kerning(struct font_t *font, char *string);

// these are defined in common/font.c
int char_max_width(struct font_t *font, char c);
int char_width(struct font_t *font, char c);
int char_height(struct font_t *font, char c);
int char_ascender(struct font_t *font, char c);
int string_width(struct font_t *font, char *str);
int string_width_no_kerning(struct font_t *font, char *str);


static inline void lock_font(struct font_t *font)
{
    mutex_lock(&font->lock);
}


static inline void unlock_font(struct font_t *font)
{
    mutex_unlock(&font->lock);
}

#endif      /* GUI_FONT_H */
