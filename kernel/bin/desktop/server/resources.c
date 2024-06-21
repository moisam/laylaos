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
 *  Functions to work with image and font resources on the server side.
 */

#define GUI_SERVER
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include "../include/gui.h"
#include "../include/resources.h"
#include "../include/bitmap.h"
#include "../include/server/event.h"
//#include "../../../../misc/hash.h"
#include "../include/font.h"
#include <sys/hash.h>

#include "font-array.h"
#include "font-array-bold.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#define GLOB                __global_gui_data
#define SYSFONT_FILE        DEFAULT_FONT_PATH "/Tuffy.ttf"
#define BOLD_SYSFONT_FILE   DEFAULT_FONT_PATH "/Tuffy_Bold.ttf"

#define INIT_HASHSZ         256

#define KEY_PREFIX          0xE000

struct hashtab_t *restab = NULL;


// local function declarations
struct bitmap32_array_t *server_ico_resource_load(char *filename);
struct bitmap32_t *server_png_resource_load(char *filename);
void server_image_resource_free(void *raw);
void server_image_array_resource_free(void *raw);
void server_font_resource_free(void *raw);

static uint32_t next_resid = 1;

// system monotype font (bold face) - the typical user application does
// not need it, so those who want it have to request it from the server
struct font_t font_monobold;


static void load_sysfont(char *path, struct font_t *font, char *resname)
{
    FILE *f;
    struct resource_t *res;
    static int next_id = 1;
    key_t key = KEY_PREFIX + next_id;

    // load the default system font face to memory
    if(!(f = fopen(path, "r")))
    {
        return;
    }

    // get file size
    fseek(f, 0, SEEK_END);
    font->datasz = ftell(f);
    fseek(f, 0, SEEK_SET);

    // map memory and read file
    if((font->shmid = shmget(key, font->datasz, IPC_CREAT | IPC_EXCL | 0666)) < 0)
    {
        fclose(f);
        return;
    }

    if((font->data = shmat(font->shmid, NULL, 0)) < 0)
    {
        fclose(f);
        return;
    }

    next_id++;

    fread(font->data, font->datasz, 1, f);
    fclose(f);

    // now get the font face
    if(FT_New_Memory_Face(GLOB.ftlib, font->data, font->datasz, 0,
                                        &font->ft_face) != 0)
    {
        shmdt(font->data);
        font->data = NULL;
        font->datasz = 0;
        return;
    }

    font->flags = FONT_FLAG_TRUE_TYPE | FONT_FLAG_DATA_SHMEM |
                  FONT_FLAG_SYSTEM_FONT;
    font->ptsz = 16;

    // arguments to FT_Set_Char_Size():
    //   font face,
    //   0 => char width in 1/64 of points, 0 means same as height,
    //   10 * 64 => char height in 1/64 of points,
    //   0 => horizontal device resolution, 0 means same as vertical,
    //   0 => vertical device resolution, 0 means default (72 dpi),

    FT_Set_Char_Size(font->ft_face, 0, font->ptsz * 64, 0, 0);

    // add to the resource hashtab
    if(!(res = malloc(sizeof(struct resource_t))))
    {
        return;
    }

    A_memset(res, 0, sizeof(struct resource_t));
    res->type = RESOURCE_FONT;
    res->filename = strdup(resname);
    res->data = font;
    res->free_func = server_font_resource_free;
    res->refs = 1;

    hashtab_add(restab, res->filename, res);
    res->resid = next_resid++;
}


void server_init_resources(void)
{
    struct resource_t *res;

#define STRCMP          (int (*)(void *, void *))strcmp

    if(!(restab = hashtab_create(INIT_HASHSZ, calc_hash_for_str, STRCMP)))
    {
        return;
    }

#undef STRCMP
    
    // load the default executable file icon
    server_resource_load(DEFAULT_EXE_ICON_PATH);
    
    // add the default monospace system font - all user applications request
    // this font on init
    GLOB.mono.charw = mono_char_width;
    GLOB.mono.charh = mono_char_height;
    GLOB.mono.data = mono_font_array;
    GLOB.mono.datasz = mono_datasz;
    GLOB.mono.flags = FONT_FLAG_FIXED_WIDTH | FONT_FLAG_SYSTEM_FONT;

    if(!(res = malloc(sizeof(struct resource_t))))
    {
        return;
    }

    A_memset(res, 0, sizeof(struct resource_t));
    res->type = RESOURCE_FONT;
    res->filename = strdup("font-monospace");
    res->data = &GLOB.mono;
    res->free_func = server_font_resource_free;
    res->refs = 1;

    hashtab_add(restab, res->filename, res);
    res->resid = next_resid++;

    // add the default monospace system font (bold face) - only interested
    // user applications will request this
    font_monobold.charw = mono_char_width;
    font_monobold.charh = mono_char_height;
    font_monobold.data = mono_bold_font_array;
    font_monobold.datasz = mono_bold_datasz;
    font_monobold.flags = FONT_FLAG_FIXED_WIDTH | FONT_FLAG_SYSTEM_FONT;

    if(!(res = malloc(sizeof(struct resource_t))))
    {
        return;
    }

    A_memset(res, 0, sizeof(struct resource_t));
    res->type = RESOURCE_FONT;
    res->filename = strdup("font-monospace-bold");
    res->data = &font_monobold;
    res->free_func = server_font_resource_free;
    res->refs = 1;

    hashtab_add(restab, res->filename, res);
    res->resid = next_resid++;

    // initialize the FreeType library
    if(FT_Init_FreeType(&GLOB.ftlib) != 0)
    {
        return;
    }

    // add the default system font - regular and bold
    load_sysfont(SYSFONT_FILE, &GLOB.sysfont, "font-system");
    load_sysfont(BOLD_SYSFONT_FILE, &GLOB.sysfont_bold, "font-system-bold");

    // initialize system icons
    server_init_sysicon_resources();
}


