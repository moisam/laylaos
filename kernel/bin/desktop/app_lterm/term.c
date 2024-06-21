/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: term.c
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
 *  \file term.c
 *
 *  Helper functions to handle terminal output. This file is part of the
 *  graphical terminal application.
 */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "../include/gui.h"
#include "lterm.h"
#include "../include/client/window.h"
#include "../include/resources.h"
#include "../include/rgb.h"

// get the standard terminal RGB color definitions
#include "../../../gui/rgb_colors.h"

#define GLOB                __global_gui_data

struct tty_cell_t
{
    uint32_t bold:1,
             underlined: 1,
             bright:1,
             blink:1,
             dirty:1;
    uint16_t fg:8, bg:8;
    uint8_t chr;
};

// how many pages we can hold in the back buffer
#define BACKBUF_PAGES       16

// the delete key
#define DEL                 127

// maximum parameters for a CSI-sequence
#define NPAR                16

// parameters of a CSI-sequence
unsigned long npar, par[NPAR];

uint32_t terminal_width = 0;
uint32_t terminal_height = 0;

uint32_t terminal_row, saved_row;
uint32_t terminal_col, saved_col;
uint8_t  fgcolor, default_fg, saved_fg;
uint8_t  bgcolor, default_bg, saved_bg;
//uint32_t old_row, old_col;
uint32_t first_text_row, first_visible_row, mouse_scroll_top;

struct tty_cell_t *cells;

uint32_t terminal_attribs = 0, saved_attribs = 0;
uint32_t terminal_flags = 0;
volatile int cursor_shown = 0;

uint8_t state = 0;

struct termios termios;
pid_t terminal_pgid, terminal_sid;
int fd_master, fd_slave;
struct winsize windowsz;
unsigned int scroll_top, scroll_bottom;

int pending_refresh = 0;

int charh = 16;
int charw = 8;
uint32_t line_height = 0;   // bytes per line = pitch * char_height

#define pixel_width         (main_window->gc->pixel_width)
#define pitch               (main_window->canvas_pitch)
#define total_char_width    (pixel_width * charw)

struct font_t *boldfont, actual_boldfont;

// functions for painting cells
void draw_cell8(struct tty_cell_t *cell, uint32_t col, uint32_t row);
void draw_cell16(struct tty_cell_t *cell, uint32_t col, uint32_t row);
void draw_cell24(struct tty_cell_t *cell, uint32_t col, uint32_t row);
void draw_cell32(struct tty_cell_t *cell, uint32_t col, uint32_t row);

typedef void (*cell_repaint_func_t)(struct tty_cell_t *, uint32_t, uint32_t);

static cell_repaint_func_t cell_repaint_funcs[] =
{
    NULL,           // unused
    draw_cell8,     // pixel_width = 1
    draw_cell16,    // pixel_width = 2
    draw_cell24,    // pixel_width = 3
    draw_cell32     // pixel_width = 4
};


static inline void console_reset_colors(void)
{
    default_fg = COLOR_LIGHT_GREY;
    default_bg = COLOR_BLACK;
    fgcolor = default_fg;
    bgcolor = default_bg;
    saved_fg = default_fg;
    saved_bg = default_bg;
}


static inline void console_reset(void)
{
    terminal_row = 0;
    saved_row = 0;
    terminal_col = 0;
    saved_col = 0;
    first_text_row = terminal_height * (BACKBUF_PAGES - 1);
    first_visible_row = first_text_row;
    mouse_scroll_top = first_text_row;
    console_reset_colors();

    erase_display(terminal_width, terminal_height, 2);
    repaint_cursor();
}


int init_terminal(char *myname, uint32_t w, uint32_t h)
{
    // init struct termios control chars
    memcpy(termios.c_cc, ttydefchars, sizeof(ttydefchars));
    
    charh = __global_gui_data.mono.charh;
    charw = __global_gui_data.mono.charw;
    
    terminal_width = w;
    terminal_height = h;
    terminal_row = 0;
    saved_row = 0;
    terminal_col = 0;
    saved_col = 0;
    first_text_row = h * (BACKBUF_PAGES - 1);
    first_visible_row = first_text_row;
    mouse_scroll_top = first_text_row;
    line_height = main_window->canvas_pitch * charh;

    console_reset_colors();
    
    // init window size
    windowsz.ws_row = h;
    windowsz.ws_col = w;
    //windowsz.ws_xpixel = 0;
    //windowsz.ws_ypixel = 0;
    
    scroll_top = 1;
    scroll_bottom = h;
    terminal_flags = (TTY_FLAG_AUTOWRAP | TTY_FLAG_LFNL);

    /* input modes */
    termios.c_iflag = TTYDEF_IFLAG;
    /* output modes: change outgoing NL to CRNL */
    termios.c_oflag = TTYDEF_OFLAG;
    /* control modes */
    termios.c_cflag = TTYDEF_CFLAG;
    /* local modes */
    termios.c_lflag = TTYDEF_LFLAG;

    /* input speed */
    termios.c_ispeed = TTYDEF_SPEED;
    /* output speed */
    termios.c_ospeed = TTYDEF_SPEED;

    if(!(cells = calloc(windowsz.ws_row * windowsz.ws_col * BACKBUF_PAGES *
                                sizeof(struct tty_cell_t), 1)))
    {
        fprintf(stderr, "%s: failed to alloc buffer: %s\n", 
                        myname, strerror(errno));
        return 0;
    }

    // Try to get the monospace bold font from the server. If we fail,
    // fallback to using the monospace font that should have been loaded
    // by the gui library's init function.
    if(font_load("font-monospace-bold", &actual_boldfont) != INVALID_RESID)
    {
        boldfont = &actual_boldfont;
    }
    else
    {
        boldfont = &GLOB.mono;
    }

    return 1;
}


