/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: colorchooser-dialog.c
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
 *  \file colorchooser-dialog.c
 *
 *  The implementation of a color chooser dialog box.
 */

#include "../include/event.h"
#include "../include/resources.h"
#include "../include/client/button.h"
#include "../include/client/label.h"
#include "../include/client/dialog.h"

#include "inlines.c"

#define GLOB                        __global_gui_data

#define SPECTRUM_WIDTH              200
#define SPECTRUM_HEIGHT             200
#define SPECTRUM_X                  290
#define SPECTRUM_Y                  20

#define PALETTE_ROWS                7
#define PALETTE_COLS                10
#define PALETTE_X                   20
#define PALETTE_Y                   20
#define PALETTE_WIDTH               (25 * PALETTE_COLS)
#define PALETTE_HEIGHT              (25 * PALETTE_ROWS)

#define RAINBOW_X                   510
#define RAINBOW_Y                   22
#define RAINBOW_BASECOLS_COUNT      7
#define RAINBOW_REGION_SIZE         32
#define RAINBOW_HEIGHT              (RAINBOW_BASECOLS_COUNT * RAINBOW_REGION_SIZE)
#define RAINBOW_WIDTH               20

#define COLORBOX_X                  290
#define COLORBOX_Y                  230
#define COLORBOX_WIDTH              30
#define COLORBOX_HEIGHT             28

#define COLORTEXT_X                 330
#define COLORTEXT_Y                 230
#define COLORTEXT_WIDTH             80


extern void messagebox_dispatch_event(struct event_t *ev);
extern void dialog_button_handler(struct button_t *button, int x, int y);
static void reset_color_rainbow(void);


static uint32_t standard_palette[PALETTE_ROWS][PALETTE_COLS] =
{
    { 0x000000ff, 0x262626ff, 0x454545ff, 0x646464ff, 0x7f7f7fff, 0x989898ff, 0xafafafff, 0xc8c8c8ff, 0xe3e3e3ff, 0xffffffff },
    { 0x980000ff, 0xff0000ff, 0xff9900ff, 0xffff00ff, 0x00ff00ff, 0x00ffffff, 0x4a86e8ff, 0x0000ffff, 0x9900ffff, 0xff00ffff },
    { 0xe6b8afff, 0xf4ccccff, 0xfce5cdff, 0xfff2ccff, 0xd9ead3ff, 0xd0e0e3ff, 0xc9daf8ff, 0xcfe2f3ff, 0xd9d2e9ff, 0xead1dcff },
    { 0xdd7e6bff, 0xea9999ff, 0xf9cb9cff, 0xffe599ff, 0xb6d7a8ff, 0xa2c4c9ff, 0xa4c2f4ff, 0x9fc5e8ff, 0xb4a7d6ff, 0xd5a6bdff },
    { 0xcc4125ff, 0xe06666ff, 0xf6b26bff, 0xffd966ff, 0x93c47dff, 0x76a5afff, 0x6d9eebff, 0x6fa8dcff, 0x8e7cc3ff, 0xc27ba0ff },
    { 0xa61c00ff, 0xcc0000ff, 0xe69138ff, 0xf1c232ff, 0x6aa84fff, 0x45818eff, 0x3c78d8ff, 0x3d85c6ff, 0x674ea7ff, 0xa64d79ff },
    { 0x5b0f00ff, 0x660000ff, 0x783f04ff, 0x7f6000ff, 0x274e13ff, 0x0c343dff, 0x1c4587ff, 0x073763ff, 0x20124dff, 0x4c1130ff },
};

static uint32_t rainbow_base_colors[] =
{
    // red, orange, yellow, green, blue, indigo, violet
    0xFF0000FF, 0xFFA500FF, 0xFFFF00FF, 0x00FF00FF, 0x0000FFFF, 0x4B0082FF, 0x800080FF
};

static uint32_t rainbow_colors[RAINBOW_HEIGHT];


struct colorchooser_dialog_t *colorchooser_dialog_create(winid_t owner)
{
    struct colorchooser_dialog_t *dialog;
    
    if(!(dialog = malloc(sizeof(struct colorchooser_dialog_t))))
    {
        return NULL;
    }
    
    A_memset(dialog, 0, sizeof(struct colorchooser_dialog_t));

