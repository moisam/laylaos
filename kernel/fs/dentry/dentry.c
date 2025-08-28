/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: dentry.c
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
 *  \file dentry.c
 *
 *  This file includes functions to work with and manipulate dentries. The
 *  kernel keeps a cache of open files and directories and their absolute
 *  paths in the dentry list, which is used in path traversal.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>
#include <kernel/task.h>
#include <mm/kheap.h>
#include <fs/dentry.h>


void init_dentries(void)
{
    struct bdev_ops_t *dev, *ldev = &bdev_tab[NR_DEV];
    int sz = NR_DEV * sizeof(struct dentry_list_t);
    
    for(dev = bdev_tab; dev < ldev; dev++)
    {
        //A_memset(&dev->dentry_list, 0, sizeof(struct dentry_list_t));

        // init dentry tables for block devices (but only the ones that have 
        // actual drivers)

        if(dev->strategy)
        {
            dev->dentry_list = kmalloc(sz);
            A_memset(dev->dentry_list, 0, sz);
        }
    }

#define INIT_AND_ZERO(i)                            \
    if(!bdev_tab[ i ].dentry_list)                  \
    {                                               \
        bdev_tab[ i ].dentry_list = kmalloc(sz);    \
        A_memset(bdev_tab[ i ].dentry_list, 0, sz); \
    }

    // special filesystems e.g. devfs, devpts
    INIT_AND_ZERO(240);

    // tmpfs filesystems
    INIT_AND_ZERO(241);

    // procfs filesystems
    INIT_AND_ZERO(243);

#undef INIT_AND_ZERO

}


struct dentry_t *alloc_dentry(struct fs_node_t *node, char *path)
{
    struct dentry_t *ent = (struct dentry_t *)kmalloc(sizeof(struct dentry_t));
    
    if(ent)
    {
        A_memset(ent, 0, sizeof(struct dentry_t));
        ent->path = path;
        //ent->node = node;
        ent->dev = node->dev;
        ent->inode = node->inode;
        ent->refs = 1;
        ent->last_accessed = ticks;
    }
    
    return ent;
}


void free_dentry(struct dentry_t *ent)
{
    kfree(ent->path);
    kfree(ent);
}


static inline void add_to_list(struct dentry_list_t *list,
                               struct dentry_t *ent)
{
    struct dentry_t *ent2;

    ent->list = list;
    ent2 = list->first_dentry;
    
    if(ent2)
    {
        for( ; ent2->dev_next != NULL; ent2 = ent2->dev_next)
        {
            ;
        }
        
        ent2->dev_next = ent;
    }
    else
    {
        list->first_dentry = ent;
    }
}


int get_dentry(struct fs_node_t *dir, struct dentry_t **dent)
{
    int maj, min, res;
    char *path;
    struct dentry_t *ent, *newent;
    struct dentry_list_t *list;
    
    if(!dir || !dent)
    {
        return -EINVAL;
    }
    
    *dent = NULL;
    maj = MAJOR(dir->dev);
    min = MINOR(dir->dev);
    
    if(maj >= NR_DEV || min >= NR_DEV)
    {
        return -EINVAL;
    }

    if(!bdev_tab[maj].dentry_list)
    {
        return -EINVAL;
    }

    // search for the dentry in the device's dentries list
    //list = &bdev_tab[maj].dentry_list;
    list = &(bdev_tab[maj].dentry_list[min]);
    elevated_priority_lock(&list->lock);

    for(ent = list->first_dentry; ent != NULL; ent = ent->dev_next)
    {
        if(ent->inode == dir->inode)
        {
            ent->refs++;
            ent->last_accessed = ticks;
            elevated_priority_unlock(&list->lock);
            *dent = ent;
            return 0;
        }
    }

    elevated_priority_unlock(&list->lock);
    
    // dentry not found, so we try to create it now
    if((res = getpath(dir, &path)) < 0)
    {
        return -ENOENT;
    }
    
    if(!(newent = alloc_dentry(dir, path)))
    {
        kfree(path);
        return -ENOMEM;
    }

    // make sure no one has added this entry while we looped
    elevated_priority_relock(&list->lock);

    for(ent = list->first_dentry; ent != NULL; ent = ent->dev_next)
    {
        if(ent->inode == dir->inode)
        {
            ent->refs++;
            ent->last_accessed = ticks;
            elevated_priority_unlock(&list->lock);
            *dent = ent;
            free_dentry(newent);
            return 0;
        }
    }

    // now add it to the device's dentries list
    add_to_list(list, newent);

    elevated_priority_unlock(&list->lock);
    *dent = newent;
    return 0;
}


