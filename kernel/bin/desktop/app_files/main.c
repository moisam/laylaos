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
 *  A program to navigate the filesystem and create, copy and delete files
 *  and directories.
 */

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#define _GNU_SOURCE     1
#undef __GNU_VISIBLE
#define __GNU_VISIBLE   1
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../include/gui.h"
#include "../include/event.h"
#include "../include/keys.h"
#include "../include/menu.h"
#include "../include/cursor.h"
#include "../include/clipboard.h"
#include "../include/client/inputbox.h"
#include "../include/client/file-selector.h"
#include "../include/client/dialog.h"
#include "../include/client/statusbar.h"
#include "../include/client/label.h"
#include "../include/client/imgbutton.h"
#include "defs.h"

//#define LOCATION_BAR_HEIGHT         32
#define LOCATION_BAR_HEIGHT         (INPUTBOX_HEIGHT + 8)

#define GLOB                        __global_gui_data

struct window_t *main_window;

struct imgbutton_t *imgbutton_back;
struct imgbutton_t *imgbutton_forward;
struct imgbutton_t *imgbutton_up;
struct inputbox_t *location_bar;
struct file_selector_t *selector;

// menu items in the View menu
struct menu_item_t *properties_mi, *iconview_mi, *listview_mi, *compactview_mi;
// menu items in the Edit menu
struct menu_item_t *cut_mi, *copy_mi, *paste_mi, *rename_mi, *delete_mi;
// menu items in the Go menu
struct menu_item_t *parent_mi, *back_mi, *forward_mi;

char *curdir = NULL;

int is_cutting_items = 0;

void menu_view_refresh_handler(winid_t winid);


void set_my_title(char *str)
{
    char *name = basename(str);
    
    // our implemetation of basename() returns '\0' if the path is "/"
    if(*name)
    {
        char buf[strlen(name) + 16];

        sprintf(buf, "%s - %s", APP_TITLE, name);
        window_set_title(main_window, buf);
    }
    else
    {
        char buf[strlen(str) + 16];

        sprintf(buf, "%s - %s", APP_TITLE, str);
        window_set_title(main_window, buf);
    }
}


void set_my_status(char *fmt, ...)
{
    char buf[128];
    
	va_list args;
	
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
    
    statusbar_set_text((struct window_t *)main_window->statusbar, buf);
}


int reload_path(char *newdir)
{
    int res;

    cursor_show(main_window, CURSOR_WAITING);
    res = file_selector_set_path(selector, newdir);
    cursor_show(main_window, CURSOR_NORMAL);

    if(res == 0)
    {
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        if(curdir)
        {
            free(curdir);
        }
        
        curdir = newdir;
        set_my_title(curdir);
        inputbox_set_text((struct window_t *)location_bar, curdir);
        window_repaint(main_window);
        window_invalidate(main_window);

        // if current directory is not root, enable the Go -> Parent menu item
        if(curdir[0] == '/' && curdir[1] == '\0')
        {
            menu_item_set_enabled(parent_mi, 0);
            imgbutton_disable(imgbutton_up);
        }
        else
        {
            menu_item_set_enabled(parent_mi, 1);
            imgbutton_enable(imgbutton_up);
        }

        menu_item_set_enabled(properties_mi, 0);
        menu_item_set_enabled(cut_mi, 0);
        menu_item_set_enabled(copy_mi, 0);
        menu_item_set_enabled(rename_mi, 0);
        menu_item_set_enabled(delete_mi, 0);
        
        //set_my_status("%d item(s)", file_selector_get_selected(selector, NULL));
        set_my_status("%d item(s)", selector->entry_count);
        
        return 0;
    }
    else
    {
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        char buf[strlen(newdir) + 48];
        
        sprintf(buf, "Failed to open directory %s: %s", newdir, strerror(errno));
        messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);
        //free(newdir);
        return -1;
    }
}


