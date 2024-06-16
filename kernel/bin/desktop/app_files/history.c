/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: history.c
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
 *  \file history.c
 *
 *  Functions to work with history entries when navigating the filesystem
 *  using the files application.
 */

#include <stdlib.h>
#include <string.h>

#define HISTORY_COUNT           1024

static char *history[HISTORY_COUNT];

static int hist_current = -1, hist_last = -1;


void history_push(char *path)
{
    int i;
    
    hist_current++;
    
    if(hist_current >= HISTORY_COUNT)
    {
        free(history[0]);
        
        for(i = 0; i < HISTORY_COUNT - 1; i++)
        {
            history[i] = history[i + 1];
        }
        
        history[i] = NULL;
        //memcpy(history, &history[1], (HISTORY_COUNT - 1) * sizeof(char *));
        hist_current--;
        hist_last = hist_current;
    }
    
    if(hist_current != hist_last)
    {
        for(i = hist_current; i < hist_last; i++)
        {
            history[i] = history[i + 1];
        }

        history[i] = NULL;
    }
    
    if(history[hist_current])
    {
        free(history[hist_current]);
        history[hist_current] = NULL;
    }
    
    history[hist_current] = strdup(path);
    hist_last = hist_current;
}


char *history_back(void)
{
    char *res;
    
    if(hist_current <= 0)
    {
        return NULL;
    }
    
    res = history[--hist_current];
    
    return res;
}


char *history_forward(void)
{
    char *res;
    
    if(hist_current >= hist_last)
    {
        return NULL;
    }
    
    res = history[++hist_current];
    
    return res;
}


int get_history_current(void)
{
    return hist_current;
}


int get_history_last(void)
{
    return hist_last;
}

