/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: resources.c
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
 *  \file resources.c
 *
 *  Functions to work with image and font resources on the client side.
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include "../include/gui.h"
#include "../include/resources.h"
#include "../include/event.h"
#include "../include/directrw.h"


#define GLOB        __global_gui_data

// defined in common/next-event.c
extern mutex_t __global_evlock;


/*********************************************
 * Functions to work with image resources
 *********************************************/

static inline resid_t __copy_bitmap(uint32_t seqid, struct bitmap32_t *bitmap)
{
    struct event_t *ev2;
    struct event_res_t *evbuf;
    resid_t resid;

    if(!(ev2 = get_server_reply(seqid)))
    {
        return INVALID_RESID;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return INVALID_RESID;
    }

    //bufsz = evbuf->img.w * 4 * evbuf->img.h;
    evbuf = (struct event_res_t *)ev2;

    if(!(bitmap->data = malloc(evbuf->datasz)))
    {
        free(ev2);
        __set_errno(ENOMEM);
        return INVALID_RESID;
    }

    bitmap->width = evbuf->img.w;
    bitmap->height = evbuf->img.h;
    A_memcpy(bitmap->data, evbuf->data, evbuf->datasz);

    resid = evbuf->resid;
    free(ev2);

    return resid;
}


static inline void prep_image_request(struct event_res_t *evbuf,
                                      struct bitmap32_t *bitmap,
                                      uint32_t type, uint32_t restype,
                                      size_t datasz)
{
    evbuf->type = type;
    evbuf->seqid = __next_seqid();
    evbuf->datasz = datasz;
    evbuf->src = TO_WINID(GLOB.mypid, 0);
    evbuf->dest = GLOB.server_winid;
    evbuf->restype = restype;
    evbuf->img.w = bitmap->width;
    evbuf->img.h = bitmap->height;
}


static int ensure_buffer_big_enough(uint32_t seqid)
{
    struct event_t *ev2;
    size_t expected;

    if(!(ev2 = get_server_reply(seqid)))
    {
        return 0;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return 0;
    }

    // Next, we check our receiving buffer size
    expected = ((struct event_res_t *)ev2)->datasz +
                                sizeof(struct event_res_t);
    free(ev2);

    if(expected > GLOB.evbufsz)
    {
        mutex_lock(&__global_evlock);

        if(!(ev2 = realloc(GLOB.evbuf_internal, expected)))
        {
            mutex_unlock(&__global_evlock);
            return 0;
        }

        GLOB.evbuf_internal = ev2;
        GLOB.evbufsz = expected;

        mutex_unlock(&__global_evlock);
    }

    return 1;
}


static resid_t __image_load(char *filename, struct bitmap32_t *bitmap,
                            uint32_t restype)
{
    if(!filename || !*filename || !bitmap)
    {
        __set_errno(EINVAL);
        return INVALID_RESID;
    }

    size_t namelen = strlen(filename) + 1;
    size_t bufsz = sizeof(struct event_res_t) + namelen;
    struct event_res_t *evbuf = malloc(bufsz + 1);
    uint32_t seqid;

    if(!evbuf)
    {
        __set_errno(ENOMEM);
        return INVALID_RESID;
    }

    // First, we query the server for the expected size of the image, so
    // that we can expand our receiving buffer if the image is bigger
    A_memset((void *)evbuf, 0, bufsz);
    memcpy((void *)evbuf->data, filename, namelen);
    prep_image_request(evbuf, bitmap, REQUEST_RESOURCE_LOAD,
                       restype | RESOURCE_TYPE_SIZEONLY, namelen);
    seqid = evbuf->seqid;
    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);
    //write(GLOB.serverfd, (void *)evbuf, bufsz);

    if(!ensure_buffer_big_enough(seqid))
    {
        free(evbuf);
        __set_errno(ENOMEM);
        return INVALID_RESID;
    }

    // Finally, get the actual data
    evbuf->seqid = __next_seqid();
    evbuf->restype = restype;
    seqid = evbuf->seqid;
    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);

    free(evbuf);
    
    return __copy_bitmap(seqid, bitmap);
}


resid_t image_load(char *filename, struct bitmap32_t *bitmap)
{
    return __image_load(filename, bitmap, RESOURCE_TYPE_IMAGE);
}