void adjust_back_forward_menus(void)
{
    int hist_cur = get_history_current();

    // if we are at the beginning of the history list, disable the 
    // Go -> Back menu item
    if(hist_cur <= 0)
    {
        menu_item_set_enabled(back_mi, 0);
        imgbutton_disable(imgbutton_back);
    }
    else
    {
        menu_item_set_enabled(back_mi, 1);
        imgbutton_enable(imgbutton_back);
    }

    // if we are at the end of the history list, disable the 
    // Go -> Forward menu item
    if(hist_cur >= get_history_last())
    {
        menu_item_set_enabled(forward_mi, 0);
        imgbutton_disable(imgbutton_forward);
    }
    else
    {
        menu_item_set_enabled(forward_mi, 1);
        imgbutton_enable(imgbutton_forward);
    }
}


void menu_file_newfile_handler(winid_t winid)
{
    char *name = inputbox_show(main_window->winid, "New file",
                               "Enter the name of the new file:");

    if(name == NULL)
    {
        return;
    }

    size_t curdir_len = strlen(curdir);
    size_t len = curdir_len + strlen(name);
    char *newfile;
    int i;
    
    if(!(newfile = malloc(len + 2)))
    {
        messagebox_show(main_window->winid, 
                        "Error!", "Insufficient memory!", DIALOG_OK, 0);
        free(name);
        return;
    }
    
    if(curdir[curdir_len - 1] == '/')
    {
        sprintf(newfile, "%s%s", curdir, name);
    }
    else
    {
        sprintf(newfile, "%s/%s", curdir, name);
    }

    if((i = open(newfile, 0666 | O_RDWR | O_CREAT)) >= 0)
    {
        close(i);
        menu_view_refresh_handler(winid);
    }
    else
    {
        char buf[len + 48];
        
        sprintf(buf, "Failed to create %s: %s", newfile, strerror(errno));
        messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);
    }

    free(name);
    free(newfile);
}


void menu_file_newdir_handler(winid_t winid)
{
    char *name = inputbox_show(main_window->winid, "New directory",
                               "Enter the name of the new directory:");

    if(name == NULL)
    {
        return;
    }

    size_t curdir_len = strlen(curdir);
    size_t len = curdir_len + strlen(name);
    char *newfile;
    int i;
    
    if(!(newfile = malloc(len + 2)))
    {
        messagebox_show(main_window->winid, "Error!", "Insufficient memory!",
                        DIALOG_OK, 0);
        free(name);
        return;
    }
    
    if(curdir[curdir_len - 1] == '/')
    {
        sprintf(newfile, "%s%s", curdir, name);
    }
    else
    {
        sprintf(newfile, "%s/%s", curdir, name);
    }

    if((i = mkdir(newfile, 0777)) == 0)
    {
        menu_view_refresh_handler(winid);
    }
    else
    {
        char buf[len + 64];
        
        sprintf(buf, "Failed to create %s: %s", newfile, strerror(errno));
        messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);
    }

    free(name);
    free(newfile);
}


void menu_file_properties_handler(winid_t winid)
{
    show_properties_dialog();
}


void menu_file_close_handler(winid_t winid)
{
    window_destroy(main_window);
    gui_exit(EXIT_SUCCESS);
}


void copy_items(int cutting)
{
    struct file_entry_t *entries, *entry;
    int count;
    char *buf, *p;
    size_t curdirlen;
    size_t buflen = 0;

    is_cutting_items = 0;

    if((count = file_selector_get_selected(selector, &entries)) <= 0)
    {
        set_my_status("No items selected to %s", cutting ? "cut" : "copy");
        return;
    }
    
    curdirlen = strlen(curdir) + 1;

    for(entry = &entries[0]; entry < &entries[count]; entry++)
    {
        buflen += curdirlen + strlen(entry->name) + 1;
    }
    
    if(!(buf = malloc(buflen + 1)))
    {
        file_selector_free_list(entries, count);
        set_my_status("Insufficient memory to %s items", cutting ? "cut" : "copy");
        return;
    }
    
    p = buf;

    for(entry = &entries[0]; entry < &entries[count]; entry++)
    {
        sprintf(p, "%s/%s", curdir, entry->name);
        p += strlen(p);
        *p++ = '\n';
        *p = '\0';
    }

    file_selector_free_list(entries, count);
    
    if(!clipboard_set_data(CLIPBOARD_FORMAT_TEXT, buf, buflen + 1))
    {
        set_my_status("Failed to %s items!", cutting ? "cut" : "copy");
        return;
    }

    p = (count > 1) ? "items" : "item";
    set_my_status("%d %s copied to clipboard (will be %s when you paste)",
                  count, p, cutting ? "cut" : "copied");

    if(cutting)
    {
        is_cutting_items = 1;
    }
}


