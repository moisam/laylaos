/*
 * Copyright 2024 Mohammed Isam <mohammed_isam1984@yahoo.com>
 * Copyright 2011 3dEyes** <3dEyes@gmail.com>
 * Based on the Haiku port.
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "config.h"
#include "osdep/keycodes.h"
#include "input/input.h"
#include "input/mouse.h"
#include "mp_msg.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"
#include "mp_fifo.h"
#include "sub/sub.h"

#include <kernel/keycodes.h>
#include <gui/gui.h>
#include <gui/event.h>
#include <gui/cursor.h>
#include <gui/mouse.h>
#include <gui/keys.h>

#define GLOB                        __global_gui_data

static const vo_info_t info =
{
    "LaylaOS video output",
    "laylaos",
    "Mohammed Isam <mohammed_isam1984@yahoo.com>",
    ""
};

vo_functions_t video_out_laylaos =
{
	&info,
	preinit,
	config,
	control,
	draw_frame,
	draw_slice,
	draw_osd,
	flip_page,
	check_events,
	uninit
};

// the buffer's actual w & h
static uint32_t image_width, image_height;
static uint32_t image_depth;
static uint32_t image_format;
static uint32_t image_size;
static unsigned char *image_data = NULL;

// the window w & h (the buffer could be stretched to fit these)
static uint32_t render_width, render_height;

static uint32_t left_margin, top_margin;

static struct window_t *laylaos_win = NULL;

static void resize(unsigned int w, unsigned int h)
{
    float vaspect = (float)image_width / image_height;
    unsigned int dw = w;
    unsigned int dh = w / vaspect;

    if(dh > h) {
        dh = h;
        dw = h * vaspect;
    }

    left_margin = (w - dw) / 2;
    top_margin = (h - dh) / 2;
    render_width = dw;
    render_height = dh;
}

static void set_full_screen(int fs)
{
    if(laylaos_win) {
        if(fs == 1) {
            window_enter_fullscreen(laylaos_win);
        } else
        {
            window_exit_fullscreen(laylaos_win);
        }
    }
}

static int draw_slice(uint8_t *image[], int stride[], int w, int h, int x, int y)
{
	return 0;
}

static void draw_alpha(int x0, int y0, int w, int h, 
                       unsigned char *src, unsigned char *srca, int stride)
{
	switch (image_format)
	{
		case IMGFMT_BGR32:
			vo_draw_alpha_rgb32(w, h, src, srca, stride,
			                    image_data + 4 * (y0 * image_width + x0),
			                    4 * image_width);
		break;
	}
}

static void draw_osd(void)
{
	if(image_data)
		vo_draw_text(image_width, image_height, draw_alpha);
}

static void flip_page(void)
{
    if(laylaos_win)
    {
        struct bitmap32_t bitmap;

        bitmap.width = image_width;
        bitmap.height = image_height;
        bitmap.data = (uint32_t *)image_data;

        // black out the left & right margins
        if(left_margin) {
            gc_fill_rect(laylaos_win->gc, 0, 0,
                         left_margin, laylaos_win->h, 0x000000FF);
            gc_fill_rect(laylaos_win->gc, laylaos_win->w - left_margin, 0,
                         left_margin, laylaos_win->h, 0x000000FF);
        }

        // black out the top & bottom margins
        if(top_margin) {
            gc_fill_rect(laylaos_win->gc, 0, 0,
                         laylaos_win->w, top_margin, 0x000000FF);
            gc_fill_rect(laylaos_win->gc, 0, laylaos_win->h - top_margin,
                         laylaos_win->w, top_margin, 0x000000FF);
        }

        // now blit the frame
        /*
        __asm__ __volatile__("xchg %%bx, %%bx"::);
        printf("flip_page: left %u, top %u, rw %u, rh %u, iw %u, ih %u\n",
                            left_margin, top_margin, render_width, render_height,
                            image_width, image_height);
        */

        gc_stretch_bitmap(laylaos_win->gc, &bitmap,
                          left_margin, top_margin, render_width, render_height,
                          0, 0, image_width, image_height);
        //gc_blit_bitmap(laylaos_win->gc, &bitmap, 0, 0, 0, 0,
        //                                image_width, image_height);

        window_invalidate(laylaos_win);
    }
}

