/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023 (c)
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
    struct dentry_t *ent;
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
    kernel_mutex_lock(&list->lock);
    
    for(ent = list->first_dentry; ent != NULL; ent = ent->dev_next)
    {
        if(ent->inode == dir->inode)
        {
            //kernel_mutex_lock(&ent->lock);
            ent->refs++;
            //kernel_mutex_unlock(&ent->lock);
            kernel_mutex_unlock(&list->lock);
            *dent = ent;
            return 0;
        }
    }
    
    // dentry not found, so we try to create it now
    if((res = getpath(dir, &path)) < 0)
    {
        kernel_mutex_unlock(&list->lock);
        return -ENOENT;
    }
    
    if(!(ent = alloc_dentry(dir, path)))
    {
        kernel_mutex_unlock(&list->lock);
        kfree(path);
        return -ENOMEM;
    }
    
    // now add it to the device's dentries list
    add_to_list(list, ent);
    
    //kernel_mutex_lock(&ent->lock);
    kernel_mutex_unlock(&list->lock);
    *dent = ent;
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
    kernel_mutex_lock(&list->lock);

    for(dent = list->first_dentry; dent != NULL; dent = dent->dev_next)
    {
        //if(dent->node == file)
        if(dent->inode == file->inode)
        {
            // it does - no need to do anything else
            kernel_mutex_unlock(&list->lock);
            return 0;
        }
    }

    // get_dentry() needs to lock the list itself
    kernel_mutex_unlock(&list->lock);

    //if(dir->dev == TO_DEVID(240, 3) || file->dev == TO_DEVID(240, 3)) __asm__ __volatile__("xchg %%bx, %%bx"::);
    
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
    
    /*
    printk("create_file_dentry: path '%s'\n", path);
    screen_refresh(NULL);
    __asm__ __volatile__("xchg %%bx, %%bx"::);
    */

    release_dentry(dent);
    
    if(!(dent = alloc_dentry(file, path)))
    {
        kfree(path);
        return -ENOMEM;
    }

    // now add it to the device's dentries list
    list = &(bdev_tab[majf].dentry_list[minf]);
    //list = &bdev_tab[majf].dentry_list;
    kernel_mutex_lock(&list->lock);
    add_to_list(list, dent);
    kernel_mutex_unlock(&list->lock);
    release_dentry(dent);

    return 0;
}


void release_dentry(struct dentry_t *ent)
{
    if(!ent)
    {
        return;
    }
    
    kernel_mutex_lock(&ent->list->lock);
    //kernel_mutex_lock(&ent->lock);
    ent->refs--;
    //kernel_mutex_unlock(&ent->lock);
    kernel_mutex_unlock(&ent->list->lock);
}


/*
 * We invalidate a dentry when:
 *    - the inode is freed (i.e. links == 0)
 *    - containing device is unmounted (all dev dentries are invalidated)
 */
void invalidate_dentry(struct fs_node_t *dir)
{
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
    kernel_mutex_lock(&list->lock);
    
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
        
        while(ent->refs)
        {
            kernel_mutex_unlock(&list->lock);
            lock_scheduler();
            //preempt(&ct->r);
            scheduler();
            unlock_scheduler();
            kernel_mutex_lock(&list->lock);
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

    kernel_mutex_unlock(&list->lock);
}


void invalidate_dev_dentries(dev_t dev)
{
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
    kernel_mutex_lock(&list->lock);
    
    for(ent = list->first_dentry; ent != NULL; )
    {
        while(ent->refs)
        {
            kernel_mutex_unlock(&list->lock);
            lock_scheduler();
            //preempt(&ct->r);
            scheduler();
            unlock_scheduler();
            kernel_mutex_lock(&list->lock);
        }
        
        ent2 = ent->dev_next;
        free_dentry(ent);
        ent = ent2;
        
        // fix the linked list link so no one will try to access the deleted
        // ent while we sleep in the loop above
        list->first_dentry = ent;
    }

    list->first_dentry = NULL;
    kernel_mutex_unlock(&list->lock);
}