int create_file_dentry(struct fs_node_t *dir, struct fs_node_t *file,
                       char *filename)
{
    int majd, majf, minf, res;
    struct dentry_t *dent;
    char *path;
    struct dentry_list_t *list;
    size_t filelen, dentlen;
    
    if(!dir || !file || !filename)
    {
        return -EINVAL;
    }
    
    majd = MAJOR(dir->dev);
    majf = MAJOR(file->dev);
    minf = MINOR(file->dev);

    if(majd >= NR_DEV || majf >= NR_DEV || minf >= NR_DEV)
    {
        return -EINVAL;
    }
    
    // don't create dentries for '.' and '..'
    if(filename[0] == '.' && (filename[1] == '\0' ||
                              (filename[1] == '.' && filename[2] == '\0')))
    {
        return 0;
    }
    
    // or for procfs files/dirs as inode allocations can change quickly
    if(dir->dev == PROCFS_DEVID || file->dev == PROCFS_DEVID)
    {
        return 0;
    }

    if(!bdev_tab[majf].dentry_list)
    {
        return 0;
    }

    // first, check if the dentry already exists
    list = &(bdev_tab[majf].dentry_list[minf]);
    //list = &bdev_tab[majf].dentry_list;
    elevated_priority_lock(&list->lock);

    for(dent = list->first_dentry; dent != NULL; dent = dent->dev_next)
    {
        //if(dent->node == file)
        if(dent->inode == file->inode)
        {
            // it does - no need to do anything else
            dent->last_accessed = ticks;
            elevated_priority_unlock(&list->lock);
            return 0;
        }
    }

    // get_dentry() needs to lock the list itself
    elevated_priority_unlock(&list->lock);

    if((res = get_dentry(dir, &dent)) < 0)
    {
        return res;
    }
    
    filelen = strlen(filename);
    dentlen = strlen(dent->path);
    
    if(!(path = (char *)kmalloc(filelen + dentlen + 2)))
    {
        release_dentry(dent);
        return -ENOMEM;
    }
    
    if(dent->path[dentlen - 1] == '/')
    {
        ksprintf(path, (filelen + dentlen + 2), "%s%s", dent->path, filename);
    }
    else
    {
        ksprintf(path, (filelen + dentlen + 2), "%s/%s", dent->path, filename);
    }

    release_dentry(dent);
    
    if(!(dent = alloc_dentry(file, path)))
    {
        kfree(path);
        return -ENOMEM;
    }

    // now add it to the device's dentries list
    list = &(bdev_tab[majf].dentry_list[minf]);
    //list = &bdev_tab[majf].dentry_list;
    elevated_priority_relock(&list->lock);
    add_to_list(list, dent);
    elevated_priority_unlock(&list->lock);
    release_dentry(dent);

    return 0;
}


void release_dentry(struct dentry_t *ent)
{
    if(!ent)
    {
        return;
    }

    elevated_priority_lock(&ent->list->lock);
    ent->refs--;
    elevated_priority_unlock(&ent->list->lock);
}


/*
 * We invalidate a dentry when:
 *    - the inode is freed (i.e. links == 0)
 *    - containing device is unmounted (all dev dentries are invalidated)
 */
