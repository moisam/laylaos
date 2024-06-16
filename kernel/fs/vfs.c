/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: vfs.c
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
 *  \file vfs.c
 *
 *  The kernel's Virtual Filesystem (VFS) implementation.
 */

//#define __DEBUG

#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/clock.h>
#include <kernel/mutex.h>
#include <kernel/dev.h>
#include <kernel/syscall.h>
#include <kernel/ksignal.h>
#include <mm/kheap.h>
#include <fs/pipefs.h>
#include <fs/sockfs.h>
#include <fs/procfs.h>
#include <fs/dummy.h>


struct file_t ftab[NR_FILE];


/*
 * Update node's atime.
 */
void update_atime(struct fs_node_t *node)
{
    struct mount_info_t *dinfo;
    time_t t = now();
    
    if(!node)
    {
        return;
    }
    
    if(!(dinfo = node_mount_info(node)))
    //if(!(dinfo = get_mount_info(node->dev)))
    {
        node->atime = t;
        node->flags |= FS_NODE_DIRTY;
        return;
    }

    if((dinfo->mountflags & MS_NOATIME))
    {
        return;
    }

    if(S_ISDIR(node->mode) && (dinfo->mountflags & MS_NODIRATIME))
    {
        return;
    }

    node->atime = t;
    node->flags |= FS_NODE_DIRTY;
}


/*
 * Get a kalloc()'d copy of the path and remove any trailing'/'s.
 * Used to sanitize pathnames we pass to get_parent_dir().
 *
 * Returns:
 *     - pointer to the kalloc'd copy on success, NULL on failure.
 *     - trailing_slash is set to 1 if the path ends in a slash and
 *       trailing_slash is non-NULL.
 */
char *path_remove_trailing_slash(char *path, int kernel, int *trailing_slash)
{
    //printk("path_remove_trailing_slash: path %s\n", path);

    if(!path)
    {
        return NULL;
    }
    
    /*
     * NOTE: strlen() looks at user memory before we fully validate the given
     *       pointer (we verify the pointer itself is valid but not the whole
     *       string).
     * TODO: fix this.
     */
    
    if(!kernel && 
       valid_addr(cur_task, (virtual_addr)path, (virtual_addr)path + 1) != 0)
    {
        add_task_segv_signal(cur_task, SIGSEGV, SEGV_MAPERR, (void *)path);
        return NULL;
    }

    size_t pathlen = strlen(path);
    char *p2 = (char *)kmalloc(pathlen + 1);
    char *p3;
    int tmp = 0;
    
    if(!p2)
    {
        return NULL;
    }
    
    if(kernel)
    {
        A_memcpy(p2, path, pathlen + 1);
    }
    else if(copy_from_user(p2, path, pathlen + 1) != 0)
    {
        kfree(p2);
        return NULL;
    }
    
    p3 = p2 + pathlen;
    
    while(--p3 > p2)
    {
        if(*p3 != '/')
        {
            break;
        }
        
        *p3 = '\0';
        tmp = 1;
    }

    if(trailing_slash)
    {
        *trailing_slash = tmp;
    }
    
    return p2;
}


/*
 * Get the node of the parent directory for the given path. We don't get the
 * requested file directly, as we might need to create it, in which case we
 * need access to the parent directory.
 *
 * NOTE: path should NOT end in '/'. The caller has the responsibility to
 * ensure that, otherwise the returned node will be of the base file, NOT
 * the parent directory!
 *
 * Returns:
 * 0 on success, -errno on failure
 * filename: pointer to the first char in the basename of the requested path
 * dirnode: pointer to the parent directory's node
 */
int get_parent_dir(char *pathname, int dirfd, char **filename,
                   struct fs_node_t **dirnode, int follow_mpoints)
{
    char *fname, *tmp;
    struct fs_node_t *node, *node2, *parent;
    int len, res;
    dev_t dev;
    ino_t n;
    struct dirent *entry;
    struct cached_page_t *dbuf;
    size_t dbuf_off;
    int symlinks = 0;
    struct task_t *ct = cur_task;
    
    if(!pathname || !*pathname)
    {
        return -EINVAL;
    }
    
    if(!ct)
    {
        return -EINVAL;
    }
    
    // for safety
    *filename = NULL;
    *dirnode = NULL;
    
    if(*pathname == '/')
    {
        if(!ct->fs || !ct->fs->root || ct->fs->root->refs == 0)
        {
            // Kernel tasks do not have valid cwd or root entries, and they
            // should not usually be accessing files/dirs except in some 
            // cases, e.g. when the CD-ROM task is trying to auto-mount a
            // removable disk
            if(ct->user)
            {
                printk("vfs: current task has no root directory!\n");
                return -EINVAL;
            }

            node = system_root_node;
        }
        else
        {
            node = ct->fs->root;
        }

        pathname++;
    }
    else if(dirfd != AT_FDCWD)
    {
        if(!ct->ofiles || dirfd < 0 || dirfd >= NR_OPEN ||
           ct->ofiles->ofile[dirfd] == NULL ||
           (node = ct->ofiles->ofile[dirfd]->node) == NULL)
        {
            return -EBADF;
        }

        if(!S_ISDIR(node->mode) || has_access(node, EXECUTE, 0) != 0)
        {
            return -EPERM;
        }
    }
    else
    {
        if(!ct->fs || !ct->fs->cwd || ct->fs->cwd->refs == 0)
        {
            printk("vfs: current task has no cwd!\n");
            return -EINVAL;
        }

        node = ct->fs->cwd;
    }
    
