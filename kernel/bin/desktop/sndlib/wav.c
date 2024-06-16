/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: wav.c
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
 *  \file wav.c
 *
 *  This file contains a shared function that loads audio (*.WAV) files.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/sound.h"


#define SHOW_INFO               0

/*
 * See: http://soundfile.sapp.org/doc/WaveFormat/
 */

struct wav_fmt_chunk_t
{
    char chunkid[4];
    uint32_t chunksz;
    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} __attribute__((packed));

struct wav_data_chunk_t
{
    char chunkid[4];
    uint32_t chunksz;
} __attribute__((packed));

struct wav_hdr_t
{
    char chunkid[4];
    uint32_t chunksz;
    char format[4];
    struct wav_fmt_chunk_t fmt;
    struct wav_data_chunk_t data;
} __attribute__((packed));


struct sound_t *wav_load(char *file_name, struct sound_t *loaded_wav)
{
    FILE *file;
    struct wav_hdr_t hdr;

    if((file = fopen(file_name, "rb")) == NULL)
    {
        fprintf(stderr, "Error opening '%s': %s\n", file_name, strerror(errno));
        return NULL;
    }

    fread(&hdr, 1, sizeof(hdr), file);
    
    if(hdr.chunkid[0] != 'R' || hdr.chunkid[1] != 'I' ||
       hdr.chunkid[2] != 'F' || hdr.chunkid[3] != 'F')
    {
        fprintf(stderr, "Error: invalid RIFF signature: %c%c%c%c\n",
                hdr.chunkid[0], hdr.chunkid[1], hdr.chunkid[2], hdr.chunkid[3]);
        fclose(file);
        return NULL;
    }

    if(hdr.fmt.chunkid[0] != 'f' || hdr.fmt.chunkid[1] != 'm' ||
       hdr.fmt.chunkid[2] != 't' || hdr.fmt.chunkid[3] != ' ')
    {
        fprintf(stderr, "Error: invalid FMT signature: %c%c%c%c\n",
                hdr.fmt.chunkid[0], hdr.fmt.chunkid[1], 
                hdr.fmt.chunkid[2], hdr.fmt.chunkid[3]);
        fclose(file);
        return NULL;
    }

    if(hdr.data.chunkid[0] != 'd' || hdr.data.chunkid[1] != 'a' ||
       hdr.data.chunkid[2] != 't' || hdr.data.chunkid[3] != 'a')
    {
        fprintf(stderr, "Error: invalid DATA signature: %c%c%c%c\n",
                hdr.data.chunkid[0], hdr.data.chunkid[1], 
                hdr.data.chunkid[2], hdr.data.chunkid[3]);
        fclose(file);
        return NULL;
    }

#if BYTE_ORDER != LITTLE_ENDIAN
    hdr.chunksz = swap_dword(hdr.chunksz);
    hdr.fmt.chunksz = swap_dword(hdr.fmt.chunksz);
    hdr.fmt.audio_format = swap_word(hdr.fmt.audio_format);
    hdr.fmt.channels = swap_word(hdr.fmt.channels);
    hdr.fmt.sample_rate = swap_dword(hdr.fmt.sample_rate);
    hdr.fmt.byte_rate = swap_dword(hdr.fmt.byte_rate);
    hdr.fmt.block_align = swap_word(hdr.fmt.block_align);
    hdr.fmt.bits_per_sample = swap_word(hdr.fmt.bits_per_sample);
    hdr.data.chunksz = swap_dword(hdr.data.chunksz);
#endif


    if(SHOW_INFO)
    {
        printf("Your system is %s-endian.\n",
#if BYTE_ORDER == LITTLE_ENDIAN
                                        "little"
#else
                                        "big"
#endif
              );

        printf("File size: %u\n", hdr.chunksz);
        printf("Audio format: %u\n", hdr.fmt.audio_format);
        printf("Channels: %u\n", hdr.fmt.channels);
        printf("Sample rate: %u\n", hdr.fmt.sample_rate);
        printf("Byte rate: %u\n", hdr.fmt.byte_rate);
        printf("Block alignment: %u\n", hdr.fmt.block_align);
        printf("Bits per sample: %u\n", hdr.fmt.bits_per_sample);
        printf("Data size: %u\n", hdr.data.chunksz);
    }
    
    if(!(loaded_wav->data = malloc(hdr.data.chunksz)))
    {
        fprintf(stderr, "Error: insufficient memory\n");
        fclose(file);
        return NULL;
    }

    if(fread(loaded_wav->data, 1, hdr.data.chunksz, file) != hdr.data.chunksz)
    {
        fprintf(stderr, "Error: premature EOF\n");
        fclose(file);
        return NULL;
    }
    
    loaded_wav->bits_per_sample = hdr.fmt.bits_per_sample;
    loaded_wav->sample_rate = hdr.fmt.sample_rate;
    loaded_wav->channels = hdr.fmt.channels;
    loaded_wav->datasz = hdr.data.chunksz;
    
    fclose(file);

    return loaded_wav;
}