static inline void cell_dirty(uint32_t col, uint32_t row)
{
    struct tty_cell_t *cell = &cells[row  * terminal_width + col];
    
    cell->dirty = 1;
}


static inline void tset_terminal_row(uint32_t row)
{
    cell_dirty(terminal_col, terminal_row + first_visible_row);

    terminal_row = row;

    pending_refresh = 1;
}


static inline void tset_terminal_col(uint32_t col)
{
    cell_dirty(terminal_col, terminal_row + first_visible_row);

    terminal_col = col;

    pending_refresh = 1;
}


static void tset_terminal_col_row(uint32_t col, uint32_t row)
{
    if(terminal_flags & TTY_FLAG_CURSOR_RELATIVE)
    {
        row += scroll_top - 1;
    }

    if(row < scroll_top)
    {
        row = scroll_top - 1;
    }
    else if(row >= scroll_bottom)
    {
        row = scroll_bottom - 1;
    }

    if(col >= windowsz.ws_col)
    {
        col = windowsz.ws_col - 1;
    }
    
    cell_dirty(terminal_col, terminal_row + first_visible_row);
    
    terminal_col = col;
    terminal_row = row;

    pending_refresh = 1;
}


static void tremove_last_char(uint32_t screenw)
{
    cell_dirty(terminal_col, terminal_row + first_visible_row);

    if(terminal_col == 0)
    {
        if(terminal_row)
        {
            terminal_col = screenw - 1;
            terminal_row--;
        }
    }
    else
    {
        terminal_col--;
    }

    pending_refresh = 1;
}


static inline void clear_cell(struct tty_cell_t *cell)
{
    cell->fg = fgcolor;
    cell->bg = bgcolor;
    cell->chr = ' ';
    cell->dirty = 1;
    cell->bold = 0;
    cell->underlined = 0;
    cell->blink = 0;
    cell->bright = 0;
}