    if(!(node = get_node(node->dev, node->inode, follow_mpoints)))
    {
        printk("vfs: failed to get current task's cwd/root!\n");
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        return -EINVAL;
    }

    parent = node;

    KDEBUG("get_parent_dir: dev 0x%x, ino 0x%x\n", node->dev, node->inode);

    INC_NODE_REFS(node);
    
    while(1)
    {
        KDEBUG("get_parent_dir: pathname = %s\n", pathname);

        // if it's a symbolic link, follow it and count the symlinks
        if(S_ISLNK(node->mode))
        {
            if(++symlinks >= MAXSYMLINKS)
            {
                release_node(node);
                release_node(parent);
                return -ELOOP;
            }
        
            if((res = follow_symlink(node, parent, O_RDONLY, &node2)) < 0)
            {
                release_node(node);
                release_node(parent);
                return res;
            }
        
            release_node(node);
            node = node2;
        }

        release_node(parent);
        parent = node;

        KDEBUG("get_parent_dir: dev 0x%x, ino 0x%x\n", node->dev, node->inode);

        INC_NODE_REFS(node);
        
        while(*pathname == '/')
        {
            pathname++;
        }
        
        fname = pathname;

        KDEBUG("get_parent_dir: fname = %s\n", fname);
        
        if(!S_ISDIR(node->mode) || has_access(node, EXECUTE, 0) != 0)
        {
            release_node(node);
            release_node(parent);
            return -EPERM;
        }
        
        for(len = 0; *pathname && *pathname != '/'; pathname++)
        {
            len++;
        }
        
        // end of path
        if(!*pathname)
        {
            KDEBUG("get_parent_dir: final fname = %s\n", fname);

            (*filename) = fname;
            (*dirnode) = node;
            release_node(parent);
            return 0;
        }
        
        // get a local copy of this path segment
        if(!(tmp = (char *)kmalloc(len + 1)))
        {
            release_node(node);
            release_node(parent);
            return -ENOMEM;
        }
        
        A_memcpy(tmp, fname, len);
        tmp[len] = '\0';

        KDEBUG("get_parent_dir: tmp = %s\n", tmp);
        
        // find this path segment in the current directory
        if((res = vfs_finddir(node, tmp, &entry, &dbuf, &dbuf_off)) < 0)
        {
            kfree(tmp);
            release_node(node);
            release_node(parent);
            return res;
        }
        
        release_cached_page(dbuf);
        //brelse(dbuf);
        dev = node->dev;
        n = entry->d_ino;
        kfree(tmp);
        kfree(entry);
        release_node(node);

        KDEBUG("filename @ 0x%x\n", filename);
        
        // get the node
        if(!(node = get_node(dev, n, follow_mpoints)))
        {
            return -ENOENT;
        }
    }
}


/*
 * Set the node's select() function according to the file type.
 */
static void set_select_func(struct fs_node_t *node)
{
    dev_t dev;
    int major;
    mode_t mode = node->mode;
    
    node->select = NULL;
    node->poll = NULL;
    node->read = NULL;
    node->write = NULL;
    
    if(IS_PIPE(node))
    {
        node->select = pipefs_select;
        node->poll = pipefs_poll;

        node->read = pipefs_read;
        node->write = pipefs_write;
    }
    else if(node->flags & FS_NODE_SOCKET)
    {
        node->select = sockfs_select;
        node->poll = sockfs_poll;

        node->read = sockfs_read;
        node->write = sockfs_write;
    }
    else if(S_ISCHR(mode) || S_ISBLK(mode))
    {
        dev = node->blocks[0];
        major = MAJOR(dev);
    
        if(major < NR_DEV)
        {
            int (*select)(struct file_t *, int) =
                            S_ISCHR(mode) ? cdev_tab[major].select :
                                            bdev_tab[major].select;
            int (*poll)(struct file_t *, struct pollfd *) =
                            S_ISCHR(mode) ? cdev_tab[major].poll :
                                            bdev_tab[major].poll;

            node->select = select;
            node->poll = poll;

            if(S_ISCHR(mode))
            {
                node->write = cdev_tab[major].write ? cdev_tab[major].write :
                                                      dummyfs_write;
                node->read  = cdev_tab[major].read  ? cdev_tab[major].read  :
                                                      dummyfs_read;
                //node->read = chr_read;
                //node->write = chr_write;
            }
            else
            {
                node->read = block_read;
                node->write = block_write;
            }
        }
    }
    
    if(!node->select)
    {
        node->select = dummyfs_select;
    }

    if(!node->poll)
    {
        node->poll = dummyfs_poll;
    }

    if(!node->read)
    {
        node->read = vfs_read;
    }

    if(!node->write)
    {
        node->write = vfs_write;
    }
}


