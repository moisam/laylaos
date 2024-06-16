/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: defs.h
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
 *  \file defs.h
 *
 *  Header file for the files application.
 */

#ifndef FILES_APP_H
#define FILES_APP_H

#define APP_TITLE                   "Files"

// help.c
void show_shortcuts_dialog(void);
void show_about_dialog(void);

// history.c
void history_push(char *path);
char *history_back(void);
char *history_forward(void);
int get_history_current(void);
int get_history_last(void);

// properties.c
void show_properties_dialog(void);

#endif      /* FILES_APP_H */
