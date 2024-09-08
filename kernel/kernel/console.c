/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: console.c
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
 *  \file console.c
 *
 *  The kernel console implementation.
 */

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             TTY_BUF_SIZE

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <kernel/io.h>
#include <kernel/vga.h>
#include <kernel/tty.h>
#include <kernel/asm.h>
#include <kernel/kqueue.h>
#include <gui/vbe.h>
#include <gui/fb.h>
#include <mm/kheap.h>

#include "tty_inlines.h"

// NOTE: uncomment to use the hardware (instead of software) cursor
//#define USE_HARDWARE_CURSOR

// Early during boot, we initialize the console to print boot messages.
// We use a static buffer to represent cell attribs as we know the width 
// and height of our standard display. Later on, when the framebuffer is
// initialisted (and the virtual memory manager is running), we allocate a
// dynamic buffer with the proper VGA width and height.
uint8_t tty1_cellattribs[STANDARD_VGA_WIDTH * STANDARD_VGA_HEIGHT];

// function prototypes
extern void *memsetw(void* bufptr, int value, size_t size);

extern void tty_init_queues(int i);

static void ega_move_cur(struct tty_t *tty);
static void ega_enable_cursor(struct tty_t *tty, uint8_t cursor_start, 
                                                 uint8_t cursor_end);
static void ega_tputchar(struct tty_t *tty, char c);
static void ega_hide_cur(struct tty_t *tty);

// function pointers
void (*erase_display)(struct tty_t *, uint32_t, uint32_t, unsigned long) = NULL;
void (*erase_line)(struct tty_t *, unsigned long) = NULL;
void (*delete_chars)(struct tty_t *, unsigned long) = NULL;
void (*insert_chars)(struct tty_t *, unsigned long) = NULL;
void (*move_cur)(struct tty_t *) = NULL;
void (*enable_cursor)(struct tty_t *, uint8_t, uint8_t) = NULL;
void (*hide_cur)(struct tty_t *) = NULL;
void (*tputchar)(struct tty_t *, char) = NULL;
void (*scroll_up)(struct tty_t *, uint32_t, uint32_t, uint32_t) = NULL;
void (*scroll_down)(struct tty_t *, uint32_t, uint32_t) = NULL;
void (*set_attribs)(struct tty_t *, unsigned long, unsigned long *) = NULL;

void (*restore_screen)(struct tty_t *) = NULL;


static void console_reset(struct tty_t *tty)
{
    // Do our bit
    tty->default_color = make_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    tty->color = tty->default_color;
    tty->vga_width = STANDARD_VGA_WIDTH;
    tty->vga_height = STANDARD_VGA_HEIGHT;

    // Then let the framebuffer device handle the rest
    fb_reset(tty);
}


/*
 * Initialise the console and clear the screen.
 */
void console_init(void)
{
    // init queues
    tty_init_queues(1);

    ttytab[1].write = console_write;
    ttytab[1].process_key = process_key;
    ttytab[1].copy_to_buf = copy_to_buf;

    ttytab[1].vga_width = STANDARD_VGA_WIDTH;
    ttytab[1].vga_height = STANDARD_VGA_HEIGHT;

    tty_set_defaults(&ttytab[1]);

    ttytab[1].buf = (uint16_t *)VGA_MEMORY_VIRTUAL;
    ttytab[1].flags = TTY_FLAG_ACTIVE | TTY_FLAG_AUTOWRAP;

    ttytab[1].cellattribs = tty1_cellattribs;
    //A_memset(ttytab[1].cellattribs, 0, ttytab[1].vga_width * ttytab[1].vga_height);
    
    // set our functions
    erase_display = ega_erase_display;
    erase_line = ega_erase_line;
    delete_chars = ega_delete_chars;
    insert_chars = ega_insert_chars;
    move_cur = ega_move_cur;
    enable_cursor = ega_enable_cursor;
    hide_cur = ega_hide_cur;
    tputchar = ega_tputchar;
    scroll_up = ega_scroll_up;
    scroll_down = ega_scroll_down;
    set_attribs = ega_set_attribs;
    //save_state = ega_save_state;
    //restore_state = ega_restore_state;

    //save_screen = ega_save_screen;
    restore_screen = ega_restore_screen;
    
    console_reset(&ttytab[1]);
}


static inline void tset_terminal_col_row(struct tty_t *tty, 
                                         uint32_t col, uint32_t row)
{
    if(tty->flags & TTY_FLAG_CURSOR_RELATIVE)
    {
        row += tty->scroll_top - 1;
    }

    if(row < tty->scroll_top)
    {
        row = tty->scroll_top - 1;
    }
    else if(row >= tty->scroll_bottom)
    {
        row = tty->scroll_bottom - 1;
    }

    if(col >= tty->window.ws_col)
    {
        col = tty->window.ws_col - 1;
    }
    
    tty->col = col;
    tty->row = row;
}


