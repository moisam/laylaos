/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
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
int syscall_link(char *oldname, char *newname)
{
    return vfs_linkat(AT_FDCWD, oldname, AT_FDCWD, newname, 0);
}


/*
 * Handler for syscall linkat().
 */
int syscall_linkat(int olddirfd, char *oldname,
                   int newdirfd, char *newname, int flags)
{
    return vfs_linkat(olddirfd, oldname, newdirfd, newname, flags);
}


/*
 * Handler for syscall unlink().
 */
int syscall_unlink(char *pathname)
{
    return vfs_unlinkat(AT_FDCWD, pathname, 0);
}


/*
 * Handler for syscall unlinkat().
 */
int syscall_unlinkat(int dirfd, char *pathname, int flags)
{
    return vfs_unlinkat(dirfd, pathname, flags);
}

