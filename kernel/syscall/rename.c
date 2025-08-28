/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: rename.c
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
 *  \file rename.c
 *
 *  Functions for renaming files.
 */

#include <errno.h>
#include <fcntl.h>
#include <kernel/vfs.h>
#include <kernel/syscall.h>


#define open_flags          (OPEN_USER_CALLER | OPEN_NOFOLLOW_SYMLINK)

/*
 * Handler for syscall renameat().
 *
 * TODO: this is a very lazy implementation of rename, which should be fixed
 *       to properly handle the different case scenarios listed in the link 
 *       below.
 *
 * See: https://man7.org/linux/man-pages/man2/rename.2.html
 */
long syscall_renameat(int olddirfd, char *oldpath, int newdirfd, char *newpath)
{
    struct fs_node_t *oldnode, *newnode;
    long res;
    int link_flags = 0;

    // check old file existence
    if((res = vfs_open_internal(oldpath, olddirfd, &oldnode, open_flags)) < 0)
    {
        return -ENOENT;
    }

    res = vfs_open_internal(newpath, newdirfd, &newnode, open_flags);

    // make sure both files do not point to the same inode, in which
    // case we do not do anything and return success
    if(newnode && newnode->dev == oldnode->dev && 
                  newnode->inode == oldnode->inode)
    {
        release_node(newnode);
        release_node(oldnode);
        return 0;
    }

    if(S_ISDIR(oldnode->mode))
    {
        link_flags = OPEN_RENAME_DIR;

        // if oldpath is a directory, newpath must either not exist or refer
        // to an empty directory
        if(newnode)
        {
            if(!S_ISDIR(newnode->mode))
            {
                release_node(newnode);
                release_node(oldnode);
                return -ENOTDIR;
            }

            if(newnode->ops && newnode->ops->dir_empty)
            {
                if(!newnode->ops->dir_empty(newnode))
                {
                    release_node(newnode);
                    release_node(oldnode);
                    return -ENOTEMPTY;
                }
            }

            release_node(newnode);
            //newnode = NULL;

            // remove the new dir so it can be overwritten
            if((res = vfs_rmdir(newdirfd, newpath, 0)) < 0)
            {
                release_node(oldnode);
                return res;
            }
        }
    }
    else
    {
        if(S_ISLNK(oldnode->mode))
        {
            link_flags = OPEN_RENAME_LINK;
        }

        // check new file existence
        if(newnode)
        {
            release_node(newnode);

            // remove the new file so it can be overwritten
            if((res = vfs_unlinkat(newdirfd, newpath, 0)) < 0)
            {
                release_node(oldnode);
                return res;
            }
        }
    }

    // now create a new link and remove the old file
    if((res = vfs_linkat(olddirfd, oldpath, newdirfd, newpath, link_flags)) == 0)
    {
        // TODO: we should unlink the newly created file or dir if this step
        //       fails for whatever reason
        res = (link_flags == OPEN_RENAME_DIR) ? 
                           vfs_rmdir(olddirfd, oldpath, link_flags) :
                           vfs_unlinkat(olddirfd, oldpath, 0);
    }

    release_node(oldnode);
    
    return res;
}

#undef open_flags


/*
 * Handler for syscall rename().
 */
long syscall_rename(char *oldpath, char *newpath)
{
    return syscall_renameat(AT_FDCWD, oldpath, AT_FDCWD, newpath);
}