int vfs_open_internal(char *path, int dirfd, 
                      struct fs_node_t **filenode, int open_flags)
{
    int res, trailing_slash;
    int followlink = (open_flags & OPEN_FOLLOW_SYMLINK);
    int kernel = (open_flags & OPEN_KERNEL_CALLER);
    struct fs_node_t *node, *node2, *parent;
    dev_t dev;
    ino_t n;
    struct dirent *entry;
    char *filename;
    char *p2 = path_remove_trailing_slash(path, kernel, &trailing_slash);
    struct cached_page_t *dbuf;
    size_t dbuf_off;
    
    *filenode = NULL;
    
    if(!p2)
    {
        return -ENOMEM;
    }
    
    if((res = get_parent_dir(p2, dirfd, &filename, &node, 1)) < 0)
    {
        kfree(p2);
        return res;
    }

    // this indicates root '/'
    if(!*filename)
    {
        KDEBUG("vfs_open_internal: node: dev 0x%x, n 0x%x\n", node->dev, node->inode);

        kfree(p2);
        *filenode = node;
        return 0;
    }

    // get the file entry
    if((res = vfs_finddir(node, filename, &entry, &dbuf, &dbuf_off)) < 0)
    {
        kfree(p2);
        release_node(node);
        return res;
    }
    
    release_cached_page(dbuf);

    // and the file's node
    dev = node->dev;
    n = entry->d_ino;
    kfree(entry);
    parent = node;

    KDEBUG("vfs_open_internal - 5 (path '%s', dev 0x%x, n 0x%x)\n", path, dev, n);

    // get the node
    if(!(node = get_node(dev, n, 1)))
    {
        kfree(p2);
        release_node(parent);
        return -ENOENT;
    }

    // if it's a symbolic link, follow it.
    // if path ends in /, force following the link.
    if(S_ISLNK(node->mode) && (followlink || trailing_slash))
    {
        if((res = follow_symlink(node, parent, O_RDONLY, &node2)) < 0)
        {
            release_node(node);
            release_node(parent);
            kfree(p2);
            return res;
        }
        
        release_node(node);
        node = node2;
    }
    
    // stat() et al. don't accept paths ending in '/' if the path is not
    // a directory
    if(!S_ISDIR(node->mode) && trailing_slash)
    {
        release_node(node);
        release_node(parent);
        kfree(p2);
        return -ENOTDIR;
    }

    if(S_ISSOCK(node->mode))
    {
        node->flags |= FS_NODE_SOCKET;
    }

    KDEBUG("vfs_open_internal: node: dev 0x%x, n 0x%x\n", node->dev, node->inode);

    if(open_flags & OPEN_CREATE_DENTRY)
    {
        create_file_dentry(parent, node, filename);
    }

    release_node(parent);
    kfree(p2);
    
    update_atime(node);

    *filenode = node;
    set_select_func(node);
    
    return 0;
}


/*
 * Open the file/dir with the given path, flags and access mode.
 *
 * Returns:
 *     0 on success, -errno on failure
 *     filenode: pointer to the file/dir's node
 */
int vfs_open(char *path, int flags, mode_t mode, int dirfd,
             struct fs_node_t **filenode, int open_flags)
{
    int res;
    char *filename;
    dev_t dev;
    ino_t n;
    struct fs_node_t *dnode, *fnode, *fnode2;
    struct dirent *entry;
    struct cached_page_t *dbuf;
    size_t dbuf_off;
    int follow_mpoints, rootdir;
    int kernel = (open_flags & OPEN_KERNEL_CALLER);
    struct task_t *ct = cur_task;

    *filenode = NULL;
    
    // add write access if truncate is requested without write/rw access
    if((flags & O_TRUNC) && !(flags & (O_WRONLY | O_RDWR)))
    {
        flags |= O_WRONLY;
    }
    
    // OPEN_NOFOLLOW_MPOINT is only set when vfs_mount() calls us, to ensure we
    // open the actual path and not follow the mountpoint to the mounted
    // filesystem's root node
    follow_mpoints = !(open_flags & OPEN_NOFOLLOW_MPOINT);
    
    mode = (mode & S_IFMT) | (mode & 0777 & ~ct->fs->umask);

    // if the file type == 0, it is a regular file
    if(!(mode & S_IFMT))
    {
        mode |= S_IFREG;
    }
    
    char *p2 = path_remove_trailing_slash(path, kernel, NULL);
    
    if(!p2)
    {
        return -ENOMEM;
    }

    KDEBUG("vfs_open: 1 - p2 %s\n", p2);

