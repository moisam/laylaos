/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: show_sysinfo.c
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
 *  \file show_sysinfo.c
 *
 *  Part of the system settings program. Shows a window with system settings.
 */

#include <sys/utsname.h>
#include "../include/gui.h"
#include "../include/client/dialog.h"
#include "defs.h"

static inline void mayfree(char *s)
{
    if(s)
    {
        free(s);
    }
}

winid_t show_window_sysinfo(void)
{
    struct window_t *window;
    struct window_attribs_t attribs;
    struct utsname osinfo;
    FILE *f;
    size_t sz;
    char buf[4096], *s;
    char *processor = NULL;
    char *memtotal = NULL, *memfree = NULL;

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = 400;
    attribs.h = 160;
    attribs.flags = WINDOW_NORESIZE;

    if(!(window = window_create(&attribs)))
    {
        char buf[128];

        sprintf(buf, "Failed to create window: %s", strerror(errno));
        messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);
        return 0;
    }

    // get OS info
    if(uname(&osinfo) < 0)
    {
        strcpy(osinfo.sysname, "Layla OS");
        strcpy(osinfo.release, "Unknown");
    }

    // get processor info
    if((f = fopen("/proc/cpuinfo", "r")))
    {
        while(fgets(buf, sizeof(buf), f) != NULL)
        {
            // fall back if we don't have a model name
            if((s = strstr(buf, "vendor_id")))
            {
                if((s = strchr(buf, ':')))
                {
                    processor = strdup(s + 2);
                    sz = strlen(processor);

                    if(processor[sz - 1] == '\n')
                    {
                        processor[sz - 1] = '\0';
                    }

                    continue;
                }
            }

            // prefer this if available
            if((s = strstr(buf, "model name")))
            {
                if((s = strchr(buf, ':')))
                {
                    if(processor)
                    {
                        free(processor);
                        processor = NULL;
                    }

                    processor = strdup(s + 2);
                    sz = strlen(processor);

                    if(processor[sz - 1] == '\n')
                    {
                        processor[sz - 1] = '\0';
                    }

                    break;
                }
            }
        }

        fclose(f);
    }

    // get memory info
    if((f = fopen("/proc/meminfo", "r")))
    {
        while(fgets(buf, sizeof(buf), f) != NULL)
        {
            if((s = strstr(buf, "MemTotal:")))
            {
                s += 9;

                while(*s == ' ' || *s == '\t')
                {
                    s++;
                }

                memtotal = strdup(s);
                sz = strlen(memtotal);

                if(memtotal[sz - 1] == '\n')
                {
                    memtotal[sz - 1] = '\0';
                }
            }
            else if((s = strstr(buf, "MemFree:")))
            {
                s += 8;

                while(*s == ' ' || *s == '\t')
                {
                    s++;
                }

                memfree = strdup(s);
                sz = strlen(memfree);

                if(memfree[sz - 1] == '\n')
                {
                    memfree[sz - 1] = '\0';
                }
            }
        }

        fclose(f);
    }

    // get disk info -- TODO

    window_set_title(window, "System information");
    window_set_icon(window, "settings.ico");

    // paint the window
    gc_fill_rect(window->gc, 0, 0, window->w, window->h, window->bgcolor);

    gc_draw_text(window->gc, "Operating System:", 8, 8, window->fgcolor, 0);
    gc_draw_text(window->gc, "Operating System Version:", 8, 26, window->fgcolor, 0);
    gc_draw_text(window->gc, "Processor:", 8, 44, window->fgcolor, 0);
    gc_draw_text(window->gc, "Total Memory:", 8, 62, window->fgcolor, 0);
    gc_draw_text(window->gc, "Free Memory:", 8, 80, window->fgcolor, 0);

    gc_draw_text(window->gc, osinfo.sysname, 200, 8, window->fgcolor, 0);
    gc_draw_text(window->gc, osinfo.release, 200, 26, window->fgcolor, 0);
    gc_draw_text(window->gc, processor ? processor : "Unknown", 200, 44, window->fgcolor, 0);
    gc_draw_text(window->gc, memtotal ? memtotal : "Unknown", 200, 62, window->fgcolor, 0);
    gc_draw_text(window->gc, memfree ? memfree : "Unknown", 200, 80, window->fgcolor, 0);

    window_show(window);

    mayfree(processor);
    mayfree(memtotal);
    mayfree(memfree);

    return window->winid;
}