void menu_edit_cut_handler(winid_t winid)
{
    copy_items(1);
}


void menu_edit_copy_handler(winid_t winid)
{
    copy_items(0);
}


void menu_edit_paste_handler(winid_t winid)
{
    size_t datasz;
    void *data;
    
    if(!(datasz = clipboard_has_data(CLIPBOARD_FORMAT_TEXT)))
    {
        set_my_status("Cannot paste. Clipboard is empty!");
        return;
    }
    
    if(!(data = clipboard_get_data(CLIPBOARD_FORMAT_TEXT)))
    {
        set_my_status("Cannot paste. Insufficient memory!");
        return;
    }

    set_my_status("Clipboard has items!");
    messagebox_show(main_window->winid, "Test!", data, DIALOG_OK, 0);
}


void menu_edit_selectall_handler(winid_t winid)
{
    file_selector_select_all(selector);
    selector->window.repaint((struct window_t *)selector,
                             IS_ACTIVE_CHILD((struct window_t *)selector));
    child_invalidate((struct window_t *)selector);
}


void menu_edit_rename_handler(winid_t winid)
{
    struct file_entry_t *entries;
    int count;
    char *name;
    char *origfile, *newfile;
    int i;
    size_t curdir_len = strlen(curdir);

    is_cutting_items = 0;

    if((count = file_selector_get_selected(selector, &entries)) <= 0)
    {
        return;
    }

    // this should not happen
    if(count > 1)
    {
        set_my_status("Cannot rename multiple entries");
        file_selector_free_list(entries, count);
        return;
    }

    char buf_msg[strlen(entries[0].name) + 64];

    sprintf(buf_msg, "Rename %s to:", entries[0].name);
    name = inputbox_show(main_window->winid, "Rename", buf_msg);

    if(name == NULL)
    {
        file_selector_free_list(entries, count);
        return;
    }
    
    if(!(origfile = malloc(curdir_len + strlen(entries[0].name) + 2)))
    {
        messagebox_show(main_window->winid, "Error!", "Insufficient memory!",
                        DIALOG_OK, 0);
        file_selector_free_list(entries, count);
        free(name);
        return;
    }

    if(!(newfile = malloc(curdir_len + strlen(name) + 2)))
    {
        messagebox_show(main_window->winid, "Error!", "Insufficient memory!",
                        DIALOG_OK, 0);
        file_selector_free_list(entries, count);
        free(origfile);
        free(name);
        return;
    }
    
    if(curdir[curdir_len - 1] == '/')
    {
        sprintf(origfile, "%s%s", curdir, entries[0].name);
        sprintf(newfile, "%s%s", curdir, name);
    }
    else
    {
        sprintf(origfile, "%s/%s", curdir, entries[0].name);
        sprintf(newfile, "%s/%s", curdir, name);
    }

    if((i = rename(origfile, newfile)) == 0)
    {
        menu_view_refresh_handler(winid);
    }
    else
    {
        sprintf(buf_msg, "Failed to rename %s: %s", 
                         entries[0].name, strerror(errno));
        messagebox_show(main_window->winid, "Error!", buf_msg, DIALOG_OK, 0);
    }

    file_selector_free_list(entries, count);
    free(name);
    free(origfile);
    free(newfile);
}


static int delete_file(char *path)
{
    while(1)
    {
        if(unlink(path) == 0)
        {
            return 0;
        }

        char buf_msg[strlen(path) + 64];

        sprintf(buf_msg, "Failed to delete %s: %s", path, strerror(errno));

        if(messagebox_show(main_window->winid, "Error!", buf_msg,
                              DIALOG_RETRY_CANCEL, 0) == DIALOG_RESULT_CANCEL)
        {
            return -1;
        }
    }
}


