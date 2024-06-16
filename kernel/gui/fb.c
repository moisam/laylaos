/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: fb.c
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
 *  \file fb.c
 *
 *  This file implements the kernel framebuffer, which provides a high level
 *  abstraction to allow user programs to draw to the screen without knowing
 *  much of the low level details of how the screen works or how to interface
 *  with hardware.
 */

#define __USE_XOPEN_EXTENDED

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             TTY_BUF_SIZE

#include <errno.h>
#include <string.h>
#include <mm/mmap.h>
#include <mm/kheap.h>
#include <kernel/laylaos.h>
#include <kernel/asm.h>
#include <kernel/vga.h>
#include <kernel/tty.h>
#include <kernel/dev.h>
#include <kernel/user.h>
#include <kernel/mouse.h>
#include <kernel/task.h>
#include <gui/vbe.h>
#include <gui/fb.h>

#include "../kernel/tty_inlines.h"

#include "../bin/desktop/include/rect-struct.h"

// get the standard terminal RGB color definitions
#include "rgb_colors.h"
#include "rgb.h"

// change this include to change the font used, e.g. fb_font_8x16.h
#include "fb_font_8x16.h"
//#include "fb_font_8x8_alt.h"

static uint8_t char_width = CHAR_WIDTH;
static uint8_t char_height = CHAR_HEIGHT;
static uint8_t *font_data = FONT_DATA;

unsigned int line_words;

struct task_t *screen_task = NULL;

//virtual_addr framebuf_mem = 0;
uint8_t *fb_backbuf_text, *fb_backbuf_gui, *fb_cur_backbuf;

// function prototypes
static void vga_erase_display(struct tty_t *tty, uint32_t width,
                              uint32_t height, unsigned long cmd);
static void vga_erase_line(struct tty_t *tty, unsigned long cmd);
static void vga_delete_chars(struct tty_t *tty, unsigned long count);
static void vga_insert_chars(struct tty_t *tty, unsigned long count);

static void vga_move_cur_8(struct tty_t *tty, size_t col, size_t row);
static void vga_move_cur_16(struct tty_t *tty, size_t col, size_t row);
static void vga_move_cur_24(struct tty_t *tty, size_t col, size_t row);
static void vga_move_cur_32(struct tty_t *tty, size_t col, size_t row);

static void vga_hide_cur_8(struct tty_t *tty);
static void vga_hide_cur_16(struct tty_t *tty);
static void vga_hide_cur_24(struct tty_t *tty);
static void vga_hide_cur_32(struct tty_t *tty);

static void vga_enable_cursor_8(struct tty_t *tty, uint8_t cursor_start,
                                                   uint8_t cursor_end);
static void vga_enable_cursor_16(struct tty_t *tty, uint8_t cursor_start,
                                                    uint8_t cursor_end);
static void vga_enable_cursor_24(struct tty_t *tty, uint8_t cursor_start,
                                                    uint8_t cursor_end);
static void vga_enable_cursor_32(struct tty_t *tty, uint8_t cursor_start,
                                                    uint8_t cursor_end);
static void vga_tputchar(struct tty_t *tty, char c);
static void vga_scroll_up(struct tty_t *tty, uint32_t width, 
                          uint32_t screenh, uint32_t row);
static void vga_scroll_down(struct tty_t *tty, uint32_t width, 
                                               uint32_t height);
static void vga_set_attribs(struct tty_t *tty, unsigned long npar,
                                               unsigned long *par);

static void vga_restore_screen(struct tty_t *tty);

static inline void __tputchar8(struct tty_t *tty, char c, 
                                                  uint32_t fg, uint32_t bg);
static inline void __tputchar16(struct tty_t *tty, char c, 
                                                   uint32_t fg, uint32_t bg);
static inline void __tputchar24(struct tty_t *tty, char c, 
                                                   uint32_t fg, uint32_t bg);
static inline void __tputchar32(struct tty_t *tty, char c, 
                                                   uint32_t fg, uint32_t bg);

void (*__tputchar)(struct tty_t *, char, uint32_t, uint32_t) = NULL;


/*
 * Reset framebuffer colors.
 */
void fb_reset_colors(struct tty_t *tty)
{
    tty->fb_fgcolor = fb_default_fgcolor;
    tty->fb_bgcolor = fb_default_bgcolor;
    tty->saved_state.fb_fgcolor = fb_default_fgcolor;
    tty->saved_state.fb_bgcolor = fb_default_bgcolor;
}

/*
 * Reset the framebuffer device.
 */
void fb_reset(struct tty_t *tty)
{
    tty->row = 0;
    tty->col = 0;

    tty->attribs = 0;
    tty->cursor_shown = 0;
    tty->cursor_enabled = 0;

    tty->state = 0;

    fb_reset_colors(tty);

    save_tty_state(tty);

    erase_display(tty, tty->vga_width, tty->vga_height, 2);
    enable_cursor(tty, 0, tty->vga_height - 1);
    move_cur(tty, tty->col, tty->row);
}


/*
 * Initialise the framebuffer device.
 */
