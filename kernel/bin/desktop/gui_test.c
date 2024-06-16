#include <errno.h>
#include "include/gui.h"
#include "include/event.h"
#include "include/resources.h"

struct window_t *main_window;


#define CA 0x000000FF //Black
#define CB 0xFFFFFFFF //White
#define C_ 0x00FF00FF //Clear (Green)

#define IMG_WIDTH         16
#define IMG_HEIGHT        24
#define IMG_BUFSZ         (IMG_WIDTH * IMG_HEIGHT)

uint32_t sample_image[IMG_BUFSZ] =
{
    C_, C_, C_, CA, CA, CA, CA, CA, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CA, CB, CA, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CA, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, C_, C_, CA, CB, CA, C_, C_, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CA, CB, CA, CA, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CB, CB, CB, CB, CB, CA, C_, C_, C_, C_, C_, C_,
    C_, C_, C_, CA, CA, CA, CA, CA, CA, CA, C_, C_, C_, C_, C_, C_,
};


void myrepaint(struct window_t *window, int is_active_child)
{
    (void)is_active_child;

    gc_fill_rect(window->gc, 0, 0, window->w, window->h, 0xffff00ff);
    /*
    gc_fill_rect_clipped(window->gc, &window->gc->clipping, 0, 0,
                         window->w, window->h, 0xffff00ff);
    */

    gc_draw_rect_thick_clipped(window->gc, &window->gc->clipping,
                               30, 140, 60, 40, 10, 0x333333ff);

    gc_circle_clipped(window->gc, &window->gc->clipping,
                      20, 20, 50, 4, 0xff00ffff);

    gc_circle_filled_clipped(window->gc, &window->gc->clipping,
                             70, 70, 30, 0xaaaa33ff);

    gc_arc_clipped(window->gc, &window->gc->clipping,
                    150, 150, 35, 15, 115, 3, 0x999999ff);

    gc_line_clipped(window->gc, &window->gc->clipping,
                    100, 100, 150, 150, 7, 0x11ffaaff);

    gc_line_clipped(window->gc, &window->gc->clipping,
                    100, 100, 150, 100, 3, 0x11ffaaff);

    gc_line_clipped(window->gc, &window->gc->clipping,
                    100, 100, 100, 150, 1, 0x11ffaaff);

    int vertices[10] = { 150, 100, 175, 100, 160, 130, 120, 140, 160, 110 };
    gc_polygon_fill_clipped(window->gc, &window->gc->clipping,
                            vertices, 5, 0xff1111ff);

    int vertices2[8] = { 120, 150, 130, 180, 80, 160, 60, 190 };
    gc_polygon_fill_clipped(window->gc, &window->gc->clipping,
                            vertices2, 4, 0xff1111ff);

    gc_polygon_clipped(window->gc, &window->gc->clipping,
                       vertices2, 4,
                       6, 0x111111ff);

    struct bitmap32_t bmp;

    bmp.width = IMG_WIDTH;
    bmp.height = IMG_HEIGHT;
    bmp.data = sample_image;

    gc_stretch_bitmap(window->gc, &bmp,
                      10, 200, IMG_WIDTH, IMG_HEIGHT,
                      0, 0, IMG_WIDTH, IMG_HEIGHT);

    gc_stretch_bitmap(window->gc, &bmp,
                      40, 200, IMG_WIDTH * 2, IMG_HEIGHT,
                      0, 0, IMG_WIDTH, IMG_HEIGHT);

    gc_stretch_bitmap(window->gc, &bmp,
                      80, 200, IMG_WIDTH, IMG_HEIGHT * 2,
                      0, 0, IMG_WIDTH, IMG_HEIGHT);

    gc_stretch_bitmap(window->gc, &bmp,
                      120, 200, IMG_WIDTH * 2, IMG_HEIGHT * 2,
                      0, 0, IMG_WIDTH, IMG_HEIGHT);

    gc_stretch_bitmap(window->gc, &bmp,
                      160, 200, IMG_WIDTH / 2, IMG_HEIGHT,
                      0, 0, IMG_WIDTH, IMG_HEIGHT);

    gc_stretch_bitmap(window->gc, &bmp,
                      10, 240, IMG_WIDTH, IMG_HEIGHT / 2,
                      0, 0, IMG_WIDTH, IMG_HEIGHT);

    gc_stretch_bitmap(window->gc, &bmp,
                      40, 240, IMG_WIDTH / 2, IMG_HEIGHT / 2,
                      0, 0, IMG_WIDTH, IMG_HEIGHT);


    bmp.width = 64;
    bmp.height = 64;
    bmp.data = NULL;

    if((sysicon_load("sign-left", &bmp)))
    {
        gc_stretch_bitmap(window->gc, &bmp, 200, 10, 16, 16,
                          0, 0, bmp.width, bmp.height);

        gc_stretch_bitmap(window->gc, &bmp, 200, 30, 28, 28,
                          0, 0, bmp.width, bmp.height);

        gc_stretch_bitmap(window->gc, &bmp, 200, 60, 48, 48,
                          0, 0, bmp.width, bmp.height);

        gc_stretch_bitmap(window->gc, &bmp, 200, 110, bmp.width, bmp.height,
                          0, 0, bmp.width, bmp.height);
    }


    window_invalidate(window);
}

int main(int argc, char **argv)
{
    /* volatile */ struct event_t *ev = NULL;
    struct window_attribs_t attribs;

    gui_init(argc, argv);

    attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
    attribs.x = 30;
    attribs.y = 30;
    attribs.w = 400;
    attribs.h = 300;
    attribs.flags = 0;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n", argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    main_window->repaint = myrepaint;
    window_repaint(main_window);
    window_show(main_window);

    while(1)
    {
        if(!(ev = next_event()))
        {
            continue;
        }
        
        if(event_dispatch(ev))
        {
            free(ev);
            continue;
        }

        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        free(ev);
    }
}

