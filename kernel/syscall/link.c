/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: link.c
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
 *  \file link.c
 *
 *  Functions for linking and unlinking files.
 */

#include <fcntl.h>
#include <kernel/syscall.h>


/*
 * Handler for syscall link().
 */
long syscall_link(char *oldname, char *newname)
{
    return vfs_linkat(AT_FDCWD, oldname, AT_FDCWD, newname, OPEN_FOLLOW_SYMLINK /* AT_SYMLINK_FOLLOW */);
}


/*
 * Handler for syscall linkat().
 */
long syscall_linkat(int olddirfd, char *oldname,
                    int newdirfd, char *newname, int __flags)
{
    int flags;

    /*
     * NOTE: we only support this flag for now.
     */
    if(__flags & ~(AT_SYMLINK_FOLLOW))
    {
        return -EINVAL;
    }

    flags = (__flags & AT_SYMLINK_FOLLOW) ? OPEN_FOLLOW_SYMLINK : OPEN_NOFOLLOW_SYMLINK;

    return vfs_linkat(olddirfd, oldname, newdirfd, newname, flags);
}


/*
 * Handler for syscall unlink().
 */
long syscall_unlink(char *pathname)
{
    return vfs_unlinkat(AT_FDCWD, pathname, 0);
}


/*
 * Handler for syscall unlinkat().
 */
long syscall_unlinkat(int dirfd, char *pathname, int flags)
{
    return vfs_unlinkat(dirfd, pathname, flags);
}