    // we will fill this array later before showing the dialog box
    if(!(dialog->internal.color_spectrum = 
                malloc(SPECTRUM_WIDTH * SPECTRUM_HEIGHT * 4)))
    {
        free(dialog);
        return NULL;
    }

    dialog->ownerid = owner;
    // choose any arbitrary color (blue)
    dialog->internal.color = 0x0000FFFF;

    // init the color rainbow
    reset_color_rainbow();
    
    return dialog;
}


#define R(c)        (((c) >> 24) & 0xff)
#define G(c)        (((c) >> 16) & 0xff)
#define B(c)        (((c) >> 8 ) & 0xff)


static void reset_color_rainbow(void)
{
    int i, j, k;
    int sr, sg, sb;
    uint32_t col1, col2, cur;
    float r, g, b;
    // steps for the R, G, B components
    float rstep, gstep, bstep;

    for(i = 0, k = 0; i < RAINBOW_BASECOLS_COUNT - 1; i++)
    {
        col1 = rainbow_base_colors[i];
        col2 = rainbow_base_colors[i + 1];
        sr = R(col1);
        sg = G(col1);
        sb = B(col1);
        r = sr; g = sg; b = sb;
        rstep = ((int)R(col2) - sr) / RAINBOW_REGION_SIZE;
        gstep = ((int)G(col2) - sg) / RAINBOW_REGION_SIZE;
        bstep = ((int)B(col2) - sb) / RAINBOW_REGION_SIZE;

        for(j = 0; j < RAINBOW_REGION_SIZE; j++, k++)
        {
            cur = ((int)r << 24) | ((int)g << 16) | ((int)b << 8) | 0xff;
            rainbow_colors[k] = cur;
            r += rstep;
            g += gstep;
            b += bstep;
        }
    }
}


/*
 * Alpha blending using the given alpha channel.
 * See: https://www.virtualdub.org/blog2/entry_117.html
 */
static inline uint32_t alpha_blend(uint32_t c1, int a)
{
    uint32_t rbdst, gdst, tmp, tmp2;
    uint32_t alpha = (a & 0xff);

    rbdst = (c1 & 0xff00ff00) >> 8;
    gdst  = (c1 & 0x00ff0000) >> 16;
    tmp = (rbdst + (((0 - rbdst) * alpha + 0x800080) >> 8)) & 0xff00ff;
    tmp2 = (gdst + (((0 - gdst) * alpha + 0x80) >> 8)) & 0xff;

    return ((tmp << 8) | (tmp2 << 16) | 0xff);
}

/*
 * To create a spectrum for the chosen color, we use two gradients:
 *   - Horizontal gradient from white (0xFFFFFFFF) to the chosen color. This
 *     will be the primary color for the given pixel.
 *   - Vertical gradient from nothing (0x00000000) to black (0x000000FF)/ This
 *     will be an "alpha mask" we apply to each pixel.
 * We create the horizontal gradient colors, then blend each pixel in every
 * row with the vertical alpha mask of its row.
 */
static void reset_color_spectrum(struct __colorchooser_internal_state_t *internal)
{
    int i, j, k;
    uint32_t col1 = 0xFFFFFFFF;
    uint32_t col2 = internal->color;
    uint32_t cur;
    int sr, sg, sb;
    float r, g, b, a = 0.0;
    // horizontal steps for the R, G, B components
    float rhstep = ((int)R(col2) - (int)R(col1)) / (float)SPECTRUM_WIDTH;
    float ghstep = ((int)G(col2) - (int)G(col1)) / (float)SPECTRUM_WIDTH;
    float bhstep = ((int)B(col2) - (int)B(col1)) / (float)SPECTRUM_WIDTH;
    // vertical step for the alpha mask
    float avstep = 0xFF / (float)SPECTRUM_HEIGHT;

    for(i = 0, k = 0; i < SPECTRUM_HEIGHT; i++)
    {
        sr = R(col1);
        sg = G(col1);
        sb = B(col1);
        r = sr; g = sg; b = sb;

        for(j = 0; j < SPECTRUM_WIDTH; j++, k++)
        {
            cur = ((int)r << 24) | ((int)g << 16) | ((int)b << 8) | 0xff;
            internal->color_spectrum[k] = alpha_blend(cur, (int)a);
            r += rhstep;
            g += ghstep;
            b += bhstep;
        }

        a += avstep;
    }

    // the chosen color is always at the top-right corner of the spectrum
    internal->spectrumx = SPECTRUM_X + SPECTRUM_WIDTH - 1;
    internal->spectrumy = SPECTRUM_Y;
}