    rootdir = (p2[0] == '/' && p2[1] == '\0');
    
    // If vfs_mount() is trying to mount sysroot '/', we shouldn't follow the
    // mount point, as we will end up with the mounted filesystem's root 
    // directory. For all other opens, we follow mount points along the path 
    // to find the desired file's parent directory.
    
    if((res = get_parent_dir(p2, dirfd, &filename, &dnode, 
                             rootdir ? follow_mpoints : 1)) < 0)
    {
        kfree(p2);
        return res;
    }
    
    // this indicates root '/'
    if(!*filename)
    {
        kfree(p2);

        // can't create or truncate sys root
        if(!(flags & (O_CREAT | O_TRUNC)))
        {
            *filenode = dnode;
            return 0;
        }
        
        release_node(dnode);
        return -EISDIR;
    }
    
    KDEBUG("vfs_open: filename = '%s'\n", filename);

    // find the file in the parent directory
    if(vfs_finddir(dnode, filename, &entry, &dbuf, &dbuf_off) == 0)
    {
        release_cached_page(dbuf);
        dev = dnode->dev;
        n = entry->d_ino;

        kfree(entry);

        // get the file's node
        if(!(fnode = get_node(dev, n, follow_mpoints)))
        {
            // the file exists but we can't access it
            release_node(dnode);
            kfree(p2);
            return -EACCES;
        }

        // if it's a symbolic link, follow it if indicated
        if(S_ISLNK(fnode->mode))
        {
            // TODO: we should check O_PATH as well
            if(flags & O_NOFOLLOW)
            {
                release_node(dnode);
                kfree(p2);
                return -ELOOP;
            }
        
            if((res = follow_symlink(fnode, dnode, flags, &fnode2)) < 0)
            {
                release_node(dnode);
                kfree(p2);
                return res;
            }
        
            release_node(fnode);
            fnode = fnode2;
        }

        // request for exclusive opening fails if file exists
        if((flags & O_CREAT) && (flags & O_EXCL))
        {
            release_node(dnode);
            release_node(fnode);
            kfree(p2);
            return -EEXIST;
        }
        
        // also fail if O_DIRECTORY is set but the file isn't a directory
        if((flags & O_DIRECTORY) && !S_ISDIR(fnode->mode))
        {
            release_node(dnode);
            release_node(fnode);
            kfree(p2);
            return -ENOTDIR;
        }
        
        // do we have access permission to the file?
        int perm = (flags & O_RDWR) ? (WRITE | READ) :
                   (flags & O_WRONLY) ? WRITE : READ;
        
        if(has_access(fnode, perm, 0) != 0)
        {
            release_node(dnode);
            release_node(fnode);
            kfree(p2);
            return -EPERM;
        }
        
        // continue after the if-else block
    }
    else
    {
        // entry not found
        // didn't ask to create it
        if(!(flags & O_CREAT))
        {
            release_node(dnode);
            kfree(p2);
            return -ENOENT;
        }
        
        // do we have write permission to the parent directory?
        if((res = has_access(dnode, WRITE, 0)) != 0)
        {
            release_node(dnode);
            kfree(p2);
            return res;
        }
        
        // create a new file
        if(!(fnode = new_node(dnode->dev)))
        {
            release_node(dnode);
            kfree(p2);
            return -ENOSPC;
        }
        
        // mark it dirty, so that we'll update the disk even if we fail
        fnode->mode = mode;
        fnode->flags |= FS_NODE_DIRTY;
        
        // add the filename to the parent directory
        if((res = vfs_addir(dnode, filename, fnode->inode)) == 0)
        {
            // make sure we don't call truncate on a new, empty file!
            flags &= ~O_TRUNC;

            dnode->links++;
        
            // continue after the if-else block
        }
        else
        {
            fnode->links = 0;
            release_node(dnode);
            release_node(fnode);
            return res;
        }
        
        // if the parent directory has its SGID bit set, the new file inherits
        // the parent's gid, otherwise it uses the calling task's egid (the
        // latter case is done in the new_node() call above).
        if(dnode->mode & S_ISGID)
        {
            fnode->gid = dnode->gid;
        }
    }

    if(S_ISSOCK(fnode->mode))
    {
        fnode->flags |= FS_NODE_SOCKET;
    }

    if(open_flags & OPEN_CREATE_DENTRY)
    {
        KDEBUG("vfs_open: filename = '%s'\n", filename);
        create_file_dentry(dnode, fnode, filename);
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
    }
    
    kfree(p2);
    
    // update the dir and file's access time
    update_atime(dnode);
    update_atime(fnode);
    
    release_node(dnode);
    
    // truncate file if needed
    if(flags & O_TRUNC)
    {
        truncate_node(fnode, 0);
    }
    
    *filenode = fnode;
    set_select_func(fnode);

    KDEBUG("vfs_open: done\n");

    return 0;
}