static inline void tset_terminal_row(struct tty_t *tty, uint32_t row)
{
    tset_terminal_col_row(tty, tty->col, row);
}


static inline void tset_terminal_col(struct tty_t *tty, uint32_t col)
{
    tset_terminal_col_row(tty, col, tty->row);
}


/*
 * Remove last character.
 */
void tremove_last_char(struct tty_t *tty)
{
    if(tty->col == 0)
    {
        if(tty->row)
        {
            tty->col = tty->window.ws_col - 1;
            tty->row--;
        }
    }
    else
    {
        tty->col--;
    }
}


#define NEED_BLIT(tty)                          \
    ((tty->flags & TTY_FLAG_ACTIVE) &&          \
     !(tty->flags & TTY_FLAG_FRAMEBUFFER) &&    \
     (tty->buf != (uint16_t *)VGA_MEMORY_VIRTUAL))

static inline void may_blit_buffer(struct tty_t *tty)
{
    // if this is the active tty and is not managed by the framebuffer device,
    // copy our internal buffer to the screen
    if(NEED_BLIT(tty))
    {
        A_memcpy((void *)VGA_MEMORY_VIRTUAL, tty->buf, VGA_MEMORY_SIZE(tty));
    }
}


/*
 * Scroll the screen up by copying each line to the line before it, starting 
 * at the given row (if row == 0, the whole screen is scrolled up).
 */
void ega_scroll_up(struct tty_t *tty, uint32_t width, 
                                      uint32_t height, uint32_t row)
{
    // Scroll the text cells
    uint16_t *dest = tty->buf + (row * width);
    uint16_t *src = tty->buf + ((row + 1) * width);
    uint16_t *end = tty->buf + ((height - 1) * width);
    uint32_t count = (width * 2);
    
    for( ; dest < end; src += width, dest += width)
    {
        A_memcpy(dest, src, count);
    }
    
    // Reset last line to spaces
    memsetw(dest, vga_entry(' ', tty->color), width);

    // Now scroll their attributes
    uint8_t *adest = tty->cellattribs + (row * width);
    uint8_t *asrc = tty->cellattribs + ((row + 1) * width);
    uint8_t *aend = tty->cellattribs + ((height - 1) * width);
    
    for( ; adest < aend; asrc += width, adest += width)
    {
        A_memcpy(adest, asrc, width);
    }
    
    // Reset last line to default attribs
    A_memset(adest, 0, width);

    may_blit_buffer(tty);
}


/*
 * Scroll the screen down by copying each line to the line below it, ending at
 * the the current row (if row == 0, the whole screen is scrolled down).
 */
void ega_scroll_down(struct tty_t *tty, uint32_t width, uint32_t height)
{
    // Scroll the text cells
    uint16_t *dest = tty->buf + ((height - 1) * width);
    uint16_t *src = tty->buf + ((height - 2) * width);
    uint16_t *end = tty->buf + (tty->row * width);
    uint32_t count = (width * 2);
    
    for( ; dest > end; src -= width, dest -= width)
    {
        A_memcpy(dest, src, count);
    }
    
    // Reset last line to spaces
    memsetw(dest, vga_entry(' ', tty->color), width);

    // Now scroll their attributes
    uint8_t *adest = tty->cellattribs + ((height - 1) * width);
    uint8_t *asrc = tty->cellattribs + ((height - 2) * width);
    uint8_t *aend = tty->cellattribs + (tty->row * width);

    for( ; adest > aend; asrc -= width, adest -= width)
    {
        A_memcpy(adest, asrc, width);
    }
    
    // Reset last line to default attribs
    A_memset(adest, 0, width);

    may_blit_buffer(tty);
}


#ifdef USE_HARDWARE_CURSOR

static void ega_hide_cur(struct tty_t *tty)
{
    UNUSED(tty);
}

/*
 * Moves the hardware cursor.
 * Uses VGA port 0x3D4 to tell the VGA driver to write to registers
 * 0x0E (14 for the high byte) and 0x0F (15 for the low byte). Writing
 * is performed using port 0x3D5.
 */
static void ega_move_cur(struct tty_t *tty)
{
    size_t location = tty->row * VGA_WIDTH + tty->col;
    outb(0x3D4, 0x0E);
    outb(0x3D5, location >> 8);
    outb(0x3D4, 0x0F);
    outb(0x3D5, location);
}

static void ega_enable_cursor(struct tty_t *tty, uint8_t cursor_start,
                                                 uint8_t cursor_end)
{
	outb(0x3D4, 0x0A);
	outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
 
	outb(0x3D4, 0x0B);
	outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
	
	tty->cursor_enabled = 1;
}

#else       /* !USE_HARDWARE_CURSOR */