void invalidate_dentry(struct fs_node_t *dir)
{
    volatile int refs;
    int maj, min;
    struct dentry_t *ent, *prev = NULL;
    struct dentry_list_t *list;
    //register struct task_t *ct = get_cur_task();

    if(!dir)
    {
        return;
    }
    
    maj = MAJOR(dir->dev);
    min = MINOR(dir->dev);
    
    if(maj >= NR_DEV || min >= NR_DEV)
    {
        return;
    }

    if(!bdev_tab[maj].dentry_list)
    {
        return;
    }

    // search for the dentry in the device's dentries list
    list = &(bdev_tab[maj].dentry_list[min]);
    //list = &bdev_tab[maj].dentry_list;
    elevated_priority_lock(&list->lock);
    
    for(ent = list->first_dentry; ent != NULL; ent = ent->dev_next)
    {
        KDEBUG("invalidate_dentry: dev 0x%x, n 0x%x\n", ent->dev, ent->inode);
        
        //if(ent->node != dir)
        if(ent->inode != dir->inode)
        {
            prev = ent;
            continue;
        }
        
        KDEBUG("invalidate_dentry: ent->refs = %d\n", ent->refs);
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        refs = ent->refs;

        while(refs)
        {
            elevated_priority_unlock(&list->lock);
            scheduler();
            elevated_priority_relock(&list->lock);
            refs = ent->refs;
        }
        
        if(prev)
        {
            prev->dev_next = ent->dev_next;
        }
        
        free_dentry(ent);
        
        if(ent == list->first_dentry)
        {
            list->first_dentry = NULL;
        }
        
        break;
    }

    elevated_priority_unlock(&list->lock);
}


void invalidate_dev_dentries(dev_t dev)
{
    volatile int refs;
    int maj, min;
    struct dentry_t *ent, *ent2;
    struct dentry_list_t *list;
    //register struct task_t *ct = get_cur_task();
    
    maj = MAJOR(dev);
    min = MINOR(dev);
    
    if(maj >= NR_DEV || min >= NR_DEV)
    {
        return;
    }

    if(!bdev_tab[maj].dentry_list)
    {
        return;
    }

    list = &(bdev_tab[maj].dentry_list[min]);
    //list = &bdev_tab[maj].dentry_list;
    elevated_priority_lock(&list->lock);
    
    for(ent = list->first_dentry; ent != NULL; )
    {
        refs = ent->refs;

        while(refs)
        {
            elevated_priority_unlock(&list->lock);
            scheduler();
            elevated_priority_relock(&list->lock);
            refs = ent->refs;
        }
        
        ent2 = ent->dev_next;
        free_dentry(ent);
        ent = ent2;
        
        // fix the linked list link so no one will try to access the deleted
        // ent while we sleep in the loop above
        list->first_dentry = ent;
    }

    list->first_dentry = NULL;
    elevated_priority_unlock(&list->lock);
}


void remove_old_dentries(unsigned long long older_than_ticks)
{
    struct bdev_ops_t *dev, *ldev = &bdev_tab[NR_DEV];
    struct dentry_list_t *list, *llist;
    struct dentry_t *ent, *ent2, *prev;
    unsigned long long older_than = ticks - older_than_ticks;

    // check that the given time have passed since booting
    if(ticks <= older_than_ticks)
    {
        return;
    }
    
    for(dev = bdev_tab; dev < ldev; dev++)
    {
        if(!dev->dentry_list)
        {
            continue;
        }

        for(list = dev->dentry_list, llist = &dev->dentry_list[NR_DEV];
            list < llist;
            list++)
        {
            elevated_priority_lock(&list->lock);

            for(prev = NULL, ent = list->first_dentry; ent != NULL; )
            {
                if(ent->refs || ent->last_accessed >= older_than)
                {
                    prev = ent;
                    ent = ent->dev_next;
                    continue;
                }

                if(prev)
                {
                    prev->dev_next = ent->dev_next;
                }
        
                ent2 = ent->dev_next;
                free_dentry(ent);
        
                if(ent == list->first_dentry)
                {
                    list->first_dentry = ent2;
                    prev = NULL;
                }

                ent = ent2;
            }

            elevated_priority_unlock(&list->lock);
        }
    }
}

