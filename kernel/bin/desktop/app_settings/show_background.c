/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: show_background.c
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
 *  \file show_background.c
 *
 *  Part of the system settings program. Shows a window with desktop 
 *  background settings.
 */

#include <sys/stat.h>
#include "../include/gui.h"
#include "../include/client/dialog.h"
#include "../include/client/radio-button.h"
#include "../include/client/combobox.h"
#include "../desktop/desktop.h"
#include "defs.h"

#define GLOB                    __global_gui_data
#define BACKGROUNDS_DIR_PATH    "/usr/share/gui/desktop/backgrounds"

#define TEMPLATE_FGCOLOR        0xFFFFFFFF

#define __                      0x00000000,
#define _B                      GLOBAL_BLACK_COLOR,
#define _F                      TEMPLATE_FGCOLOR,

#define BGCOLOR_IMGW            26
#define BGCOLOR_IMGH            26

static uint32_t bgcolor_img_template[] =
{
  __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
  __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
  __ __ _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _F _B __ __
  __ __ _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B _B __ __
  __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
  __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __
};

#undef __
#undef _B
#undef _F

static uint32_t bgcolor_img_data[BGCOLOR_IMGW * BGCOLOR_IMGH * 4];

static struct bitmap32_t bgcolor_img =
{
    .width = BGCOLOR_IMGW, .height = BGCOLOR_IMGH, .data = bgcolor_img_data,
};

uint32_t cur_bgcolor = 0x16A085FF;
char *cur_bgimage = NULL;
int cur_bgimage_aspect = 0;

struct __internal_t
{
    struct imgbutton_t *imgbutton;
    struct button_t *button;
    struct combobox_t *combobox;
};


static inline char *simple_basename(char *name)
{
    char *slash = strrchr(name, '/');

    return (slash && slash[1] != '\0') ? slash + 1 : name;
}


static winid_t get_desktop_winid(void)
{
    struct event_t ev, *ev2;
    uint32_t seqid;
    static winid_t desktop_winid = 0;

    if(desktop_winid != 0)
    {
        return desktop_winid;
    }

    seqid = __next_seqid();
    ev.seqid = seqid;
    ev.type = REQUEST_GET_ROOT_WINID;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(ev));

    if(!(ev2 = get_server_reply(seqid)))
    {
        return 0;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return 0;
    }

    desktop_winid = ev2->winattr.winid;
    free(ev2);

    return desktop_winid;
}


/*
 * Get the current back color/image from the desktop task.
 */
void get_desktop_bg(void)
{
    struct event_t ev, *ev2;
    struct event_desktop_bg_t *evres;
    uint32_t seqid;
    winid_t desktop_winid;

    if(!(desktop_winid = get_desktop_winid()))
    {
        messagebox_show(main_window->winid, "Error!",
                        "Failed to get desktop window id.", DIALOG_OK, 0);
        return;
    }

    seqid = __next_seqid();
    ev.seqid = seqid;
    ev.type = REQUEST_GET_DESKTOP_BACKGROUND;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = desktop_winid;
    ev.valid_reply = 1;         // so the desktop app would not filter it out
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(ev));

    if(!(ev2 = get_server_reply(seqid)))
    {
        return;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return;
    }

    evres = (struct event_desktop_bg_t *)ev2;

    if(!evres->bg_is_image)
    {
        cur_bgcolor = *((uint32_t *)evres->data);
    }
    else
    {
        if(cur_bgimage)
        {
            free(cur_bgimage);
        }

        cur_bgimage = strdup(evres->data);
    }

    free(ev2);
}


/*
 * Tell the desktop task to set background to the given color.
 */
void set_desktop_bg_color(uint32_t color)
{
    size_t tmpsz = sizeof(struct event_desktop_bg_t) + sizeof(uint32_t);
    char tmp[tmpsz];
    struct event_desktop_bg_t *evres = (struct event_desktop_bg_t *)tmp;
    uint32_t seqid;
    winid_t desktop_winid;

    if(!(desktop_winid = get_desktop_winid()))
    {
        messagebox_show(main_window->winid, "Error!",
                        "Failed to get desktop window id.", DIALOG_OK, 0);
        return;
    }

    seqid = __next_seqid();
    evres->seqid = seqid;
    evres->type = REQUEST_SET_DESKTOP_BACKGROUND;
    evres->src = TO_WINID(GLOB.mypid, 0);
    evres->dest = desktop_winid;
    evres->datasz = sizeof(uint32_t);
    *((uint32_t *)evres->data) = color;
    evres->bg_is_image = 0;
    evres->valid_reply = 1;         // so the desktop app would not filter it out
    direct_write(GLOB.serverfd, (void *)evres, tmpsz);
}