static inline void clear_cells(struct tty_cell_t *dest, uint32_t count)
{
    while(count--)
    {
        dest->fg = fgcolor;
        dest->bg = bgcolor;
        dest->chr = ' ';
        dest->dirty = 1;
        dest->bold = 0;
        dest->underlined = 0;
        dest->blink = 0;
        dest->bright = 0;
        dest++;
    }
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


static inline struct font_t *select_font(struct tty_cell_t *cell)
{
    return cell->bold ? boldfont : &GLOB.mono;
}


void draw_cell8(struct tty_cell_t *cell, uint32_t col, uint32_t row)
{
    struct font_t *font = select_font(cell);
    uint8_t *chr = &font->data[cell->chr * charh];
    uint8_t fgcol = to_rgb8(main_window->gc, ega_to_vga(cell->fg));
    uint8_t bgcol = to_rgb8(main_window->gc, ega_to_vga(cell->bg));
    int l, i;
    unsigned where = (col * total_char_width) +
                     (row * line_height);
    uint8_t *buf = (uint8_t *)(main_window->canvas + where);
    uint8_t *buf2;

    for(l = 0; l < charh; l++)
    {
        buf2 = buf;

        for(i = charw - 1; i >= 0; i--)
        {
            if(chr[l] & (1 << i))
            {
            	*buf2 = fgcol;
            }
            else
            {
            	*buf2 = bgcol;
            }

            buf2++;
        }

        buf += pitch;
    }
}


void draw_cell16(struct tty_cell_t *cell, uint32_t col, uint32_t row)
{
    struct font_t *font = select_font(cell);
    uint8_t *chr = &font->data[cell->chr * charh];
    uint16_t fgcol = to_rgb16(main_window->gc, ega_to_vga(cell->fg));
    uint16_t bgcol = to_rgb16(main_window->gc, ega_to_vga(cell->bg));
    int l, i, j;
    unsigned where = (col * total_char_width) +
                     (row * line_height);
    uint16_t *buf = (uint16_t *)(main_window->canvas + where);
    //unsigned int line_words = pitch / pixel_width;
    unsigned int line_words = pitch >> 1;

    for(l = 0; l < charh; l++)
    {
        j = 0;

        for(i = charw - 1; i >= 0; i--)
        {
            if(chr[l] & (1 << i))
            {
            	buf[j] = fgcol;
            }
            else
            {
            	buf[j] = bgcol;
            }

            j++;
        }

        buf += line_words;
    }
}


void draw_cell24(struct tty_cell_t *cell, uint32_t col, uint32_t row)
{
    struct font_t *font = select_font(cell);
    uint8_t *chr = &font->data[cell->chr * charh];
    uint32_t fgcol = to_rgb24(main_window->gc, ega_to_vga(cell->fg));
    uint32_t bgcol = to_rgb24(main_window->gc, ega_to_vga(cell->bg));
    int l, i;
    unsigned where = (col * total_char_width) +
                     (row * line_height);
    uint8_t *buf = (uint8_t *)(main_window->canvas + where);
    uint8_t *buf2;

    for(l = 0; l < charh; l++)
    {
        buf2 = buf;

        for(i = charw - 1; i >= 0; i--)
        {
            if(chr[l] & (1 << i))
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

        buf += pitch;
    }
}


void draw_cell32(struct tty_cell_t *cell, uint32_t col, uint32_t row)
{
    struct font_t *font = select_font(cell);
    uint8_t *chr = &font->data[cell->chr * charh];
    uint32_t fgcol, bgcol;
    int l, i;
    unsigned where = (col * charw * 4) + (row * line_height);
    uint8_t *buf = (uint8_t *)(main_window->canvas + where);
    uint32_t *buf2;

    fgcol = to_rgb32(main_window->gc, ega_to_vga(cell->fg));
    bgcol = to_rgb32(main_window->gc, ega_to_vga(cell->bg));

    for(l = 0; l < charh; l++)
    {
        buf2 = (uint32_t *)buf;

        for(i = (1 << (charw - 1)); i != 0; i >>= 1)
        {
            if(chr[l] & i)
            {
                *buf2 = fgcol;
            }
            else
            {
                *buf2 = bgcol;
            }

            buf2++;
        }

        //buf += line_words;
        buf += pitch;
    }
}


void repaint_cursor(void)
{
    struct tty_cell_t *cell =
            &cells[(terminal_row + first_visible_row) * terminal_width + 
                                    terminal_col];
    uint8_t tmp = cell->fg;

    cell->fg = cell->bg;
    cell->bg = tmp;

    cell_repaint_funcs[pixel_width](cell, terminal_col, 
                    terminal_row + (first_visible_row - mouse_scroll_top));
    
    cell->bg = cell->fg;
    cell->fg = tmp;
}


static inline int cursor_in_view(void)
{
    return (((terminal_row + first_visible_row) <
                        (mouse_scroll_top + terminal_height)) &&
            ((terminal_row + first_visible_row) >= (mouse_scroll_top)));
}


/* static */ void repaint_all(void)
{
    struct tty_cell_t *cell = &cells[mouse_scroll_top * terminal_width];
    uint32_t i, j;

    cell_repaint_func_t draw_func = cell_repaint_funcs[pixel_width];

    for(i = 0; i < terminal_height; i++)
    {
        for(j = 0; j < terminal_width; j++)
        {
            draw_func(cell, j, i);
            cell->dirty = 0;
            cell++;
        }
    }

    // only draw cursor if it is in the viewable window
    if(cursor_in_view())
    {
        repaint_cursor();
    }

    pending_refresh = 0;
    
    window_invalidate(main_window);
}


void repaint_dirty(void)
{
    struct tty_cell_t *cell = &cells[mouse_scroll_top * terminal_width];
    uint32_t i, j;

    cell_repaint_func_t draw_func = cell_repaint_funcs[pixel_width];
    
    for(i = 0; i < terminal_height; i++)
    {
        for(j = 0; j < terminal_width; j++)
        {
            if(!cell->dirty)
            {
                cell++;
                continue;
            }

            draw_func(cell, j, i);
            cell->dirty = 0;
            cell++;
        }
    }

    // only draw cursor if it is in the viewable window
    if(cursor_in_view())
    {
        repaint_cursor();
    }

    pending_refresh = 0;
    
    window_invalidate(main_window);
}


/*
 * Scroll the screen up by copying each line to the line before it, starting at
 * the given row (if row == 0, the whole screen is scrolled up).
 */
void scroll_up(uint32_t width, uint32_t height, uint32_t row)
{
    char *src;
    char *dest;
    int i /* , j */;
    size_t row_bytes = sizeof(struct tty_cell_t) * width;

    // scroll up into the back buffer if we are starting at the top row
    if(row == 0)
    {
        if(first_text_row > 0)
        {
            first_text_row--;
        }

        row = first_text_row;
        height += first_visible_row;
    }
    else
    {
        row += first_visible_row;
        height += first_visible_row;
    }
        
    dest = (char *)&cells[row * terminal_width];
    src = (char *)&cells[(row + 1) * terminal_width];
    
    // now scroll
    for(i = row; i < height - 1; i++)
    {
        A_memcpy(dest, src, row_bytes);
        src += row_bytes;
        dest += row_bytes;
    }

    // reset last line to spaces
    clear_cells(&cells[i * terminal_width], width);
}


/*
 * Scroll the screen down by copying each line to the line below it, ending at
 * the the current row (if row == 0, the whole screen is scrolled down).
 */
void scroll_down(uint32_t width, uint32_t height)
{
    int i, j;
    uint32_t row = terminal_row;

    // scroll down from the back buffer if we are starting at the top row
    if(row == 0)
    {
        row = first_text_row;
        height += first_visible_row;

        if(first_text_row < first_visible_row)
        {
            first_text_row++;
        }
    }
    else
    {
        row += first_visible_row;
        height += first_visible_row;
    }
    
    // now scroll
    for(i = height - 1; i > row; i++)
    //for(i = height - 1; i > terminal_row; i++)
    {
        struct tty_cell_t *src;
        struct tty_cell_t *dest;
        
        src  = &cells[(i - 1) * terminal_width];
        dest  = &cells[i * terminal_width];

        for(j = 0; j < width; j++)
        {
            src->fg = dest->fg;
            src->bg = dest->bg;
            src->chr = dest->chr;
            src->dirty = 1;
            src->bold = dest->bold;
            src->underlined = dest->underlined;
            src->blink = dest->blink;
            src->bright = dest->bright;
            src++;
            dest++;
        }
    }

    // reset last line to spaces
    clear_cells(&cells[i * terminal_width], width);
}


/*
 * Erase display, the start and end of erased area depends on cmd:
 *    0 - erase from cursor to end of display
 *    1 - erase from start to cursor
 *    2 - erase whole display
 *    3 - erase whole display, including scroll-back buffer (not implemented)
 */
void erase_display(uint32_t width, uint32_t height, unsigned long cmd)
{
    size_t location = ((terminal_row + first_visible_row) * width) + terminal_col;
    //size_t location = (terminal_row * width) + terminal_col;
    size_t start, end, count;
    
    switch(cmd)
    {
        case 0:
            start = location;
            //end = (height * width) - 1;
            end = ((height + first_visible_row) * width) - 1;
            break;
            
        case 1:
            //start = 0;
            start = first_visible_row * width;
            end = location;
            break;
            
        case 2:
            //start = 0;
            //end = (height * width) - 1;
            start = first_visible_row * width;
            end = ((height + first_visible_row) * width) - 1;
            break;

        case 3:         // NOTE: this case is not fully implemented!
            //start = 0;
            //end = (height * width) - 1;
            start = first_text_row * width;
            end = ((height + first_visible_row) * width) - 1;
            //first_text_row = windowsz.ws_row * (BACKBUF_PAGES - 1);
            first_text_row = first_visible_row;
            mouse_scroll_top = first_visible_row;
            break;
        
        default:
            return;
    }

    count = end - start;
    clear_cells(cells + start, count);
    
    repaint_all();
}


/*
 * Erase line, the start and end of erased area depends on cmd:
 *    0 - erase from cursor to end of line
 *    1 - erase from start of line to cursor
 *    2 - erase whole line
 */
void erase_line(unsigned long cmd)
{
    uint32_t width = windowsz.ws_col;
    uint32_t row = terminal_row + first_visible_row;
    size_t location = (row * width) + terminal_col;
    //size_t location = (terminal_row * width) + terminal_col;
    size_t start, end, count;
    
    switch(cmd)
    {
        case 0:
            start = location;
            //end = ((terminal_row + 1) * width) - 1;
            end = ((row + 1) * width);
            break;
            
        case 1:
            //start = (terminal_row * width);
            start = (row * width);
            end = location;
            break;
            
        case 2:
            //start = (terminal_row * width);
            //end = ((terminal_row + 1) * width) - 1;
            start = (row * width);
            end = ((row + 1) * width);
            break;
        
        default:
            return;
    }

    count = end - start;
    clear_cells(cells + start, count);
    pending_refresh = 1;
}


/*
 * Insert empty lines at the cursor's position.
 */
void insert_lines(uint32_t width, uint32_t height, unsigned long count)
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
        scroll_down(width, height);
    }

    repaint_all();
}


