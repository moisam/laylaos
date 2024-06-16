/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
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


/*
 * Handler for syscall renameat().
 *
 * TODO: this is a very lazy implementation of rename, which should be fixed
 *       to properly handle the different case scenarios listed in the link 
 *       below.
 *
 * See: https://man7.org/linux/man-pages/man2/rename.2.html
 */
int syscall_renameat(int olddirfd, char *oldpath, int newdirfd, char *newpath)
{
    int res;
    
    if((res = vfs_linkat(olddirfd, oldpath, newdirfd, newpath, 0)) == 0)
    {
        if((res = vfs_unlinkat(olddirfd, oldpath, 0)) == 0)
        {
            return 0;
        }
    }
    
    return res;
}


/*
 * Handler for syscall rename().
 */
int syscall_rename(char *oldpath, char *newpath)
{
    return syscall_renameat(AT_FDCWD, oldpath, AT_FDCWD, newpath);
}