void fb_init(void)
{
    uint32_t vgaw = 80, vgah = 25;
    int i;

    // set our functions
    erase_display = vga_erase_display;
    erase_line = vga_erase_line;
    delete_chars = vga_delete_chars;
    insert_chars = vga_insert_chars;
    tputchar = vga_tputchar;
    scroll_up = vga_scroll_up;
    scroll_down = vga_scroll_down;
    set_attribs = vga_set_attribs;
    //save_state = vga_save_state;
    //restore_state = vga_restore_state;
    
    //save_screen = vga_save_screen;
    restore_screen = vga_restore_screen;
    
    if(vbe_framebuffer.pixel_width == 1)        // 8 bits-per-pixel (indexed)
    {
        __tputchar = __tputchar8;
        move_cur = vga_move_cur_8;
        enable_cursor = vga_enable_cursor_8;
        hide_cur = vga_hide_cur_8;
    }
    else if(vbe_framebuffer.pixel_width == 2)   // 16 bits-per-pixel
    {
        __tputchar = __tputchar16;
        move_cur = vga_move_cur_16;
        enable_cursor = vga_enable_cursor_16;
        hide_cur = vga_hide_cur_16;
    }
    else if(vbe_framebuffer.pixel_width == 3)   // 24 bits-per-pixel
    {
        __tputchar = __tputchar24;
        move_cur = vga_move_cur_24;
        enable_cursor = vga_enable_cursor_24;
        hide_cur = vga_hide_cur_24;
    }
    else                                        // 32 bits-per-pixel
    {
        __tputchar = __tputchar32;
        move_cur = vga_move_cur_32;
        enable_cursor = vga_enable_cursor_32;
        hide_cur = vga_hide_cur_32;
    }
    
    if(vbe_framebuffer.type == 0 /* palette-indexed */ ||
       vbe_framebuffer.type == 1 /* RGB */)
    {
        vgaw = vbe_framebuffer.width / char_width;
        vgah = vbe_framebuffer.height / char_height;
        vbe_framebuffer.line_height = vbe_framebuffer.pitch * char_height;
    }
    else if(vbe_framebuffer.type == 2 /* EGA-standard text mode */)
    {
        vgaw = vbe_framebuffer.width;
        vgah = vbe_framebuffer.height;
        vbe_framebuffer.line_height = vbe_framebuffer.pitch;
    }

    line_words = vbe_framebuffer.pitch / vbe_framebuffer.pixel_width;

    // fix the console device to use our memory now
    ttytab[1].vga_width = vgaw;
    ttytab[1].vga_height = vgah;
    ttytab[1].window.ws_row = vgah;
    ttytab[1].window.ws_col = vgaw;
    ttytab[1].scroll_bottom = vgah;

    if((ttytab[1].buf = kmalloc(VGA_MEMORY_SIZE(&ttytab[1]))))
    {
        A_memset(ttytab[1].buf, 0, VGA_MEMORY_SIZE(&ttytab[1]));
    }

    for(i = 1; i < NTTYS; i++)
    {
        ttytab[i].flags |= TTY_FLAG_FRAMEBUFFER;
    }
    
    fb_reset(&ttytab[1]);
}


void screen_task_func(void *arg)
{
    UNUSED(arg);

    for(;;)
    {
        //printk("Z");
        screen_refresh(NULL);
        block_task2(&screen_task, PIT_FREQUENCY / 5);
    }
}


/*
 * Initialize the framebuffer screen.
 */
void fb_init_screen(void)
{
    if(fb_backbuf_text)
    {
        fb_cur_backbuf = fb_backbuf_text;

        A_memcpy(fb_cur_backbuf,
                    (void *)vbe_framebuffer.virt_addr,
                    vbe_framebuffer.memsize);

        (void)start_kernel_task("screen", screen_task_func, NULL,
                                &screen_task, KERNEL_TASK_ELEVATED_PRIORITY);
    }
}


#define line_height         vbe_framebuffer.line_height
#define total_char_width    (vbe_framebuffer.pixel_width * char_width)


/*
 * Helper function to insert a blank line of the given width.
 */
static inline void blank_line(uint8_t *dest, uint32_t width, uint32_t bgcolor)
{
    uint32_t i, j;

    width *= char_width;

    if(vbe_framebuffer.pixel_width == 1)
    {
        uint8_t bgcol = to_rgb8(bgcolor);
        uint8_t *buf;
        
        for(j = 0; j < char_height; j++)
        {
            buf = (uint8_t *)dest;

            for(i = 0; i < width; i++)
            {
                buf[i] = bgcol;
            }
            
            dest += vbe_framebuffer.pitch;
        }
    }
    else if(vbe_framebuffer.pixel_width == 2)
    {
        uint16_t bgcol = to_rgb16(bgcolor);
        uint16_t *buf;
        
        for(j = 0; j < char_height; j++)
        {
            buf = (uint16_t *)dest;

            for(i = 0; i < width; i++)
            {
                buf[i] = bgcol;
            }
            
            dest += vbe_framebuffer.pitch;
        }
    }
    else if(vbe_framebuffer.pixel_width == 3)
    {
        uint32_t bgcol = to_rgb24(bgcolor);
        uint8_t b1 = bgcol & 0xff;
        uint8_t b2 = (bgcol >> 8) & 0xff;
        uint8_t b3 = (bgcol >> 16) & 0xff;
        uint8_t *buf, *lbuf;
        
        for(j = 0; j < char_height; j++)
        {
            buf = (uint8_t *)dest;
            lbuf = (uint8_t *)(&buf[width * 3]);

            while(buf < lbuf)
            {
                // do it this stupid way because if we read 4-bytes at a
                // time, the last pixel at the bottom right of the screen
                // will probably cause a page fault when we read it
                buf[0] = b1;
                buf[1] = b2;
                buf[2] = b3;
                buf += 3;
            }
            
            dest += vbe_framebuffer.pitch;
        }
    }
    else
    {
        uint32_t bgcol = to_rgb32(bgcolor);
        uint32_t *buf, *lbuf;
        
        for(j = 0; j < char_height; j++)
        {
            buf = (uint32_t *)dest;
            lbuf = (uint32_t *)(&buf[width]);

            while(buf < lbuf)
            {
                *buf++ = bgcol;
            }
            
            dest += vbe_framebuffer.pitch;
        }
    }
}


/*
 * Erase display, the start and end of erased area depends on cmd:
 *    0 - erase from cursor to end of display
 *    1 - erase from start to cursor
 *    2 - erase whole display
 *    3 - erase whole display, including scroll-back buffer (not implemented)
 */