#define invert(tty)                                                    \
{                                                                      \
    size_t location = (tty->row * tty->vga_width) + tty->col;          \
    INVERT_COLOR_AT_POS(tty->buf, location);                           \
    if(NEED_BLIT(tty))                                                 \
        INVERT_COLOR_AT_POS((uint16_t *)VGA_MEMORY_VIRTUAL, location); \
}

static void ega_hide_cur(struct tty_t *tty)
{
    if(tty->cursor_enabled)
    {
        invert(tty);
    }
}

static void ega_move_cur(struct tty_t *tty)
{
    if(tty->cursor_enabled)
    {
        invert(tty);
    }
}

static void ega_enable_cursor(struct tty_t *tty, uint8_t cursor_start, 
                                                 uint8_t cursor_end)
{
    UNUSED(cursor_start);
    UNUSED(cursor_end);
    invert(tty);
    tty->cursor_enabled = 1;
}

#endif      /* USE_HARDWARE_CURSOR */


/*
 * Erase display, the start and end of erased area depends on cmd:
 *    0 - erase from cursor to end of display
 *    1 - erase from start to cursor
 *    2 - erase whole display
 *    3 - erase whole display, including scroll-back buffer (not implemented)
 */
void ega_erase_display(struct tty_t *tty, 
                       uint32_t width, uint32_t height, unsigned long cmd)
{
    size_t location = (tty->row * width) + tty->col;
    size_t start, end, count;
    
    switch(cmd)
    {
        case 0:
            start = location;
            end = ((height - 1) * width) + width;
            break;
            
        case 1:
            start = 0;
            end = location;
            break;
            
        case 2:
        case 3:         // NOTE: this case is not fully implemented!
            start = 0;
            end = ((height - 1) * width) + width;
            break;
        
        default:
            return;
    }

    count = end - start;
    memsetw(tty->buf + start, vga_entry(' ', tty->color), count);
    memset(tty->cellattribs + start, 0, count);

    may_blit_buffer(tty);
}


/*
 * Erase line, the start and end of erased area depends on cmd:
 *    0 - erase from cursor to end of line
 *    1 - erase from start of line to cursor
 *    2 - erase whole line
 */
void ega_erase_line(struct tty_t *tty, unsigned long cmd)
{
    uint32_t width = tty->window.ws_col;
    size_t location = (tty->row * width) + tty->col;
    size_t start, end, count;
    
    switch(cmd)
    {
        case 0:
            start = location;
            end = ((tty->row + 1) * width) - 1;
            break;
            
        case 1:
            start = (tty->row * width);
            end = location;
            break;
            
        case 2:
            start = (tty->row * width);
            end = ((tty->row + 1) * width) - 1;
            break;
        
        default:
            return;
    }

    count = end - start;
    memsetw(tty->buf + start, vga_entry(' ', tty->color), count);
    A_memset(tty->cellattribs + start, 0, count);

    may_blit_buffer(tty);
}


/*
 * Insert empty lines at the cursor's position.
 */
static void insert_lines(struct tty_t *tty, uint32_t width, uint32_t height,
                                            unsigned long count)
{
    if(count > height)
    {
        count = height;
    }
    else if(count == 0)
    {
        count = 1;
    }
    
    while(count--)
    {
        scroll_down(tty, width, height);
    }
}


/*
 * Delete lines from the cursor's position.
 */
static void delete_lines(struct tty_t *tty, uint32_t width, uint32_t height, 
                                            unsigned long count)
{
    if(count > height)
    {
        count = height;
    }
    else if(count == 0)
    {
        count = 1;
    }
    
    while(count--)
    {
        scroll_up(tty, width, height, tty->row);
    }
}


/*
 * Delete count chars from the cursor's position.
 */
void ega_delete_chars(struct tty_t *tty, unsigned long count)
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

    size_t location = tty->row * width + tty->col;
    size_t last = ((tty->row + 1) * width) - 1;
    
    while(count--)
    {
        uint16_t *dest = tty->buf + location;
        uint16_t *src = dest + 1;
        uint16_t *end = tty->buf + last;
        uint8_t *attrdest = tty->cellattribs + location;
        
        for( ; dest < end; src++, dest++, attrdest++)
        {
            *dest = *src;
            *attrdest = attrdest[1];
        }
        
        *dest = vga_entry(' ', tty->color);
        *attrdest = 0;
    }

    may_blit_buffer(tty);
}


/*
 * Insert count blank chars at the cursor's position.
 */
void ega_insert_chars(struct tty_t *tty, unsigned long count)
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

    size_t location = ((tty->row + 1) * width) - 1;
    size_t last = (tty->row * width) + tty->col;
    
    while(count--)
    {
        uint16_t *src = tty->buf + last;
        uint16_t *dest = src - 1;
        uint16_t *end = tty->buf + location;
        uint8_t *attrdest = tty->cellattribs + last - 1;

        for( ; dest > end; src--, dest--, attrdest--)
        {
            *dest = *src;
            *attrdest = attrdest[1];
        }
        
        *dest = vga_entry(' ', tty->color);
        *attrdest = 0;
    }

    may_blit_buffer(tty);
}


