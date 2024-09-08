/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: widget_volume.c
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
 *  \file widget_volume.c
 *
 *  The sound volume widget. This is shown in the right corner of the top
 *  panel, beside the clock widget.
 */

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/audioio.h>
#include "../include/panels/widget.h"

#include "../sndlib/wav.c"

#define ICON_PATH           "/usr/share/gui/icons/24/"

struct bitmap32_t vol_off;
struct bitmap32_t vol_mid;
struct bitmap32_t vol_hi;
int dsp_fd;
int has_audio_info;
int played_start_sound;
audio_info_t ainfo;


void play_start_sound(void)
{
    struct sound_t wav;
    size_t bytes = 0, remaining = 0;
    size_t bufsz = ainfo.play.buffer_size;

    if(!wav_load("/usr/share/gui/audio/Appear-48.wav", &wav))
    {
        return;
    }
    
    remaining = wav.datasz;
    
    while(remaining)
    {
        if(remaining < bufsz)
        {
            write(dsp_fd, wav.data + bytes, remaining);
            bytes += remaining;
            remaining = 0;
        }
        else
        {
            write(dsp_fd, wav.data + bytes, bufsz);
            bytes += bufsz;
            remaining -= bufsz;
        }
    }
    
    free(wav.data);
    played_start_sound = 1;
}


void widget_repaint_volume(struct window_t *widget_win, int is_active_child)
{
    struct widget_t *widget = (struct widget_t *)widget_win;

    if(dsp_fd < 0)
    {
        dsp_fd = open("/dev/dsp", O_RDWR);

        if(dsp_fd >= 0)
        {
            if(ioctl(dsp_fd, AUDIO_GETINFO, &ainfo) >= 0)
            {
                has_audio_info = 1;
                
                if(!played_start_sound)
                {
                    play_start_sound();
                }
            }
        }
    }

    widget_fill_background(widget);

#define DRAW_ICON(which)        \
    if(which .data)             \
        widget_fill_bitmap(widget, 0, 0, which .width, which .height, &which);

    if(!has_audio_info || ainfo.output_muted)
    {
        DRAW_ICON(vol_off);
    }
    else
    {
        if(ainfo.play.gain >= 128)
        {
            DRAW_ICON(vol_hi);
        }
        else
        {
            DRAW_ICON(vol_mid);
        }
    }

#undef DRAW_ICON

}


#define LOAD_IMG(bmp, file)                     \
{                                               \
    bmp.width = 24;                             \
    bmp.height = 24;                            \
    bmp.data = NULL;                            \
    sprintf(buf, "%s%s.png", ICON_PATH, file);  \
    widget_image_load(buf, &bmp);               \
}


int widget_init_volume(void)
{
    char buf[64];
    struct widget_t *widget;

    if(!(widget = widget_create()))
    {
        return 0;
    }

    LOAD_IMG(vol_off, "vol_off");
    LOAD_IMG(vol_mid, "vol_mid");
    LOAD_IMG(vol_hi, "vol_hi");

    //dsp_fd = open("/dev/dsp", O_RDONLY);
    dsp_fd = -1;
    memset(&ainfo, 0, sizeof(audio_info_t));
    //ainfo.output_muted = 1;
    has_audio_info = 0;
    played_start_sound = 0;

    widget->win.w = 25;
    widget->win.repaint = widget_repaint_volume;
    widget->win.title = "Volume";
    widget->flags |= WIDGET_FLAG_INITIALIZED;

    return 1;
}

#undef LOAD_IMG