void vga_erase_display(struct tty_t *tty,
                       uint32_t width, uint32_t height, unsigned long cmd)
{
    uint8_t *location;
    uint32_t i;

    // don't update the backbuffer if this is not the foreground tty
    if(!(tty->flags & TTY_FLAG_ACTIVE))
    {
        ega_erase_display(tty, width, height, cmd);
        return;
    }
    
    switch(cmd)
    {
        case 0:
            // erase the rest of the current line
            location = (uint8_t *)(fb_cur_backbuf +
                                (tty->row * line_height) +
                                (tty->col * total_char_width));
            blank_line(location, width - tty->col, tty->fb_bgcolor);
            
            // erase the remaining lines
            if(tty->row >= height - 1)
            {
                break;
            }

            location = (uint8_t *)(fb_cur_backbuf +
                                   ((tty->row + 1) * line_height));
            
            for(i = tty->row; i < height - 1; i++)
            {
                blank_line(location, width, tty->fb_bgcolor);
                location += line_height;
            }
            break;
            
        case 1:
            // erase the beginning of the current line
            if(tty->row && tty->col)
            {
                location = (uint8_t *)(fb_cur_backbuf +
                                    (tty->row * line_height) +
                                    (tty->col * total_char_width));
                blank_line(location, tty->col, tty->fb_bgcolor);
            }

            // erase the remaining lines
            if(tty->row == 0)
            {
                break;
            }

            location = (uint8_t *)(fb_cur_backbuf);
            
            for(i = 0; i < tty->row; i++)
            {
                blank_line(location, width, tty->fb_bgcolor);
                location += line_height;
            }
            break;
            
        case 2:
        case 3:         // NOTE: this case is not fully implemented!
            location = (uint8_t *)(fb_cur_backbuf);
            
            for(i = 0; i < height /* - 1 */; i++)
            {
                blank_line(location, width, tty->fb_bgcolor);
                location += line_height;
            }
            break;
        
        default:
            return;
    }

    ega_erase_display(tty, width, height, cmd);
}


/*
 * Erase line, the start and end of erased area depends on cmd:
 *    0 - erase from cursor to end of line
 *    1 - erase from start of line to cursor
 *    2 - erase whole line
 */
void vga_erase_line(struct tty_t *tty, unsigned long cmd)
{
    uint8_t *location;
    uint32_t width = tty->window.ws_col;

    // don't update the backbuffer if this is not the foreground tty
    if(!(tty->flags & TTY_FLAG_ACTIVE))
    {
        ega_erase_line(tty, cmd);
        return;
    }
    
    switch(cmd)
    {
        case 0:
            location = (uint8_t *)(fb_cur_backbuf +
                                (tty->row * line_height) +
                                (tty->col * total_char_width));
            blank_line(location, width - tty->col, tty->fb_bgcolor);
            break;
            
        case 1:
            if(tty->row && tty->col)
            {
                location = (uint8_t *)(fb_cur_backbuf +
                                    (tty->row * line_height) +
                                    (tty->col * total_char_width));
                blank_line(location, tty->col, tty->fb_bgcolor);
            }
            break;
            
        case 2:
            location = (uint8_t *)(fb_cur_backbuf +
                                (tty->row * line_height));
            blank_line(location, width, tty->fb_bgcolor);
            break;
        
        default:
            return;
    }

    ega_erase_line(tty, cmd);
}


void invert_8(struct tty_t *tty)
{
    /*
     * Assume every cell on the screen can have only 2 colors: fg & bg.
     * Assume the pixel at the top-left corner is always bg.
     * Then, the first pixel that doesn't match this color must be fg.
     */
    uint8_t fgcol;
    uint8_t bgcol;
    int l, i, found;
    unsigned where = (tty->col * total_char_width) +
                     (tty->row * line_height);
    uint8_t *buf = (uint8_t *)(fb_cur_backbuf + where);
    uint8_t *buf2;
    
    bgcol = *buf;
    fgcol = to_rgb8(tty->cursor_shown ? tty->fb_bgcolor : tty->fb_fgcolor);

    for(found = 0, l = 0; l < char_height; l++)
    {
        buf2 = buf;
        
        for(i = (1 << (char_width - 1)); i >= 1; i >>= 1)
        {
            if(*buf2 != bgcol)
            {
                fgcol = *buf2;
                found = 1;
                break;
            }

            buf2++;
        }
        
        if(found)
        {
            break;
        }

        buf += vbe_framebuffer.pitch;
    }

    buf = (uint8_t *)(fb_cur_backbuf + where);

    for(l = 0; l < char_height; l++)
    {
        buf2 = buf;
        
        for(i = (1 << (char_width - 1)); i >= 1; i >>= 1)
        {
            *buf2 = (*buf2 == bgcol) ? fgcol : bgcol;
            buf2++;
        }

        buf += vbe_framebuffer.pitch;
    }
}


void invert_16(struct tty_t *tty)
{
    /*
     * Assume every cell on the screen can have only 2 colors: fg & bg.
     * Assume the pixel at the top-left corner is always bg.
     * Then, the first pixel that doesn't match this color must be fg.
     */
    uint16_t fgcol;
    uint16_t bgcol;
    int l, i, found;
    unsigned where = (tty->col * total_char_width) +
                     (tty->row * line_height);
    uint16_t *buf = (uint16_t *)(fb_cur_backbuf + where);
    uint16_t *buf2;
    
    bgcol = *buf;
    fgcol = to_rgb16(tty->cursor_shown ? tty->fb_bgcolor : tty->fb_fgcolor);

    for(found = 0, l = 0; l < char_height; l++)
    {
        buf2 = buf;
        
        for(i = (1 << (char_width - 1)); i >= 1; i >>= 1)
        {
            if(*buf2 != bgcol)
            {
                fgcol = *buf2;
                found = 1;
                break;
            }

            buf2++;
        }
        
        if(found)
        {
            break;
        }

        buf += line_words;
    }

    buf = (uint16_t *)(fb_cur_backbuf + where);

    for(l = 0; l < char_height; l++)
    {
        buf2 = buf;
        
        for(i = (1 << (char_width - 1)); i >= 1; i >>= 1)
        {
            *buf2 = (*buf2 == bgcol) ? fgcol : bgcol;
            buf2++;
        }

        buf += line_words;
    }
}


