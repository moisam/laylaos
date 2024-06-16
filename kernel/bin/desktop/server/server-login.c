/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: server-login.c
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
 *  \file server-login.c
 *
 *  The graphical login screen. This is currently a no-op and the user is
 *  automatically logged in.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

#include "../../login-creds.c"

/*
 * TODO: Implement a proper GUI login screen.
 *       This has to be done after the server finishes setup so that we can
 *       use the keyboard & mouse, draw to screen, etc.
 */

void server_login(char *myname)
{
    struct passwd *pwd = NULL;

    if(!(pwd = getpwuid(getuid())))
    {
        fprintf(stderr, "%s: cannot find current user in database\n",
                        myname);
        //exit(EXIT_FAILURE);
        return;
    }

    set_creds(pwd);
    setsid();
}