/*
 * Set the terminal's graphics attributes.
 *
 * For more info, see: 
 *      https://man7.org/linux/man-pages/man4/console_codes.4.html
 */
void ega_set_attribs(struct tty_t *tty, unsigned long npar, 
                                        unsigned long *par)
{
    unsigned long i;
    
    for(i = 0; i < npar; i++)
    {
        switch(par[i])
        {
            case 0:       // reset to default
                tty->color = tty->default_color;
                tty->flags &= ~TTY_FLAG_REVERSE_VIDEO;
                break;

            case 1:         // set bold
                tty->attribs |= ATTRIB_BOLD;
                break;

            case 2:         // set bright
                tty->attribs |= ATTRIB_BRIGHT_FG;
                break;

            case 4:         // set underscore
                tty->attribs |= ATTRIB_UNDERLINE;
                break;

            case 5:         // set blink (simulated by a bright background)
                tty->attribs |= ATTRIB_BRIGHT_BG;
                break;

            case 7:       // set reverse video
                tty->flags |= TTY_FLAG_REVERSE_VIDEO;
                break;

            case 21:        // set underline
                tty->attribs |= ATTRIB_UNDERLINE;
                break;

            case 22:        // set normal intensity
                tty->attribs &= ~ATTRIB_BOLD;
                tty->attribs &= ~ATTRIB_BRIGHT_FG;
                tty->attribs &= ~ATTRIB_BRIGHT_BG;
                break;

            case 24:        // underline off
                tty->attribs &= ~ATTRIB_UNDERLINE;
                break;

            case 25:        // blink off
                tty->attribs &= ~ATTRIB_BRIGHT_BG;
                break;

            case 27:        // reverse video off
                tty->flags &= ~TTY_FLAG_REVERSE_VIDEO;
                break;

            case 30:      // set black foreground
                tty->color = (tty->color & 0xf0) | COLOR_BLACK;
                break;

            case 31:      // set red foreground
                tty->color = (tty->color & 0xf0) | COLOR_RED;
                break;

            case 32:      // set green foreground
                tty->color = (tty->color & 0xf0) | COLOR_GREEN;
                break;

            case 33:      // set brown foreground
                tty->color = (tty->color & 0xf0) | COLOR_BROWN;
                break;

            case 34:      // set blue foreground
                tty->color = (tty->color & 0xf0) | COLOR_BLUE;
                break;

            case 35:      // set magenta foreground
                tty->color = (tty->color & 0xf0) | COLOR_MAGENTA;
                break;

            case 36:      // set cyan foreground
                tty->color = (tty->color & 0xf0) | COLOR_CYAN;
                break;

            case 37:      // set white foreground
                tty->color = (tty->color & 0xf0) | COLOR_WHITE;
                break;

            case 38:
            case 39:      // set default foreground color
                tty->color = (tty->color & 0xf0) | 
                                 (tty->default_color & 0xf);
                break;

            case 40:      // set black background
            case 100:
                tty->color = (tty->color & 0xf) | (COLOR_BLACK << 4);
                break;

            case 41:      // set red background
            case 101:
                tty->color = (tty->color & 0xf) | (COLOR_RED << 4);
                break;

            case 42:      // set green background
            case 102:
                tty->color = (tty->color & 0xf) | (COLOR_GREEN << 4);
                break;

            case 43:      // set brown background
            case 103:
                tty->color = (tty->color & 0xf) | (COLOR_BROWN << 4);
                break;

            case 44:      // set blue background
            case 104:
                tty->color = (tty->color & 0xf) | (COLOR_BLUE << 4);
                break;

            case 45:      // set magenta background
            case 105:
                tty->color = (tty->color & 0xf) | (COLOR_MAGENTA << 4);
                break;

            case 46:      // set cyan background
            case 106:
                tty->color = (tty->color & 0xf) | (COLOR_CYAN << 4);
                break;

            case 47:      // set white background
            case 107:
                tty->color = (tty->color & 0xf) | (COLOR_WHITE << 4);
                break;

            case 48:
            case 49:      // set default background color
                tty->color = (tty->color & 0xf) | 
                                 (tty->default_color & 0xf0);
                break;
        }
    }
}