char *file_extension(char *filename)
{
    char *oext = filename + strlen(filename);
    char *ext = oext;

    while(--ext >= filename)
    {
        if(*ext == '.')
        {
            return ext;
        }
    }
    
    return oext;
}


struct resource_t *server_resource_create_struct(int type,
                                                 char *filename,
                                                 void *data,
                                                 void (*free_func)(void *))
{
    struct resource_t *res;

    if(!(res = (struct resource_t *)malloc(sizeof(struct resource_t))))
    {
        return NULL;
    }
    
    A_memset(res, 0, sizeof(struct resource_t));
    res->type = type;
    res->filename = strdup(filename);
    res->data = data;
    res->free_func = free_func;
    res->refs = 1;
    
    if(!res->filename)
    {
        free(res);
        res = NULL;
    }
    
    return res;
}


void server_resource_free(struct resource_t *res)
{
    if(!res)
    {
        return;
    }
    
    if(--res->refs)
    {
        return;
    }
    
    res->free_func(res->data);
    //res->data = NULL;
    hashtab_remove(restab, res->filename);
    free(res->filename);
    free(res);
}


struct resource_t *server_resource_get(resid_t resid)
{
    int i;
    struct resource_t *res;
    struct hashtab_item_t *hitem;

    if(!resid)
    {
        return NULL;
    }

    for(i = 0; i < restab->count; i++)
    {
        hitem = restab->items[i];

        while(hitem)
        {
            res = (struct resource_t *)hitem->val;

            if(res->resid == resid)
            {
                return res;
            }

            hitem = hitem->next;
        }
    }
    
    return NULL;
}


struct resource_t *server_resource_load(char *filename)
{
    char *ext;
    struct resource_t *res = NULL;
    struct hashtab_item_t *hitem;

    if(!filename || !*filename)
    {
        return NULL;
    }

    // check for an existing resource
    if((hitem = hashtab_lookup(restab, filename)))
    {
        if((res = (struct resource_t *)hitem->val))
        {
            res->refs++;
            return res;
        }
    }

    // resource needs to be loaded - check the file extension to determine
    // how to load
    ext = file_extension(filename);
    
    if(strcasecmp(ext, ".png") == 0)
    {
        struct bitmap32_t *img;

        if(!(img = server_png_resource_load(filename)))
        {
            return NULL;
        }
        
        if(!(res = server_resource_create_struct(RESOURCE_IMAGE, filename,
                                                 img, server_image_resource_free)))
        {
            server_image_resource_free(img);
            return NULL;
        }
    }
    else if(strcasecmp(ext, ".ico") == 0)
    {
        struct bitmap32_array_t *imga;

        if(!(imga = server_ico_resource_load(filename)))
        {
            return NULL;
        }
        
        if(!(res = 
            server_resource_create_struct(RESOURCE_IMAGE_ARRAY, 
                                       filename, imga, 
                                       server_image_array_resource_free)))
        {
            server_image_array_resource_free(imga);
            return NULL;
        }
    }
    else if(strcasecmp(ext, ".sysicon") == 0)
    {
        // sysicon resources do not exist as .sysicon files, instead we read
        // them from the system icon library "sysicons.icolib"
        struct bitmap32_array_t *imga;

        // remove the extension
        *ext = '\0';

        // load the icon
        if(!(imga = server_sysicon_resource_load(filename)))
        {
            return NULL;
        }

        // restore the file extension
        *ext = '.';

        if(!(res = 
                server_resource_create_struct(RESOURCE_IMAGE_ARRAY,
                                           filename, imga, 
                                           server_image_array_resource_free)))
        {
            server_image_array_resource_free(imga);
            return NULL;
        }
    }
    else
    {
        // unknown resource - do nothing for now
        return NULL;
    }