/*
 * Delete lines from the cursor's position.
 */
void delete_lines(uint32_t width, uint32_t height, unsigned long count)
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
        scroll_up(width, height, terminal_row);
    }

    repaint_all();
}


/*
 * Delete count chars from the cursor's position.
 */
void delete_chars(unsigned long count)
{
    uint32_t width = windowsz.ws_col;

    if(terminal_col + count >= width)
    {
        count = width - terminal_col - 1;
    }

    if(count == 0)
    {
        //count = 1;
        return;
    }
    
    if(terminal_col >= width - 1)
    {
        return;
    }

    //size_t location = terminal_row * width + terminal_col;
    //size_t last = ((terminal_row + 1) * width) - 1;
    uint32_t row = terminal_row + first_visible_row;
    size_t location = row * width + terminal_col;
    size_t last = ((row + 1) * width) - 1;
    
    while(count--)
    {
        struct tty_cell_t *dest = cells + location;
        struct tty_cell_t *src = dest + 1;
        struct tty_cell_t *end = cells + last;
        
        for( ; dest < end; src++, dest++)
        {
            *dest = *src;
        }
        
        clear_cell(dest);
    }
    
    pending_refresh = 1;
}


/*
 * Insert count blank chars at the cursor's position.
 */
void insert_chars(unsigned long count)
{
    uint32_t width = windowsz.ws_col;

    if(terminal_col + count >= width)
    {
        count = width - terminal_col - 1;
    }

    if(count == 0)
    {
        //count = 1;
        return;
    }
    
    if(terminal_col >= width - 1)
    {
        return;
    }

    uint32_t row = terminal_row + first_visible_row;
    size_t location = ((row + 1) * width) - 1;
    size_t last = (row * width) + terminal_col;
    
    while(count--)
    {
        struct tty_cell_t *src = cells + last;
        struct tty_cell_t *dest = src - 1;
        struct tty_cell_t *end = cells + location;
        
        for( ; dest > end; src--, dest--)
        {
            *dest = *src;
        }
        
        clear_cell(dest);
    }

    pending_refresh = 1;
}