/*
 * Find the file with the given filename in the parent directory represented
 * by the dir node.
 *
 * Inputs:
 *    dir => the parent directory's node
 *    filename => the searched-for filename
 *
 * Outputs:
 *   0 on success, -errno on failure
 *   entry: pointer to a kmalloc()'d struct dirent representing the file. It is
 *          the caller's responsibility to call kfree() to release this struct!
 *   dbuf: pointer to the disk buffer containing the found entry. Useful for
 *         things like removing the entry from parent directory without needing
 *         to re-read the block from disk again.
 *   dbuf_off: offset of the entry in the disk buffer.
 *
 * Returns:
 *    0 on success, -errno on failure
 */
int vfs_finddir(struct fs_node_t *dir, char *filename, struct dirent **entry,
                struct cached_page_t **dbuf, size_t *dbuf_off)
{
    int res = -EINVAL;
    
    if(!dir || !filename)
    {
        return res;
    }
    
    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;
    
    // not a directory
    if(!S_ISDIR(dir->mode))
    {
        return -ENOTDIR;
    }
    
    if(dir->ops && dir->ops->finddir)
    {
        res = dir->ops->finddir(dir, filename, entry, dbuf, dbuf_off);
        update_atime(dir);
    }
    
    return res;
}


/*
 * Find the given inode in the parent directory.
 * Called during pathname resolution when constructing the absolute pathname
 * of a given inode.
 *
 * Inputs:
 *    dir => the parent directory's node
 *    node => the searched-for inode
 *
 * Outputs:
 *    entry => if the node is found, its entry is converted to a kmalloc'd
 *             dirent struct (by calling ext2_entry_to_dirent() above), and 
 *             the result is stored in this field
 *    dbuf => the disk buffer representing the disk block containing the found
 *            filename, this is useful if the caller wants to delete the file
 *            after finding it (vfs_unlink(), for example)
 *    dbuf_off => the offset in dbuf->data at which the caller can find the
 *                file's entry
 *
 * Returns:
 *    0 on success, -errno on failure
 */
int vfs_finddir_by_inode(struct fs_node_t *dir, struct fs_node_t *node,
                         struct dirent **entry,
                         struct cached_page_t **dbuf, size_t *dbuf_off)
{
    int res = -EINVAL;

    if(!dir || !node)
    {
        return res;
    }

    // for safety
    *entry = NULL;
    *dbuf = NULL;
    *dbuf_off = 0;
    
    // not a directory
    if(!S_ISDIR(dir->mode))
    {
        return -ENOTDIR;
    }
    
    if(dir->ops && dir->ops->finddir_by_inode)
    {
        res = dir->ops->finddir_by_inode(dir, node, entry, dbuf, dbuf_off);
        update_atime(dir);
    }
    
    return res;
}


/*
 * Add the given filename to the parent directory represented by the dir node.
 * The new file's inode number is passed as the n parameter.
 *
 * Returns:
 *     0 on success, -errno on failure
 */
int vfs_addir(struct fs_node_t *dir, char *filename, ino_t n)
{
    int res = -EINVAL;

    if(!dir || !filename)
    {
        return res;
    }
    
    // not a directory
    if(!S_ISDIR(dir->mode))
    {
        return -ENOTDIR;
    }
    
    if(dir->ops && dir->ops->addir)
    {
        res = dir->ops->addir(dir, filename, n);
        dir->mtime = now();
        //dir->atime = dir->mtime;
        update_atime(dir);
        dir->flags |= FS_NODE_DIRTY;
    }

    //printk("vfs_addir: done\n");

    return res;
}


/*
 * Generic function to read from a file.
 */
ssize_t vfs_read_node(struct fs_node_t *node, off_t *pos,
                      unsigned char *buf, size_t count, int kernel)
{
    size_t left;
    size_t offset, i, j;
    struct cached_page_t *dbuf;
    char *p;
    
    //printk("vfs_read: count %d\n", count);

    if(!node || !pos || !buf)
    {
        return 0;
    }
    
    // adjust reading pointer if the file is not on procfs as those
    // files usually have a size of 0 despite having content
    if((count + (*pos) > node->size) && (node->dev != PROCFS_DEVID))
    {
        count = node->size - (*pos);
    }
        
    if(count <= 0)
    {
        return 0;
    }

    left = count;

    // if the node has a size of 0 and it is on /proc filesystem, let procfs
    // handle the read, as most procfs files are falsely reported as zero-sized
    if(node->dev == PROCFS_DEVID && node->size == 0)
    {
        return procfs_read_file(node, pos, buf, count);
    }

    // handle other, regular, files
    while(left)
    {
        dbuf = NULL;
        //offset = (*pos) / PAGE_SIZE;
        offset = (*pos) & ~(PAGE_SIZE - 1);
        
        if(!(dbuf = get_cached_page(node, offset, 0)))
        {
            //printk("vfs_read: could not read block 0x%x\n", i);
            break;
        }
        
        i = (*pos) % PAGE_SIZE;
        j = MIN((PAGE_SIZE - i), left);
        (*pos) += j;
        left -= j;

        //printk("vfs_read: buf 0x%lx, j %d, count %d\n", buf, j, count);

        p = (char *)(dbuf->virt + i);

        if(kernel)
        {
            A_memcpy(buf, p, j);
        }
        else
        {
            copy_to_user(buf, p, j);
        }

        release_cached_page(dbuf);
        buf += j;
        //offset += PAGE_SIZE;
        //i = 0;
    }
    
    // read() syscall updates the access time, so we only do this if we are
    // being calling from within the kernel
    if(kernel)
    {
        update_atime(node);
    }
    
    //printk("vfs_read: res %d\n", count - left);

    return count - left;
}