    hashtab_add(restab, res->filename, res);
    res->resid = next_resid++;

    return res;
}


struct bitmap32_t *dup_bitmap_struct(struct bitmap32_t *bitmap)
{
    struct bitmap32_t *copy;
    
    if(!(copy = (struct bitmap32_t *)malloc(sizeof(struct bitmap32_t))))
    {
        return NULL;
    }
    
    A_memcpy(copy, bitmap, sizeof(struct bitmap32_t));
    
    return copy;
}


void server_image_resource_free(void *raw)
{
    struct bitmap32_t *img;

    if(!(img = (struct bitmap32_t *)raw))
    {
        return;
    }

    // free the loaded image
    if(img->data)
    {
        free(img->data);
        //img->data = NULL;
    }
    
    free(img);
}


void server_font_resource_free(void *raw)
{
    struct font_t *font;

    if(!(font = (struct font_t *)raw))
    {
        return;
    }

    // free the loaded font
    if(!(font->flags & FONT_FLAG_SYSTEM_FONT))
    {
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
        free(font);
    }
}


void server_image_array_resource_free(void *raw)
{
    struct bitmap32_array_t *ba;

    if(!(ba = (struct bitmap32_array_t *)raw))
    {
        return;
    }
    
    bitmap32_array_free(ba);
}


struct bitmap32_t *server_png_resource_load(char *filename)
{
    struct bitmap32_t tmp, *copy;

    if(!png_load(filename, &tmp))
    {
        return NULL;
    }
    
    if(!(copy = dup_bitmap_struct(&tmp)))
    {
        // free the loaded image
        if(tmp.data)
        {
            free(tmp.data);
        }

        return NULL;
    }
    
    return copy;
}


struct resource_t *server_load_image_from_memory(unsigned int w, 
                                                 unsigned int h,
                                                 uint32_t *data, size_t datasz)
{
    struct resource_t *res;
    struct bitmap32_t *img;
    char buf[32];
    uint32_t resid;
    uint32_t *data_copy;
    
    if(!data || !w || !h || !datasz)
    {
        return NULL;
    }
    
    if(datasz != (w * 4 * h))
    {
        return NULL;
    }

    if(!(img = (struct bitmap32_t *)malloc(sizeof(struct bitmap32_t))))
    {
        return NULL;
    }
    
    if(!(data_copy = malloc(datasz)))
    {
        free(img);
        return NULL;
    }

    memcpy(data_copy, data, datasz);
    img->data = data_copy;
    img->width = w;
    img->height = h;

    // Resources are usually identified by their filenames. As this resource
    // is a memory resource, we will fake a name using the new resource id
    resid = next_resid++;
    sprintf(buf, "Resource%d", resid);

    if(!(res = server_resource_create_struct(RESOURCE_IMAGE, buf,
                                             img, server_image_resource_free)))
    {
        server_image_resource_free(img);
        return NULL;
    }

    hashtab_add(restab, res->filename, res);
    res->resid = resid;

    return res;
}


struct bitmap32_array_t *server_ico_resource_load(char *filename)
{
    return ico_load(filename);
}


static void __bmp_loaded_event(int fd, winid_t dest, struct bitmap32_t *bmp,
                               resid_t resid, uint32_t seqid, int sizeonly)
{
    size_t bmpsz = bmp->width * 4 * bmp->height;
    size_t bufsz = sizeof(struct event_res_t) + (sizeonly ? 0 : bmpsz);
    struct event_res_t *evbuf = malloc(bufsz + 1);

    if(!evbuf)
    {
        return;
    }

    A_memset(evbuf, 0, bufsz);

    if(!sizeonly)
    {
        A_memcpy(evbuf->data, bmp->data, bmpsz);
    }

    evbuf->type = EVENT_RESOURCE_LOADED;
    evbuf->seqid = seqid;
    evbuf->datasz = bmpsz;
    evbuf->src = TO_WINID(GLOB.mypid, 0);
    evbuf->dest = dest;
    evbuf->restype = RESOURCE_TYPE_IMAGE;
    evbuf->resid = resid;
    evbuf->img.w = bmp->width;
    evbuf->img.h = bmp->height;
    evbuf->valid_reply = 1;
    write(fd, evbuf, bufsz);

    free(evbuf);
}


/*
 * For monospace fonts, we send the font data in the reply payload.
 * For TTF fonts, we send the shared memory identifier (shmid) associated
 * with the font file, so the caller program can attach the loaded file to
 * itself and load it using the FreeType library.
 */