/*
 * Set the terminal's graphics attributes.
 *
 * For more info, see: https://man7.org/linux/man-pages/man4/console_codes.4.html
 */
void set_attribs(unsigned long npar, unsigned long *par)
{
    unsigned long i;
    
    for(i = 0; i < npar; i++)
    {
        switch(par[i])
        {
            case 0:         // reset to default
                fgcolor = default_fg;
                bgcolor = default_bg;
                terminal_flags &= ~TTY_FLAG_REVERSE_VIDEO;
                terminal_flags &= ~(TTY_FLAG_UNDERLINED|TTY_FLAG_BRIGHT|
                                    TTY_FLAG_BOLD|TTY_FLAG_BLINK);
                break;

            case 1:         // set bold (simulated by a bright color)
                terminal_flags |= TTY_FLAG_BOLD;
                break;

            case 2:         // set bright
                terminal_flags |= TTY_FLAG_BRIGHT;
                break;

            case 4:         // set underscore (simulated by a bright background)
                terminal_flags |= TTY_FLAG_UNDERLINED;
                break;

            case 5:         // set blink (simulated by a bright background)
                terminal_flags |= TTY_FLAG_BLINK;
                break;

            case 7:         // set reverse video
                terminal_flags |= TTY_FLAG_REVERSE_VIDEO;
                break;

            case 21:        // set underline (simulated by setting normal intensity)
                terminal_flags |= TTY_FLAG_UNDERLINED;
                break;

            case 22:        // set normal intensity
                terminal_flags &= ~(TTY_FLAG_UNDERLINED|TTY_FLAG_BRIGHT|
                                    TTY_FLAG_BOLD|TTY_FLAG_BLINK);
                break;

            case 24:        // underline off
                terminal_flags &= ~TTY_FLAG_UNDERLINED;
                break;

            case 25:        // blink off
                terminal_flags &= ~TTY_FLAG_BLINK;
                break;

            case 27:        // reverse video off
                terminal_flags &= ~TTY_FLAG_REVERSE_VIDEO;
                break;

            case 30:        // set black foreground
                fgcolor = COLOR_BLACK;
                break;

            case 31:        // set red foreground
                fgcolor = COLOR_RED;
                break;

            case 32:        // set green foreground
                fgcolor = COLOR_GREEN;
                break;

            case 33:        // set brown foreground
                fgcolor = COLOR_BROWN;
                break;

            case 34:        // set blue foreground
                fgcolor = COLOR_BLUE;
                break;

            case 35:        // set magenta foreground
                fgcolor = COLOR_MAGENTA;
                break;

            case 36:        // set cyan foreground
                fgcolor = COLOR_CYAN;
                break;

            case 37:        // set white foreground
                fgcolor = COLOR_WHITE;
                break;

            case 38:
            case 39:        // set default foreground color
                fgcolor = default_fg;
                break;

            case 40:        // set black background
            case 100:
                bgcolor = COLOR_BLACK;
                break;

            case 41:        // set red background
            case 101:
                bgcolor = COLOR_RED;
                break;

            case 42:        // set green background
            case 102:
                bgcolor = COLOR_GREEN;
                break;

            case 43:        // set brown background
            case 103:
                bgcolor = COLOR_BROWN;
                break;

            case 44:        // set blue background
            case 104:
                bgcolor = COLOR_BLUE;
                break;

            case 45:        // set magenta background
            case 105:
                bgcolor = COLOR_MAGENTA;
                break;

            case 46:        // set cyan background
            case 106:
                bgcolor = COLOR_CYAN;
                break;

            case 47:        // set white background
            case 107:
                bgcolor = COLOR_WHITE;
                break;

            case 48:
            case 49:        // set default background color
                bgcolor = default_bg;
                break;
        }
    }
}