static int delete_dir(char *path)
{
    DIR *dirp;
    struct dirent *ent;
    struct stat st;
    char buf[PATH_MAX];

    if((dirp = opendir(path)) == NULL)
    {
        sprintf(buf, "Failed to open %s: %s", path, strerror(errno));
        messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);

        return -1;
    }
    
    for(;;)
    {
        errno = 0;
        
        if((ent = readdir(dirp)) == NULL)
        {
            if(errno)
            {
                closedir(dirp);

                sprintf(buf, "Failed to readdir %s: %s", path, strerror(errno));
                messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);

                return -1;
            }
            
            break;
        }
        
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }
        
        snprintf(buf, PATH_MAX, "%s/%s", path, ent->d_name);

        if(stat(buf, &st) == -1)
        {
            continue;
        }

        if(S_ISDIR(st.st_mode))
        {
            if(delete_dir(buf) < 0)
            {
                return -1;
            }
        }
        else
        {
            if(delete_file(buf) < 0)
            {
                return -1;
            }
        }
    }
    
    closedir(dirp);

    return 0;
}


void menu_edit_delete_handler(winid_t winid)
{
    struct file_entry_t *entries, *entry;
    int res, count, success = 0;
    char buf[PATH_MAX];

    if((count = file_selector_get_selected(selector, &entries)) <= 0)
    {
        set_my_status("No items selected to delete");
        return;
    }
    
    for(entry = &entries[0]; entry < &entries[count]; entry++)
    {
        /*
         * TODO: we should have a Recycle Bin.
         */
        sprintf(buf, "%s/%s", curdir, entry->name);

        /*
         * if this is a directory, descend into it and delete its contents
         * recursively:
         */
        if(S_ISDIR(entry->mode))
        {
            res = delete_dir(buf);
        }
        else
        {
            res = delete_file(buf);
        }

        if(res == 0)
        {
            success++;
        }
        else if(res < 0)
        {
            break;
        }
    }

    file_selector_free_list(entries, count);

    if(success)
    {
        menu_view_refresh_handler(winid);
    }

    set_my_status("Deleted %d %s", success, (success != 1) ? "items" : "item");
}


void menu_view_refresh_handler(winid_t winid)
{
    char *newdir = strdup(curdir);
    
    if(reload_path(newdir) != 0)
    {
        free(newdir);
    }
}

void menu_view_icons_handler(winid_t winid)
{
    menu_item_set_checked(iconview_mi, 1);
    menu_item_set_checked(listview_mi, 0);
    menu_item_set_checked(compactview_mi, 0);
    file_selector_set_viewmode(selector, FILE_SELECTOR_ICON_VIEW);
    window_repaint(main_window);
    window_invalidate(main_window);
}

void menu_view_list_handler(winid_t winid)
{
    menu_item_set_checked(iconview_mi, 0);
    menu_item_set_checked(listview_mi, 1);
    menu_item_set_checked(compactview_mi, 0);
    file_selector_set_viewmode(selector, FILE_SELECTOR_LIST_VIEW);
    window_repaint(main_window);
    window_invalidate(main_window);
}

void menu_view_compact_handler(winid_t winid)
{
    menu_item_set_checked(iconview_mi, 0);
    menu_item_set_checked(listview_mi, 0);
    menu_item_set_checked(compactview_mi, 1);
    file_selector_set_viewmode(selector, FILE_SELECTOR_COMPACT_VIEW);
    window_repaint(main_window);
    window_invalidate(main_window);
}

void menu_go_parent_handler(winid_t winid)
{
    if(!curdir || !*curdir)
    {
        return;
    }

    char *slash = curdir + strlen(curdir) - 1;
    char *newdir;
    
    while(slash > curdir && *slash != '/')
    {
        slash--;
    }
    
    if(slash < curdir || *slash != '/')
    {
        return;
    }

    if(slash == curdir)
    {
        newdir = strdup("/");
    }
    else
    {
        *slash = '\0';
        newdir = strdup(curdir);
        *slash = '/';
    }
    
    if(reload_path(newdir) != 0)
    {
        free(newdir);
    }
    else
    {
        history_push(newdir);
        adjust_back_forward_menus();
    }
}