static void label_palette_repaint(struct window_t *label_window, int is_active_child)
{
    struct window_t *parent = label_window->parent;
    struct __colorchooser_internal_state_t *internal = parent->internal_data;
    int x, y = PALETTE_Y;
    int i, j;

    // draw the standard palette buckets on the left
    for(i = 0; i < PALETTE_ROWS; i++)
    {
        x = PALETTE_X;

        for(j = 0; j < PALETTE_COLS; j++)
        {
            gc_draw_rect(label_window->gc, x, y, 20, 20, GLOBAL_BLACK_COLOR);
            gc_fill_rect(label_window->gc, x + 1, y + 1, 18, 18, standard_palette[i][j]);

            // draw an inverted 3D border around the selected palette color
            if(j == internal->palettecol && i == internal->paletterow)
            {
                draw_inverted_3d_border(label_window->gc, x, y, 20, 20);
            }

            x += 25;
        }

        y += 25;
    }
}


static void label_spectrum_repaint(struct window_t *label_window, int is_active_child)
{
    struct window_t *parent = label_window->parent;
    struct __colorchooser_internal_state_t *internal = parent->internal_data;
    struct bitmap32_t spectrum;

    spectrum.width = SPECTRUM_WIDTH;
    spectrum.height = SPECTRUM_HEIGHT;
    spectrum.data = internal->color_spectrum;

    struct clipping_t saved_clipping;
    struct clipping_t new_clipping = { label_window->clip_rects, 1 };
    gc_get_clipping(label_window->gc, &saved_clipping);
    gc_set_clipping(label_window->gc, &new_clipping);

    // draw the color spectrum on the right
    gc_blit_bitmap(label_window->gc, &spectrum,
                   SPECTRUM_X, SPECTRUM_Y,
                   0, 0, SPECTRUM_WIDTH, SPECTRUM_HEIGHT);

    // draw a circle around the spectrum color
    gc_circle(label_window->gc, internal->spectrumx, internal->spectrumy,
              3, 2, GLOBAL_BLACK_COLOR);

    gc_set_clipping(label_window->gc, &saved_clipping);
}


static void label_rainbow_repaint(struct window_t *label_window, int is_active_child)
{
    struct window_t *parent = label_window->parent;
    struct __colorchooser_internal_state_t *internal = parent->internal_data;
    int i, y = RAINBOW_Y, sy = -1;

    // draw the color rainbow on the far right
    for(i = 0; i < RAINBOW_HEIGHT; i++, y++)
    {
        gc_horizontal_line(label_window->gc, RAINBOW_X, y, 
                           RAINBOW_WIDTH, rainbow_colors[i]);

        if(internal->color == rainbow_colors[i])
        {
            sy = y;
        }
    }

    // draw a black rectangle around the selected color
    if(sy >= 0)
    {
        gc_draw_rect(label_window->gc, RAINBOW_X, sy - 1, 
                     RAINBOW_WIDTH, 3, GLOBAL_BLACK_COLOR);
    }
}


static void label_colorbox_repaint(struct window_t *label_window, int is_active_child)
{
    struct window_t *parent = label_window->parent;
    struct __colorchooser_internal_state_t *internal = parent->internal_data;

    gc_draw_rect(label_window->gc, COLORBOX_X, COLORBOX_Y, 
                 COLORBOX_WIDTH, COLORBOX_HEIGHT, GLOBAL_BLACK_COLOR);
    gc_fill_rect(label_window->gc, COLORBOX_X + 1, COLORBOX_Y + 1, 
                 COLORBOX_WIDTH - 2, COLORBOX_HEIGHT - 2, internal->color);
}


static inline uint8_t hex_digit(uint8_t d)
{
    return (d >= 10 && d <= 15) ? 'a' + d - 10 : '0' + d;
}