void handle_dec_sequence(unsigned long cmd, int set)
{
    switch(cmd)
    {
        case 1:
            break;
        
        case 5:
            if(set)
            {
                terminal_flags |= TTY_FLAG_REVERSE_VIDEO;
            }
            else
            {
                terminal_flags &= ~TTY_FLAG_REVERSE_VIDEO;
            }
            break;

        case 6:
            if(set)
            {
                terminal_flags |= TTY_FLAG_CURSOR_RELATIVE;
            }
            else
            {
                terminal_flags &= ~TTY_FLAG_CURSOR_RELATIVE;
            }
            break;
        
        case 7:
            // NOTE: we wrap anyway regardless of the flag
            if(set)
            {
                terminal_flags |= TTY_FLAG_AUTOWRAP;
            }
            else
            {
                terminal_flags &= ~TTY_FLAG_AUTOWRAP;
            }
            break;

        case 20:
            if(set)
            {
                terminal_flags |= TTY_FLAG_LFNL;
            }
            else
            {
                terminal_flags &= ~TTY_FLAG_LFNL;
            }
            break;

        default:
            break;
    }
}


void set_scroll_region(unsigned long row1, unsigned long row2)
{
    if(row1 == 0)
    {
        row1++;
    }
    else if(row1 > windowsz.ws_row)
    {
        row1 = windowsz.ws_row;
    }

    if(row2 == 0 || row2 > windowsz.ws_row)
    {
        row2 = windowsz.ws_row;
    }

    if(row1 >= row2)
    {
        return;
    }
    
    scroll_top = row1;
    scroll_bottom = row2;
}


static inline void tputcharat(char c, uint32_t x, uint32_t y, 
                                      uint8_t fg, uint8_t bg)
{
    struct tty_cell_t *cell = &cells[y * terminal_width + x];
    
    cell->chr = c;
    cell->fg = fg;
    cell->bg = bg;
    cell->dirty = 1;
    
    cell->bold = !!(terminal_flags & TTY_FLAG_BOLD);
    cell->underlined = !!(terminal_flags & TTY_FLAG_UNDERLINED);
    cell->blink = !!(terminal_flags & TTY_FLAG_BLINK);
    cell->bright = !!(terminal_flags & TTY_FLAG_BRIGHT);
    
    pending_refresh = 1;
}


static inline void adjust_row_col(uint32_t screenw, uint32_t screenh,
                                  uint32_t screentop)
{
    if(terminal_col >= screenw)
    {
        if(terminal_flags & TTY_FLAG_AUTOWRAP)
        {
            terminal_col = 0;
            terminal_row++;
        }
        else
        {
            terminal_col = screenw - 1;
        }
    }
  
    if(terminal_row >= screenh)
    {
        // scroll up
        scroll_up(screenw, screenh, screentop - 1);
        terminal_row = screenh - 1;
        repaint_all();
    }
}


void tputchar(char c)
{
    uint8_t fg, bg;
    uint32_t screenw = windowsz.ws_col;
    uint32_t screenh = scroll_bottom;
    uint32_t screentop = scroll_top;
    uint32_t visible_row = terminal_row + first_visible_row;
    
    if(terminal_flags & TTY_FLAG_REVERSE_VIDEO)
    {
        fg = bgcolor;
        bg = fgcolor;
    }
    else
    {
        fg = fgcolor;
        bg = bgcolor;
    }

    // line feed, vertical tab, and form feed
    if(c == LF || c == VT || c == FF)
    {
        cell_dirty(terminal_col, visible_row);
        pending_refresh = 1;

        terminal_col = 0;
        terminal_row++;
    }
    else if(c == '\a')
    {
        ;
    }
    else if(c == '\b')
    {
        tremove_last_char(screenw);
    }
    else if(c == CR)
    {
        cell_dirty(terminal_col, visible_row);
        pending_refresh = 1;

        terminal_col = 0;
    }
    else if(c == '\t')
    {
        size_t new_col = (terminal_col + 8) & ~(8 - 1);
        
        for( ; terminal_col < new_col; terminal_col++)
        {
    	    tputcharat(' ', terminal_col, visible_row, fg, bg);
    	}
    }
    else if(c == '\033' /* '\e' */)
    {
        // print ESC as ^[
        tputcharat('^', terminal_col++, visible_row, fg, bg);
        adjust_row_col(screenw, screenh, screentop);
        tputcharat('[', terminal_col++, terminal_row + first_visible_row, fg, bg);
    }
    else
    {
        tputcharat(c, terminal_col, visible_row, fg, bg);
        terminal_col++;
    }
    
    adjust_row_col(screenw, screenh, screentop);
}


/*
 * Send the DEC private identification in response to the escape sequence
 * ESC-Z. Linux claims it is a VT102, and so do we!
 */
void decid(void)
{
    const char *s = "\033[?6c";
    
    write(fd_master, s, 5);
}


/*
 * Save cursor position.
 */
