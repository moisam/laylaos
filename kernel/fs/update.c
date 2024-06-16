/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: update.c
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
 *  \file update.c
 *
 *  Disk updater function. Part of the Virtual Filesystem (VFS).
 */

#define __DEBUG

#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/mutex.h>
#include <kernel/pcache.h>


void update(dev_t dev)
{
    static int updating = 0;
    
    //return;

    KDEBUG("update: starting\n");

    /* another update is on way */
    if(updating)
    {
        return;
    }
    
    updating = 1;

    KDEBUG("update: syncing super blocks\n");

    /* 1- update the mounted volumes */
    sync_super(dev);

    KDEBUG("update: syncing inodes\n");

    /* 2- update the modified inodes */
    sync_nodes(dev);

    /* allow other "updates" to continue */
    updating = 0;

    KDEBUG("update: syncing cached pages\n");

    /* 3- forcefully flush any pending "delayed write" blocks */
    flush_cached_pages(dev);
    
    KDEBUG("update: done\n");
}

