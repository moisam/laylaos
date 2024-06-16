/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: misc.c
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
 *  \file misc.c
 *
 *  Miscellaneous utility functions not fitting anywhere else.
 */

#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#define GIGABYTE                        (1024 * 1024 * 1024)
#define MEGABYTE                        (1024 * 1024)
#define KILOBYTE                        (1024)

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


void stringify_file_size(char *buf, off_t file_size)
{
    if(file_size >= GIGABYTE)
    {
        sprintf(buf, "%.1f GiB", (double)file_size / GIGABYTE);
    }
    else if(file_size >= MEGABYTE)
    {
        sprintf(buf, "%.1f MiB", (double)file_size / MEGABYTE);
    }
    else if(file_size >= KILOBYTE)
    {
        sprintf(buf, "%.1f KiB", (double)file_size / KILOBYTE);
    }
    else
    {
        sprintf(buf, "%ld B", file_size);
    }
}