static void __font_loaded_event(int fd, winid_t dest, struct font_t *font,
                                resid_t resid, uint32_t seqid)
{
    size_t datasz = font->datasz;
    int is_ttf = !!(font->flags & FONT_FLAG_TRUE_TYPE);
    size_t bufsz = sizeof(struct event_res_t) + (is_ttf ? 0 : datasz);
    struct event_res_t *evbuf = malloc(bufsz + 1);

    if(!evbuf)
    {
        return;
    }

    A_memset(evbuf, 0, bufsz);

    if(!is_ttf)
    {
        A_memcpy(evbuf->data, font->data, datasz);
    }

    evbuf->type = EVENT_RESOURCE_LOADED;
    evbuf->seqid = seqid;
    evbuf->datasz = datasz;
    evbuf->src = TO_WINID(GLOB.mypid, 0);
    evbuf->dest = dest;
    evbuf->restype = RESOURCE_TYPE_FONT;
    evbuf->resid = resid;
    evbuf->font.charw = font->charw;
    evbuf->font.charh = font->charh;
    evbuf->font.is_ttf = is_ttf;
    evbuf->font.shmid = font->shmid;    // 0 for mono, non-zero for TTF
    evbuf->valid_reply = 1;
    write(fd, evbuf, bufsz);

    free(evbuf);
}


void send_res_load_event(int clientfd, struct event_res_t *evres,
                         struct resource_t *res)
{
    /*
     * For image resources (and system icons):
     *
     * If the resource is a single image (PNG, JPG, etc.), notify the
     * client if (a) the image resolution matches what the client wants,
     * or (b) the client did not give any preferences.
     *
     * If the resource contains more than one image (e.g. ICO files),
     * choose either (a) the resolution that matches what the client wants,
     * or (b) the highest resolution if the client did not give any preferences.
     *
     * For other resources: TODO // //.
     */
    int sizeonly = (evres->restype & RESOURCE_TYPE_SIZEONLY);

    evres->restype &= ~RESOURCE_TYPE_SIZEONLY;

    if(evres->restype == RESOURCE_TYPE_IMAGE ||
       evres->restype == RESOURCE_TYPE_SYSICON)
    {
        uint32_t w = evres->img.w;
        uint32_t h = evres->img.h;

        if(res->type == RESOURCE_IMAGE)
        {
            struct bitmap32_t *bmp = (struct bitmap32_t *)res->data;

            //if((w == 0 || bmp->width == w) && (h == 0 || bmp->height == h))
            {
                __bmp_loaded_event(clientfd, evres->src,
                                   bmp, res->resid, evres->seqid, sizeonly);
                return;
            }
        }
        else if(res->type == RESOURCE_IMAGE_ARRAY)
        {
            int i, j;
            struct bitmap32_array_t *ba = (struct bitmap32_array_t *)res->data;

            if(w && h)
            {
                for(i = 0; i < ba->count; i++)
                {
                    if(ba->bitmaps[i].width == w && ba->bitmaps[i].height == h)
                    {
                        __bmp_loaded_event(clientfd, evres->src,
                                           &ba->bitmaps[i],
                                           res->resid, evres->seqid, sizeonly);
                        return;
                    }
                }
            }

            // find the highest resolution
            w = 0;
            h = 0;
            j = 0;

            for(i = 0; i < ba->count; i++)
            {
                if(ba->bitmaps[i].width > w || ba->bitmaps[i].height > h)
                {
                    j = i;
                    w = ba->bitmaps[i].width;
                    h = ba->bitmaps[i].height;
                }
            }

            __bmp_loaded_event(clientfd, evres->src,
                               &ba->bitmaps[j],
                               res->resid, evres->seqid, sizeonly);
            return;
        }
        else
        {
        }
    }
    else if(evres->restype == RESOURCE_TYPE_FONT)
    {
        __font_loaded_event(clientfd, evres->src, (struct font_t *)res->data,
                                      res->resid, evres->seqid);
        return;
    }
    else
    {
    }

    //server_resource_free(res);
    send_err_event(clientfd, evres->src, EVENT_RESOURCE_LOADED, EINVAL, evres->seqid);
}


void server_resource_unload(resid_t resid)
{
    int i;
    struct hashtab_item_t *hitem;
    struct resource_t *res;

    if(!restab)
    {
        return;
    }

    for(i = 0; i < restab->count; i++)
    {
        hitem = restab->items[i];

        while(hitem)
        {
            res = (struct resource_t *)hitem->val;

            if(res->resid == resid)
            {
                server_resource_free(res);
                return;
            }

            hitem = hitem->next;
        }
    }
}