void menu_go_back_handler(winid_t winid)
{
    char *newdir;
    
    if(get_history_current() <= 0)
    {
        adjust_back_forward_menus();
        return;
    }
    
    newdir = strdup(history_back());

    if(reload_path(newdir) != 0)
    {
        free(newdir);
    }
    
    adjust_back_forward_menus();
}

void menu_go_forward_handler(winid_t winid)
{
    char *newdir;
    
    if(get_history_current() >= get_history_last())
    {
        adjust_back_forward_menus();
        return;
    }

    newdir = strdup(history_forward());

    if(reload_path(newdir) != 0)
    {
        free(newdir);
    }
    
    adjust_back_forward_menus();
}


void menu_help_shortcuts_handler(winid_t winid)
{
    show_shortcuts_dialog();
}


void menu_help_about_handler(winid_t winid)
{
    show_about_dialog();
}


void open_or_execute(struct file_selector_t *selector,
                     struct file_entry_t *entry)
{
    size_t curdir_len = strlen(curdir);
    size_t len = curdir_len + strlen(entry->name);
    char *fullpath;
    
    if(!(fullpath = malloc(len + 2)))
    {
        return;
    }
    
    if(curdir[curdir_len - 1] == '/')
    {
        sprintf(fullpath, "%s%s", curdir, entry->name);
    }
    else
    {
        sprintf(fullpath, "%s/%s", curdir, entry->name);
    }

    // if it is an executable file, run it
    if((entry->mode & (S_IXUSR|S_IXGRP|S_IXOTH)))
    {
        if(!fork())
        {
            char *argv[] = { entry->name, NULL };

            execvp(fullpath, argv);
            exit(EXIT_FAILURE);
        }
    }

    free(fullpath);
}


void fileentry_doubleclick_callback(struct file_selector_t *selector,
                                    struct file_entry_t *entry)
{
    if(!S_ISDIR(entry->mode) || !curdir)
    {
        open_or_execute(selector, entry);
        return;
    }
    
    size_t curdir_len = strlen(curdir);
    size_t len = curdir_len + strlen(entry->name);
    char *newdir;
    
    if(!(newdir = malloc(len + 2)))
    {
        return;
    }
    
    if(curdir[curdir_len - 1] == '/')
    {
        sprintf(newdir, "%s%s", curdir, entry->name);
    }
    else
    {
        sprintf(newdir, "%s/%s", curdir, entry->name);
    }

    if(reload_path(newdir) != 0)
    {
        free(newdir);
    }
    else
    {
        history_push(newdir);
        adjust_back_forward_menus();
    }
}


void fileentry_selection_change_callback(struct file_selector_t *selector)
{
    int count = file_selector_get_selected(selector, NULL);
    
    if(count <= 0)
    {
        menu_item_set_enabled(properties_mi, 0);
        menu_item_set_enabled(cut_mi, 0);
        menu_item_set_enabled(copy_mi, 0);
        menu_item_set_enabled(rename_mi, 0);
        menu_item_set_enabled(delete_mi, 0);
        set_my_status("No items selected");
        return;
    }
    
    if(count > 1)
    {
        menu_item_set_enabled(rename_mi, 0);
        menu_item_set_enabled(delete_mi, 0);
        set_my_status("%d items selected", count);
    }
    else
    {
        menu_item_set_enabled(rename_mi, 1);
        menu_item_set_enabled(delete_mi, 1);
        set_my_status("1 item selected");
    }

    menu_item_set_enabled(properties_mi, 1);
    menu_item_set_enabled(cut_mi, 1);
    menu_item_set_enabled(copy_mi, 1);
}


void fileentry_click_callback(struct file_selector_t *selector, 
                              struct file_entry_t *entry)
{
    fileentry_selection_change_callback(selector);
}


