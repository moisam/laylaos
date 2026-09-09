/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: systray.h
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
 *  \file systray.h
 *
 *  Declarations and struct definitions for the system tray.
 */

#ifndef CLIENT_SYSTRAY_H
#define CLIENT_SYSTRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../rect-struct.h"

extern winid_t systray_manager_winid;

// This is for the systray manager to register itself
void register_systray_manager(winid_t winid);

// These are for client applications to communicate with the manager
winid_t systray_get_manager_winid(void);
int systray_add(winid_t src);
int systray_remove(winid_t src);
int systray_set_icon_visibility(winid_t src, int visible);
int systray_set_icon(winid_t src, const char *data, size_t datasz);
int systray_set_tooltip(winid_t src, const char *tooltip);
int systray_get_bounds(winid_t src, Rect *r);
int systray_show_message(winid_t src, const char *title, const char *msg,
                         const void *icon, size_t iconsz, int msgtype, int msgduration);

#ifdef __cplusplus
}
#endif

#endif      /* CLIENT_SYSTRAY_H */
