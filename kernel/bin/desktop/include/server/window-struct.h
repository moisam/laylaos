/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: window-struct.h
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
 *  \file window-struct.h
 *
 *  Definition of the server window structure.
 */

struct clientfd_t
{
    int fd;
    int clients;
    int flags;
};

struct server_window_t
{
    struct server_window_t *parent;
    mutex_t lock;

    int8_t state;
    int16_t x, y, xw1, yh1, client_xw1, client_yh1;
    int16_t client_x, client_y;
    uint16_t w, h, minw, minh;
    uint16_t client_w, client_h;

    uint32_t flags;

    struct
    {
        int8_t state;
        int16_t x, y;
        int16_t client_x, client_y;
        uint16_t w, h;
        uint16_t client_w, client_h;
        uint32_t flags;
    } saved;

    struct
    {
        int16_t x, y;
        uint16_t w, h;
        int shmid;
        uint8_t *canvas;
        uint32_t canvas_alloced_size;
        uint32_t canvas_size;
        uint32_t canvas_pitch;
    } resize;

    int type;
    int controlbox_state;
    char *title;

    winid_t winid;
    winid_t owner_winid;        // only used for popups & menus
    int shmid;
    uint8_t *canvas;
    uint32_t canvas_alloced_size;
    uint32_t canvas_size;
    uint32_t canvas_pitch;

    int drag_type;
    uint16_t drag_off_x;
    uint16_t drag_off_y;
    int tracking_mouse;
    
    uint32_t cursor_id;
    struct clientfd_t *clientfd;

    struct server_window_t *active_child;
    struct server_window_t *drag_child;
    struct server_window_t *tracked_child;
    struct server_window_t *focused_child;
    struct server_window_t *mouseover_child;
    List *children;

    struct clipping_t clipping;

    int pending_x, pending_y;
    int pending_w, pending_h;
    int pending_resize;
    
    struct resource_t *icon;
    
    union
    {
        //struct server_window_t *displayed_menu;
        //struct server_window_t *next_displayed;
        struct server_window_t *displayed_dialog;
    };
};