int locationbar_keypress(struct window_t *inputbox_window, 
                         char code, char modifiers)
{
    // call the standard function to do all the hardwork
    int res = inputbox_keypress(inputbox_window, code, modifiers);
    
    // if the user pressed Enter, navigate to the new path
    if(code == KEYCODE_ENTER)
    {
        char *newdir = realpath(inputbox_window->title, NULL);

        if(!newdir)
        {
            char buf[strlen(inputbox_window->title) + 48];
        
            sprintf(buf, "Failed to open directory %s: %s", 
                         inputbox_window->title, strerror(errno));
            messagebox_show(main_window->winid, "Error!", buf, DIALOG_OK, 0);

            return 1;
        }

        if(reload_path(newdir) != 0)
        {
            free(newdir);
        }
        else
        {
            history_push(newdir);
            adjust_back_forward_menus();

            // give focus back to the file selector so the user can 
            // interact with it
            window_set_focus_child(main_window, (struct window_t *)selector);
        }
    }
    
    return res;
}


void imgbutton_back_handler(struct imgbutton_t *button, int x, int y)
{
    menu_go_back_handler(0);

    // give focus back to the file selector so the user can interact with it
    window_set_focus_child(main_window, (struct window_t *)selector);
}


void imgbutton_forward_handler(struct imgbutton_t *button, int x, int y)
{
    menu_go_forward_handler(0);

    // give focus back to the file selector so the user can interact with it
    window_set_focus_child(main_window, (struct window_t *)selector);
}


void imgbutton_up_handler(struct imgbutton_t *button, int x, int y)
{
    menu_go_parent_handler(0);

    // give focus back to the file selector so the user can interact with it
    window_set_focus_child(main_window, (struct window_t *)selector);
}


