/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: symlink.c
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
 *  \file symlink.c
 *
 *  Functions for reading and writing symbolic links.
 *
 *  See: https://man7.org/linux/man-pages/man2/symlink.2.html
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <fcntl.h>
//#include <sys/syslimits.h>
#include <limits.h>
#include <kernel/laylaos.h>
#include <kernel/vfs.h>
#include <kernel/syscall.h>
#include <kernel/clock.h>
#include <kernel/user.h>
#include <fs/procfs.h>
#include <fs/dentry.h>
#include <mm/kheap.h>


long write_symlink(struct fs_node_t *node, char *path, int kernel)
{
    long res;
    size_t len = 0, i;
    char *kpath = NULL;

    // check the filesystem supports symlink creation
    if(!node->ops || !node->ops->write_symlink)
    {
        return -EPERM;      // TODO: is this the right errno here?
    }

    if(kernel)
    {
        kpath = path;
        len = strlen(path);
    }
    else
    {
        if((res = copy_str_from_user(path, &kpath, &len)) < 0)
        {
            return res;
        }
    }

    // Pass the symlink (non-NULL-terminated) to the filesystem driver.
    // If we count the NULL-terminator, fsck complains about invalid symlinks.
    //len++;
    i = node->ops->write_symlink(node, kpath, len, 1);

    if(kpath != path)
    {
        kfree(kpath);
    }

    time_t t = now();
	node->mtime = t;
	//node->atime = t;
	node->flags |= FS_NODE_DIRTY;
	update_atime(node);

	return (i == len) ? 0 : -EIO;
}


/*
 * Handler for syscall symlink().
 */
long syscall_symlink(char *target, char *linkpath)
{
    return syscall_symlinkat(target, AT_FDCWD, linkpath);
}


/*
 * Handler for syscall symlinkat().
 */
long syscall_symlinkat(char *target, int newdirfd, char *linkpath)
{
    long res;
	struct fs_node_t *node = NULL;
    size_t i;

    if(!target || !*target || !linkpath || !*linkpath)
    {
		return -EFAULT;
    }

#define OPEN_FLAGS          (OPEN_USER_CALLER | OPEN_NOFOLLOW_SYMLINK)

	// check if it already exists
	if((res = vfs_open_internal(linkpath, newdirfd, &node, OPEN_FLAGS)) == 0)
	{
		release_node(node);
		return -EEXIST;
	}

#undef OPEN_FLAGS

    // create the node
    if((res = vfs_open(linkpath, O_RDWR|O_CREAT, 0777|S_IFLNK, newdirfd,
                        &node, OPEN_USER_CALLER)) < 0)
    {
        return res;
    }

    i = write_symlink(node, target, 0);

    if(i == 0)
    {
        // ensure the file mode reflects it is a symlink
        node->mode &= ~S_IFMT;
        node->mode |= S_IFLNK;
    }

    release_node(node);

	return i;
}


/* 
 * Handler for syscall readlink().
 *
 * Arguments and return value are as described in readlinkat below.
 */
long syscall_readlink(char *pathname, char *buf,
                      size_t bufsize, ssize_t *__copied)
{
    return syscall_readlinkat(AT_FDCWD, pathname, buf, bufsize, __copied);
}


/* 
 * Handler for syscall readlinkat().
 *
 * Input:
 *    dirfd => used to interpret relative pathnames
 *    pathname => the path to the link we want to read
 *    bufsize => size of buf
 *
 * Output:
 *    buf => contents of the link are stored here
 *    __copied => number of bytes copied to buf is stored here (the C library
 *                will return this as the result of the call)
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long syscall_readlinkat(int dirfd, char *pathname, char *buf,
                        size_t bufsize, ssize_t *__copied)
{
    long res;
    ssize_t copied;
	struct fs_node_t *node = NULL;

    if(!pathname /* || !*pathname */ || !buf || !bufsize)
    {
		return -EINVAL;
    }

#define OPEN_FLAGS          (OPEN_USER_CALLER | OPEN_NOFOLLOW_SYMLINK)

	// open the link - don't follow symlink
	if((res = vfs_open_internal(pathname, dirfd, &node, OPEN_FLAGS)) < 0)
	{
		release_node(node);
		return res;
	}

#undef OPEN_FLAGS
	
	if(!node)
	{
	    return -ENOENT;
	}

    res = read_symlink(node, buf, bufsize, 0);
    release_node(node);
    
    // res >= 0 is the number of bytes written, negative res is error
    if(res >= 0)
    {
        copied = res;
        //COPY_TO_USER(__copied, &copied, sizeof(ssize_t));
        COPY_VAL_TO_USER(__copied, &copied);
        res = 0;
    }
    
    return res;
}