ssize_t vfs_read(struct file_t *f, off_t *pos,
                 unsigned char *buf, size_t count, int kernel)
{
    return vfs_read_node(f->node, pos, buf, count, kernel);
}


/*
 * Generic function to write to a file.
 */
ssize_t vfs_write_node(struct fs_node_t *node, off_t *pos,
                       unsigned char *buf, size_t count, int kernel)
{
    size_t done = 0;
    size_t offset, i, k;
    struct cached_page_t *dbuf;
    char *p;
    struct task_t *ct = cur_task;

    //printk("vfs_write: count %d\n", count);

    if(!node || !pos || !buf)
    {
        return 0;
    }
    
    i = *pos;

    if(exceeds_rlimit(ct, RLIMIT_FSIZE, (i + count)))
    {
        user_add_task_signal((struct task_t *)ct, SIGXFSZ, 1);
        return -EFBIG;
    }

    while(done < count)
    {
        dbuf = NULL;
        //offset = i / PAGE_SIZE;
        offset = i & ~(PAGE_SIZE - 1);
        
        if(!(dbuf = get_cached_page(node, offset, PCACHE_AUTO_ALLOC)))
        {
            break;
        }
        
        k = i % PAGE_SIZE;
        p = (char *)(dbuf->virt + k);
        k = PAGE_SIZE - k;
        
        if(k > count - done)
        {
            k = count - done;
        }
        
        i += k;
        
        if(i > node->size)
        {
            node->size = i;
            node->flags |= FS_NODE_DIRTY;
        }
        
        done += k;
        
        if(kernel)
        {
            A_memcpy(p, buf, k);
        }
        else
        {
            copy_from_user(p, buf, k);
        }

        release_cached_page(dbuf);
    }

    *pos = i;
    
    //printk("vfs_write: done - i %u, done %u, count %u\n", i, done, count);
    
    //return i ? (int)i : -EIO;
    return done ? (ssize_t)done : -EIO;
}

ssize_t vfs_write(struct file_t *f, off_t *pos,
                  unsigned char *buf, size_t count, int kernel)
{
    return vfs_write_node(f->node, pos, buf, count, kernel);
}


int vfs_linkat(int olddirfd, char *oldname, 
               int newdirfd, char *newname, int flags)
{
    int res;
    struct fs_node_t *oldnode = NULL;
    char *filename = NULL;
    struct fs_node_t *dnode = NULL;
    struct dirent *entry = NULL;
    struct cached_page_t *dbuf = NULL;
    size_t dbuf_off;
    char *name2;
    int followlink = (flags & AT_SYMLINK_FOLLOW);
    int open_flags = OPEN_USER_CALLER |
                   (followlink ? OPEN_FOLLOW_SYMLINK : OPEN_NOFOLLOW_SYMLINK);

    if(!oldname || !newname)
    {
        return -EINVAL;
    }
    
    // check file existence
    if((res = vfs_open_internal(oldname, olddirfd, &oldnode, open_flags)) < 0)
    {
        return -ENOENT;
    }
    
    // ensure it is a regular file
    if(!S_ISREG(oldnode->mode))
    {
        release_node(oldnode);
        return -EPERM;
    }

    if(!(name2 = path_remove_trailing_slash(newname, 0, NULL)))
    {
        release_node(oldnode);
        return -ENOMEM;
    }
    
    // get the parent dir of the new file
    if((res = get_parent_dir(name2, newdirfd, &filename, &dnode, 1)) < 0)
    {
        release_node(oldnode);
        kfree(name2);
        return res;
    }

    // can't link sys root
    if(!*filename)
    {
        res = -EPERM;
        goto error;
    }
    
    // can't hard-link across devices
    if(dnode->dev != oldnode->dev)
    {
        res = -EXDEV;
        goto error;
    }

    // check write permission to parent dir
    if((res = has_access(dnode, WRITE, 0)) != 0)
    {
        goto error;
    }

    // check if the new file already exists
    if(vfs_finddir(dnode, filename, &entry, &dbuf, &dbuf_off) == 0)
    {
        release_cached_page(dbuf);
        //brelse(dbuf);
        kfree(entry);
        res = -EEXIST;
        goto error;
    }

    // add the new file entry
    if((res = vfs_addir(dnode, filename, oldnode->inode)) < 0)
    {
        goto error;
    }

    time_t t = now();
    oldnode->links++;
    oldnode->ctime = t;
    oldnode->flags |= FS_NODE_DIRTY;

    dnode->links++;
    //dnode->atime = t;
    dnode->mtime = t;
    dnode->flags |= FS_NODE_DIRTY;
    update_atime(dnode);

    release_node(dnode);
    release_node(oldnode);
    kfree(name2);
    return 0;

error:
    release_node(dnode);
    release_node(oldnode);
    kfree(name2);
    return res;
}