void create_main_menu(void)
{
    //struct menu_t *m = menu_new(NULL, NULL);
    struct menu_item_t *file_menu = mainmenu_new_item(main_window, "&File");
    struct menu_item_t *edit_menu = mainmenu_new_item(main_window, "&Edit");
    struct menu_item_t *view_menu = mainmenu_new_item(main_window, "&View");
    struct menu_item_t *go_menu = mainmenu_new_item(main_window, "&Go");
    struct menu_item_t *help_menu = mainmenu_new_item(main_window, "&Help");
    
    struct menu_item_t *mi;
    
    /*
     * Create the File menu.
     */
    mi = menu_new_item(file_menu, "New &file");
    mi->handler = menu_file_newfile_handler;
    mi = menu_new_item(file_menu, "New &directory");
    mi->handler = menu_file_newdir_handler;

    menu_new_item(file_menu, "-");

    properties_mi = menu_new_item(file_menu, "&Properties");
    properties_mi->handler = menu_file_properties_handler;
    menu_item_set_enabled(properties_mi, 0);
    // assign the shortcut: ALT + Enter
    menu_item_set_shortcut(main_window, properties_mi, 
                            KEYCODE_ENTER, MODIFIER_MASK_ALT);

    menu_new_item(file_menu, "-");

    mi = menu_new_icon_item(file_menu, "&Exit", NULL, MENU_FILE_EXIT);
    mi->handler = menu_file_close_handler;
    // assign the shortcut: CTRL + Q
    menu_item_set_shortcut(main_window, mi, KEYCODE_Q, MODIFIER_MASK_CTRL);

    /*
     * Create the Edit menu.
     */
    cut_mi = menu_new_icon_item(edit_menu, "C&ut", NULL, MENU_EDIT_CUT);
    cut_mi->handler = menu_edit_cut_handler;
    menu_item_set_enabled(cut_mi, 0);
    // assign the shortcut: CTRL + X
    menu_item_set_shortcut(main_window, cut_mi, KEYCODE_X, MODIFIER_MASK_CTRL);

    copy_mi = menu_new_icon_item(edit_menu, "&Copy", NULL, MENU_EDIT_COPY);
    copy_mi->handler = menu_edit_copy_handler;
    menu_item_set_enabled(copy_mi, 0);
    // assign the shortcut: CTRL + C
    menu_item_set_shortcut(main_window, copy_mi, KEYCODE_C, MODIFIER_MASK_CTRL);

    paste_mi = menu_new_icon_item(edit_menu, "&Paste", NULL, MENU_EDIT_PASTE);
    paste_mi->handler = menu_edit_paste_handler;
    //menu_item_set_enabled(paste_mi, 0);
    // assign the shortcut: CTRL + V
    menu_item_set_shortcut(main_window, paste_mi, KEYCODE_V, MODIFIER_MASK_CTRL);

    menu_new_item(edit_menu, "-");

    mi = menu_new_item(edit_menu, "Select all");
    mi->handler = menu_edit_selectall_handler;
    // assign the shortcut: CTRL + A
    menu_item_set_shortcut(main_window, mi, KEYCODE_A, MODIFIER_MASK_CTRL);

    menu_new_item(edit_menu, "-");

    rename_mi = menu_new_item(edit_menu, "Rename");
    rename_mi->handler = menu_edit_rename_handler;
    menu_item_set_enabled(rename_mi, 0);
    // assign the shortcut: F2
    menu_item_set_shortcut(main_window, rename_mi, KEYCODE_F12, 0);

    delete_mi = menu_new_icon_item(edit_menu, "Delete", NULL, MENU_FILE_CLOSE);
    delete_mi->handler = menu_edit_delete_handler;
    menu_item_set_enabled(delete_mi, 0);
    // assign the shortcut: Delete
    menu_item_set_shortcut(main_window, delete_mi, KEYCODE_DELETE, 0);

    /*
     * Create the View menu.
     */
    mi = menu_new_icon_item(view_menu, "Refresh", NULL, MENU_VIEW_REFRESH);
    mi->handler = menu_view_refresh_handler;
    // assign the shortcut: CTRL + R
    menu_item_set_shortcut(main_window, mi, KEYCODE_R, MODIFIER_MASK_CTRL);

    iconview_mi = menu_new_checked_item(view_menu, "Icon view");
    iconview_mi->handler = menu_view_icons_handler;
    menu_item_set_checked(iconview_mi, 1);
    // assign the shortcut: CTRL + 1
    menu_item_set_shortcut(main_window, iconview_mi, 
                            KEYCODE_1, MODIFIER_MASK_CTRL);

    listview_mi = menu_new_checked_item(view_menu, "List view");
    listview_mi->handler = menu_view_list_handler;
    // assign the shortcut: CTRL + 2
    menu_item_set_shortcut(main_window, listview_mi, 
                            KEYCODE_2, MODIFIER_MASK_CTRL);

    compactview_mi = menu_new_checked_item(view_menu, "Compact view");
    compactview_mi->handler = menu_view_compact_handler;
    // assign the shortcut: CTRL + 3
    menu_item_set_shortcut(main_window, compactview_mi, 
                            KEYCODE_3, MODIFIER_MASK_CTRL);

    /*
     * Create the Go menu.
     */
    parent_mi = menu_new_icon_item(go_menu, "Open parent", 
                                    NULL, MENU_SYSTEM_ARROW_UP);
    parent_mi->handler = menu_go_parent_handler;
    // assign the shortcut: ALT + Up
    menu_item_set_shortcut(main_window, parent_mi, 
                            KEYCODE_UP, MODIFIER_MASK_ALT);

    back_mi = menu_new_icon_item(go_menu, "Back", 
                                    NULL, MENU_SYSTEM_ARROW_LEFT);
    back_mi->handler = menu_go_back_handler;
    menu_item_set_enabled(back_mi, 0);
    // assign the shortcut: ALT + Left
    menu_item_set_shortcut(main_window, back_mi, 
                            KEYCODE_LEFT, MODIFIER_MASK_ALT);

    forward_mi = menu_new_icon_item(go_menu, "Forward", 
                                    NULL, MENU_SYSTEM_ARROW_RIGHT);
    forward_mi->handler = menu_go_forward_handler;
    menu_item_set_enabled(forward_mi, 0);
    // assign the shortcut: ALT + Right
    menu_item_set_shortcut(main_window, forward_mi, 
                            KEYCODE_RIGHT, MODIFIER_MASK_ALT);

    /*
     * Create the Help menu.
     */
    mi = menu_new_item(help_menu, "Keyboard shortcuts");
    mi->handler = menu_help_shortcuts_handler;
    // assign the shortcut: CTRL + F1
    menu_item_set_shortcut(main_window, mi, KEYCODE_F1, MODIFIER_MASK_CTRL);

    mi = menu_new_item(help_menu, "About");
    mi->handler = menu_help_about_handler;

    finalize_menus(main_window);
}