void invert_24(struct tty_t *tty)
{
    /*
     * Assume every cell on the screen can have only 2 colors: fg & bg.
     * Assume the pixel at the top-left corner is always bg.
     * Then, the first pixel that doesn't match this color must be fg.
     */
    uint32_t fgcol;
    uint32_t bgcol;
    int l, i, found;
    unsigned where = (tty->col * total_char_width) +
                     (tty->row * line_height);
    uint8_t *buf = (uint8_t *)(fb_cur_backbuf + where), *buf2;
    uint32_t c;
    
    bgcol = (uint32_t)buf[0] |
            ((uint32_t)buf[1]) << 8 |
            ((uint32_t)buf[2]) << 16;
    fgcol = to_rgb24(tty->cursor_shown ? tty->fb_bgcolor : tty->fb_fgcolor);
    
    for(found = 0, l = 0; l < char_height; l++)
    {
        buf2 = buf;
        
        for(i = (1 << (char_width - 1)); i >= 1; i >>= 1)
        {
            c = (uint32_t)buf2[0] |
                ((uint32_t)buf2[1]) << 8 |
                ((uint32_t)buf2[2]) << 16;

            if(c != bgcol)
            {
                fgcol = c;
                found = 1;
                break;
            }

            buf2 += 3;
        }
        
        if(found)
        {
            break;
        }

        buf += vbe_framebuffer.pitch;
    }
    
    buf = (uint8_t *)(fb_cur_backbuf + where);

    for(l = 0; l < char_height; l++)
    {
        buf2 = buf;
        
        for(i = (1 << (char_width - 1)); i >= 1; i >>= 1)
        {
            c = (uint32_t)buf2[0] |
                ((uint32_t)buf2[1]) << 8 |
                ((uint32_t)buf2[2]) << 16;

            if(c == bgcol)
            {
                buf2[0] = fgcol & 0xff;
                buf2[1] = (fgcol >> 8) & 0xff;
                buf2[2] = (fgcol >> 16) & 0xff;
            }
            else
            {
                buf2[0] = bgcol & 0xff;
                buf2[1] = (bgcol >> 8) & 0xff;
                buf2[2] = (bgcol >> 16) & 0xff;
            }

            buf2 += 3;
        }

        buf += vbe_framebuffer.pitch;
    }
}


void invert_32(struct tty_t *tty)
{
    /*
     * Assume every cell on the screen can have only 2 colors: fg & bg.
     * Assume the pixel at the top-left corner is always bg.
     * Then, the first pixel that doesn't match this color must be fg.
     */
    uint32_t fgcol;
    uint32_t bgcol;
    int l, i, found;
    unsigned where = (tty->col * total_char_width) +
                     (tty->row * line_height);
    uint32_t *buf = (uint32_t *)(fb_cur_backbuf + where);
    uint32_t *buf2;
    
    bgcol = *buf;
    fgcol = to_rgb32(tty->cursor_shown ? tty->fb_bgcolor : tty->fb_fgcolor);
    
    for(found = 0, l = 0; l < char_height; l++)
    {
        buf2 = buf;
        
        for(i = (1 << (char_width - 1)); i >= 1; i >>= 1)
        {
            if(*buf2 != bgcol)
            {
                fgcol = *buf2;
                found = 1;
                break;
            }

            buf2++;
        }
        
        if(found)
        {
            break;
        }

        buf += line_words;
    }
    
    buf = (uint32_t *)(fb_cur_backbuf + where);

    for(l = 0; l < char_height; l++)
    {
        buf2 = buf;
        
        for(i = (1 << (char_width - 1)); i >= 1; i >>= 1)
        {
            *buf2 = (*buf2 == bgcol) ? fgcol : bgcol;
            buf2++;
        }

        buf += line_words;
    }
}


static inline void do_hide_cur(struct tty_t *tty,
                               void (*func)(struct tty_t *))
{
    if(tty->cursor_enabled && tty->cursor_shown)
    {
        if(tty->flags & TTY_FLAG_ACTIVE)
        {
            func(tty);
        }

        tty->cursor_shown = 0;
    }
}

void vga_hide_cur_8(struct tty_t *tty)
{
    do_hide_cur(tty, invert_8);
}

void vga_hide_cur_16(struct tty_t *tty)
{
    do_hide_cur(tty, invert_16);
}

void vga_hide_cur_24(struct tty_t *tty)
{
    do_hide_cur(tty, invert_24);
}

void vga_hide_cur_32(struct tty_t *tty)
{
    do_hide_cur(tty, invert_32);
}

static inline void do_move_cur(struct tty_t *tty,
                               void (*func)(struct tty_t *))
{
    if(tty->cursor_enabled && !tty->cursor_shown)
    {
        if(tty->flags & TTY_FLAG_ACTIVE)
        {
            func(tty);
        }

        tty->cursor_shown = 1;
        //screen_refresh();
    }
}

void vga_move_cur_8(struct tty_t *tty, size_t col, size_t row)
{
    UNUSED(col);
    UNUSED(row);
    do_move_cur(tty, invert_8);
}

void vga_move_cur_16(struct tty_t *tty, size_t col, size_t row)
{
    UNUSED(col);
    UNUSED(row);
    do_move_cur(tty, invert_16);
}

void vga_move_cur_24(struct tty_t *tty, size_t col, size_t row)
{
    UNUSED(col);
    UNUSED(row);
    do_move_cur(tty, invert_24);
}

void vga_move_cur_32(struct tty_t *tty, size_t col, size_t row)
{
    UNUSED(col);
    UNUSED(row);
    do_move_cur(tty, invert_32);
}

static inline void do_enable_cur(struct tty_t *tty,
                                 void (*func)(struct tty_t *))
{
	tty->cursor_enabled = 1;
    
    if(!tty->cursor_shown)
    {
        if(tty->flags & TTY_FLAG_ACTIVE)
        {
            func(tty);
        }

        tty->cursor_shown = 1;
        //screen_refresh();
    }
}