int vfs_unlinkat(int dirfd, char *name, int flags)
{
    int res;
    char *filename = NULL;
    struct fs_node_t *dnode = NULL, *fnode = NULL;
    struct dirent *entry = NULL;
    struct cached_page_t *dbuf = NULL;
    size_t dbuf_off;
    char *name2;
    time_t t = now();
    
    if(!name)
    {
        return -EINVAL;
    }
    
    if(flags & AT_REMOVEDIR)
    {
        return vfs_rmdir(dirfd, name);
    }
    
    name2 = path_remove_trailing_slash(name, 0, NULL);
    
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

    // can't unlink sys root
    if(!*filename)
    {
        res = -ENOENT;
        goto error;
    }

    // check write permission to parent dir
    if((res = has_access(dnode, WRITE, 0)) != 0)
    {
        goto error;
    }

    // get the file entry
    if((res = vfs_finddir(dnode, filename, &entry, &dbuf, &dbuf_off)) < 0)
    {
        goto error;
    }
    
    // and the file's node
    if(!(fnode = get_node(dnode->dev, entry->d_ino, 1)))
    {
        kfree(entry);
        res = -ENOENT;
        goto error;
    }
    
    // check it is not a directory
    if(S_ISDIR(fnode->mode))
    //if(!S_ISREG(fnode->mode))
    {
        kfree(entry);
        release_node(fnode);
        //res = -EPERM;
        res = -EISDIR;
        goto error;
    }

    // check we're not removing an already deleted file
    if(!fnode->links)
    {
        // we'll decrement this to zero below
        fnode->links = 1;
    }
    else
    {
        dnode->links--;
        dnode->atime = t;
        //dnode->mtime = t;
        dnode->flags |= FS_NODE_DIRTY;
        update_atime(dnode);
    }
    
    // and remove the entry from the parent dir
    if((res = vfs_deldir(dnode, entry, dbuf, dbuf_off)) < 0)
    {
        kfree(entry);
        release_node(fnode);
        goto error;
    }

    release_cached_page(dbuf);
    kfree(name2);
    kfree(entry);

    fnode->links--;
    fnode->flags |= FS_NODE_DIRTY;
    fnode->ctime = t;

    release_node(dnode);
    release_node(fnode);

    return 0;

error:
    if(dbuf)
    {
        release_cached_page(dbuf);
    }
    
    kfree(name2);
    release_node(dnode);
    return res;
}


int vfs_rmdir(int dirfd, char *pathname)
{
    int res;
    char *filename = NULL;
    struct fs_node_t *dnode = NULL, *fnode = NULL;
    struct dirent *entry = NULL;
    struct cached_page_t *dbuf = NULL;
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

    // can't rmdir sys root
    if(!*filename)
    {
        res = -ENOENT;
        goto error;
    }

    // get the file entry
    if((res = vfs_finddir(dnode, filename, &entry, &dbuf, &dbuf_off)) < 0)
    {
        goto error;
    }

    // check write permission to parent dir
    if(has_access(dnode, WRITE, 0) != 0)
    {
        kfree(entry);
        res = -EACCES;
        goto error;
    }

    // can't mkdir if the filesystem was mount readonly
    if((dinfo = node_mount_info(dnode)) && (dinfo->mountflags & MS_RDONLY))
    //if((dinfo = get_mount_info(dnode->dev)) && (dinfo->mountflags & MS_RDONLY))
    {
        kfree(entry);
        res = -EROFS;
        goto error;
    }

    // get the file's node
    if(!(fnode = get_node(dnode->dev, entry->d_ino, 1)))
    {
        kfree(entry);
        res = -ENOENT;
        goto error;
    }
    
    // can't rmdir '.'
    if(fnode->inode == dnode->inode)
    {
        res = -EPERM;
        goto error2;
    }

    if(!S_ISDIR(fnode->mode))
    {
        res = -ENOTDIR;
        goto error2;
    }
    
    // don't remove a mountpoint while still mounted
    if(fnode->flags & FS_NODE_MOUNTPOINT)
    {
        res = -EBUSY;
        goto error2;
    }

    if(dnode->ops && dnode->ops->dir_empty)
    {
        if(!dnode->ops->dir_empty(fnode))
        {
            res = -ENOTEMPTY;
            goto error2;
        }
    }
    else
    {
        res = -EPERM;
        goto error2;
    }
    
    if((dnode->mode & S_ISVTX) && !suser(ct) &&
       (ct->euid != fnode->uid) && (ct->euid != dnode->uid))
    {
        res = -EPERM;
        goto error2;
    }

    // remove the entry from the parent directory
    if((res = vfs_deldir(dnode, entry, dbuf, dbuf_off)) < 0)
    {
        goto error2;
    }

    release_cached_page(dbuf);
    truncate_node(fnode, 0);

    fnode->links = 0;
    fnode->flags |= FS_NODE_DIRTY;

    dnode->links--;
    dnode->ctime = dnode->mtime = now();
    dnode->flags |= FS_NODE_DIRTY;

    kfree(entry);
    kfree(name2);
    release_node(fnode);
    release_node(dnode);

    return 0;

error2:
    kfree(entry);
    release_node(fnode);

error:
    if(dbuf)
    {
        release_cached_page(dbuf);
    }

    kfree(name2);
    release_node(dnode);
    return res;
}