static void handle_dec_sequence(struct tty_t *tty, unsigned long cmd, int set)
{
    switch(cmd)
    {
        case 1:
            break;
        
        case 5:
            if(set)
            {
                tty->flags |= TTY_FLAG_REVERSE_VIDEO;
            }
            else
            {
                tty->flags &= ~TTY_FLAG_REVERSE_VIDEO;
            }
            break;

        case 6:
            if(set)
            {
                tty->flags |= TTY_FLAG_CURSOR_RELATIVE;
            }
            else
            {
                tty->flags &= ~TTY_FLAG_CURSOR_RELATIVE;
            }
            break;
        
        case 7:
            // NOTE: we wrap anyway regardless of the flag
            if(set)
            {
                tty->flags |= TTY_FLAG_AUTOWRAP;
            }
            else
            {
                tty->flags &= ~TTY_FLAG_AUTOWRAP;
            }
            break;

        case 20:
            if(set)
            {
                tty->flags |= TTY_FLAG_LFNL;
            }
            else
            {
                tty->flags &= ~TTY_FLAG_LFNL;
            }
            break;

        case 25:
            if(set)
            {
                tty->cursor_enabled = 1;
                
                if(!tty->cursor_shown)
                {
                    move_cur(tty);
                }
                
                tty->cursor_shown = 1;
            }
            else
            {
                hide_cur(tty);
                tty->cursor_enabled = 0;
                tty->cursor_shown = 0;
            }
            break;

        default:
            //printk("%s: Unknown cmd '%d', %d    ", __func__, cmd, set);
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            break;
    }
}


static void set_scroll_region(struct tty_t *tty, 
                              unsigned long row1, unsigned long row2)
{
    if(row1 == 0)
    {
        row1++;
    }
    else if(row1 > tty->window.ws_row)
    {
        row1 = tty->window.ws_row;
    }

    if(row2 == 0 || row2 > tty->window.ws_row)
    {
        row2 = tty->window.ws_row;
    }

    if(row1 >= row2)
    {
        return;
    }
    
    tty->scroll_top = row1;
    tty->scroll_bottom = row2;
}


static inline void __ega_tputchar(struct tty_t *tty, char c, 
                                  uint8_t flags, uint8_t color)
{
    int i = tty->row * tty->vga_width + tty->col;
    uint16_t col = vga_entry(c, color);

    tty->buf[i] = col;
    tty->cellattribs[i] = flags;

    if(NEED_BLIT(tty))
    {
        ((uint16_t *)VGA_MEMORY_VIRTUAL)[i] = col;
    }
}

static void ega_tputchar(struct tty_t *tty, char c)
{
    uint8_t color = (tty->flags & TTY_FLAG_REVERSE_VIDEO) ?
                        INVERT_COLOR(tty->color) : tty->color;

    // EGA memory is mapped as a 2-byte array of cells, with a byte for color
    // and a byte for the character. There are 16 possible colors, with the
    // foreground and background colors taking 4 bits each. As such, we do
    // not have space to indicate attributes like underline, bold or 
    // brightness. Some attributes are simulated, e.g. underline is simulated
    // by a bright foreground. As for bold, our framebuffer uses two different
    // fonts for bold and regular text. When we switch virtual consoles, we
    // need to save this info somewhere. We use a separate struct to store
    // this information for each cell.
    uint8_t flags = ((tty->attribs & ATTRIB_BOLD) ? CELL_FLAG_BOLD : 0) |
                    CELL_FLAG_CHARSET_LATIN;

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
    	    __ega_tputchar(tty, ' ', flags, color);
    	}
    }
    else if(c == '\033' /* '\e' */)
    {
        // print ESC as ^[
        __ega_tputchar(tty, '^', flags, color);
        tty->col++;
        tty_adjust_indices(tty);
        __ega_tputchar(tty, '[', flags, color);
        tty->col++;
    }
    else
    {
        __ega_tputchar(tty, c, flags, color);
        tty->col++;
    }
    
    tty_adjust_indices(tty);
}


/*
 * Send the DEC private identification in response to the escape sequence
 * ESC-Z. Linux claims it is a VT102, and so do we!
 */
static void decid(struct tty_t *tty)
{
    volatile char *s = "\033[?6c";
    
    // ensure no reader interrupts us while we write the response
    int i = int_off();
    
    while(*s)
    {
        ttybuf_enqueue(&tty->read_q, *s++);
    }
    
    int_on(i);
    
    copy_to_buf(tty);
}


#include "nanoprintf.h"

int ksprintf(char *buf, size_t sz, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	//i = vsprintf(buf, fmt, args);
	i = npf_vsnprintf(buf, sz, fmt, args);
	va_end(args);

	return i;
}


/*
 * Device status report. Response depends on cmd:
 *    5 - answer is "ESC [ 0 n" (i.e. terminal ok)
 *    6 - cursor position report, answer is "ESC [ y ; x R"
 */
