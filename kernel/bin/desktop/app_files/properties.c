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
 *  Functions to show the properties of a file or directory.
 */

#include <sys/stat.h>
#include "../include/gui.h"
#include "../include/resources.h"
#include "../include/client/file-selector.h"
#include "../include/client/dialog.h"

#define GLOB                        __global_gui_data

extern struct window_t *main_window;
extern struct file_selector_t *selector;

static char *weekdays[] =
{
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

static char *months[] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
    "Aug", "Sep", "Oct", "Nov", "Dec",
};

// we use our keyboard messagebox infrastructure to show a two-column dialog
static char *shortcuts[] =
{
    "Name:",
    "Type:",
    "Size:",
    "",
    "Accessed:",
    "Created:",
    "Modified:",
    "",
    "User perm:",
    "Group perm:",
    "Others perm:",
    NULL,
};

static char *descriptions[] =
{
    NULL,
    NULL,
    NULL,
    "",
    NULL,
    NULL,
    NULL,
    "",
    NULL,
    NULL,
    NULL,
    NULL,
};


static char *format_time(char *buf, time_t t)
{
    struct tm *tm = gmtime(&t);

    sprintf(buf, "%s %d %s %02u:%02u", weekdays[tm->tm_wday], tm->tm_mday,
                                       months[tm->tm_mon], tm->tm_hour, 
                                       tm->tm_min);

    return buf;
}

static char *format_type(char *buf, mode_t mode)
{
    if(S_ISBLK(mode))
    {
        sprintf(buf, "Block device");
    }
    else if(S_ISCHR(mode))
    {
        sprintf(buf, "Character device");
    }
    else if(S_ISFIFO(mode))
    {
        sprintf(buf, "FIFO or pipe");
    }
    else if(S_ISLNK(mode))
    {
        sprintf(buf, "Soft link");
    }
    else if(S_ISSOCK(mode))
    {
        sprintf(buf, "Socket");
    }
    else if(S_ISDIR(mode))
    {
        sprintf(buf, "Directory");
    }
    else
    {
        /*
         * TODO: Provide more details, e.g. image, audio file, ...
         */
        sprintf(buf, "File");
    }
    
    return buf;
}


#define READ_PERM       (S_IRUSR|S_IRGRP|S_IROTH)
#define WRITE_PERM      (S_IWUSR|S_IWGRP|S_IWOTH)
#define EXEC_PERM       (S_IXUSR|S_IXGRP|S_IXOTH)

static char *format_perm(char *buf, mode_t mode)
{
    char *p = buf;
    
    *p = '\0';
    
    if(mode & READ_PERM)
    {
        strcpy(p, "Read");
        p += 4;
    }

    if(mode & WRITE_PERM)
    {
        if(p != buf)
        {
            strcpy(p, ", Write");
            p += 7;
        }
        else
        {
            strcpy(p, "Write");
            p += 5;
        }
    }

    if(mode & EXEC_PERM)
    {
        if(p != buf)
        {
            strcpy(p, ", Execute");
            p += 9;
        }
        else
        {
            strcpy(p, "Execute");
            p += 7;
        }
    }

    if(p == buf)
    {
        strcpy(p, "None");
        p += 4;
    }
    
    return buf;
}

#undef READ_PERM
#undef WRITE_PERM
#undef EXEC_PERM


static inline int print_str(char *p, char *fmt, char *arg)
{
    return sprintf(p, fmt, arg);
}


void show_properties_dialog(void)
{
    struct file_entry_t *entries, *entry;
    int count;
    
    if(!selector)
    {
        return;
    }
    
    if((count = file_selector_get_selected(selector, &entries)) <= 0)
    {
        return;
    }
    
    if(count > 1)
    {
        char buf[256];
        int files = 0, dirs = 0;
        
        for(entry = &entries[0]; entry < &entries[count]; entry++)
        {
            if(S_ISDIR(entry->mode))
            {
                dirs++;
            }
            else
            {
                files++;
            }
        }
        
        sprintf(buf, "Multiple items are selected:\n"
                     "   %d file(s)\n"
                     "   %d dir(s)\n"
                     "\n"
                     "Select a single item to see its details.",
                     files, dirs);

        messagebox_show(main_window->winid, "Properties", buf, DIALOG_OK, 0);
    }
    else
    {
        struct shortcuts_dialog_t *dialog;
        char buf_type[32], buf_size[32];
        char buf_atime[32], buf_ctime[32], buf_mtime[32];
        char buf_uperm[32], buf_gperm[32], buf_operm[32];

        entry = &entries[0];

        descriptions[0] = entry->name;
        descriptions[1] = buf_type;
        descriptions[2] = buf_size;
        descriptions[4] = buf_atime;
        descriptions[5] = buf_ctime;
        descriptions[6] = buf_mtime;
        descriptions[8] = buf_uperm;
        descriptions[9] = buf_gperm;
        descriptions[10] = buf_operm;

        format_type(descriptions[1], entry->mode);
        stringify_file_size(descriptions[2], entry->file_size);
        format_time(descriptions[4], entry->atime);
        format_time(descriptions[5], entry->ctime);
        format_time(descriptions[6], entry->mtime);
        format_perm(descriptions[8], (entry->mode & S_IRWXU));
        format_perm(descriptions[9], (entry->mode & S_IRWXG));
        format_perm(descriptions[10], (entry->mode & S_IRWXO));

        // use our keyboard messagebox infrastructure to show a two-column dialog
        if(!(dialog = shortcuts_dialog_create(main_window->winid,
                                              shortcuts, descriptions)))
        {
            file_selector_free_list(entries, count);
            return;
        }
    
        shortcuts_dialog_set_title(dialog, "Properties");
        shortcuts_dialog_show(dialog);
        shortcuts_dialog_destroy(dialog);
    }

    file_selector_free_list(entries, count);
}