void save_cursor(void)
{
    saved_row = terminal_row;
    saved_col = terminal_col;
}


/*
 * Restore cursor to saved position.
 */
void restore_cursor(void)
{
    terminal_row = saved_row;
    terminal_col = saved_col;
}


/*
 * Save current terminal state. This should include cursor coordinates,
 * attributes, and character sets pointed at by G0 and G1.
 *
 * FIXME: this is not fully implemented yet!
 */
void save_state(void)
{
    save_cursor();
    saved_fg = fgcolor;
    saved_bg = bgcolor;
    saved_attribs = terminal_attribs;
}


/*
 * Restore terminal state that was most recently saved. This should include
 * cursor coordinates, attributes, and character sets pointed at by G0 and G1.
 *
 * FIXME: this is not fully implemented yet!
 */
void restore_state(void)
{
    restore_cursor();
    fgcolor = saved_fg;
    bgcolor = saved_bg;
    terminal_attribs = saved_attribs;
}


/*
 * Device status report. Response depends on cmd:
 *    5 - answer is "ESC [ 0 n" (i.e. terminal ok)
 *    6 - cursor position report, answer is "ESC [ y ; x R"
 */
void status_report(unsigned long cmd)
{
    static char *ok = "\033[0n";
    volatile char *s = ok;
    char buf[32];
    
    switch(cmd)
    {
        case 6:
            buf[0] = '\0';
            sprintf(buf, "\033[%d;%dR",
                    (int)(terminal_row + 1), (int)(terminal_col + 1));
            s = buf;
            __attribute__((fallthrough));
        
        case 5:
            write(fd_master, (void *)s, strlen((char *)s));
            break;

        default:
            return;
    }
}


/*
 * Write output to the system console.
 *
 * See: https://man7.org/linux/man-pages/man4/console_codes.4.html
 */