static void status_report(struct tty_t *tty, unsigned long cmd)
{
    static char *ok = "\033[0n";
    volatile char *s = ok;
    char buf[32];
    int i;
    
    switch(cmd)
    {
        case 6:
            buf[0] = '\0';
            ksprintf(buf, sizeof(buf), "\033[%d;%dR",
                     (int)(tty->row + 1), (int)(tty->col + 1));
            s = buf;
            __attribute__((fallthrough));
        
        case 5:
            // ensure no reader interrupts us while we write the response
            i = int_off();

            while(*s)
            {
                ttybuf_enqueue(&tty->read_q, *s++);
            }
    
            int_on(i);
            break;

        default:
            return;
    }
    
    copy_to_buf(tty);
}


/*
 * Write output to the system console.
 * Read characters from the given tyy's output buffer and write them to
 * screen, while also processing control sequences, updating cursor
 * position, and scrolling the screen as appropriate.
 *
 * See: https://man7.org/linux/man-pages/man4/console_codes.4.html
 */
void console_write(struct tty_t *tty)
{
    char c;
    int csi_ignore = 0;
    
    if(!tty->buf)
    {
        if(!(tty->buf = kmalloc(VGA_MEMORY_SIZE(tty))))
        {
            return;
        }
        
        A_memset(tty->buf, 0, VGA_MEMORY_SIZE(tty));
    }
    
    if(tty->flags & TTY_FLAG_ACTIVE)
    {
        repaint_screen = 0;
        hide_cur(tty);
    }
    
    while(ttybuf_used(&tty->write_q))
    {
        c = ttybuf_dequeue(&tty->write_q);

        // dequeued an empty queue
        if(c == 0)
        {
            continue;
        }

#define PUTCH(c)    tputchar(tty, c)

        switch(tty->state)
        {
            /*
             * Normal state: output printable chars
             */
            case 0:
                // 8 => backspace
                // 9 => tab
                // 10 => linefeed
                // 11 => vertical tab
                // 12 => form feed
                // 13 => carriage return
                if((c >= '\b' && c <= '\r') || (c >= ' ' && c < DEL))
                {
                    PUTCH(c);
                }
                else if(c == '\033' /* '\e' */)
                {
                    tty->state = 1;
                }
                else if(c == 0x0e)
                {
                    // activate the G1 character set into GL
                    tty->gl = tty->g[1];
                    tty->glbold = tty->gbold[1];
                }
                else if(c == 0x0f)
                {
                    // activate the G0 character set into GL
                    tty->gl = tty->g[0];
                    tty->glbold = tty->gbold[0];
                }
                else if(c == (char)tty->termios.c_cc[VERASE])
                {
                    PUTCH('\b');
                    PUTCH(' ');
                    PUTCH('\b');
                }
                /*
                else
                {
                    __asm__ __volatile__("xchg %%bx, %%bx":::);
                    printk("%s: Unknown chr '%c' - '%d'    ", __func__, c, c);
                }
                */
                break;
                
            /*
             * Escaped state: After encountering an ESC char in the normal 
             *                state. Depending on the char following ESC, we 
             *                might have a CSI-sequence (ESC followed by '['),
             *                or an ESC-sequence (ESC followed by something 
             *                else). For more info, see the link above.
             */
            case 1:
                tty->state = 0;
                
                switch(c)
                {
                    case '[':       // control sequence introducer
                        tty->state = 2;
                        break;
                        
                    case '(':       // G0 charset sequence introducer
                        tty->state = 5;
                        break;
                        
                    case ')':       // G1 charset sequence introducer
                        tty->state = 6;
                        break;
                        
                    case 'c':       // reset
                        console_reset(tty);
                        break;
                        
                    case 'D':       // linefeed
                        PUTCH('\n');
                        break;
                        
                    case 'E':       // newline
                        tset_terminal_col_row(tty, 0, tty->row + 1);
                        break;
                        
                    case 'M':       // reverse linefeed
                        if(tty->row >= tty->scroll_top)
                        {
                            tty->row--;
                        }
                        else
                        {
                            scroll_down(tty, tty->window.ws_col, 
                                             tty->scroll_bottom);
                        }
                        break;
                        
                    case 'Z':       // DEC private identification
                        decid(tty);
                        break;
                        
                    case '7':       // save current state
                        save_tty_state(tty);
                        break;
                        
                    case '8':       // restore current state
                        restore_tty_state(tty);
                        break;

                    case '>':       // set numeric keypad mode
                        tty->flags &= ~TTY_FLAG_APP_KEYMODE;
                        break;

                    case '=':       // set application keypad mode
                        tty->flags |= TTY_FLAG_APP_KEYMODE;
                        break;

                    case ']':       // set/reset palette
                        tty->state = 7;
                        break;

                    /*
                    default:
                        __asm__ __volatile__("xchg %%bx, %%bx":::);
                        printk("%s: Unknown CSI '%c' - '%d'    ", __func__, c, c);
                    */
                }
                break;
                
            /*
             * CSI state: after encountering '[' in the escaped state.
             *            ESC-[ is followed by a sequence of parameters
             *            (max is NPAR). These are decimal numbers, separated
             *            by semicolons. Absent parameters are taken as 0. 
             *            The parameters might be preceded by a '?'.
             *            For more info, see the link above.
             */
            case 2:
                memset(tty->par, 0, sizeof(tty->par));
                tty->npar = 0;
                tty->state = 3;

                // if CSI is followed by another '[', one char is read and the
                // whole sequence is discarded (to ignore an echoed function 
                // key)
                if((csi_ignore = (c == '[')))
                {
                    break;
                }
                
                // read and discard the optional '?'
                // this will continue the loop and read the next input char
                if(c == '?')
                {
                    break;
                }
                
                // otherwise, start reading parameters
                __attribute__((fallthrough));

            case 3:
                // see the previous case for why we do this
                if(csi_ignore)
                {
                    tty->state = 0;
                    csi_ignore = 0;
                    break;
                }
                
                if(c == ';' && tty->npar < NPAR - 1)
                {
                    // we have room for more parameters
                    tty->npar++;
                    break;
                }
                else if(c >= '0' && c <= '9')
                {
                    // add digit to current parameter
                    tty->par[tty->npar] = (10 * tty->par[tty->npar]) + c - '0';
                    break;
                }
                else
                {
                    tty->state = 4;
                }
                
                // otherwise, start reading parameters
                __attribute__((fallthrough));

            case 4:
                tty->state = 0;

                switch(c)
                {
                    // Move cursor up the indicated # of rows, to column 1
                    case 'F':
                        tset_terminal_col(tty, 0);
                        __attribute__((fallthrough));

                    // Move cursor up the indicated # of rows
                    case 'A':
                        if(!tty->par[0])
                        {
                            // move at least one row
                            tty->par[0]++;
                        }
                        
                        tset_terminal_row(tty, tty->row - tty->par[0]);
                        break;
                        
                    // Move cursor down the indicated # of rows, to column 1
                    case 'E':
                        tset_terminal_col(tty, 0);
                        __attribute__((fallthrough));

                    // Move cursor down the indicated # of rows
                    case 'B':
                    case 'e':
                        if(!tty->par[0])
                        {
                            // move at least one row
                            tty->par[0]++;
                        }
                        
                        tset_terminal_row(tty, tty->row + tty->par[0]);
                        break;
                        
                    // Move cursor right the indicated # of columns
                    case 'C':
                    case 'a':
                        if(!tty->par[0])
                        {
                            // move at least one column
                            tty->par[0]++;
                        }
                        
                        tset_terminal_col(tty, tty->col + tty->par[0]);
                        break;
                        
                    // Move cursor left the indicated # of columns
                    case 'D':
                        if(!tty->par[0])
                        {
                            // move at least one column
                            tty->par[0]++;
                        }
                        
                        tset_terminal_col(tty, tty->col - tty->par[0]);
                        break;
                        
                    // Move cursor to indicated column in current row
                    case '`':
                    case 'G':
                        if(tty->par[0])
                        {
                            // make the column zero-based
                            tty->par[0]--;
                        }
                        
                        tset_terminal_col(tty, tty->par[0]);
                        break;
                        
                    // Move cursor to indicated row, current column
                    case 'd':
                        if(tty->par[0])
                        {
                            // make the row zero-based
                            tty->par[0]--;
                        }
                        
                        tset_terminal_row(tty, tty->par[0]);
                        break;
                        
                    // Move cursor to indicated row, column
                    case 'H':
                    case 'f':
                        if(tty->par[0])
                        {
                            // make the row zero-based
                            tty->par[0]--;
                        }

                        if(tty->par[1])
                        {
                            // make the column zero-based
                            tty->par[1]--;
                        }
                        
                        tset_terminal_col_row(tty, tty->par[1], tty->par[0]);
                        break;
                        
                    // Erase display (par[0] is interpreted as shown in the
                    // erase_display() function's comment)
                    case 'J':
                        erase_display(tty, tty->window.ws_col, 
                                           tty->window.ws_row, tty->par[0]);
                        break;
                        
                    // Erase line (par[0] is interpreted as shown in the
                    // erase_line() function's comment)
                    case 'K':
                        erase_line(tty, tty->par[0]);
                        break;
                        
                    // Insert the indicated # of blank lines
                    case 'L':
                        insert_lines(tty, tty->window.ws_col, 
                                          tty->scroll_bottom, tty->par[0]);
                        break;
                        
                    // Delete the indicated # of lines
                    case 'M':
                        delete_lines(tty, tty->window.ws_col, 
                                          tty->scroll_bottom, tty->par[0]);
                        break;
                        
                    // Erase the indicated # of chars in the current line
                    // NOTE: not implemented yet!
                    case 'X': ;
                        __attribute__((fallthrough));

                    // Delete the indicated # of chars in the current line
                    case 'P':
                        delete_chars(tty, tty->par[0]);
                        break;
                        
                    // Insert the indicated # of blank chars
                    case '@':
                        insert_chars(tty, tty->par[0]);
                        break;
                        
                    // Set graphics attributes
                    case 'm':
                        set_attribs(tty, tty->npar + 1, tty->par);
                        break;
                        
                    // Answer ESC [ ? 6 c: "I am a VT102"
                    case 'c':
                        decid(tty);
                        break;
                        
                    // Status report
                    case 'n':
                        status_report(tty, tty->par[0]);
                        break;
                        
                    // Save cursor location
                    case 's':
                        save_tty_cursor_state(tty);
                        break;
                        
                    // Restore cursor location
                    case 'u':
                        restore_tty_cursor_state(tty);
                        break;

                    // Set scrolling region
                    case 'r':
                        set_scroll_region(tty, tty->par[0], tty->par[1]);
                        tset_terminal_col_row(tty, 0, tty->scroll_top - 1);
                        break;

                    // Private mode (DECSET/DECRST) sequences.
                    // 'h' sequences set, and 'l' sequences reset modes.
                    // For more info, see the link above.
                    case 'h':
                        handle_dec_sequence(tty, tty->par[0], 1);
                        break;

                    case 'l':
                        handle_dec_sequence(tty, tty->par[0], 0);
                        break;

                    default:
                        /*
                        printk("%s: Unknown cmd '%c' - '%d'    ", 
                                __func__, c, tty->par[0]);
                        __asm__ __volatile__("xchg %%bx, %%bx"::);
                        */
                        break;
                }
                break;

            /*
             * FIXME: Define G0 charset:
             *        ESC-( is followed by B, 0, U or K:
             *          B - Select default (ISO 8859-1 mapping)
             *          0 - Select VT100 graphics mapping
             *          U - Select null mapping (straight to character ROM)
             *          K - Select user mapping (one loaded with mapscrn(8))
             *        For more info, see the link above.
             */
            case 5:
                // We only use charsets in framebuffer mode (for now)
                fb_change_charset(tty, 0, c);
                tty->state = 0;
                break;

            /*
             * FIXME: Define G1 charset:
             *        ESC-) is followed by B, 0, U or K (as above).
             *        For more info, see the link above.
             */
            case 6:
                // We only use charsets in framebuffer mode (for now)
                fb_change_charset(tty, 1, c);
                tty->state = 0;
                break;

            /*
             * Set/reset palette:
             *        If ESC-] is followed by R, reset the palette.
             *        If ESC-] is followed by P, set palette color. The param
             *          is given as 7 digits: nrrggbb (n is the color 0-15),
             *          and rrggbb indicate the red/green/blue component
             *          values (0-255). We currently use palette in the
             *          framebuffer mode only.
             *        For more info, see the link above.
             */
            case 7:
                if(c == 'R')
                {
                    fb_reset_palette(tty);
                    tty->state = 0;
                }
                else if(c == 'P')
                {
                    tty->state = 8;
                }
                else if(c == '0' || c == '1' || c == '2')
                {
                    // xterm escape sequences -- see case 9 below
                    tty->state = 9;
                }
                else
                {
                    tty->state = 0;
                }

                tty->npar = 0;
                break;

            case 8:
                if(tty->npar < 7)
                {
                    tty->palette_str[tty->npar++] = c;
                }

                if(tty->npar == 7)
                {
                    tty->palette_str[7] = '\0';
                    fb_set_palette_from_str(tty, tty->palette_str);
                    tty->npar = 0;
                    tty->state = 0;
                }
                break;

            /*
             * We lie and say we are xterm-color (mainly to make ncurses-aware
             * programs run in color). As a result, some programs (e.g. bash)
             * might try and set the window title and/or icon. Obviously, there
             * is no window in the console, so we have to silently wait for
             * the whole string to come in and discard it. We know the string
             * is finished when we receive a BELL character.
             *
             * See: https://tldp.org/HOWTO/Xterm-Title-3.html
             */
            case 9:
                if(c == '\a')
                {
                    tty->state = 0;
                }
                break;
        }

#undef PUTCH

    }

    if(tty->flags & TTY_FLAG_ACTIVE)
    {
        move_cur(tty);
        repaint_screen = 1;
    }
}


void ega_restore_screen(struct tty_t *tty)
{
    if(tty->buf == NULL)
    {
        return;
    }
    
    may_blit_buffer(tty);
}


/*
 * The following functions are used internally by printk().
 */

void twritestr(const char *data)
{
    repaint_screen = 0;

    hide_cur(&ttytab[cur_tty]);
    
    for( ; *data; data++)
    {
        tputchar(&ttytab[cur_tty], *data);
    }

    move_cur(&ttytab[cur_tty]);

    repaint_screen = 1;
}

