/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <mm/kheap.h>
#include "dirent.h"


DIR *opendir(const char *name)
{
    int open_flags = OPEN_KERNEL_CALLER | OPEN_CREATE_DENTRY;
    int flags = O_RDONLY | O_DIRECTORY | O_CLOEXEC;
    struct fs_node_t *node;
    DIR *dirp;

    if(vfs_open((char *)name, flags, 0555, AT_FDCWD, &node, open_flags) != 0)
    {
        return NULL;
    }

    if((dirp = (DIR *)kmalloc(sizeof(DIR))) == NULL)
    {
        release_node(node);
        return NULL;
    }

    /*
     * If CLSIZE is an exact multiple of DIRBLKSIZ, use a CLSIZE
     * buffer that it cluster boundary aligned.
     * Hopefully this can be a big win someday by allowing page trades
     * to user space to be done by getdirentries()
     */
    dirp->dd_buf = kmalloc(512);
    dirp->dd_len = 512;

    if(dirp->dd_buf == NULL)
    {
        kfree(dirp);
        release_node(node);
        return NULL;
    }

    dirp->dd_node = node;
    dirp->dd_loc = 0;
    dirp->dd_seek = 0;
    dirp->dd_fpos = 0;

    return dirp;
}


int getdents(DIR *dirp)
{
    if(dirp->dd_fpos >= (off_t)dirp->dd_node->size)
    {
        return 0;
    }

    return vfs_getdents(dirp->dd_node, &dirp->dd_fpos,
                        dirp->dd_buf, dirp->dd_len);
}


/*
 * get next entry in a directory.
 */
struct dirent *readdir(DIR *dirp)
{
    register struct dirent *dp;
 
    for(;;)
    {
        if(dirp->dd_loc == 0)
        {
            dirp->dd_size = getdents(dirp);
      
            if(dirp->dd_size <= 0)
            {
                return NULL;
            }
        }

        if(dirp->dd_loc >= dirp->dd_size)
        {
            dirp->dd_loc = 0;
            continue;
        }

        dp = (struct dirent *)(dirp->dd_buf + dirp->dd_loc);
    
        if(dp->d_reclen <= 0 ||
           dp->d_reclen > dirp->dd_len + 1 - dirp->dd_loc)
        {
            return NULL;
        }

        dirp->dd_loc += dp->d_reclen;

        if(dp->d_ino == 0)
        {
            continue;
        }

        return (dp);
    }
}


/*
 * close a directory.
 */
int closedir(DIR *dirp)
{
    //int rc;

    release_node(dirp->dd_node);
    //_cleanupdir(dirp);
    kfree((void *)dirp->dd_buf);
    kfree((void *)dirp);
    //return rc;
    return 0;
}

