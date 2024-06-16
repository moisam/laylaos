/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: event.h
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
 *  \file event.h
 *
 *  Declarations and struct definitions for working with events.
 *  Some important definitions:
 *  - An event is a message sent from the server to a client application
 *  - A request is a message sent from a client application to the server
 *
 *  Most events are sent from the server in response to a client request,
 *  but some of them are sent without a preceding request. There is a close
 *  relationship between a request and its response (event), which are
 *  detailed in the table below.
 */

#ifndef GUI_EVENT_H
#define GUI_EVENT_H

#include <sys/types.h>
#include <kernel/mouse.h>
#include "window-defs.h"        // typedef winid_t
#include "cursor-struct.h"      // typedef curid_t
#include "resource-type.h"      // typedef resid_t


#ifndef GUI_SERVER
#undef A_memcpy
#undef A_memset
#define A_memcpy        memcpy /* local_memcpy */
#define A_memset        memset /* local_memset */
#endif

#ifdef __cplusplus
extern "C" {
#endif

// prototypes for optimized memory functions from asmlib.a
// Library source can be downloaded from:
//     https://www.agner.org/optimize/#asmlib
void * A_memcpy(void * dest, const void * src, size_t count);
void * A_memset(void * dest, int c, size_t count);


/*
 * This table shows the types of requests (sent by the client) and their
 * responses (sent by the server). Some requests do not generate responses.
 * The response can be sent to either the client or the client's parent,
 * but not both.
 *
 * Request                      Event (i.e. Response)           Event receiver
 * ---------------------------------------------------------------------------
 * REQUEST_SCREEN_INFO          EVENT_SCREEN_INFO                       Child
 *
 * REQUEST_WINDOW_CREATE        Two events are generated:
 *
 *                              EVENT_WINDOW_CREATED                    Child
 *                              EVENT_CHILD_WINDOW_CREATED              Parent
 *
 * REQUEST_WINDOW_DESTROY       EVENT_CHILD_WINDOW_DESTROYED            Parent
 *
 * REQUEST_WINDOW_SHOW          May generate events (if window not shown):
 *                              EVENT_WINDOW_SHOWN                      Child
 *                              EVENT_CHILD_WINDOW_SHOWN                Parent
 *
 * REQUEST_WINDOW_HIDE          May generate events (if window not hidden):
 *                              EVENT_WINDOW_HIDDEN                     Child
 *                              EVENT_CHILD_WINDOW_HIDDEN               Parent
 *
 * REQUEST_WINDOW_MAXIMIZE      First event generated if child is hidden,
 *                              Second event is always generated:
 *
 *                              EVENT_CHILD_WINDOW_SHOWN                Parent
 *                              EVENT_WINDOW_RESIZE                     Child
 *
 * REQUEST_WINDOW_MINIMIZE      May generate event (if window not hidden):
 *                              EVENT_CHILD_WINDOW_HIDDEN               Parent
 *
 * REQUEST_WINDOW_RESTORE       May generate event (if window hidden):
 *                              EVENT_CHILD_WINDOW_SHOWN                Parent
 *
 * REQUEST_WINDOW_RAISE         EVENT_WINDOW_RAISED                     Child
 *                              May also generate a parent event (if window not on top):
 *
 *                              EVENT_CHILD_WINDOW_SHOWN                Parent
 *                              EVENT_CHILD_WINDOW_RAISED               Parent
 *
 * REQUEST_WINDOW_TOGGLE_STATE  One event generated, depending on window state:
 *
 *                              EVENT_CHILD_WINDOW_SHOWN                Parent
 *                              EVENT_CHILD_WINDOW_HIDDEN               Parent
 *                              EVENT_CHILD_WINDOW_RAISED               Parent
 *
 * REQUEST_WINDOW_ENTER_FULLSCREEN  May generate one event (if window not shown):
 *
 *                              EVENT_CHILD_WINDOW_SHOWN                Parent
 *                              EVENT_CHILD_WINDOW_RAISED               Parent
 *
 * REQUEST_WINDOW_EXIT_FULLSCREEN   May generate one event (if window not shown):
 *
 *                              EVENT_CHILD_WINDOW_SHOWN                Parent
 *                              EVENT_CHILD_WINDOW_RAISED               Parent
 *
 * REQUEST_WINDOW_RESIZE_ACCEPT May generate event (if pending resize requests):
 *                              EVENT_WINDOW_RESIZE                     Child
 *
 * REQUEST_WINDOW_RESIZE        EVENT_WINDOW_RESIZE                     Child
 *
 * REQUEST_WINDOW_SET_POS       No event generated                      -
 *
 * REQUEST_WINDOW_SET_TITLE     EVENT_CHILD_WINDOW_TITLE_SET            Parent
 *
 * REQUEST_WINDOW_SET_ATTRIBS   No event generated                      -
 *
 * REQUEST_WINDOW_GET_ATTRIBS   EVENT_WINDOW_ATTRIBS                    Child
 *
 * REQUEST_WINDOW_SET_ICON      No event generated                      -
 *
 * REQUEST_WINDOW_INVALIDATE    No event generated                      -
 *
 * REQUEST_WINDOW_DESTROY_CANVAS    No event generated                  -
 *
 * REQUEST_WINDOW_NEW_CANVAS    EVENT_WINDOW_NEW_CANVAS                 Child
 *
 * REQUEST_GRAB_MOUSE           EVENT_MOUSE_GRABBED                     Child
 *
 * REQUEST_UNGRAB_MOUSE         No event generated                      -
 *
 * REQUEST_CURSOR_LOAD          EVENT_CURSOR_LOADED                     Child
 *
 * REQUEST_CURSOR_FREE          No event generated                      -
 *
 * REQUEST_CURSOR_SHOW          No event generated                      -
 *
 * REQUEST_CURSOR_HIDE          No event generated                      -
 *
 * REQUEST_CURSOR_SET_POS       No event generated                      -
 *
 * REQUEST_CURSOR_GET_INFO      EVENT_CURSOR_INFO                       Child
 *
 * REQUEST_GRAB_KEYBOARD        EVENT_KEYBOARD_GRABBED                  Child
 *
 * REQUEST_UNGRAB_KEYBOARD      No event generated                      -
 *
 * REQUEST_GET_MODIFIER_KEYS    EVENT_MODIFIER_KEYS                     Child
 *
 * REQUEST_GET_KEYS_STATE       EVENT_KEYS_STATE                        Child
 *
 * REQUEST_CREATE_QUEUE         No event generated                      -
 *
 * REQUEST_DESTROY_QUEUE        No event generated                      -
 *
 * REQUEST_SET_DESKTOP_BOUNDS   No event generated                      -
 *
 * REQUEST_MENU_ADD             EVENT_MENU_ADDED                        Child
 */

/*
 * Types of requests. A request is a message sent by a client to the GUI
 * server to request something like creating a window or getting a resource.
 */
enum
{
    REQUEST_SCREEN_INFO = 1,
    REQUEST_WINDOW_CREATE,
    REQUEST_WINDOW_DESTROY,
    REQUEST_WINDOW_SHOW,
    REQUEST_WINDOW_HIDE,
    REQUEST_WINDOW_MAXIMIZE,
    REQUEST_WINDOW_MINIMIZE,
    REQUEST_WINDOW_RESTORE,
    REQUEST_WINDOW_RAISE,
    REQUEST_WINDOW_TOGGLE_STATE,
    REQUEST_WINDOW_SET_MIN_SIZE,
    REQUEST_WINDOW_RESIZE,
    REQUEST_WINDOW_RESIZE_ACCEPT,
    REQUEST_WINDOW_RESIZE_FINALIZE,
    REQUEST_WINDOW_SET_POS,
    REQUEST_WINDOW_SET_TITLE,
    REQUEST_WINDOW_SET_ICON,
    REQUEST_WINDOW_LOAD_ICON,
    REQUEST_WINDOW_SET_ATTRIBS,
    REQUEST_WINDOW_INVALIDATE,
    REQUEST_WINDOW_GET_ICON,
    REQUEST_WINDOW_GET_ATTRIBS,
    REQUEST_WINDOW_GET_STATE,
    REQUEST_WINDOW_ENTER_FULLSCREEN,
    REQUEST_WINDOW_EXIT_FULLSCREEN,
    REQUEST_WINDOW_DESTROY_CANVAS,
    REQUEST_WINDOW_NEW_CANVAS,
    REQUEST_GRAB_MOUSE,
    REQUEST_GRAB_AND_CONFINE_MOUSE,
    REQUEST_UNGRAB_MOUSE,
    REQUEST_CURSOR_LOAD,
    REQUEST_CURSOR_FREE,
    REQUEST_CURSOR_SHOW,
    REQUEST_CURSOR_HIDE,
    REQUEST_CURSOR_SET_POS,
    REQUEST_CURSOR_GET_INFO,
    REQUEST_GRAB_KEYBOARD,
    REQUEST_UNGRAB_KEYBOARD,
    REQUEST_GET_INPUT_FOCUS,
    REQUEST_GET_MODIFIER_KEYS,
    REQUEST_GET_KEYS_STATE,
    REQUEST_BIND_KEY,
    REQUEST_UNBIND_KEY,
    REQUEST_SET_DESKTOP_BOUNDS,
    REQUEST_MENU_FRAME_CREATE,
    REQUEST_MENU_FRAME_SHOW,
    REQUEST_MENU_FRAME_HIDE,
    REQUEST_MENU_ADD,
    REQUEST_DIALOG_CREATE,
    REQUEST_DIALOG_SHOW,
    REQUEST_DIALOG_HIDE,
    REQUEST_RESOURCE_LOAD,
    REQUEST_RESOURCE_GET,
    REQUEST_RESOURCE_UNLOAD,
    REQUEST_CLIPBOARD_SET,
    REQUEST_CLIPBOARD_GET,
    REQUEST_CLIPBOARD_QUERY,
    REQUEST_COLOR_PALETTE,
    REQUEST_LAST,       // currently 58
};

/*
 * Types of events. An event is a message sent by the GUI server to a cient
 * (or to the client's parent) to notify about something (like mouse movements
 * or keyboard presses), or as a reply to certain requests (like a child being
 * created or terminated, or to give the client a shared resouce e.g. a font).
 */
enum
{
    EVENT_SCREEN_INFO = 128,
    EVENT_WINDOW_CREATED,
    EVENT_WINDOW_SHOWN,
    EVENT_WINDOW_HIDDEN,
    EVENT_WINDOW_RAISED,
    EVENT_WINDOW_LOWERED,
    //EVENT_WINDOW_RESIZE,
    EVENT_WINDOW_RESIZE_OFFER,
    EVENT_WINDOW_RESIZE_CONFIRM,
    EVENT_WINDOW_POS_CHANGED,
    EVENT_WINDOW_LOST_FOCUS,
    EVENT_WINDOW_GAINED_FOCUS,
    EVENT_WINDOW_CLOSING,
    EVENT_WINDOW_ATTRIBS,
    EVENT_WINDOW_STATE,
    EVENT_WINDOW_NEW_CANVAS,
    EVENT_CHILD_WINDOW_CREATED,
    EVENT_CHILD_WINDOW_TITLE_SET,
    EVENT_CHILD_WINDOW_ICON_SET,
    EVENT_CHILD_WINDOW_SHOWN,
    EVENT_CHILD_WINDOW_HIDDEN,
    EVENT_CHILD_WINDOW_RAISED,
    EVENT_CHILD_WINDOW_LOWERED,
    EVENT_CHILD_WINDOW_DESTROYED,
    EVENT_MOUSE_GRABBED,
    //EVENT_MOUSE_UNGRABBED,
    EVENT_CURSOR_LOADED,
    EVENT_CURSOR_INFO,
    EVENT_KEYBOARD_GRABBED,
    //EVENT_KEYBOARD_UNGRABBED,
    EVENT_MODIFIER_KEYS,
    EVENT_KEYS_STATE,
    EVENT_MENU_FRAME_CREATED,
    EVENT_MENU_FRAME_HIDDEN,
    //EVENT_MENU_ADDED,
    EVENT_MENU_SELECTED,
    EVENT_DIALOG_CREATED,
    //EVENT_DIALOG_HIDDEN,
    EVENT_MOUSE,
    EVENT_MOUSE_ENTER,
    EVENT_MOUSE_EXIT,
    EVENT_KEY_PRESS,
    EVENT_KEY_RELEASE,
    EVENT_RESOURCE_LOADED,
    EVENT_CLIPBOARD_SET,
    EVENT_CLIPBOARD_DATA,
    EVENT_CLIPBOARD_HAS_DATA,
    //EVENT_FONT_RESOURCE,
    //EVENT_ICON_LOADED,
    EVENT_COLOR_PALETTE_DATA,
    EVENT_ERROR,
};

/*
 * Generic struct to represent requests (client messages) and events (server
 * messages). Which fields to be used is determined by the message type.
 */
struct event_t
{
    // the first 4 fields should be the same in all event struct types
    // request or event type (see above enums)
    uint32_t type;
    uint32_t seqid;

    // window id of event's source and destination
    winid_t src, dest;
    
    // non-zero if this is a valid server reply, 0 if server error happened
    int valid_reply;
        
    union
    {
        // events and replies involving window creation, termination, ...
        struct
        {
            int gravity;
            int16_t x, y;
            uint16_t w, h;
            uint32_t flags;
            int32_t shmid;
            uint32_t canvas_size;
            uint32_t canvas_pitch;
            winid_t owner;      // only set for popups & menus
        } win;
        
        struct
        {
            int8_t state;   // only filled in server replies
        } winst;
        
        struct
        {
            resid_t resid;
        } img;

        // requests and replies involving window attributes
        struct
        {
            winid_t winid;
            int16_t x, y;
            uint16_t w, h;
            uint32_t flags;
        } winattr;

        // events and replies involving rectangles, e.g. invalidating a
        // screen area
        struct
        {
            int top;
            int left;
            int bottom;
            int right;
        } rect;
        
        // used to get screen info from the server
        struct
        {
            uint32_t w, h;
            uint8_t pixel_width;
            uint8_t red_pos, red_mask_size;
            uint8_t green_pos, green_mask_size;
            uint8_t blue_pos, blue_mask_size;
            int rgb_mode;   // 0 = palette-indexed, 1 = rgb
        } screen;
        
        // mouse events (movement, button clicks)
        struct
        {
            int x, y;
            mouse_buttons_t buttons;
        } mouse;
        
        // key events (press & release, including modifier kets like ALT, 
        // CTRL, SHIFT)
        struct
        {
            char modifiers;
            char code;
        } key;

        // events involving menu items
        struct
        {
            uint16_t entry_id;   // ID for a menu item (e.g. File->New)
            uint16_t menu_id;    // ID for a menu (e.g. File)
        } menu;
        
        // mouse cursor info
        struct cursor_info_t cur;
        
        // key state info
        struct
        {
            char bits[32];
        } keybmp;

        // request a key binding
        struct
        {
            char key;
            char modifiers;
            int action;
        } keybind;
        
        // request clipboard data (also set when acknowledging data is set)
        struct
        {
            int fmt;
            size_t sz;
        } clipboard;

        // events returning an error result
        struct
        {
            int _errno;
        } err;
    };
};

/*
 * Generic event struct used to send a single item that needs a big buffer,
 * like a window title for example.
 */
struct event_buf_t
{
    // the first 4 fields should be the same in all event struct types
    uint32_t type;
    uint32_t seqid;

    // window id of event's source and destination
    winid_t src, dest;

    // non-zero if this is a valid server reply, 0 if server error happened
    int valid_reply;

    size_t bufsz;
    char buf[];
};

/*
 * Event struct used to send cursor icon data.
 */
struct event_cur_t
{
    // the first 4 fields should be the same in all event struct types
    uint32_t type;
    uint32_t seqid;

    // window id of event's source and destination
    winid_t src, dest;

    // non-zero if this is a valid server reply, 0 if server error happened
    int valid_reply;

    int w, h;
    int hotx, hoty;
    size_t datasz;
    uint32_t data[];
};

#include "resource-type.h"

/*
 * Event struct used to send/request resource data.
 */
struct event_res_t
{
    // the first 4 fields should be the same in all event struct types
    uint32_t type;
    uint32_t seqid;

    // window id of event's source and destination
    winid_t src, dest;

    // non-zero if this is a valid server reply, 0 if server error happened
    int valid_reply;

    // see resources.h for the different RESOURCE_TYPE_* definitions
    uint32_t restype;
    resid_t resid;

    // the only valid fields are those related to the resource type
    union
    {
        struct
        {
            uint32_t w, h;
        } img;

        struct
        {
            int is_ttf;
            int charw, charh;       // for monospace fonts
            int shmid;              // for TTF fonts (is_ttf != 0)
        } font;

        struct
        {
            int fmt;
        } clipboard;

        struct
        {
            uint8_t color_count;
        } palette;
    };

    // When requesting a resource to be loaded, data contains the resource's
    // pathname. When returning a loaded resource to the client, data contains
    // the actual data (image pixels, font data, etc).
    size_t datasz;
    char data[];
};

#if 0

#include "menu-icon.h"

/*
 * Generic event struct used to request the server to add a new menu or a 
 * menu entry to the client's window.
 */
struct event_menu_t
{
    // the first 4 fields should be the same in all event struct types
    uint32_t type;
    uint32_t seqid;

    // window id of event's source and destination
    winid_t src, dest;

    // non-zero if this is a valid server reply, 0 if server error happened
    int valid_reply;

    uint8_t entry_id;

    uint8_t icon_type;
    uint8_t icon_index;
    uint8_t icon_file[128];

    uint8_t menu_id;
    uint8_t entry_type;
    uint8_t entry_title[128];
};

#endif

/*
 * Struct to represent an event in the event queue (used in next-event.c).
 */
struct queued_ev_t
{
    struct queued_ev_t *prev, *next;
    void *data;
};


/******************************************************
 * Functions common to client and server applications
 ******************************************************/

struct event_t *next_event(void);
ssize_t __get_event(int evfd, volatile struct event_t *evbuf, 
                        size_t bufsz, int wait);
void notify_win_title_event(int fd, char *title, winid_t dest, winid_t src);


/******************************************************
 * Client-only functions
 ******************************************************/

#ifndef GUI_SERVER

#include "list.h"
#include "mouse-state-struct.h"
#include "client/window-attrib-struct.h"
#include "client/window-struct.h"

void set_desktop_bounds(int top, int left, int bottom, int right);
int get_win_attribs(winid_t winid, struct window_attribs_t *attribs);
struct event_t *next_event_for_seqid(struct window_t *window, 
                                     uint32_t seqid, int wait);
struct event_t *get_server_reply(uint32_t seqid);
int pending_events_timeout(time_t secs);
int pending_events_utimeout(suseconds_t usecs);
int pending_events(void);
int event_dispatch(struct event_t *ev);

#endif      /* !GUI_SERVER */

#ifdef __cplusplus
}
#endif

#endif      /* GUI_EVENT_H */