/*
 * Tell the desktop task to set background to the given image.
 */
void set_desktop_bg_image(char *path)
{
    size_t pathlen = path ? strlen(path) + 1 : 0;
    size_t tmpsz = sizeof(struct event_desktop_bg_t) + pathlen;
    char tmp[tmpsz];
    struct event_desktop_bg_t *evres = (struct event_desktop_bg_t *)tmp;
    uint32_t seqid;
    winid_t desktop_winid;

    if(!path || !*path)
    {
        return;
    }

    if(!(desktop_winid = get_desktop_winid()))
    {
        messagebox_show(main_window->winid, "Error!",
                        "Failed to get desktop window id.", DIALOG_OK, 0);
        return;
    }

    seqid = __next_seqid();
    evres->seqid = seqid;
    evres->type = REQUEST_SET_DESKTOP_BACKGROUND;
    evres->src = TO_WINID(GLOB.mypid, 0);
    evres->dest = desktop_winid;
    evres->datasz = pathlen;
    A_memcpy(evres->data, path, pathlen);
    evres->bg_is_image = 1;
    evres->bg_image_aspect = cur_bgimage_aspect;
    evres->valid_reply = 1;         // so the desktop app would not filter it out
    direct_write(GLOB.serverfd, (void *)evres, tmpsz);
}


static void update_bgcolor_img(void)
{
    int i, j, k;

    for(i = 0, k = 0; i < BGCOLOR_IMGH; i++)
    {
        for(j = 0; j < BGCOLOR_IMGW; j++, k++)
        {
            if(bgcolor_img_template[k] == TEMPLATE_FGCOLOR)
            {
                bgcolor_img_data[k] = cur_bgcolor;
            }
            else
            {
                bgcolor_img_data[k] = bgcolor_img_template[k];
            }
        }
    }
}


/*
 * If one radio button is selected, enable its associated button and disable
 * the other radio button's associated button.
 */
static void radiobutton_handler(struct radiobutton_t *button, int x, int y)
{
    struct window_t *window = button->window.parent;
    struct __internal_t *internal = window->internal_data;

    if(!internal->imgbutton || !internal->button)
    {
        return;
    }

    if(button->window.internal_data == (void *)(intptr_t)1)
    {
        imgbutton_enable(internal->imgbutton);
        button_disable(internal->button);
        combobox_disable(internal->combobox);
    }
    else
    {
        imgbutton_disable(internal->imgbutton);
        button_enable(internal->button);
        combobox_enable(internal->combobox);
    }
}


static void select_bgcolor_button_handler(struct imgbutton_t *button, int x, int y)
{
    struct window_t *button_window = (struct window_t *)button;
    struct window_t *window = button->window.parent;
    struct colorchooser_dialog_t *dialog;
    
    if(!(dialog = colorchooser_dialog_create(window->winid)))
    {
        return;
    }
    
    colorchooser_dialog_set_color(dialog, cur_bgcolor);

    if(colorchooser_dialog_show(dialog) == DIALOG_RESULT_OK)
    {
        cur_bgcolor = colorchooser_dialog_get_color(dialog);
        update_bgcolor_img();
        imgbutton_repaint(button_window, 1);
        child_invalidate(button_window);
        set_desktop_bg_color(cur_bgcolor);
    }

    colorchooser_dialog_destroy(dialog);
}


