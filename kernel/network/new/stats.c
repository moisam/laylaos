/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: stats.c
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
 *  \file stats.c
 *
 *  Network statistics.
 */

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/user.h>
#include <kernel/net/stats.h>

struct netstats netstats = { 0, };


int get_netstats(struct netstats *ns)
{
    struct netstats tmp;
    
    if(!ns)
    {
        return -EINVAL;
    }
    
    A_memcpy(&tmp, &netstats, sizeof(struct netstats));
    return copy_to_user(ns, &tmp, sizeof(struct netstats));
}


void stats_init(void)
{
    static int inited = 0;
    
    if(inited)
    {
        return;
    }
    
    inited = 1;
    A_memset(&netstats, 0, sizeof(struct netstats));
}