resid_t image_get(resid_t resid, struct bitmap32_t *bitmap)
{
    struct event_res_t evres;
    uint32_t seqid;

    if(!resid || !bitmap)
    {
        __set_errno(EINVAL);
        return INVALID_RESID;
    }

    prep_image_request(&evres, bitmap, REQUEST_RESOURCE_GET, 
                       RESOURCE_TYPE_IMAGE | RESOURCE_TYPE_SIZEONLY, 0);
    evres.resid = resid;
    seqid = evres.seqid;
    direct_write(GLOB.serverfd, &evres, sizeof(struct event_res_t));
    //write(GLOB.serverfd, &evres, sizeof(struct event_res_t));

    if(!ensure_buffer_big_enough(seqid))
    {
        __set_errno(ENOMEM);
        return INVALID_RESID;
    }

    // Finally, get the actual data
    evres.seqid = __next_seqid();
    evres.restype = RESOURCE_TYPE_IMAGE;
    seqid = evres.seqid;
    direct_write(GLOB.serverfd, &evres, sizeof(struct event_res_t));

    return __copy_bitmap(seqid, bitmap);
}


resid_t window_icon_get(winid_t winid, struct bitmap32_t *bitmap)
{
    struct event_res_t evres;
    uint32_t seqid;

    if(!winid || !bitmap)
    {
        __set_errno(EINVAL);
        return INVALID_RESID;
    }

    prep_image_request(&evres, bitmap, REQUEST_WINDOW_GET_ICON, 
                       RESOURCE_TYPE_IMAGE | RESOURCE_TYPE_SIZEONLY, 0);
    evres.src = winid;
    seqid = evres.seqid;
    //evres.resid = resid;
    direct_write(GLOB.serverfd, &evres, sizeof(struct event_res_t));
    //write(GLOB.serverfd, &evres, sizeof(struct event_res_t));

    if(!ensure_buffer_big_enough(seqid))
    {
        __set_errno(ENOMEM);
        return INVALID_RESID;
    }

    // Finally, get the actual data
    evres.seqid = __next_seqid();
    evres.restype = RESOURCE_TYPE_IMAGE;
    seqid = evres.seqid;
    direct_write(GLOB.serverfd, &evres, sizeof(struct event_res_t));

    return __copy_bitmap(seqid, bitmap);
}


