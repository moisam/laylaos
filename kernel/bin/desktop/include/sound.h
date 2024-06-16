/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: sound.h
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
 *  \file sound.h
 *
 *  Functions to work with WAV sound files on the client side.
 */

#ifndef GUI_SOUND_H
#define GUI_SOUND_H

#include <stdint.h>
#include "endian.h"

struct sound_t
{
    size_t datasz;
    char *data;
    int bits_per_sample;
    int sample_rate;
    int channels;
};

struct sound_t *wav_load(char *file_name, struct sound_t *loaded_wav);

#endif      /* GUI_SOUND_H */