/*
 * Remove an entry from a parent directory.
 */
int vfs_deldir(struct fs_node_t *dir, struct dirent *entry,
               struct cached_page_t *dbuf, size_t dbuf_off)
{
    int res = -EINVAL;
    
    if(!dir)
    {
        return res;
    }
    
    // not a directory
    if(!S_ISDIR(dir->mode))
    {
        return -ENOTDIR;
    }
    
    if(dir->ops && dir->ops->deldir)
    {
        res = dir->ops->deldir(dir, entry, dbuf, dbuf_off);
        dir->mtime = now();
        //dir->atime = dir->mtime;
        dir->flags |= FS_NODE_DIRTY;
        update_atime(dir);
    }
    
    return res;
}


/*
 * Get dir entries.
 *
 * Inputs:
 *     dir => node of dir to read from
 *     pos => byte position to start reading entries from, which will be
 *            updated after the read to prepare for future reads
 *     dp => buffer in which to store dir entries
 *     count => max number of bytes to read (i.e. size of dp)
 *
 * Returns:
 *     number of bytes read on success, -errno on failure
 */
int vfs_getdents(struct fs_node_t *dir, off_t *pos, void *dp, int count)
{
    int res = -EINVAL;

    if(!dir || !pos || !dp)
    {
        return res;
    }
    
    // not a directory
    if(!S_ISDIR(dir->mode))
    {
        return -ENOTDIR;
    }
    
    if(dir->ops && dir->ops->getdents)
    {
        res = dir->ops->getdents(dir, pos, dp, count);
        update_atime(dir);
    }
    
    return res;
}


/*
 * TODO: haven't been tested.
 *
 * See: https://man7.org/linux/man-pages/man2/mknod.2.html
 */
int vfs_mknod(char *pathname, mode_t mode, dev_t dev, int dirfd,
              int open_flags, struct fs_node_t **res)
{
    struct fs_node_t *node = NULL;
    int error;
    
    if(!pathname || !res)
    {
        return -EINVAL;
    }
    
    *res = NULL;
    
    // check node type
    if(!S_ISREG(mode) && !S_ISCHR(mode) && !S_ISBLK(mode) &&
       !S_ISFIFO(mode) && !S_ISSOCK(mode))
    {
        return -EINVAL;
    }
    
    // for chr & blk devices, dev must be valid
    if(S_ISCHR(mode) || S_ISBLK(mode))
    {
        if(MAJOR(dev) == 0 || MAJOR(dev) >= NR_DEV ||
           MINOR(dev) == 0 || MINOR(dev) >= NR_DEV)
        {
            return -EINVAL;
        }
    }
    
    // check if it already exists
    if((error = vfs_open_internal(pathname, dirfd, &node, open_flags)) == 0)
    {
        release_node(node);
        return -EEXIST;
    }

    // create the node
    if((error = vfs_open(pathname, O_RDWR|O_CREAT, mode, dirfd, 
                         &node, open_flags)) < 0)
    {
        return error;
    }
    
    // vfs_open() creates regular files by default, let's fix this now

    // ensure we only set ONE type, in case the caller erroneously OR'd
    // more than one type in the mode field
    if(S_ISCHR(mode))
    {
        //node->mode &= ~S_IFREG;
        //node->mode |= S_IFCHR;
        node->blocks[0] = dev;
    }
    else if(S_ISBLK(mode))
    {
        //node->mode &= ~S_IFREG;
        //node->mode |= S_IFBLK;
        node->blocks[0] = dev;
    }
    else if(S_ISFIFO(mode))
    {
        //node->mode &= ~S_IFREG;
        //node->mode |= S_IFIFO;
    }
    else if(S_ISSOCK(mode))
    {
        //node->mode &= ~S_IFREG;
        //node->mode |= S_IFSOCK;
        node->flags |= FS_NODE_SOCKET;
    }
    
    *res = node;
    return 0;
}