static int draw_frame(uint8_t *src[])
{
    //uint32_t *s = (uint32_t *)(src[0]);
    //uint32_t *d = (uint32_t *)image_data;
    //uint32_t *dend = (uint32_t *)(image_data + image_size);

    // As for now, LaylaOS's GUI library expects all bitmaps to be in the RGBA
    // color format, i.e.
    //    (R << 24) | (G << 16) | (B << 8) | A
    //
    // What ffmpeg's YUV2RGB converter does is place them the other way around:
    //    (A << 24) | (B << 16) | (G << 8) | R
    //
    // So what we do is select the BGR32 format, then shift every pixel 8 bits
    // to the left (discarding the alpha component), then OR with 0xff (which
    // gives full alpha or 255)
    /*
    while(d < dend)
    {
        *d = (*s << 8) | 0xff;
        d++;
        s++;
    }
    */
	__builtin_memcpy(&image_data[1], src[0], image_size - 1);
    image_data[0] = 0xff;
	return 0;
}

static int query_format(uint32_t format)
{
	image_format = format;

	switch(format)
	{
		case IMGFMT_BGR32:
			return VFCAP_CSP_SUPPORTED|VFCAP_OSD|VFCAP_SWSCALE;
	}

	return 0;
}

static int config(uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height,
                  uint32_t flags, char *title, uint32_t format)
{
	struct window_attribs_t attribs;

	image_width = width;
	image_height = height;
	image_depth = 32;
	image_size = (image_width * image_height * image_depth + 7) / 8;

	aspect_save_orig(width, height);
	aspect_save_prescale(d_width, d_height);

	if(!laylaos_win) {
        left_margin = 0;
        top_margin = 0;
        render_width = width;
        render_height = height;

		attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
		attribs.x = 0;
		attribs.y = 0;
		attribs.w = width;
		attribs.h = height;
		attribs.flags = 0;
		if (!vo_border)
		    flags |= WINDOW_NODECORATION | WINDOW_NOCONTROLBOX | WINDOW_NOICON;

		laylaos_win = window_create(&attribs);
		window_set_title(laylaos_win, title);
		window_show(laylaos_win);
		image_data = (unsigned char *)malloc(image_size);

		if(flags & VOFLAG_FULLSCREEN) {
			vo_fs = 1;
			set_full_screen(vo_fs);
		}
	} else {
		if(image_data) free(image_data);
		image_data = (unsigned char *)malloc(image_size);
	}

	return 0;
}

static void uninit(void)
{
	if(laylaos_win) {
		cursor_show(laylaos_win, CURSOR_NORMAL);
		window_destroy(laylaos_win);
		laylaos_win = 0;
	}

	if(image_data) {
		free(image_data);
		image_data = 0;
	}
}