void vga_enable_cursor_8(struct tty_t *tty, uint8_t cursor_start, 
                                            uint8_t cursor_end)
{
    UNUSED(cursor_start);
    UNUSED(cursor_end);
    do_enable_cur(tty, invert_8);
}

void vga_enable_cursor_16(struct tty_t *tty, uint8_t cursor_start, 
                                             uint8_t cursor_end)
{
    UNUSED(cursor_start);
    UNUSED(cursor_end);
    do_enable_cur(tty, invert_16);
}

void vga_enable_cursor_24(struct tty_t *tty, uint8_t cursor_start, 
                                             uint8_t cursor_end)
{
    UNUSED(cursor_start);
    UNUSED(cursor_end);
    do_enable_cur(tty, invert_24);
}

void vga_enable_cursor_32(struct tty_t *tty, uint8_t cursor_start, 
                                             uint8_t cursor_end)
{
    UNUSED(cursor_start);
    UNUSED(cursor_end);
    do_enable_cur(tty, invert_32);
}


/*
 * Scroll the screen up by copying each line to the line before it, starting at
 * the given row (if row == 0, the whole screen is scrolled up).
 */
void vga_scroll_up(struct tty_t *tty, uint32_t width,
                                      uint32_t height, uint32_t row)
{
    // don't update the backbuffer if this is not the foreground tty
    if(!(tty->flags & TTY_FLAG_ACTIVE))
    {
        ega_scroll_up(tty, width, height, row);
        return;
    }

    uint8_t *dest = (uint8_t *)(fb_cur_backbuf +
                                (row * line_height));
    uint8_t *src = (uint8_t *)(dest + line_height);
    uint8_t *end = (uint8_t *)(fb_cur_backbuf +
                                ((height - 1) * line_height));

    uintptr_t i = int_off();
    
    A_memcpy(dest, src, end - src + line_height);
    dest = end;
    blank_line(dest, width, tty->fb_bgcolor);
    
    int_on(i);

    ega_scroll_up(tty, width, height, row);
}


/*
 * Scroll the screen down by copying each line to the line below it, ending at
 * the the current row (if row == 0, the whole screen is scrolled down).
 */
void vga_scroll_down(struct tty_t *tty, uint32_t width, uint32_t height)
{
    // don't update the backbuffer if this is not the foreground tty
    if(!(tty->flags & TTY_FLAG_ACTIVE))
    {
        ega_scroll_down(tty, width, height);
        return;
    }

    uint8_t *dest = (uint8_t *)(fb_cur_backbuf +
                                ((height - 1) * line_height));
    uint8_t *src = (uint8_t *)(dest - line_height);
    uint8_t *end = (uint8_t *)(fb_cur_backbuf +
                                (tty->row * line_height));
    uint32_t count = line_height;
    
    for( ; dest > end; src -= line_height, dest -= line_height)
    {
        A_memcpy(dest, src, count);
    }
    
    // reset last line to spaces
    blank_line(dest, width, tty->fb_bgcolor);

    ega_scroll_down(tty, width, height);
}


static inline void vga_copy_char(struct tty_t *tty, unsigned dest_col, 
                                                    unsigned src_col)
{
    unsigned swhere = (src_col * total_char_width) +
                      (tty->row * line_height);
    unsigned dwhere = (dest_col * total_char_width) +
                      (tty->row * line_height);
    uint8_t *sbuf = (uint8_t *)(fb_cur_backbuf + swhere);
    uint8_t *dbuf = (uint8_t *)(fb_cur_backbuf + dwhere);
    uint32_t j;

    for(j = 0; j < char_height; j++)
    {
        A_memcpy(dbuf, sbuf, total_char_width);
        sbuf += vbe_framebuffer.pitch;
        dbuf += vbe_framebuffer.pitch;
    }
}


/*
 * Delete count chars from the cursor's position.
 */
void vga_delete_chars(struct tty_t *tty, unsigned long count)
{
    uint32_t width = tty->window.ws_col;

    if(tty->col + count >= width)
    {
        count = width - tty->col - 1;
    }

    if(count == 0)
    {
        return;
    }
    
    if(tty->col >= width - 1)
    {
        return;
    }
    
    ega_delete_chars(tty, count);

    // don't update the backbuffer if this is not the foreground tty
    if(!(tty->flags & TTY_FLAG_ACTIVE))
    {
        return;
    }

    uint32_t dest_col = tty->col;
    uint32_t src_col = tty->col + count;

    while(src_col < width)
    {
        vga_copy_char(tty, dest_col, src_col);
        dest_col++;
        src_col++;
    }

    // erase the rest of the current line
    uint8_t *location = (uint8_t *)(fb_cur_backbuf +
                                    (tty->row * line_height) +
                                    (dest_col * total_char_width));
    blank_line(location, width - dest_col, tty->fb_bgcolor);
}


/*
 * Insert count blank chars at the cursor's position.
 */
void vga_insert_chars(struct tty_t *tty, unsigned long count)
{
    uint32_t width = tty->window.ws_col;

    if(tty->col + count >= width)
    {
        count = width - tty->col - 1;
    }

    if(count == 0)
    {
        return;
    }
    
    if(tty->col >= width - 1)
    {
        return;
    }
    
    ega_insert_chars(tty, count);

    // don't update the backbuffer if this is not the foreground tty
    if(!(tty->flags & TTY_FLAG_ACTIVE))
    {
        return;
    }

    uint32_t dest_col = width - 1;
    uint32_t src_col = dest_col - count;

    while(src_col >= tty->col)
    {
        vga_copy_char(tty, dest_col, src_col);
        dest_col--;
        src_col--;
    }

    // erase the rest of the current line
    uint8_t *location = (uint8_t *)(fb_cur_backbuf +
                                    (tty->row * line_height) +
                                    (tty->col * total_char_width));
    blank_line(location, count, tty->fb_bgcolor);
}


/*
 * Set the terminal's graphics attributes.
 *
 * For more info, see:
 *     https://man7.org/linux/man-pages/man4/console_codes.4.html
 */
