/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: clipboard.c
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
 *  \file clipboard.c
 *
 *  Functions to work with the clipboard on the server side.
 */

#include <stdlib.h>
#define GUI_SERVER
#include "../include/clipboard.h"

struct clipboard_data_t
{
    int format;
    size_t datasz;
    void *data;
};

static struct clipboard_data_t clipboard[CLIPBOARD_FORMAT_COUNT + 1] = { 0, };


size_t server_clipboard_set(struct event_res_t *evres)
{
    void *copy;
    
    if(!evres || evres->clipboard.fmt != CLIPBOARD_FORMAT_TEXT)
    {
        return 0;
    }
    
    if(!(copy = malloc(evres->datasz)))
    {
        return 0;
    }
    
    A_memcpy(copy, evres->data, evres->datasz);
    
    if(clipboard[evres->clipboard.fmt].data)
    {
        free(clipboard[evres->clipboard.fmt].data);
    }
    
    clipboard[evres->clipboard.fmt].data = copy;
    clipboard[evres->clipboard.fmt].datasz = evres->datasz;
    
    return evres->datasz;
}


void *server_clipboard_get(int format, size_t *datasz)
{
    if(!datasz || format != CLIPBOARD_FORMAT_TEXT)
    {
        *datasz = 0;
        return NULL;
    }

    *datasz = clipboard[format].datasz;
    return clipboard[format].data;
}


size_t server_clipboard_query_size(int format)
{
    if(format != CLIPBOARD_FORMAT_TEXT)
    {
        return 0;
    }

    return clipboard[format].datasz;
}