static void check_events(void)
{
    struct event_t *ev = NULL;
    int key;

    while(1)
    {
        if(!(ev = next_event_for_seqid(NULL, 0, 0)))
        {
            break;
        }
        
        switch(ev->type)
        {
            case EVENT_WINDOW_RESIZE_OFFER:
            {
                struct event_t ev2;
                int16_t x = ev->win.x, y = ev->win.y;
                uint16_t w = ev->win.w, h = ev->win.h;

 				mp_msg(MSGT_VO, MSGL_DBG3, "vo_laylaos: Window resize\n");
                
                window_resize(laylaos_win, x, y, w, h);

                // get the new window state from the server
                ev2.type = REQUEST_WINDOW_GET_STATE;
                ev2.seqid = 0;
                ev2.src = laylaos_win->winid;
                ev2.dest = GLOB.server_winid;
                write(GLOB.serverfd, (void *)&ev2, sizeof(struct event_t));

                resize(w, h);
                flip_page();
                vo_dwidth  = w;
                vo_dheight = h;

                //config(w, h, 0, 0, 0, NULL, 0);
                break;
            }

            case EVENT_WINDOW_POS_CHANGED:
                vo_dx = ev->win.x;
                vo_dy = ev->win.y;
                break;

            case EVENT_WINDOW_CLOSING:
                mplayer_put_key(KEY_CLOSE_WIN);
                break;
        
            case EVENT_WINDOW_STATE:
                vo_fs = !!(ev->winst.state == WINDOW_STATE_FULLSCREEN);
                break;

            case EVENT_MOUSE:
            {
                mouse_buttons_t obuttons = laylaos_win->last_button_state;
                mouse_buttons_t nbuttons = ev->mouse.buttons;
                
                laylaos_win->last_button_state = nbuttons;

                vo_mouse_movement(ev->mouse.x, ev->mouse.y);
                if(vo_nomouse_input) break;

#define BUTTON_PRESSED(which)   \
    (!(obuttons & MOUSE_ ## which ## _DOWN) &&  \
      (nbuttons & MOUSE_ ## which ## _DOWN))

                if(BUTTON_PRESSED(LBUTTON)) {
                    static int clicks = 0;
                    static unsigned long long last_ticks = 0;
                    unsigned long long ticks = time_in_millis();

                    if(clicks && (ticks - last_ticks < DOUBLE_CLICK_THRESHOLD)) {
                        clicks = 0;
                        last_ticks = 0;
                        vo_fs = !vo_fs;
                        set_full_screen(vo_fs);
                    } else {
                        clicks = 1;
                        last_ticks = ticks;
                    }

                    mplayer_put_key(MOUSE_BTN0);
                }

                if(BUTTON_PRESSED(RBUTTON)) mplayer_put_key(MOUSE_BTN2);

                if(BUTTON_PRESSED(MBUTTON)) mplayer_put_key(MOUSE_BTN1);

#undef BUTTON_PRESSED

                break;
            }

            case EVENT_MOUSE_ENTER:
                laylaos_win->last_button_state = ev->mouse.buttons;
                break;

            case EVENT_KEY_PRESS:
                switch(ev->key.code)
                {
                    case KEYCODE_ESC:
                        if(vo_fs == 1) {
                            vo_fs = !vo_fs;
                            set_full_screen(vo_fs);
                        } else {
                            mplayer_put_key(KEY_ESC);
                        }
                        break;

                    case KEYCODE_LEFT:
                        mplayer_put_key(KEY_LEFT);
                        break;

                    case KEYCODE_RIGHT:
                        mplayer_put_key(KEY_RIGHT);
                        break;

                    case KEYCODE_UP:
                        mplayer_put_key(KEY_UP);
                        break;

                    case KEYCODE_DOWN:
                        mplayer_put_key(KEY_DOWN);
                        break;

                    case KEYCODE_ENTER:
                        mplayer_put_key(KEY_ENTER);
                        break;

                    case KEYCODE_F1:
                    case KEYCODE_F2:
                    case KEYCODE_F3:
                    case KEYCODE_F4:
                    case KEYCODE_F5:
                    case KEYCODE_F6:
                    case KEYCODE_F7:
                    case KEYCODE_F8:
                    case KEYCODE_F9:
                    case KEYCODE_F10:
                    case KEYCODE_F11:
                    case KEYCODE_F12:
                        mplayer_put_key(KEY_F + (ev->key.code - KEYCODE_F1));
                        break;

                    default:
                    {
                        key = get_printable_char(ev->key.code, 
                                                 ev->key.modifiers);

                        if(key) mplayer_put_key(key);
                        break;
                    }
                }

            default:
                break;
        }

        free(ev);
    }
}

static int preinit(const char *arg)
{
    if (arg) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_laylaos: Unknown subdevice: %s\n", arg);
        return ENOSYS;
    }

    char *dummy_argv[] = { "MPlayer", NULL };
    gui_init(1, dummy_argv);
    vo_screenwidth = GLOB.screen.w;
    vo_screenheight = GLOB.screen.h;
    vo_depthonscreen = GLOB.screen.pixel_width * 8;

    return 0;
}

static int control(uint32_t request, void *data)
{
	switch (request) {
	case VOCTRL_QUERY_FORMAT:
		{
			return query_format(*((uint32_t*)data));
		}

	case VOCTRL_FULLSCREEN:
		{
			vo_fs = !vo_fs;

			if(laylaos_win) set_full_screen(vo_fs);
			return VO_TRUE;
		}
	}

	return VO_NOTIMPL;
}