void image_free(resid_t resid)
{
    struct event_res_t ev;

    ev.type = REQUEST_RESOURCE_UNLOAD;
    ev.seqid = __next_seqid();
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = GLOB.server_winid;
    ev.resid = resid;
    direct_write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
    //write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}


struct bitmap32_t *image_resize(struct bitmap32_t *bitmap, unsigned int width,
                                                           unsigned int height)
{
    struct bitmap32_t *copy;
    int di, y;
    float src_dx, src_dy;
    float srcy = 0, si;
    uint32_t *dest, *src;

    if(!bitmap)
    {
        return NULL;
    }

    if(!(copy = malloc(sizeof(struct bitmap32_t))))
    {
        return NULL;
    }

    copy->width = width;
    copy->height = height;

    if(!(copy->data = malloc(width * height * 4)))
    {
        free(copy);
        return NULL;
    }

    src_dx = (float)bitmap->width / (float)width;
    src_dy = (float)bitmap->height / (float)height;
    dest = copy->data;
    src = bitmap->data;

    for(y = 0; y < copy->height; y++)
    {
        for(si = 0, di = 0; di < copy->width; di++, si += src_dx)
        {
            dest[di] = src[(int)si];
        }

        dest += copy->width;
        srcy += src_dy;
        src = bitmap->data + ((int)srcy * bitmap->width);
    }

    return copy;
}


// our colors are in the RGBA format
#define R(c)            ((c >> 24) & 0xff)
#define G(c)            ((c >> 16) & 0xff)
#define B(c)            ((c >> 8) & 0xff)
#define A(c)            ((c) & 0xff)


struct bitmap32_t *image_to_greyscale(struct bitmap32_t *bitmap)
{
    struct bitmap32_t *copy;
    uint32_t *dest, *src, grey;
    unsigned int x, y;

    if(!bitmap)
    {
        return NULL;
    }

    if(!(copy = malloc(sizeof(struct bitmap32_t))))
    {
        return NULL;
    }

    copy->width = bitmap->width;
    copy->height = bitmap->height;

    if(!(copy->data = malloc(bitmap->width * bitmap->height * 4)))
    {
        free(copy);
        return NULL;
    }

    dest = copy->data;
    src = bitmap->data;

    for(y = 0; y < copy->height; y++)
    {
        for(x = 0; x < copy->width; x++)
        {
            // See: https://stackoverflow.com/questions/687261/converting-rgb-to-grayscale-intensity
            grey = (0.2126 * R(src[x]) +
                    0.7152 * G(src[x]) +
                    0.0722 * B(src[x]));
            grey &= 0xff;
            dest[x] = A(src[x]) | (grey << 8) | (grey << 16) | (grey << 24);
        }

        dest += copy->width;
        src += copy->width;
    }

    return copy;
}


#undef R
#undef G
#undef B
#undef A


/*********************************************
 * Functions to work with font resources
 *********************************************/

resid_t font_load(char *fontname, struct font_t *font)
{
    if(!fontname || !*fontname || !font)
    {
        __set_errno(EINVAL);
        return INVALID_RESID;
    }

    size_t namelen = strlen(fontname) + 1;
    size_t bufsz = sizeof(struct event_res_t) + namelen;
    struct event_res_t *evbuf = malloc(bufsz + 1);
    struct event_t *ev2;
    uint32_t seqid = __next_seqid();
    resid_t resid;

    if(!evbuf)
    {
        __set_errno(ENOMEM);
        return INVALID_RESID;
    }

    A_memset((void *)evbuf, 0, bufsz);
    memcpy((void *)evbuf->data, fontname, namelen);
    evbuf->type = REQUEST_RESOURCE_LOAD;
    evbuf->seqid = seqid;
    evbuf->datasz = namelen;
    evbuf->src = TO_WINID(GLOB.mypid, 0);
    evbuf->dest = GLOB.server_winid;
    evbuf->restype = RESOURCE_TYPE_FONT;
    //evbuf->font.ptsz = font->ptsz;
    evbuf->font.charw = font->charw;
    evbuf->font.charh = font->charh;
    direct_write(GLOB.serverfd, (void *)evbuf, bufsz);
    //write(GLOB.serverfd, (void *)evbuf, bufsz);

    free((void *)evbuf);

    if(!(ev2 = get_server_reply(seqid)))
    {
        return INVALID_RESID;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return INVALID_RESID;
    }

    //bufsz = evbuf->img.w * 4 * evbuf->img.h;
    evbuf = (struct event_res_t *)ev2;

    if(evbuf->font.is_ttf)
    {
        if((font->data = shmat(evbuf->font.shmid, NULL, 0)) < 0)
        {
            free(ev2);
            __set_errno(ENOMEM);
            return INVALID_RESID;
        }

        font->flags = FONT_FLAG_TRUE_TYPE | FONT_FLAG_DATA_SHMEM;
        font->shmid = evbuf->font.shmid;
    }
    else
    {
        if(!(font->data = malloc(evbuf->datasz)))
        {
            free(ev2);
            __set_errno(ENOMEM);
            return INVALID_RESID;
        }

        font->flags = FONT_FLAG_FIXED_WIDTH;
    }

    //font->ptsz = evbuf->font.ptsz;
    font->charw = evbuf->font.charw;
    font->charh = evbuf->font.charh;
    font->datasz = evbuf->datasz;
    font->glyph_caches = NULL;

    if(evbuf->font.is_ttf)
    {
        // initialize FreeType library if not done yet
        if(GLOB.ftlib == NULL && FT_Init_FreeType(&GLOB.ftlib) != 0)
        {
            shmdt(font->data);
            font->data = NULL;
            font->datasz = 0;
            font->shmid = 0;
            free(ev2);
            return INVALID_RESID;
        }

        // now get the font face
        if(FT_New_Memory_Face(GLOB.ftlib, font->data, font->datasz, 0,
                                            &font->ft_face) != 0)
        {
            shmdt(font->data);
            font->data = NULL;
            font->datasz = 0;
            font->shmid = 0;
            free(ev2);
            return INVALID_RESID;
        }
    }
    else
    {
        A_memcpy(font->data, evbuf->data, evbuf->datasz);
    }

    resid = evbuf->resid;
    free(ev2);

    return resid;
}


/*
 * NOTE: This function frees all the memory used by the font object, along with
 *       its buffered glyph bitmaps, the font face, and the memory used to hold
 *       the font file data. The font struct itself is not freed.
 */
void font_unload(struct font_t *font)
{
    if(!font)
    {
        return;
    }

    if(font->glyph_caches)
    //if(font->tglyph_cache)
    {
        free_tglyph_cache(font);
    }

    if(font->ft_face)
    {
        FT_Done_Face(font->ft_face);
    }

    if(font->data)
    {
        if(font->flags & FONT_FLAG_DATA_SHMEM)
        {
            shmdt(font->data);
        }
        else
        {
            free(font->data);
        }
    }
    
    font->ft_face = NULL;
    font->data = NULL;
    font->datasz = 0;
}


/***********************************************
 * Functions to work with system icon resources
 ***********************************************/

resid_t sysicon_load(char *name, struct bitmap32_t *bitmap)
{
    char buf[strlen(name) + 9];

    sprintf(buf, "%s.sysicon", name);

    return __image_load(buf, bitmap, RESOURCE_TYPE_SYSICON);
}

