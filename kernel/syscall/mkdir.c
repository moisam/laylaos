/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: mkdir.c
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
 *  \file mkdir.c
 *
 *  Functions for creating empty directories.
 */

#include <errno.h>
#include <fcntl.h>
#include <kernel/syscall.h>
#include <kernel/clock.h>
#include <kernel/pcache.h>
#include <mm/kheap.h>


/*
 * Handler for syscall mkdir().
 */
int syscall_mkdir(char *pathname, mode_t mode)
{
    return syscall_mkdirat(AT_FDCWD, pathname, mode);
}


/*
 * Handler for syscall mkdirat().
 */
int syscall_mkdirat(int dirfd, char *pathname, mode_t mode)
{
    int res;
	char *filename = NULL;
    struct fs_node_t *dnode = NULL, *fnode = NULL;
    struct dirent *entry = NULL;
    struct cached_page_t *dbuf = NULL;
    //struct IO_buffer_s *dbuf = NULL;
    size_t dbuf_off;
    char *name2 = path_remove_trailing_slash(pathname, 0, NULL);
    struct task_t *ct = cur_task;
    struct mount_info_t *dinfo;
    
    if(!name2)
    {
        return -ENOMEM;
    }

	// get the parent dir of the new file
    if((res = get_parent_dir(name2, dirfd, &filename, &dnode, 1)) < 0)
    {
	    kfree(name2);
		return res;
    }
    
    // can't create sys root
    if(!*filename)
    {
        res = -ENOENT;
        goto error;
    }
    
    // check write permission to parent dir
	if(has_access(dnode, WRITE, 0) != 0)
	{
        res = -EACCES;
        goto error;
	}

    // can't mkdir if the filesystem was mount readonly
    if((dinfo = get_mount_info(dnode->dev)) && (dinfo->mountflags & MS_RDONLY))
    {
        res = -EROFS;
        goto error;
    }
    
    // check if the new dir already exists
    if(vfs_finddir(dnode, filename, &entry, &dbuf, &dbuf_off) == 0)
    {
        release_cached_page(dbuf);
        //brelse(dbuf);
        kfree(entry);
        res = -EEXIST;
        goto error;
    }
    
    // create a new file node
    if(!(fnode = new_node(dnode->dev)))
    {
        res = -ENOSPC;
        goto error;
    }
    
    if(dnode->ops == NULL || dnode->ops->mkdir == NULL)
    {
        res = -EPERM;
        goto error2;
    }
    
    // update the dir's access times
    time_t t = now();
    //fnode->atime = t;
    fnode->mtime = t;
    fnode->ctime = t;
	fnode->mode = S_IFDIR | (mode & 0777 & ~ct->fs->umask);
    fnode->flags |= FS_NODE_DIRTY;
    update_atime(fnode);

    // if the parent directory has its SGID bit set, the new file inherits
    // the parent's gid, otherwise it uses the calling task's egid (the
    // latter case is done in the new_node() call above).
    if(dnode->mode & S_ISGID)
    {
        fnode->gid = dnode->gid;
        fnode->mode |= S_ISGID;
    }

    if((res = dnode->ops->mkdir(fnode, dnode->inode)) < 0)
    {
        goto error2;
    }
    
    // add the new directory to the parent directory
    if((res = vfs_addir(dnode, filename, fnode->inode)) < 0)
    {
        truncate_node(fnode, 0);
        goto error2;
    }
    
    // make sure the new dir has the right link count
    if(fnode->links < 2)
    {
        fnode->links = 2;
    }

    dnode->links++;
    //dnode->atime = t;
    dnode->mtime = t;
    dnode->flags |= FS_NODE_DIRTY;
    update_atime(dnode);

    kfree(name2);
    release_node(fnode);
    release_node(dnode);

	return 0;

error2:
    fnode->links = 0;
    release_node(fnode);

error:
    kfree(name2);
    release_node(dnode);
    return res;
}

