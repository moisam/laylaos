/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: dialog.h
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
 *  \file dialog.h
 *
 *  Declarations and struct definitions for working with dialog boxes.
 */

#ifndef GUI_DIALOG_H
#define GUI_DIALOG_H

// builtin dialog box types for normal dialog boxes (i.e. not inputbox,
// keyboard shortcuts, open or save box)
#define DIALOG_YES_NO               (struct dialog_button_t *)1
#define DIALOG_YES_NO_CANCEL        (struct dialog_button_t *)2
#define DIALOG_OK                   (struct dialog_button_t *)3
#define DIALOG_OK_CANCEL            (struct dialog_button_t *)4
#define DIALOG_RETRY_CANCEL         (struct dialog_button_t *)5
#define DIALOG_ACCEPT_DECLINE       (struct dialog_button_t *)6

// builtin button indexes for normal dialog boxes (i.e. not inputbox,
// keyboard shortcuts, open or save box)
#define DIALOG_RESULT_YES           1
#define DIALOG_RESULT_NO            2
#define DIALOG_RESULT_OK            3
#define DIALOG_RESULT_RETRY         4
#define DIALOG_RESULT_CANCEL        5
#define DIALOG_RESULT_ACCEPT        6
#define DIALOG_RESULT_DECLINE       7

/*
 * Structure to represent a dialog box button.
 */
struct dialog_button_t
{
    char *caption;

#define DIALOG_BUTTON_DEFAULT       1
#define DIALOG_BUTTON_CANCEL        2
    int type;
};

/*
 * Internal structure to represent dialog box status.
 */
struct dialog_status_t
{
    int selected_button;
    volatile int close_dialog;
};


#include "button.h"
#include "imgbutton.h"
#include "inputbox.h"
#include "file-selector.h"

/*
 * Structure to represent an About dialog box.
 */
struct about_dialog_t
{
    struct window_t *window;
    winid_t ownerid;
    
    /*
     * The application's icon is shown at the top of the dialog box.
     */
    resid_t app_icon_resid;
    struct bitmap32_t app_icon;

    /*
     * A title is shown on the dialog box's window bar. If no title is set,
     * a default "About" title is shown.
     */
    char *title;
    
    /*
     * An About dialog box will ideally show the following text:
     *     Application's name, e.g. MyApp
     *     Application's version, e.g. 1.0.0
     *     Short "about" description, e.g. This is a test app
     *     Copyright notice, e.g. Copyright (c) 2023 MyCompany
     *
     * The user can assign values to these strings before the About
     * dialog box is shown.
     */
    struct
    {
        char *name;
        char *ver;
        char *about;
        //char *website;
        char *copyright;
    } str;
    
    /*
     * An About dialog box can have upto 4 buttons:
     *     A Credits button (optional)
     *     A License button (optional)
     *     A Help button (optional)
     *     A Close  button (included by default)
     *
     * The user can define callback functions for the above optional
     * buttons before the About dialog box is shown.
     */
    struct
    {
        void (*credits)(struct button_t *, int, int);
        void (*license)(struct button_t *, int, int);
        void (*help)(struct button_t *, int, int);
    } callbacks;
};

/*
 * Structure to represent an About dialog box.
 */
struct shortcuts_dialog_t
{
    struct window_t *window;
    winid_t ownerid;
    
    /*
     * A title is shown on the dialog box's window bar. If no title is set,
     * a default "Keyboard Shortcuts" title is shown.
     */
    char *title;
    
    /*
     * An About dialog box will show the following text:
     *     Shortcut keys on the left side
     *     Description of each shortcut key on the right side
     *
     * The user should assign values to these strings when calling the 
     * shortcuts_dialog_create() function. Both arrays should be of the same
     * length, and the last entry in both array should be NULL so the dialog
     * box can know how many items are there to show.
     */
    struct
    {
        char **shortcuts;
        char **descriptions;
    } str;
};

/*
 * Structure to represent files returned by an Open or Save dialog box.
 */
struct opensave_file_t
{
    char *path;
    //mode_t mode;
    //time_t mtime, atime, ctime;
    //off_t file_size;
};

struct __opensave_internal_state_t
{
    // MUST be first so we can cast to struct dialog_status_t
    struct dialog_status_t status;
    struct imgbutton_t *imgbutton_back;
    struct imgbutton_t *imgbutton_forward;
    struct imgbutton_t *imgbutton_up;
    struct imgbutton_t *imgbutton_iconview;
    struct imgbutton_t *imgbutton_listview;
    struct imgbutton_t *imgbutton_compactview;
    struct inputbox_t *location_bar;
    struct file_selector_t *selector;
    struct inputbox_t *filename_inputbox;
    struct combobox_t *filter_combobox;
    char *curdir;
    int filter_count;
    char **filter_list;
};