void console_write(char c)
{
    int csi_ignore = 0;
    
    if(c == 0)
    {
        return;
    }

    // scroll down to cursor if needed
    /*
    if(mouse_scroll_top != first_visible_row)
    {
        mouse_scroll_top = first_visible_row;
        repaint_all();
    }
    */

    //hide_cur();

    switch(state)
    {
        /*
         * Normal state: output printable chars
         */
        case 0:
            // 8 => backspace, 9 => tab, 10 => linefeed, 11 => vertical tab
            // 12 => form feed, 13 => carriage return
            if((c >= '\b' && c <= '\r') || (c >= ' ' && c < DEL))
            {
                tputchar(c);
            }
            else if(c == '\033' /* '\e' */)
            {
                state = 1;
            }
            else if(c == (char)termios.c_cc[VERASE])
            {
                tputchar('\b');
                tputchar(' ');
                tputchar('\b');
            }
            break;
                
        /*
         * Escaped state: after encountering an ESC char in the normal state.
         *                depending on the char following ESC, we might have
         *                a CSI-sequence (ESC followed by '['), or an ESC-
         *                sequence (ESC followed by something else).
         *                For more info, see the link above.
         */
        case 1:
            state = 0;

            switch(c)
            {
                case '[':       // control sequence introducer
                    state = 2;
                    break;
                        
                case '(':       // G0 charset sequence introducer
                    state = 5;
                    break;
                        
                case ')':       // G1 charset sequence introducer
                    state = 6;
                    break;
                        
                case 'c':       // reset
                    console_reset();
                    break;
                        
                case 'D':       // linefeed
                    tputchar('\n');
                    break;
                        
                case 'E':       // newline
                    tset_terminal_col_row(0, terminal_row + 1);
                    break;
                        
                case 'M':       // reverse linefeed
                    if(terminal_row >= scroll_top)
                    {
                        terminal_row--;
                    }
                    else
                    {
                        scroll_down(windowsz.ws_col, scroll_bottom);
                        repaint_all();
                    }
                    break;
                        
                case 'Z':       // DEC private identification
                    decid();
                    break;
                        
                case '7':       // save current state
                    save_state();
                    break;
                        
                case '8':       // restore current state
                    restore_state();
                    break;
            }
            break;
                
        /*
         * CSI state: after encountering '[' in the escaped state.
         *            ESC-[ is followed by a sequence of parameters
         *            (max is NPAR). These are decimal numbers, separated
         *            by semicolons. Absent parameters are taken as 0. The
         *            parameters might be preceded by a '?'.
         *            For more info, see the link above.
         */
        case 2:
            memset(par, 0, sizeof(par));
            npar = 0;
            state = 3;

            // if CSI is followed by another '[', one char is read and the
            // whole sequence is discarded (to ignore an echoed function key)
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
                state = 0;
                csi_ignore = 0;
                break;
            }
                
            if(c == ';' && npar < NPAR - 1)
            {
                // we have room for more parameters
                npar++;
                break;
            }
            else if(c >= '0' && c <= '9')
            {
                // add digit to current parameter
                par[npar] = (10 * par[npar]) + c - '0';
                break;
            }
            else
            {
                state = 4;
            }
                
            // otherwise, start reading parameters
            __attribute__((fallthrough));

        case 4:
            state = 0;
                
            switch(c)
            {
                // Move cursor up the indicated # of rows, to column 1
                case 'F':
                    tset_terminal_col(0);
                    __attribute__((fallthrough));

                // Move cursor up the indicated # of rows
                case 'A':
                    if(!par[0])
                    {
                        // move at least one row
                        par[0]++;
                    }
                        
                    tset_terminal_row(terminal_row - par[0]);
                    break;
                        
                // Move cursor down the indicated # of rows, to column 1
                case 'E':
                    tset_terminal_col(0);
                    __attribute__((fallthrough));

                // Move cursor down the indicated # of rows
                case 'B':
                case 'e':
                    if(!par[0])
                    {
                        // move at least one row
                        par[0]++;
                    }
                        
                    tset_terminal_row(terminal_row + par[0]);
                    break;
                        
                // Move cursor right the indicated # of columns
                case 'C':
                case 'a':
                    if(!par[0])
                    {
                        // move at least one column
                        par[0]++;
                    }
                        
                    tset_terminal_col(terminal_col + par[0]);
                    break;
                        
                // Move cursor left the indicated # of columns
                case 'D':
                    if(!par[0])
                    {
                        // move at least one column
                        par[0]++;
                    }
                        
                    tset_terminal_col(terminal_col - par[0]);
                    break;
                        
                // Move cursor to indicated column in current row
                case '`':
                case 'G':
                    if(par[0])
                    {
                        // make the column zero-based
                        par[0]--;
                    }
                        
                    tset_terminal_col(par[0]);
                    break;
                        
                // Move cursor to indicated row, current column
                case 'd':
                    if(par[0])
                    {
                        // make the row zero-based
                        par[0]--;
                    }
                        
                    tset_terminal_row(par[0]);
                    break;
                        
                // Move cursor to indicated row, column
                case 'H':
                case 'f':
                    if(par[0])
                    {
                        // make the row zero-based
                        par[0]--;
                    }

                    if(par[1])
                    {
                        // make the column zero-based
                        par[1]--;
                    }
                        
                    tset_terminal_col_row(par[1], par[0]);
                    break;
                        
                // Erase display (par[0] is interpreted as shown in the
                // erase_display() function's comment)
                case 'J':
                    erase_display(windowsz.ws_col, windowsz.ws_row, par[0]);
                    break;
                        
                // Erase line (par[0] is interpreted as shown in the
                // erase_line() function's comment)
                case 'K':
                    erase_line(par[0]);
                    break;
                        
                // Insert the indicated # of blank lines
                case 'L':
                    insert_lines(windowsz.ws_col, scroll_bottom, par[0]);
                    break;
                        
                // Delete the indicated # of lines
                case 'M':
                    delete_lines(windowsz.ws_col, scroll_bottom, par[0]);
                    break;
                        
                // Erase the indicated # of chars in the current line
                // NOTE: not implemented yet!
                case 'X': ;
                    __attribute__((fallthrough));

                // Delete the indicated # of chars in the current line
                case 'P':
                    delete_chars(par[0]);
                    break;
                        
                // Insert the indicated # of blank chars
                case '@':
                    insert_chars(par[0]);
                    break;
                        
                // Set graphics attributes
                case 'm':
                    set_attribs(npar + 1, par);
                    break;
                        
                // Answer ESC [ ? 6 c: "I am a VT102"
                case 'c':
                    decid();
                    break;
                        
                // Status report
                case 'n':
                    status_report(par[0]);
                    break;
                        
                // Save cursor location
                case 's':
                    save_cursor();
                    break;
                        
                // Restore cursor location
                case 'u':
                    restore_cursor();
                    break;

                // Set scrolling region
                case 'r':
                    set_scroll_region(par[0], par[1]);
                    tset_terminal_col_row(0, scroll_top - 1);
                    break;

                // Private mode (DECSET/DECRST) sequences.
                // 'h' sequences set, and 'l' sequences reset modes.
                // For more info, see the link above.
                case 'h':
                    handle_dec_sequence(par[0], 1);
                    break;

                case 'l':
                    handle_dec_sequence(par[0], 0);
                    break;

                default:
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
            state = 0;
            break;

        /*
         * FIXME: Define G1 charset:
         *        ESC-) is followed by B, 0, U or K (as above).
         *        For more info, see the link above.
         */
        case 6:
            state = 0;
            break;
    }
}


void process_mouse(int x, int y, mouse_buttons_t buttons)
{
    if(buttons & MOUSE_VSCROLL_DOWN)
    {
        if(mouse_scroll_top >= first_visible_row)
        {
            return;
        }

        mouse_scroll_top++;
        repaint_all();
    }

    if(buttons & MOUSE_VSCROLL_UP)
    {
        if(mouse_scroll_top <= first_text_row)
        {
            return;
        }

        mouse_scroll_top--;
        repaint_all();
    }
}

