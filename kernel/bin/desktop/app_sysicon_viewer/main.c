/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: main.c
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
 *  \file main.c
 *
 *  The system icon viewer program.
 */

#include <errno.h>
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/keys.h"
#include "../include/icolib.h"
#include "../include/cursor.h"
#include "../include/client/dialog.h"
#include "../include/client/listview.h"
#include "../include/client/group-border.h"

#define SYSICONS_DIR        "/usr/share/gui/desktop"
#define SYSICONS_FILE       "sysicons.icolib"
#define SYSICONS_PATH        SYSICONS_DIR "/" SYSICONS_FILE

struct window_t *main_window;

FILE *sysicons_file = NULL;
struct icolib_hdr_t sysicons_filehdr;

void *tags_src = NULL;
char **tags = NULL;


static void show_error(char *fmt, ...)
{
	va_list args;
    char buf[2096];

	va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

    messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);
}


int open_icons(void)
{
    int i;
    char *p;

    if(!(sysicons_file = fopen(SYSICONS_PATH, "r")))
    {
        show_error("Failed to open %s: %s", SYSICONS_PATH, strerror(errno));
        return -1;
    }

    if((i = fread(&sysicons_filehdr, 1, sizeof(sysicons_filehdr),
                                sysicons_file)) != sizeof(sysicons_filehdr))
    {
        show_error("Failed to read file header (got %d bytes, expected %lu)",
                        i, sizeof(sysicons_filehdr));
        goto fail;
    }

    if(sysicons_filehdr.signature[0] != ICOLIB_HDR0 ||
       sysicons_filehdr.signature[1] != ICOLIB_HDR1 ||
       sysicons_filehdr.signature[2] != ICOLIB_HDR2 ||
       sysicons_filehdr.signature[3] != ICOLIB_HDR3)
    {
        show_error("Invalid header signature");
        goto fail;
    }

    if(sysicons_filehdr.version != 1)
    {
        show_error("Invalid header version (%d)", sysicons_filehdr.version);
        goto fail;
    }

    if(!(tags_src = malloc(sysicons_filehdr.tagsz)))
    {
        printf("Failed to alloc memory to check tags");
        goto fail;
    }

    if((i = fread(tags_src, 1, 
                    sysicons_filehdr.tagsz, sysicons_file)) != 
                                                sysicons_filehdr.tagsz)
    {
        printf("Failed to read tags (got %d bytes, expected %u)", 
                        i, sysicons_filehdr.tagsz);
        goto fail;
    }

    if(!(tags = malloc(sysicons_filehdr.icocount * sizeof(char *))))
    {
        printf("Failed to alloc memory to store tags");
        goto fail;
    }

    p = tags_src;

    for(i = 0; i < sysicons_filehdr.icocount; i++)
    {
        tags[i] = p;
        p += strlen(p) + 1;
    }

    return 0;

fail:

    fclose(sysicons_file);
    sysicons_file = NULL;

    if(tags_src)
    {
        free(tags_src);
        tags_src = NULL;
    }

    return -1;
}


void listentry_selection_change_callback(struct listview_t *listv)
{
    struct listview_entry_t *entries;
    struct bitmap32_t bitmap;
    int count;
    int i, y, w;
    off_t off, base = sysicons_filehdr.dataoff;
    size_t bufsz = 0;
    char *buf;

    if((count = listview_get_selected(listv, &entries)) <= 0)
    {
        return;
    }
    
    // alloc a buffer large enough for the largest icon
    for(i = 0; i < 8; i++)
    {
        if(sysicons_filehdr.icosz[i] > bufsz)
        {
            bufsz = sysicons_filehdr.icosz[i];
        }
    }

    if(!bufsz)
    {
        return;
    }

    bufsz = bufsz * bufsz * 4;

    if(!(buf = malloc(bufsz)))
    {
        return;
    }

    gc_fill_rect(main_window->gc, 290, 40, 100, 270, main_window->bgcolor);

    cursor_show(main_window, CURSOR_WAITING);

    /*
     * Load the icon with the selected index from file and show it.
     *
     * TODO: we should cache the icons we load to avoid re-reading from disk.
     */
    for(y = 40, i = 0; i < 8; i++)
    {
        if(sysicons_filehdr.icosz[i] == 0)
        {
            break;
        }

        w = sysicons_filehdr.icosz[i];
        bufsz = w * w * 4;
        off = (bufsz * entries[0].index) + base;

        if(fseek(sysicons_file, off, SEEK_SET) < 0)
        {
            break;
        }

        if((fread(buf, 1, bufsz, sysicons_file)) != bufsz)
        {
            break;
        }

        bitmap.width = w;
        bitmap.height = w;
        bitmap.data = (uint32_t *)buf;

        gc_blit_bitmap(main_window->gc, &bitmap, 290, y, 0, 0, w, w);

        y += w + 10;
        base += bufsz * sysicons_filehdr.icocount;
    }

    cursor_show(main_window, CURSOR_NORMAL);

    listview_free_list(entries, count);
    free(buf);

    window_invalidate_rect(main_window, 40, 290, 320, 400);
}


void listentry_click_callback(struct listview_t *listv, int selindex)
{
    listentry_selection_change_callback(listv);
}


int main(int argc, char **argv)
{
    int i;
    struct listview_t *list;
    struct event_t *ev = NULL;
    struct window_attribs_t attribs;

    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    gui_init(argc, argv);

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = 420;
    attribs.h = 340;
    attribs.flags = WINDOW_NORESIZE;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n", 
                        argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    list = listview_new(main_window->gc, main_window, 20, 20, 240, 300);
    //listview_set_multiselect(list, 1);
    list->entry_click_callback = listentry_click_callback;
    list->selection_change_callback = listentry_selection_change_callback;

    group_border_new(main_window->gc, main_window, 280, 20, 120, 300, "Preview:");

    window_set_title(main_window, "System icon viewer");
    window_set_icon(main_window, "image.ico");

    window_repaint(main_window);
    window_show(main_window);

    if(open_icons() != 0)
    {
        window_destroy(main_window);
        gui_exit(EXIT_FAILURE);
    }

    for(i = 0; i < sysicons_filehdr.icocount; i++)
    {
        listview_append_item(list, tags[i]);
    }

    window_repaint(main_window);
    window_invalidate(main_window);

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
    
        switch(ev->type)
        {
            case EVENT_WINDOW_CLOSING:
                fclose(sysicons_file);
                window_destroy(main_window);
                gui_exit(EXIT_SUCCESS);
                break;

            default:
                break;
        }

        free(ev);
    }
}