void vga_set_attribs(struct tty_t *tty, unsigned long npar,
                                        unsigned long *par)
{
    unsigned long i;
    
    for(i = 0; i < npar; i++)
    {
        switch(par[i])
        {
            case 0:         // reset to default
                tty->fb_fgcolor = fb_default_fgcolor;
                tty->fb_bgcolor = fb_default_bgcolor;
                tty->flags &= ~TTY_FLAG_REVERSE_VIDEO;
                break;

            case 1:         // set bold (simulated by a bright color)
                tty->attribs |= ATTRIB_BOLD;
                break;

            case 2:         // set bright
                tty->attribs |= ATTRIB_BRIGHT_FG;
                break;

            case 4:         // set underscore (simulated by a
                            // bright background)
                tty->attribs |= ATTRIB_UNDERLINE;
                break;

            case 5:         // set blink (simulated by a bright background)
                tty->attribs |= ATTRIB_BRIGHT_BG;
                break;

            case 7:         // set reverse video
                tty->flags |= TTY_FLAG_REVERSE_VIDEO;
                break;

            case 21:        // set underline (simulated by setting 
                            // normal intensity)

            case 22:        // set normal intensity

            case 24:        // underline off
                tty->attribs &= ~ATTRIB_BRIGHT_FG;
                tty->attribs &= ~ATTRIB_BRIGHT_BG;
                tty->attribs &= ~ATTRIB_UNDERLINE;
                break;

            case 25:        // blink off
                tty->attribs &= ~ATTRIB_BRIGHT_BG;
                break;

            case 27:        // reverse video off
                tty->flags &= ~TTY_FLAG_REVERSE_VIDEO;
                break;

            case 30:        // set black foreground
                tty->fb_fgcolor = RGB_COLOR_BLACK;
                break;

            case 31:        // set red foreground
                tty->fb_fgcolor = RGB_COLOR_RED;
                break;

            case 32:        // set green foreground
                tty->fb_fgcolor = RGB_COLOR_GREEN;
                break;

            case 33:        // set brown foreground
                tty->fb_fgcolor = RGB_COLOR_BROWN;
                break;

            case 34:        // set blue foreground
                tty->fb_fgcolor = RGB_COLOR_BLUE;
                break;

            case 35:        // set magenta foreground
                tty->fb_fgcolor = RGB_COLOR_MAGENTA;
                break;

            case 36:        // set cyan foreground
                tty->fb_fgcolor = RGB_COLOR_CYAN;
                break;

            case 37:        // set white foreground
                tty->fb_fgcolor = RGB_COLOR_WHITE;
                break;

            case 38:
            case 39:        // set default foreground color
                tty->fb_fgcolor = fb_default_fgcolor;
                break;

            case 40:        // set black background
            case 100:
                tty->fb_bgcolor = RGB_COLOR_BLACK;
                break;

            case 41:        // set red background
            case 101:
                tty->fb_bgcolor = RGB_COLOR_RED;
                break;

            case 42:        // set green background
            case 102:
                tty->fb_bgcolor = RGB_COLOR_GREEN;
                break;

            case 43:        // set brown background
            case 103:
                tty->fb_bgcolor = RGB_COLOR_BROWN;
                break;

            case 44:        // set blue background
            case 104:
                tty->fb_bgcolor = RGB_COLOR_BLUE;
                break;

            case 45:        // set magenta background
            case 105:
                tty->fb_bgcolor = RGB_COLOR_MAGENTA;
                break;

            case 46:        // set cyan background
            case 106:
                tty->fb_bgcolor = RGB_COLOR_CYAN;
                break;

            case 47:        // set white background
            case 107:
                tty->fb_bgcolor = RGB_COLOR_WHITE;
                break;

            case 48:
            case 49:        // set default background color
                tty->fb_bgcolor = fb_default_bgcolor;
                break;
        }
    }
    
    ega_set_attribs(tty, npar, par);
}


static inline void __tputchar8(struct tty_t *tty, char c, 
                               uint32_t fg, uint32_t bg)
{
    uint8_t *chr = &font_data[c * char_height];
    uint8_t fgcol = to_rgb8(fg);
    uint8_t bgcol = to_rgb8(bg);
    int l, i;
    unsigned where = (tty->col * total_char_width) +
                     (tty->row * line_height);
    uint8_t *buf = (uint8_t *)(fb_cur_backbuf + where);
    uint8_t *buf2;

    for(l = 0; l < char_height; l++)
    {
        buf2 = buf;
        
        for(i = (1 << (char_width - 1)); i >= 1; i >>= 1)
        {
            *buf2++ = (chr[l] & i) ? fgcol : bgcol;
        }

        buf += vbe_framebuffer.pitch;
    }
}


static inline void __tputchar16(struct tty_t *tty, char c, 
                                uint32_t fg, uint32_t bg)
{
    uint8_t *chr = &font_data[c * char_height];
    uint16_t fgcol = to_rgb16(fg);
    uint16_t bgcol = to_rgb16(bg);
    int l, i;
    unsigned where = (tty->col * total_char_width) +
                     (tty->row * line_height);
    uint16_t *buf = (uint16_t *)(fb_cur_backbuf + where);
    uint16_t *buf2;

    for(l = 0; l < char_height; l++)
    {
        buf2 = buf;
        
        for(i = (1 << (char_width - 1)); i >= 1; i >>= 1)
        {
            *buf2++ = (chr[l] & i) ? fgcol : bgcol;
        }

        buf += line_words;
    }
}


