/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: cursor-x11.c
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
 *  \file cursor-x11.c
 *
 *  Functions to work with X11 mouse cursors on the server side.
 */

#define GUI_SERVER
#include <dirent.h>
#include <sys/stat.h>
#include "../include/gui.h"
#include "../include/server/cursor.h"

#define BUFSZ                   0x1000

/*
 * Standard Freedesktop cursor mapping to legacy X11 counterparts.
 * Cursor names are not very well documented. Below names are collated from
 * multiple sources:
 *
 * https://www.w3.org/TR/css-ui-3/#predefined-cursors
 * https://developer.mozilla.org/en-US/docs/Web/CSS/Reference/Properties/cursor
 * https://www.pixelbeat.org/programming/x_cursors/
 */
struct
{
    char *name, *altname;
    curid_t curid;
} cursor_names[] =
{
    // Standard Selection & Status Indicators
    {"crosshair",   "crosshair",            CURSOR_CROSSHAIR     },
    {"default",     "left_ptr",             CURSOR_NORMAL        },
    {"help",        "whats_this",           CURSOR_HELP          },
    {"not-allowed", "crossed_circle",       CURSOR_FORBIDDEN     },
    {"pointer",     "hand2",                CURSOR_POINTING_HAND },
    {"progress",    "left_ptr_watch",       CURSOR_ARROW_WAITING },
    {"text",        "xterm",                CURSOR_IBEAM         },
    {"vertical-text","xterm",               CURSOR_VIBEAM        },
    {"wait",        "watch",                CURSOR_WAITING       },
    {"up-arrow",    "sb_up_arrow",          CURSOR_UP            },

    // Drag & Drop
    {"dnd-link",    "alias",                CURSOR_DND_LINK      },
    {"dnd-copy",    "copy",                 CURSOR_DND_COPY      },
    {"grab",        "hand1",                CURSOR_OPEN_HAND     },
    {"grabbing",    "hand2",                CURSOR_CLOSED_HAND   },
    {"dnd-move",    "move",                 CURSOR_DND_MOVE      },

    // Window Resizing
    {"e-resize",    "right_side",           CURSOR_E             },
    {"ew-resize",   "h_double_arrow",       CURSOR_WE            },
    {"n-resize",    "top_side",             CURSOR_N             },
    {"ne-resize",   "top_right_corner",     CURSOR_NE            },
    {"nesw-resize", "fd_double_arrow",      CURSOR_NESW          },
    {"ns-resize",   "v_double_arrow",       CURSOR_NS            },
    {"nw-resize",   "top_left_corner",      CURSOR_NW            },
    {"nwse-resize", "bd_double_arrow",      CURSOR_NWSE          },
    {"s-resize",    "bottom_side",          CURSOR_S             },
    {"se-resize",   "bottom_right_corner",  CURSOR_SE            },
    {"sw-resize",   "bottom_left_corner",   CURSOR_SW            },
    {"w-resize",    "left_side",            CURSOR_W             },

    // UI Panel Splitting
    {"col-resize",  "sb_h_double_arrow",    CURSOR_COLRESIZE     },
    {"row-resize",  "sb_v_double_arrow",    CURSOR_ROWRESIZE     },

    // Miscellaneous
    {"move",  "fleur",                      CURSOR_FLEUR         },
    {"cell",        "cross",                CURSOR_CELL          },
    {"context-menu","middlebutton",         CURSOR_CONTEXT_MENU  },
    {"no-drop",     "X_cursor",             CURSOR_DND_NODROP    },
    {"zoom-in",     "left_ptr",             CURSOR_ZOOMIN        },
    {"zoom-out",    "left_ptr",             CURSOR_ZOOMOUT       },
};

#define NAME_COUNT      (sizeof(cursor_names) / sizeof(cursor_names[0]))


int prep_mouse_cursor_x11(int pixelsz)
{
    int i;
    char *buf;
    struct cursor_t *cur;
    struct stat st;

    if(!(buf = malloc(BUFSZ)))
    {
        return -1;
    }

    snprintf(buf, PATH_MAX, "%s/default/cursors", DEFAULT_CUSOR_PATH);

    if(stat(buf, &st) == -1)
    {
        free(buf);
        return -1;
    }

    if(!S_ISDIR(st.st_mode))
    {
        free(buf);
        return -1;
    }

    for(i = 0; i < NAME_COUNT; i++)
    {
        // try the freedesktop cursor name first, if not found, try the X11 name
        snprintf(buf, PATH_MAX, "%s/default/cursors/%s", DEFAULT_CUSOR_PATH, cursor_names[i].name);

        if(stat(buf, &st) == -1)
        {
            snprintf(buf, PATH_MAX, "%s/default/cursors/%s", DEFAULT_CUSOR_PATH, cursor_names[i].altname);

            if(stat(buf, &st) == -1)
            {
                continue;
            }
        }

        if(!(cur = x11_cursor_load(buf)))
        {
            continue;
        }

        if(cursor[cursor_names[i].curid] != NULL)
        {
            if(cursor[cursor_names[i].curid]->bitmaps[0].data != NULL)
            {
                free(cursor[cursor_names[i].curid]->bitmaps[0].data);
                cursor[cursor_names[i].curid]->bitmaps[0].data = NULL;
            }

            free(cursor[cursor_names[i].curid]);
            cursor[cursor_names[i].curid] = NULL;
        }

        cursor[cursor_names[i].curid] = cur;
    }

    free(buf);
    return 0;
}


void server_cursor_change_syscursor(struct event_res_t *evres)
{
    curid_t curid = evres->syscur.curid;
    int pixelsz = evres->syscur.pixelsz;
    size_t bytes = evres->datasz;
    char *path = evres->data;
    struct cursor_t *cur;
    struct stat st;

    if(bytes == 0 || pixelsz <= 0 || curid == 0 || curid >= SYS_CURSOR_COUNT)
    {
        return;
    }

    if(stat(path, &st) == -1)
    {
        return;
    }

    if(!(cur = x11_cursor_load_sz(path, pixelsz)))
    {
        return;
    }

    if(cursor[curid] != NULL)
    {
        cursor_struct_free(cursor[curid]);
        cursor[curid] = NULL;
    }

    cursor[curid] = cur;
}

