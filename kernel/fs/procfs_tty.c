/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: procfs_tty.c
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
 *  \file procfs_tty.c
 *
 *  This file implements the functions needed to read terminal info from the
 *  /proc/tty directory.
 */

#include <string.h>
#include <kernel/tty.h>
#include <mm/kheap.h>
#include <fs/procfs.h>
#include <fs/devpts.h>

/*
 * Read /proc/tty/drivers.
 */
size_t get_tty_driver_list(char **buf)
{
    char *p;

    PR_MALLOC(*buf, 2048);
    p = *buf;

    ksprintf(p, 2048, "/dev/tty             /dev/tty        5       0 system:/dev/tty\n");
    p += strlen(p);
    //ksprintf(p, 2048, "/dev/console         /dev/console    5       1 system:console\n");
    //p += strlen(p);
    ksprintf(p, 2048, "/dev/tty0            /dev/tty0       4       0 system:vtmaster\n");
    p += strlen(p);
    ksprintf(p, 2048, "/dev/ptmx            /dev/ptmx       5       2 system\n");
    p += strlen(p);
    ksprintf(p, 2048, "pty_slave            /dev/pts        136  0-%d pty:slave\n", 
                MAX_PTY_DEVICES - 1);
    p += strlen(p);
    ksprintf(p, 2048, "unknown              /dev/tty        4     1-%d console\n", 
                NTTYS - 1);
    p += strlen(p);

    return (p - *buf);
}