static inline void __tputchar24(struct tty_t *tty, char c, 
                                uint32_t fg, uint32_t bg)
{
    uint8_t *chr = &font_data[c * char_height];
    int l, i;
    unsigned where = (tty->col * total_char_width) +
                     (tty->row * line_height);
    uint8_t *buf = (uint8_t *)(fb_cur_backbuf + where);
    uint8_t *buf2;
    
    fg = to_rgb24(fg);
    bg = to_rgb24(bg);

    for(l = 0; l < char_height; l++)
    {
        buf2 = buf;
        
        for(i = (1 << (char_width - 1)); i >= 1; i >>= 1)
        {
            if(chr[l] & i)
            {
                buf2[0] = fg & 0xff;
                buf2[1] = (fg >> 8) & 0xff;
                buf2[2] = (fg >> 16) & 0xff;
            }
            else
            {
                buf2[0] = bg & 0xff;
                buf2[1] = (bg >> 8) & 0xff;
                buf2[2] = (bg >> 16) & 0xff;
            }
            
            buf2 += 3;
        }

        buf += vbe_framebuffer.pitch;
    }
}


static inline void __tputchar32(struct tty_t *tty, char c, 
                                uint32_t fg, uint32_t bg)
{
    uint8_t *chr = &font_data[c * char_height];
    int l, i;
    unsigned where = (tty->col * total_char_width) +
                     (tty->row * line_height);
    uint32_t *buf = (uint32_t *)(fb_cur_backbuf + where);
    uint32_t *buf2;
    
    fg = to_rgb32(fg);
    bg = to_rgb32(bg);

    for(l = 0; l < char_height; l++)
    {
        buf2 = buf;
        
        for(i = (1 << (char_width - 1)); i >= 1; i >>= 1)
        {
            *buf2++ = (chr[l] & i) ? fg : bg;
        }

        buf += line_words;
    }
}


static inline void __ega_tputchar(struct tty_t *tty, char c, uint8_t color)
{
    int i = tty->row * tty->vga_width + tty->col;

    tty->buf[i] = vga_entry(c, color);
}


static void vga_tputchar(struct tty_t *tty, char c)
{
    uint32_t fg, bg;

    uint8_t color = (tty->flags & TTY_FLAG_REVERSE_VIDEO) ?
                        INVERT_COLOR(tty->color) : tty->color;
    
    if(tty->flags & TTY_FLAG_REVERSE_VIDEO)
    {
        fg = tty->fb_bgcolor;
        bg = tty->fb_fgcolor;
    }
    else
    {
        fg = tty->fb_fgcolor;
        bg = tty->fb_bgcolor;
    }

    if((tty->attribs & ATTRIB_BOLD) ||
       (tty->attribs & ATTRIB_BRIGHT_FG))
    {
        fg = brighten(fg);
    }

    if((tty->attribs & ATTRIB_UNDERLINE) ||
       (tty->attribs & ATTRIB_BRIGHT_BG))
    {
        bg = brighten(bg);
    }

    // line feed, vertical tab, and form feed
    if(c == LF || c == VT || c == FF)
    {
        tty->col = 0;
        tty->row++;
    }
    else if(c == '\a')
    {
        ;
    }
    else if(c == '\b')
    {
        tremove_last_char(tty);
    }
    else if(c == CR)
    {
        tty->col = 0;
    }
    else if(c == '\t')
    {
        size_t new_col = (tty->col + 8) & ~(8 - 1);
        
        for( ; tty->col < new_col; tty->col++)
        {
            // don't update the backbuffer if this is not the foreground tty
            if(tty->flags & TTY_FLAG_ACTIVE)
            {
        	    __tputchar(tty, ' ', fg, bg);
            }

    	    __ega_tputchar(tty, ' ', color);
    	}
    }
    else if(c == '\033' /* '\e' */)
    {
        // print ESC as ^[

        // don't update the backbuffer if this is not the foreground tty
        if(tty->flags & TTY_FLAG_ACTIVE)
        {
            __tputchar(tty, '^', fg, bg);
        }

        __ega_tputchar(tty, '^', color);
        tty->col++;
        tty_adjust_indices(tty);

        if(tty->flags & TTY_FLAG_ACTIVE)
        {
            __tputchar(tty, '[', fg, bg);
        }

        __ega_tputchar(tty, '[', color);
        tty->col++;
    }
    else
    {
        if(tty->flags & TTY_FLAG_ACTIVE)
        {
            __tputchar(tty, c, fg, bg);
        }

        __ega_tputchar(tty, c, color);
        tty->col++;
    }
    
    tty_adjust_indices(tty);
}


static inline uint32_t ega_to_vga(uint8_t color)
{
    switch(color)
    {
        case COLOR_BLACK         : return RGB_COLOR_BLACK;
        case COLOR_BLUE          : return RGB_COLOR_BLUE;
        case COLOR_GREEN         : return RGB_COLOR_GREEN;
        case COLOR_CYAN          : return RGB_COLOR_CYAN;
        case COLOR_RED           : return RGB_COLOR_RED;
        case COLOR_MAGENTA       : return RGB_COLOR_MAGENTA;
        case COLOR_BROWN         : return RGB_COLOR_BROWN;
        case COLOR_WHITE         : return RGB_COLOR_WHITE;
        case COLOR_LIGHT_GREY    : return RGB_COLOR_LIGHT_GREY;
        case COLOR_DARK_GREY     : return RGB_COLOR_DARK_GREY;
        case COLOR_LIGHT_BLUE    : return RGB_COLOR_LIGHT_BLUE;
        case COLOR_LIGHT_GREEN   : return RGB_COLOR_LIGHT_GREEN;
        case COLOR_LIGHT_CYAN    : return RGB_COLOR_LIGHT_CYAN;
        case COLOR_LIGHT_RED     : return RGB_COLOR_LIGHT_RED;
        case COLOR_LIGHT_MAGENTA : return RGB_COLOR_LIGHT_MAGENTA;
        case COLOR_LIGHT_BROWN   : return RGB_COLOR_LIGHT_BROWN;
        default                  : return RGB_COLOR_WHITE;
    }
}