static void select_bgimage_button_handler(struct button_t *button, int x, int y)
{
    struct window_t *parent = button->window.parent;
    struct opensave_dialog_t *dialog;
    struct opensave_file_t *files;
    struct stat stats;
    int count;

    if(!(dialog = open_dialog_create(parent->winid)))
    {
        /*
         * TODO: show an error message here.
         */
        return;
    }

    dialog->multiselect = 0;
    dialog->filetype_filter = "All formats|*.jpg;*.jpeg;*.png|"
                              "JPEG images|*.jpg;*.jpeg|"
                              "PNG images|*.png";

    // try to open the shared backgrounds directory if it exists
    if(stat(BACKGROUNDS_DIR_PATH, &stats) != -1)
    {
        dialog->path = BACKGROUNDS_DIR_PATH;
    }

    if(open_dialog_show(dialog) == DIALOG_RESULT_OK)
    {
        if((count = open_dialog_get_selected(dialog, &files)) > 0)
        {
            button_set_title(button, simple_basename(files[0].path));

            if(cur_bgimage)
            {
                free(cur_bgimage);
            }

            cur_bgimage = strdup(files[0].path);
            open_dialog_free_list(files, count);
            button_repaint((struct window_t *)button, 1);
            child_invalidate((struct window_t *)button);
            set_desktop_bg_image(cur_bgimage);
        }
    }

    open_dialog_destroy(dialog);
}


static void combobox_entry_click_callback(struct combobox_t *combobox,
                                          int selindex)
{
    if(selindex < DESKTOP_BACKGROUND_FIRST_ASPECT ||
       selindex > DESKTOP_BACKGROUND_LAST_ASPECT)
    {
        return;
    }

    cur_bgimage_aspect = selindex;
    set_desktop_bg_image(cur_bgimage);
}


winid_t show_window_background(void)
{
    struct window_t *window;
    struct radiobutton_t *r1, *r2;
    struct window_attribs_t attribs;
    struct __internal_t *internal;

    if(!(internal = malloc(sizeof(struct __internal_t))))
    {
        char buf[128];

        sprintf(buf, "Failed to alloc internal buffer: %s", strerror(errno));
        messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);
        return 0;
    }

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = 340;
    attribs.h = 180;
    attribs.flags = WINDOW_NORESIZE;

    if(!(window = window_create(&attribs)))
    {
        char buf[128];

        sprintf(buf, "Failed to create window: %s", strerror(errno));
        messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);
        return 0;
    }

    window->internal_data = internal;
    update_bgcolor_img();

    r1 = radiobutton_new(window->gc, window, 20, 20, 110, 20, "Fill with color:");
    r1->button_click_callback = radiobutton_handler;
    r1->window.internal_data = (void *)(intptr_t)1;
    r1->group = "RadioGroup1";

    r2 = radiobutton_new(window->gc, window, 20, 55, 110, 20, "Draw image:");
    r2->button_click_callback = radiobutton_handler;
    r2->window.internal_data = (void *)(intptr_t)2;
    r2->group = "RadioGroup1";

    internal->imgbutton = imgbutton_new(window->gc, window, 140, 15, 30, 30);
    imgbutton_set_image(internal->imgbutton, &bgcolor_img);
    internal->imgbutton->button_click_callback = select_bgcolor_button_handler;

    internal->button = button_new(window->gc, window, 140, 50, 120, 30, 
                                  cur_bgimage ? simple_basename(cur_bgimage) :
                                                "Select image..");
    internal->button->window.text_alignment |= TEXT_ALIGN_LEFT;
    internal->button->state = BUTTON_STATE_DISABLED;
    internal->button->button_click_callback = select_bgimage_button_handler;

    label_new(window->gc, window, 35, 90, 100, 20, "Picture aspect:");

    internal->combobox = combobox_new(window->gc, window, 140, 85, 120, NULL);
    internal->combobox->entry_click_callback =
                            combobox_entry_click_callback;
    combobox_append_item(internal->combobox, "Centered");
    combobox_append_item(internal->combobox, "Tiles");
    combobox_append_item(internal->combobox, "Scaled");
    combobox_append_item(internal->combobox, "Stretched");
    combobox_append_item(internal->combobox, "Zoomed");
    combobox_set_text((struct window_t *)internal->combobox, "Centered");
    combobox_set_selected_item(internal->combobox, 0);
    internal->combobox->flags |= COMBOBOX_FLAG_DISABLED;

    // this must be called after all children are added
    radiobutton_set_selected(r1);

    window_set_focus_child(window, (struct window_t *)r1);
    window_set_title(window, "Desktop background");
    window_set_icon(window, "settings.ico");

    window_repaint(window);
    window_show(window);

    return window->winid;
}