/*
 * Structure to represent an Open or Save dialog box.
 */
struct opensave_dialog_t
{
    struct window_t *window;
    winid_t ownerid;
    
    /*
     * Type of dialog box, determines the title shown at the top of the box.
     */
#define DIALOGBOX_OPEN          0
#define DIALOGBOX_SAVE          1
#define DIALOGBOX_SAVEAS        2
    int type;

    /*
     * Whether to allow multiple file selection. Only works for Open dialogs.
     */
    int multiselect;

    /*
     * Starting navigation path. Can be set before showing the dialog box to
     * influence the path at which navigation begins.
     */
    char *path;

    char *filetype_filter;  // If non-NULL, it contains the filter to use
                            // when navigating the filesystem. The filter
                            // format is like: "Text|*.txt|PNG|*.png".

    //char *filename;         // If non-NULL, the filename displayed when the
                            // dialog box is shown is set to this value.
                            // The user can of course change it.

    // Internal fields;
    //struct file_selector_t *selector;
    struct __opensave_internal_state_t internal;
};


/**************************************************************
 * Functions to work with "standard" message (or dialog) boxes
 **************************************************************/

// Create and show a message box
int messagebox_show(winid_t owner, char *title, char *message,
                    struct dialog_button_t *buttons, int button_count);

// Create and show an input box
char *inputbox_show(winid_t owner, char *title, char *message);

/**************************************************************
 * Functions to work with "About" dialog boxes
 **************************************************************/

// Create an About dialog box
struct about_dialog_t *aboutbox_create(winid_t owner);

// Show an About dialog box
int aboutbox_show(struct about_dialog_t *dialog);

// Destroy an About dialog box and free its resources
void aboutbox_destroy(struct about_dialog_t *dialog);

// Set an About dialog box strings (see above)
void aboutbox_set_title(struct about_dialog_t *dialog, char *title);
void aboutbox_set_name(struct about_dialog_t *dialog, char *app_name);
void aboutbox_set_version(struct about_dialog_t *dialog, char *app_ver);
void aboutbox_set_about(struct about_dialog_t *dialog, char *app_about);
void aboutbox_set_copyright(struct about_dialog_t *dialog, char *app_copyright);
void aboutbox_credits_callback(struct about_dialog_t *dialog,
                               void (*fn)(struct button_t *, int, int));
void aboutbox_license_callback(struct about_dialog_t *dialog,
                               void (*fn)(struct button_t *, int, int));
void aboutbox_help_callback(struct about_dialog_t *dialog,
                            void (*fn)(struct button_t *, int, int));

/**************************************************************
 * Functions to work with "Keyboard Shortcuts" dialog boxes
 **************************************************************/

// Create a Keyboard Shortcuts dialog box
struct shortcuts_dialog_t *shortcuts_dialog_create(winid_t owner,
                                                   char **shortcuts,
                                                   char **descriptions);

// Show a Keyboard Shortcuts dialog box
int shortcuts_dialog_show(struct shortcuts_dialog_t *dialog);

// Destroy a Keyboard Shortcuts dialog box and free its resources
void shortcuts_dialog_destroy(struct shortcuts_dialog_t *dialog);

// Set a Keyboard Shortcuts dialog box strings (see above)
void shortcuts_dialog_set_title(struct shortcuts_dialog_t *dialog, char *title);

/**************************************************************
 * Functions to work with Open and Save dialog boxes
 **************************************************************/

struct opensave_dialog_t *open_dialog_create(winid_t owner);
int open_dialog_show(struct opensave_dialog_t *dialog);
void open_dialog_destroy(struct opensave_dialog_t *dialog);

int open_dialog_get_selected(struct opensave_dialog_t *dialog, 
                             struct opensave_file_t **res);
void open_dialog_free_list(struct opensave_file_t *entries, int entry_count);

struct opensave_dialog_t *save_dialog_create(winid_t owner);
int save_dialog_show(struct opensave_dialog_t *dialog);
void save_dialog_destroy(struct opensave_dialog_t *dialog);

int save_dialog_get_selected(struct opensave_dialog_t *dialog, 
                             struct opensave_file_t **res);
void save_dialog_free_list(struct opensave_file_t *entries, int entry_count);

struct opensave_dialog_t *saveas_dialog_create(winid_t owner);
int saveas_dialog_show(struct opensave_dialog_t *dialog);
void saveas_dialog_destroy(struct opensave_dialog_t *dialog);

int saveas_dialog_get_selected(struct opensave_dialog_t *dialog, 
                               struct opensave_file_t **res);
void saveas_dialog_free_list(struct opensave_file_t *entries, int entry_count);

#endif      /* GUI_DIALOG_H */
