/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: kgroups.c
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
 *  \file kgroups.c
 *
 *  Kernel groups implementation. These groups include daemon, sys, admin, 
 *  tty and kmem.
 */

#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/kio.h>
#include <kernel/kgroups.h>
#include <kernel/dev.h>
#include <kernel/tty.h>
#include <mm/kheap.h>

struct
{
    char *name;
    size_t nlen;
    gid_t gid;
} kgroups[] =
{
    { "nogroup", 7, 0xFF },
    { "daemon",  6, 0xFF },
    { "sys",     3, 0xFF },
    { "adm",     3, 0xFF },
    { "tty",     3, 0xFF },
    { "kmem",    4, 0xFF },
};

// some defaults to use if /etc/group lacks some of the core groups
// we need to operate
#define MAX_GROUPS          (0xFF)
#define DEFAULT_NOGROUP     (0xFE)
#define DEFAULT_KGROUP_TTY  (0xFD)
#define DEFAULT_KGROUP_KMEM (0xFC)

#define GROUP_FILE          "/etc/group"

#define SKIP_TO_NEWLINE()               \
    nl = p;                             \
    while(*nl != '\n' && nl < lp) nl++;

#define SKIP_TO_COLON(c, p)             \
    c = p;                              \
    while(*c != ':' && c < nl) c++;

#define COLON(c)        (*(c) == ':' && (c) < nl)

#define MAY_FIX_GID(i, v)               \
    if(kgroups[i].gid == 0xFF) kgroups[i].gid = v;


static void post_load(void)
{
    int i;

    // ensure we have the least working valid gids
    MAY_FIX_GID(KGROUP_NOGROUP, DEFAULT_NOGROUP);
    MAY_FIX_GID(KGROUP_TTY, DEFAULT_KGROUP_TTY);
    MAY_FIX_GID(KGROUP_KMEM, DEFAULT_KGROUP_KMEM);
    
    // now fix group ids for the appropriate devices under /dev
    (void)set_dev_gid("mem", kgroups[KGROUP_KMEM].gid);
    (void)set_dev_gid("kmem", kgroups[KGROUP_KMEM].gid);
    (void)set_dev_gid("tty", kgroups[KGROUP_TTY].gid);
    (void)set_dev_gid("ptmx", kgroups[KGROUP_TTY].gid);

    for(i = 0; i < NTTYS; i++)
    {
        static char buf[8] = { 't', 't', 'y', '\0', '\0', };

        buf[3] = '0' + i;
        (void)set_dev_gid(buf, kgroups[KGROUP_TTY].gid);
    }
}


/*
 * Initialise kgroups.
 */
void kgroups_init(void)
{
    size_t buflen;
    char *buf, *p, *lp, *nl;
    char *c1, *c2, *c3;
    int j;
    gid_t gid;

    printk("kgrp: reading '%s'\n", GROUP_FILE);

    // read /etc/group
    if((j = kread_file(GROUP_FILE, &buf, &buflen)) < 0)
    {
        printk("kgrp: failed to read %s (err %d in kgroup_init)\n", 
               GROUP_FILE, j);
        kfree(buf);
        goto err;
    }

    printk("kgrp: parsing '%s'\n", GROUP_FILE);

    // load group info from file
    p = buf;
    lp = buf + buflen;
    
    while(p < lp)
    {
        // each /etc/group line has the format:
        //    name:pass:gid:members
        int found = 0;
        
        // first check if this is a line we are interested in
        for(j = 0; j <= KGROUP_LAST; j++)
        {
            if(memcmp(p, kgroups[j].name, kgroups[j].nlen) == 0)
            //if(strncmp(p, kgroups[j].name, kgroups[j].nlen) == 0)
            {
                if(p[kgroups[j].nlen] == ':')
                {
                    found = 1;
                    break;
                }
            }
        }

        // find the end of line
        SKIP_TO_NEWLINE();
        
        if(!found)
        {
            p = nl + 1;
            continue;
        }

        // find the 3 colons
        SKIP_TO_COLON(c1, p);
        SKIP_TO_COLON(c2, c1 + 1);
        SKIP_TO_COLON(c3, c2 + 1);
        
        // ensure the line has exactly 3 colons
        if(!COLON(c1) || !COLON(c2) || !COLON(c3))
        {
            printk("kgrp: skipping invalid line in %s\n", GROUP_FILE);
            p = nl + 1;
            continue;
        }
        
        // get the gid
        gid = 0;
        p = c2 + 1;
        
        if(p == c3)
        {
            printk("kgrp: skipping line with empty gid in %s\n", GROUP_FILE);
            p = nl + 1;
            continue;
        }
        
        while(p < nl)
        {
            if(p == c3)
            {
                break;
            }
            
            if(*p < '0' || *p > '9')
            {
                printk("kgrp: skipping line with invalid gid in %s\n",
                       GROUP_FILE);
                p = nl + 1;
                continue;
            }
            
            gid = (gid * 10) + (*p - '0');
            p++;
        }
        
        //printk("gid %d\n", gid);
        kgroups[j].gid = gid;
        p = nl + 1;
    }
    
    kfree(buf);
    
    post_load();
    
    return;

err:

    post_load();

    printk("kgrp: failed to init kernel groups - using builtin defaults:\n");
    printk("  nogroup %u\n", kgroups[KGROUP_NOGROUP].gid);
    printk("  daemon  %u\n", kgroups[KGROUP_DAEMON].gid);
    printk("  sys     %u\n", kgroups[KGROUP_SYS].gid);
    printk("  adm     %u\n", kgroups[KGROUP_ADMIN].gid);
    printk("  tty     %u\n", kgroups[KGROUP_TTY].gid);
    printk("  kmem    %u\n", kgroups[KGROUP_KMEM].gid);
}


gid_t get_kgroup(int i)
{
    if(i < 0 || i > KGROUP_LAST)
    {
        printk("kgrp: trying to find an invalid group (gid %d)\n", i);
        //kpanic("Cannot find group");
        return kgroups[KGROUP_NOGROUP].gid;
    }
    
    return kgroups[i].gid;
}