static inline void update_inputbox_text(struct __colorchooser_internal_state_t *internal)
{
    uint8_t r = (internal->color >> 24) & 0xff;
    uint8_t g = (internal->color >> 16) & 0xff;
    uint8_t b = (internal->color >> 8 ) & 0xff;
    char buf[16];

    sprintf(buf, "%02x%02x%02x", r, g, b);

    // redraw the color Hex box
    inputbox_set_text((struct window_t *)internal->inputbox_colorbox, buf);
    inputbox_repaint((struct window_t *)internal->inputbox_colorbox, 0);
    child_invalidate((struct window_t *)internal->inputbox_colorbox);

    // redraw the color Red component box
    buf[0] = hex_digit((r >> 4) & 0x0f);
    buf[1] = hex_digit(r & 0x0f);
    buf[2] = '\0';
    inputbox_set_text((struct window_t *)internal->inputbox_r, buf);
    inputbox_repaint((struct window_t *)internal->inputbox_r, 0);
    child_invalidate((struct window_t *)internal->inputbox_r);

    // redraw the color Green component box
    buf[0] = hex_digit((g >> 4) & 0x0f);
    buf[1] = hex_digit(g & 0x0f);
    inputbox_set_text((struct window_t *)internal->inputbox_g, buf);
    inputbox_repaint((struct window_t *)internal->inputbox_g, 0);
    child_invalidate((struct window_t *)internal->inputbox_g);

    // redraw the color Blue component box
    buf[0] = hex_digit((b >> 4) & 0x0f);
    buf[1] = hex_digit(b & 0x0f);
    inputbox_set_text((struct window_t *)internal->inputbox_b, buf);
    inputbox_repaint((struct window_t *)internal->inputbox_b, 0);
    child_invalidate((struct window_t *)internal->inputbox_b);
}


static void label_spectrum_mousedown(struct window_t *label_window, 
                                     struct mouse_state_t *mstate)
{
    struct window_t *parent = label_window->parent;
    struct __colorchooser_internal_state_t *internal = parent->internal_data;

    if(mstate->x < 0 || mstate->x > SPECTRUM_WIDTH ||
       mstate->y < 0 || mstate->y > SPECTRUM_HEIGHT)
    {
        return;
    }

    // find the color at mouse position
    internal->color = 
            internal->color_spectrum[(mstate->y * SPECTRUM_WIDTH) + mstate->x];

    // remember the mouse position
    internal->spectrumx = SPECTRUM_X + mstate->x;
    internal->spectrumy = SPECTRUM_Y + mstate->y;
    label_spectrum_repaint(label_window, 1);
    child_invalidate(label_window);

    // update the color box
    label_colorbox_repaint((struct window_t *)internal->label_colorbox, 0);
    child_invalidate((struct window_t *)internal->label_colorbox);

    update_inputbox_text(internal);
}


static void label_rainbow_mousedown(struct window_t *label_window, 
                                    struct mouse_state_t *mstate)
{
    struct window_t *parent = label_window->parent;
    struct __colorchooser_internal_state_t *internal = parent->internal_data;

    if(mstate->x < 0 || mstate->x > RAINBOW_WIDTH ||
       mstate->y < 0 || mstate->y > RAINBOW_HEIGHT)
    {
        return;
    }

    // find the color at mouse position
    internal->color = rainbow_colors[mstate->y];
    label_rainbow_repaint(label_window, 1);
    child_invalidate(label_window);

    // update the color spectrum
    reset_color_spectrum(internal);
    label_spectrum_repaint((struct window_t *)internal->label_spectrum, 0);
    child_invalidate((struct window_t *)internal->label_spectrum);

    // update the color box
    label_colorbox_repaint((struct window_t *)internal->label_colorbox, 0);
    child_invalidate((struct window_t *)internal->label_colorbox);

    update_inputbox_text(internal);
}


static void label_palette_mousedown(struct window_t *label_window, 
                                    struct mouse_state_t *mstate)
{
    struct window_t *parent = label_window->parent;
    struct __colorchooser_internal_state_t *internal = parent->internal_data;
    int x, y, oldx, oldy;

    if(mstate->x < 0 || mstate->x > PALETTE_WIDTH ||
       mstate->y < 0 || mstate->y > PALETTE_HEIGHT)
    {
        return;
    }

