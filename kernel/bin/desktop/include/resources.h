/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: resources.h
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
 *  \file resources.h
 *
 *  Declarations and struct definitions for working with resources
 *  on both client and server sides.
 */

#ifndef RESOURCES_H
#define RESOURCES_H

// error type used in function calls
#define INVALID_RESID               0x00

// types of resources to be passed in client/server messages
#define RESOURCE_TYPE_IMAGE         0x0001  /* image or image array */
#define RESOURCE_TYPE_FONT          0x0002  /* font */
#define RESOURCE_TYPE_SYSICON       0x0003  /* system icon */
#define RESOURCE_TYPE_SIZEONLY      0x0800  /* resource size only, no data */

#include "resource-type.h"


#ifdef GUI_SERVER

/*
 * TODO: remove server declarations to a separate file.
 */

struct resource_t
{

#define RESOURCE_IMAGE              0x01
#define RESOURCE_IMAGE_ARRAY        0x02
#define RESOURCE_FONT               0x03
    int type;

    resid_t resid;
    int refs;
    char *filename;
    void *data;
    void (*free_func)(void *);
};


void server_init_sysicon_resources(void);
struct bitmap32_array_t *server_sysicon_resource_load(char *name);

void server_init_resources(void);
struct resource_t *server_resource_get(resid_t resid);
struct resource_t *server_resource_load(char *filename);
void server_resource_free(struct resource_t *res);
void send_res_load_event(int clientfd, struct event_res_t *evres,
                         struct resource_t *res);
void server_resource_unload(resid_t resid);

struct resource_t *server_load_image_from_memory(unsigned int w, 
                                                 unsigned int h,
                                                 uint32_t *data, 
                                                 size_t datasz);

#else       /* !GUI_SERVER */

#include "bitmap.h"
#include "font.h"
#include "window-defs.h"

resid_t sysicon_load(char *name, struct bitmap32_t *bitmap);

resid_t image_get(resid_t resid, struct bitmap32_t *bitmap);
resid_t image_load(char *filename, struct bitmap32_t *bitmap);
void image_free(resid_t resid);
struct bitmap32_t *image_resize(struct bitmap32_t *bitmap,
                                unsigned int width,
                                unsigned int height);
struct bitmap32_t *image_to_greyscale(struct bitmap32_t *bitmap);

resid_t window_icon_get(winid_t winid, struct bitmap32_t *bitmap);

resid_t font_load(char *fontname, struct font_t *font);
void font_unload(struct font_t *font);

char *file_extension(char *filename);
void stringify_file_size(char *buf, off_t file_size);

#endif      /* GUI_SERVER */

#endif      /* RESOURCES_H */
