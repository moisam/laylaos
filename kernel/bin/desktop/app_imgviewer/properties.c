/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: properties.c
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
 *  \file properties.c
 *
 *  Functions to show the properties of the open image.
 */

#include <sys/stat.h>
#include "../include/gui.h"
#include "../include/resources.h"
#include "../include/client/dialog.h"

#define GLOB                        __global_gui_data

extern struct window_t *main_window;
extern struct bitmap32_t loaded_bitmap;
extern char *loaded_path;
extern struct stat loaded_path_stat;

// we use our keyboard messagebox infrastructure to show a two-column dialog
static char *shortcuts[] =
{
    "File name:",
    "Image type:",
    "Image size:",
    "File size:",
    NULL,
};

static char *descriptions[] =
{
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

struct ext_desc_t
{
    char *ext, *desc;
};

static struct ext_desc_t extensions[] =
{
    { ".ico" , "ICO image" },
    { ".jpg" , "JPEG image" },
    { ".jpeg", "JPEG image" },
    { ".png" , "PNG image" },
    { NULL, NULL },
};


static char *extension_description(char *filename)
{
    char *ext = file_extension(filename);
    struct ext_desc_t *p;

    for(p = extensions; p->ext; p++)
    {
        if(strcmp(ext, p->ext) == 0)
        {
            return p->desc;
        }
    }

    return "Unknown";
}


void show_properties_dialog(void)
{
    struct shortcuts_dialog_t *dialog;
    char buf_type[128];
    char buf_size[64];

    descriptions[0] = loaded_path;
    descriptions[1] = extension_description(loaded_path);

    sprintf(buf_type, "%dx%d pixels", loaded_bitmap.width, loaded_bitmap.height);
    descriptions[2] = buf_type;

    stringify_file_size(buf_size, loaded_path_stat.st_size);
    descriptions[3] = buf_size;

    // use our keyboard messagebox infrastructure to show a two-column dialog
    if(!(dialog = shortcuts_dialog_create(main_window->winid,
                                          shortcuts, descriptions)))
    {
        return;
    }
    
    shortcuts_dialog_set_title(dialog, "Image properties");
    shortcuts_dialog_show(dialog);
    shortcuts_dialog_destroy(dialog);
}