    // Each bucket is 20x20, with 5 pixel padding on the right and bottom.
    // Find the bucket's x & y position, then ensure the mouse falls in the
    // 20x20 box, not in the padding.
    x = mstate->x / 25;
    y = mstate->y / 25;

    if(mstate->x > (x * 25 + 20) || mstate->y > (y * 25 + 20))
    {
        return;
    }

    if(x == internal->palettecol && y == internal->paletterow)
    {
        return;
    }

    oldx = internal->palettecol;
    oldy = internal->paletterow;
    internal->palettecol = x;
    internal->paletterow = y;
    x = PALETTE_X + (x * 25);
    y = PALETTE_Y + (y * 25);

    // draw the previously selected color with no border
    if(oldx >= 0 && oldy >= 0)
    {
        uint32_t color = standard_palette[oldy][oldx];
        oldx = PALETTE_X + (oldx * 25);
        oldy = PALETTE_Y + (oldy * 25);
        gc_draw_rect(label_window->gc, oldx, oldy, 20, 20, GLOBAL_BLACK_COLOR);
        gc_fill_rect(label_window->gc, oldx + 1, oldy + 1, 18, 18, color);
    }

    // draw the new selected color with a 3D border
    internal->color = standard_palette[internal->paletterow][internal->palettecol];
    gc_draw_rect(label_window->gc, x, y, 20, 20, GLOBAL_BLACK_COLOR);
    gc_fill_rect(label_window->gc, x + 1, y + 1, 18, 18, internal->color);

    // draw an inverted 3D border around the selected palette color
    draw_inverted_3d_border(label_window->gc, x, y, 20, 20);
    child_invalidate(label_window);

    // update the color spectrum
    reset_color_spectrum(internal);
    label_spectrum_repaint((struct window_t *)internal->label_spectrum, 0);
    child_invalidate((struct window_t *)internal->label_spectrum);

    // update the color box
    label_colorbox_repaint((struct window_t *)internal->label_colorbox, 0);
    child_invalidate((struct window_t *)internal->label_colorbox);

    update_inputbox_text(internal);
}