int main(int argc, char **argv)
{
    struct event_t *ev = NULL;
    char *home = getenv("HOME");
    struct window_attribs_t attribs;

    gui_init(argc, argv);

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = 410;
    attribs.h = 300;
    attribs.flags = WINDOW_HASMENU | WINDOW_HASSTATUSBAR;

    if(!(main_window = window_create(&attribs)))
    {
        fprintf(stderr, "%s: failed to create window: %s\n", 
                        argv[0], strerror(errno));
        gui_exit(EXIT_FAILURE);
    }

    create_main_menu();

    // create the user interface
    imgbutton_back = imgbutton_new(main_window->gc, main_window,
                                 2, 4, 28, 28);
    imgbutton_set_sysicon(imgbutton_back, "sign-left");
    imgbutton_set_bordered(imgbutton_back, 0);
    imgbutton_disable(imgbutton_back);
    imgbutton_back->button_click_callback = imgbutton_back_handler;

    imgbutton_forward = imgbutton_new(main_window->gc, main_window,
                                 30, 4, 28, 28);
    imgbutton_set_sysicon(imgbutton_forward, "sign-right");
    imgbutton_set_bordered(imgbutton_forward, 0);
    imgbutton_disable(imgbutton_forward);
    imgbutton_forward->button_click_callback = imgbutton_forward_handler;

    imgbutton_up = imgbutton_new(main_window->gc, main_window,
                                 62, 4, 28, 28);
    imgbutton_set_sysicon(imgbutton_up, "sign-up");
    imgbutton_set_bordered(imgbutton_up, 0);
    imgbutton_up->button_click_callback = imgbutton_up_handler;

    label_new(main_window->gc, main_window, 94, 9, 70, 20, "Location:");

    location_bar = inputbox_new(main_window->gc, main_window,
                                158, 4, main_window->w - 158, NULL);
    location_bar->window.keypress = locationbar_keypress;

    widget_set_size_hints((struct window_t *)location_bar, NULL, 
                            RESIZE_FILLW, 0, 0, 0, 0);

    selector = file_selector_new(main_window->gc, main_window,
                                 0, LOCATION_BAR_HEIGHT,
                                 main_window->w,
                                 main_window->h - LOCATION_BAR_HEIGHT -
                                        MENU_HEIGHT - STATUSBAR_HEIGHT,
                                 NULL);
    selector->entry_click_callback = fileentry_click_callback;
    selector->entry_doubleclick_callback = fileentry_doubleclick_callback;
    selector->selection_change_callback = fileentry_selection_change_callback;
    widget_set_size_hints((struct window_t *)selector, NULL, 
                            RESIZE_FILLW|RESIZE_FILLH, 0, 0, 0, 0);

    if(argc == 2)
    {
        home = argv[1];
    }

    if(!home || !*home)
    {
        home = "/";
    }

    curdir = strdup(home);
    set_my_title(home);
    inputbox_set_text((struct window_t *)location_bar, curdir);
    file_selector_set_path(selector, home);
    history_push(home);

    // if current directory is not root, enable the Go -> Parent menu item
    if(curdir[0] == '/' && curdir[1] == '\0')
    {
        menu_item_set_enabled(parent_mi, 0);
        imgbutton_disable(imgbutton_up);
    }
    else
    {
        menu_item_set_enabled(parent_mi, 1);
        imgbutton_enable(imgbutton_up);
    }
    
    window_set_min_size(main_window, 200, 150);
    window_repaint(main_window);
    window_set_icon(main_window, "folder.ico");
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
    
        switch(ev->type)
        {
            case EVENT_WINDOW_CLOSING:
                window_destroy(main_window);
                gui_exit(EXIT_SUCCESS);
                break;

            default:
                break;
        }

        free(ev);
    }
}

