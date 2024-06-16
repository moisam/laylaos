/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: login-creds.c
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
 *  \file login-creds.c
 *
 *  Utility functions for use by the login program.
 */

#define DEFAULT_PATH                "/sbin:/usr/sbin:/bin:/usr/bin:/bin/desktop"
#define DEFAULT_SHELL               "/bin/bash"


void set_groups(char *name, gid_t gid)
{
    gid_t groups[1024];
    int ngroups = 1024;

    if(getgrouplist(name, gid, groups, &ngroups) < 0)
    {
        fprintf(stderr, "Failed to get user groups: %s\n", strerror(errno));
        return;
    }

    setgroups(ngroups, groups);
}


void set_creds(struct passwd *pwd)
{
    char *home = pwd->pw_dir[0] ? pwd->pw_dir : "/";
    char *exe = pwd->pw_shell[0] ? pwd->pw_shell : DEFAULT_SHELL;

    if(setenv("LOGNAME", pwd->pw_name, 1) < 0 ||
       setenv("USER", pwd->pw_name, 1) < 0 ||
       setenv("HOME", home, 1) < 0 ||
       setenv("SHELL", exe, 1) < 0 ||
       setenv("TERMINFO_DIRS", "/usr/local/share/terminfo:/usr/share/terminfo", 1) < 0 ||
       setenv("TERMINFO", "/usr/share/terminfo", 1) < 0 ||
       setenv("PATH", DEFAULT_PATH, 1) < 0)
    {
        fprintf(stderr, "Failed to setenv: %s", strerror(errno));
    }

    if(chdir(home) < 0)
    {
        fprintf(stderr, "Failed to chdir to user home: %s", strerror(errno));
    }

    set_groups(pwd->pw_name, pwd->pw_gid);
    setgid(pwd->pw_gid);
    setuid(pwd->pw_uid);
}