void vga_restore_screen(struct tty_t *tty)
{
    if(!(tty->flags & TTY_FLAG_NO_TEXT))
    {
        fb_cur_backbuf = fb_backbuf_text;

        if(!tty->buf)
        {
            return;
        }

        uint16_t *egabuf = tty->buf;
        uint32_t save_row = tty->row, save_col = tty->col;

        ega_restore_screen(tty);
        
        for(tty->row = 0; tty->row < tty->vga_height; tty->row++)
        {
            for(tty->col = 0; tty->col < tty->vga_width; tty->col++)
            {
                uint8_t c = egabuf[tty->col] & 0xff;
                uint8_t color = (egabuf[tty->col] >> 8) & 0xff;

                __tputchar(tty, c,
                              ega_to_vga(color & 0xf), 
                              ega_to_vga((color >> 4) & 0xf));
            }
            
            egabuf += tty->vga_width;
        }

        tty->row = save_row;
        tty->col = save_col;
        move_cur(tty, tty->col, tty->row);
        screen_refresh(NULL);
    }
    else
    {
        fb_cur_backbuf = fb_backbuf_gui;

        erase_display(tty, tty->vga_width, tty->vga_height, 2);
        move_cur(tty, tty->col, tty->row);
        screen_refresh(NULL);
        tty_send_signal(tty->pgid, SIGWINCH);
    }
}


/*
 * General block device control function.
 */
int fb_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel)
{
    UNUSED(dev);
    /*
    struct tty_t *tty;

    if(!(tty = get_struct_tty(dev)))
    {
        return -EINVAL;
    }
    */

    switch(cmd)
    {
        case FB_SWITCH_TTY:             // switch active TTY
            if(!arg)
            {
                return -EINVAL;
            }

            return switch_tty((int)(uintptr_t)arg);

        case FB_GET_VBE_BUF:             // get VBE buf info
            if(!arg)
            {
                return -EINVAL;
            }

            if(kernel)
            {
                A_memcpy(arg, &vbe_framebuffer, sizeof(vbe_framebuffer));
                return 0;
            }
            else
            {
                struct framebuffer_t copy;

                A_memcpy(&copy, &vbe_framebuffer, sizeof(vbe_framebuffer));
                copy.back_buffer = 0;

                if(vbe_framebuffer.type == 0)   // palette-indexed
                {
                    copy.palette_virt_addr = NULL;
                    copy.palette_phys_addr = NULL;
                }

                return copy_to_user(arg, &copy, sizeof(vbe_framebuffer));
            }

        case FB_GET_VBE_PALETTE:        // get VBE palette
            if(vbe_framebuffer.type != 0)   // must be palette-indexed
            {
                return -EINVAL;
            }

            // caller should have reserved enough memory by multiplying
            // color count (in the palette_num_colors member field) by 4
            if(!arg)
            {
                return -EINVAL;
            }

            return copy_to_user(arg, vbe_framebuffer.palette_virt_addr,
                                     vbe_framebuffer.palette_num_colors * 4);

        case FB_MAP_VBE_BACKBUF:         // map VBE back buffer
            {
                if(!arg)
                {
                    return -EINVAL;
                }

                int res;
                virtual_addr mapaddr;

                if((res = map_vbe_backbuf(&mapaddr)) < 0)
                {
                    return res;
                }

                if(kernel)
                {
                    A_memcpy(arg, &mapaddr, sizeof(virtual_addr));
                    return 0;
                }
                else
                {
                    return copy_to_user(arg, &mapaddr,
                                             sizeof(virtual_addr));
                }
            }

        /*
        case FB_SET_CURSOR:             // turn the cursor on/off
            if((int)(uintptr_t)arg)
            {
                tty->cursor_enabled = 1;
                move_cur(tty, tty->col, tty->row);
                tty->cursor_shown = 1;
            }
            else
            {
                hide_cur(tty);
                tty->cursor_enabled = 0;
                tty->cursor_shown = 0;
            }
            
            return 0;
        */

        case FB_INVALIDATE_AREA:
            if(!arg)
            {
                return -EINVAL;
            }

            if(!(ttytab[cur_tty].flags & TTY_FLAG_NO_TEXT))
            {
                return -EINVAL;
            }

            /*
            if(!(tty->flags & TTY_FLAG_ACTIVE))
            {
                return -EINVAL;
            }
            */
            
            Rect r;
            
            if(kernel)
            {
                A_memcpy(&r, arg, sizeof(Rect));
            }
            else
            {
                COPY_VAL_FROM_USER(&r.left, &(((Rect *)arg)->left));
                COPY_VAL_FROM_USER(&r.top, &(((Rect *)arg)->top));
                COPY_VAL_FROM_USER(&r.right, &(((Rect *)arg)->right));
                COPY_VAL_FROM_USER(&r.bottom, &(((Rect *)arg)->bottom));
            }
            
            //__asm__ __volatile__("xchg %%bx, %%bx"::);

            if(r.left < 0)
            {
                r.left = 0;
            }

            if(r.top < 0)
            {
                r.top = 0;
            }

            if(r.right >= (int)vbe_framebuffer.width)
            {
                r.right = (int)(vbe_framebuffer.width - 1);
            }

            if(r.bottom >= (int)vbe_framebuffer.height)
            {
                r.bottom = (int)(vbe_framebuffer.height - 1);
            }
            
            /*
            if(r.bottom <= r.top)
            {
                return -EINVAL;
            }
            */

            if(r.right <= r.left)
            {
                return -EINVAL;
            }

            unsigned where = r.left * vbe_framebuffer.pixel_width +
                             r.top * vbe_framebuffer.pitch;
            uint8_t *src = (uint8_t *)(fb_cur_backbuf + where);
            uint8_t *dest = (uint8_t *)(vbe_framebuffer.virt_addr + where);
            int cnt = (r.right - r.left + 1) * vbe_framebuffer.pixel_width;

            for( ; r.top <= r.bottom; r.top++)
            {
                A_memcpy(dest, src, cnt);
                src += vbe_framebuffer.pitch;
                dest += vbe_framebuffer.pitch;
            }

            return 0;
            

        case FB_INVALIDATE_SCREEN:      // force screen update
            repaint_screen = (int)(uintptr_t)arg;
            return 0;

        default:
            return -EINVAL;
    }
}