/* 
 * Read the contents of a symlink, open the target and return the opened
 * target file node.
 *
 * Input:
 *    link => the symlink we want to follow
 *    flags => flags to pass to vfs_open() when opening the target
 *
 * Output:
 *    target => the loaded symlink target will be stored here
 *
 * Returns:
 *    0 on success, -errno on failure
 */
long follow_symlink(struct fs_node_t *link, struct fs_node_t *parent,
                    int flags, struct fs_node_t **target)
{
    if(!link || !target)
    {
        return -EINVAL;
    }

    *target = NULL;

    long res;
    // procfs links have a filesize of 0 by default
    size_t bufsz = (link->dev == PROCFS_DEVID) ? PROCFS_LINK_SIZE :
                   (link->size > PATH_MAX) ? PATH_MAX : link->size;
    char *buf = (char *)kmalloc(bufsz + 1);
    //int dirfd = AT_FDCWD;

    KDEBUG("follow_symlink: bufsz %d\n", bufsz);
    
    // make sure we've allocated a buffer
    if(!buf)
    {
        return -ENOMEM;
    }

    // read the symlink contents
    res = read_symlink(link, buf, bufsz, 1);

    if(res < 0)
    {
        kfree(buf);
        return res;
    }
    
    // append '\0' to ensure vfs_open() does not break
    buf[res] = '\0';

    KDEBUG("follow_symlink: buf @ " _XPTR_ " '%s'\n", (uintptr_t)buf, buf);
    
    // if the link is relative, open a temporary file descriptor to reflect
    // the symlink's directory, which we'll pass to vfs_open() to use
    // instead of the task's cwd.
    if(buf[0] != '/')
    {
        struct dentry_t *dent = NULL;
        size_t len = 0, plen = 0;
        char *buf2;

        if(get_dentry(parent, &dent) < 0)
        {
            kfree(buf);
            return -EINVAL;
        }

        if(dent->path == NULL)
        {
            kfree(buf);
            release_dentry(dent);
            return -EINVAL;
        }

        plen = strlen(dent->path);
        len = res;
        bufsz = plen + len + 2;

        if(!(buf2 = kmalloc(bufsz)))
        {
            kfree(buf);
            release_dentry(dent);
            return -ENOMEM;
        }

        if(dent->path[plen - 1] == '/')
        {
            ksprintf(buf2, bufsz, "%s%s", dent->path, buf);
        }
        else
        {
            ksprintf(buf2, bufsz, "%s/%s", dent->path, buf);
        }

        kfree(buf);
        release_dentry(dent);
        buf = buf2;

        /*
        if((dirfd = open_tmp_fd(parent)) < 0)
        {
            kfree(buf);
            return -EMFILE;
        }
        */
    }
    
    KDEBUG("follow_symlink: fd %d\n", dirfd);
    
    // now try to read the symlink's target
    res = vfs_open(buf, flags, 0777, AT_FDCWD /* dirfd */, target, OPEN_KERNEL_CALLER);

    KDEBUG("follow_symlink: done -- fd %d, res %d\n", dirfd, res);
    
    /*
    if(buf[0] != '/')
    //if(dirfd != AT_FDCWD)
    {
        syscall_close(dirfd);
    }
    */
    
    kfree(buf);
    return res;
}


/* 
 * Read the contents of a symlink (effectively what the readlink syscall does,
 * except this function needs a file node pointer instead of a path).
 * If the symlink's target is longer than bufsz, the target is truncated.
 * No null-terminating byte is added to the buffer.
 *
 * Input:
 *    link => the symlink we want to read
 *    buf => buffer to place the link's target in
 *    bufsz => buf's size
 *    kernel => set if the caller is a kernel function (i.e. 'buf' address
 *              is in kernel memory), 0 if 'buf' is a userspace address
 *
 * Output:
 *    buf => the symlink target will be stored here
 *
 * Returns:
 *    number of bytes read on success, -errno on failure
 */
long read_symlink(struct fs_node_t *link, char *buf, size_t bufsz, int kernel)
{
    long res = -ENOSYS;

    if(!link || !buf)
    {
        return -EINVAL;
    }
    
    // not a link
    if(!S_ISLNK(link->mode))
    {
        return -EINVAL;
    }
    
    if(link->ops && link->ops->read_symlink)
    {
        res = link->ops->read_symlink(link, buf, bufsz, kernel);
        update_atime(link);
    }

    KDEBUG("read_symlink: res = %d, buf = '%s'\n", res, buf);
    
    return res;
}