int colorchooser_dialog_show(struct colorchooser_dialog_t *dialog)
{
    int x, y, i;
    struct window_attribs_t attribs;
    struct button_t *button;
    struct label_t *label;
    struct __colorchooser_internal_state_t *internal = &dialog->internal;

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = 550;
    attribs.h = 380;
    attribs.flags = WINDOW_NORESIZE | WINDOW_NOMINIMIZE | WINDOW_SKIPTASKBAR;

    if(!(dialog->window = __window_create(&attribs, WINDOW_TYPE_DIALOG, 
                                                    dialog->ownerid)))
    {
        return 0;
    }

    dialog->window->event_handler = messagebox_dispatch_event;
    internal->status.close_dialog = 0;
    internal->status.dialog_thread = pthread_self();
    dialog->window->internal_data = internal;
    window_set_title(dialog->window, "Choose a color");

    x = 380;
    y = 340;
    button = button_new(dialog->window->gc, dialog->window,
                        x, y, 70, 30, "Cancel");
    button->internal_data = (void *)(intptr_t)1;
    button->button_click_callback = dialog_button_handler;

    x += 80;
    button = button_new(dialog->window->gc, dialog->window,
                        x, y, 70, 30, "Ok");
    button->internal_data = (void *)(intptr_t)2;
    button->button_click_callback = dialog_button_handler;

    // Create a label for drawing the color spectrum. We will override its
    // repaint function so we can draw our spectrum, and its mouse_down
    // function so we can update the chosen color
    internal->label_spectrum = label_new(dialog->window->gc, dialog->window,
                                         SPECTRUM_X, SPECTRUM_Y, 
                                         SPECTRUM_WIDTH, SPECTRUM_WIDTH, NULL);
    internal->label_spectrum->window.repaint = label_spectrum_repaint;
    internal->label_spectrum->window.mousedown = label_spectrum_mousedown;

    // Same for the color rainbow
    internal->label_rainbow = label_new(dialog->window->gc, dialog->window,
                                        RAINBOW_X, RAINBOW_Y, 
                                        RAINBOW_WIDTH, RAINBOW_HEIGHT, NULL);
    internal->label_rainbow->window.repaint = label_rainbow_repaint;
    internal->label_rainbow->window.mousedown = label_rainbow_mousedown;

    // And the palette buckets label
    internal->label_palette = label_new(dialog->window->gc, dialog->window,
                                        PALETTE_X, PALETTE_Y, 
                                        PALETTE_WIDTH, PALETTE_HEIGHT, NULL);
    internal->label_palette->window.repaint = label_palette_repaint;
    internal->label_palette->window.mousedown = label_palette_mousedown;

    // And the selected colour box
    internal->label_colorbox = label_new(dialog->window->gc, dialog->window,
                                         COLORBOX_X, COLORBOX_Y, 
                                         COLORBOX_WIDTH, COLORBOX_HEIGHT, NULL);
    internal->label_colorbox->window.repaint = label_colorbox_repaint;

    // Add an input box to show the color Hex value
    internal->inputbox_colorbox = inputbox_new(dialog->window->gc, dialog->window,
                                               COLORTEXT_X, COLORTEXT_Y,
                                               COLORTEXT_WIDTH, NULL);

    // Add input boxes for the color's R, G, B components, along with labels
    // to describe the input boxes
    label = label_new(dialog->window->gc, dialog->window,
                      COLORTEXT_X + COLORTEXT_WIDTH + 40, COLORTEXT_Y + 5, 
                      50, 28, "Red:");
    label->window.text_alignment = TEXT_ALIGN_RIGHT;
    internal->inputbox_r = 
              inputbox_new(dialog->window->gc, dialog->window,
                           COLORTEXT_X + COLORTEXT_WIDTH + 100, COLORTEXT_Y,
                           30, NULL);

    label = label_new(dialog->window->gc, dialog->window,
                      COLORTEXT_X + COLORTEXT_WIDTH + 40, COLORTEXT_Y + 35, 
                      50, 28, "Green:");
    label->window.text_alignment = TEXT_ALIGN_RIGHT;
    internal->inputbox_g = 
              inputbox_new(dialog->window->gc, dialog->window,
                           COLORTEXT_X + COLORTEXT_WIDTH + 100, COLORTEXT_Y + 30,
                           30, NULL);

    label = label_new(dialog->window->gc, dialog->window,
                      COLORTEXT_X + COLORTEXT_WIDTH + 40, COLORTEXT_Y + 65, 
                      50, 28, "Blue:");
    label->window.text_alignment = TEXT_ALIGN_RIGHT;
    internal->inputbox_b = 
              inputbox_new(dialog->window->gc, dialog->window,
                           COLORTEXT_X + COLORTEXT_WIDTH + 100, COLORTEXT_Y + 60,
                           30, NULL);

    // reset color spectrum
    reset_color_spectrum(internal);
    internal->paletterow = -1;
    internal->palettecol = -1;

    // now paint and show the dialog box
    window_repaint(dialog->window);
    update_inputbox_text(internal);

    simple_request(REQUEST_DIALOG_SHOW, GLOB.server_winid, 
                                        dialog->window->winid);
    dialog->window->flags &= ~WINDOW_HIDDEN;

    while(1)
    {
        struct event_t *ev = NULL;
        
        if((ev = next_event_for_seqid(NULL /* dialog->window */, 0, 1)))
        {
            //messagebox_dispatch_event(ev);
            event_dispatch(ev);
            free(ev);
        }
        
        if(internal->status.close_dialog)
        {
            i = internal->status.selected_button;
            break;
        }
    }
    
    simple_request(REQUEST_DIALOG_HIDE, GLOB.server_winid, dialog->window->winid);
    dialog->window->flags |= WINDOW_HIDDEN;

    return (i == 2) ? DIALOG_RESULT_OK : DIALOG_RESULT_CANCEL;
}


void colorchooser_dialog_destroy(struct colorchooser_dialog_t *dialog)
{
    if(!dialog || !dialog->window)
    {
        return;
    }
    
    window_destroy_children(dialog->window);
    window_destroy(dialog->window);
    free(dialog);
}


void colorchooser_dialog_set_color(struct colorchooser_dialog_t *dialog, uint32_t color)
{
    // ensure it has full alpha channel
    dialog->internal.color = color | 0xff;
}


uint32_t colorchooser_dialog_get_color(struct colorchooser_dialog_t *dialog)
{
    return dialog->internal.color;
}

