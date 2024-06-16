/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: procfs_file.c
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
 *  \file procfs_file.c
 *
 *  This file implements some procfs filesystem functions (mainly the ones
 *  used to read from "regular" procfs files).
 *  Functions implementing filesystem operations are exported to the rest of
 *  the kernel via the \ref procfs_ops structure, which is defined in procfs.c.
 */

//#define __DEBUG

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/sysinfo.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>
#include <kernel/user.h>
#include <kernel/clock.h>
#include <kernel/pci.h>
#include <kernel/modules.h>
#include <kernel/softint.h>
#include <kernel/pcache.h>
#include <kernel/ipc.h>
#include <kernel/net/dhcp.h>
#include <kernel/ksymtab.h>
#include <mm/mmngr_phys.h>
#include <mm/mmngr_virtual.h>
#include <mm/kheap.h>
#include <mm/kstack.h>
#include <fs/procfs.h>
#include <fs/devfs.h>
#include <fs/tmpfs.h>

#define PAGES_TO_KBS(x)         ((x) * PAGE_SIZE / 1024)

/*
 * Read /proc/devices.
 */
size_t get_device_list(char **_buf)
{
    struct devnode_t *dev = dev_list;
    size_t len, count = 0, bufsz = 2048;
    char *buf, *p;
    char tmp[32];         // max dev name is 8 chars plus room for formatting
    
    PR_MALLOC(buf, bufsz);
    p = buf;
    *p = '\0';
    
    while(dev)
    {
        //sprintf(tmp, "%c %3d, %3d %s\n", S_ISBLK(dev->mode) ? 'b' : 'c',
        ksprintf(tmp, sizeof(tmp), "%c %3d, %3d %s\n",
                                         S_ISBLK(dev->mode) ? 'b' : 'c',
                                         (int)MAJOR(dev->dev),
                                         (int)MINOR(dev->dev),
                                         dev->name);
        len = strlen(tmp);
        
        if(count + len >= bufsz)
        {
            *_buf = buf;
            PR_REALLOC(buf, bufsz, count);
            p = buf + count;
        }

        count += len;
        
        /*
        if(count >= PAGE_SIZE)
        {
            break;
        }
        */
        
        strcpy(p, tmp);
        p += len;
        dev = dev->next;
    }
    
    *_buf = buf;
    return count;
}


/*
 * Read /proc/interrupts.
 */
size_t get_interrupt_info(char **_buf)
{
    size_t len, count = 0, bufsz = 2048;
    char *buf, *p;
    char tmp[48];
    int i;
    
    PR_MALLOC(buf, bufsz);
    p = buf;
    *p = '\0';
    
    ksprintf(buf, bufsz, "IRQ        Hits      Ticks Name\n");
    count = strlen(p);
    p += count;
    
    for(i = 32; i < (32 + 16); i++)
    {
        if(interrupt_handlers[i] == NULL)
        {
            ksprintf(tmp, sizeof(tmp), "%3d: %10d %10d %s\n", i - 32, 
                                       0, 0, "--");
        }
        else
        {
            ksprintf(tmp, sizeof(tmp), "%3d: %10d %10d %s\n", i - 32, 
                                       interrupt_handlers[i]->hits,
                                       interrupt_handlers[i]->ticks,
                                       interrupt_handlers[i]->short_name);
        }

        len = strlen(tmp);
        
        if(count + len >= bufsz)
        {
            *_buf = buf;
            PR_REALLOC(buf, bufsz, count);
            p = buf + count;
        }

        count += len;
        
        /*
        if(count >= PAGE_SIZE)
        {
            break;
        }
        */
        
        strcpy(p, tmp);
        p += len;
    }
    
    *_buf = buf;
    return count;
}


/*
 * Read /proc/filesystems.
 */
size_t get_fs_list(char **_buf)
{
    struct fs_info_t *f = fstab;
    struct fs_info_t *lf = &fstab[NR_FILESYSTEMS];
    //size_t len, count = 0;
    char tmp[16];         // max fs name is 8 chars
    size_t len, count = 0, bufsz = (8 + 2) * NR_FILESYSTEMS;
    char *buf, *p;

    PR_MALLOC(buf, bufsz);
    p = buf;
    *p = '\0';
    
    for( ; f < lf; f++)
    {
        if(f->name[0] == 0)
        {
            continue;
        }

        //sprintf(tmp, "%s\n", f->name);
        ksprintf(tmp, sizeof(tmp), "%s\n", f->name);
        len = strlen(tmp);
        count += len;
        
        /*
        if(count >= PAGE_SIZE)
        {
            break;
        }
        */
        
        strcpy(p, tmp);
        p += len;
    }
    
    *_buf = buf;
    return count;
}


/*
 * Read /proc/uptime.
 */
size_t get_uptime(char **buf)
{
    register struct task_t *idle_task = get_idle_task();
    time_t uptime = monotonic_time.tv_sec;  // now();
    time_t idle = (idle_task->user_time + idle_task->sys_time) / PIT_FREQUENCY;

    PR_MALLOC(*buf, 32);
    ksprintf(*buf, 32, "%ld %ld\n", (long int)uptime, (long int)idle);

    return strlen(*buf);
}


/*
 * Read /proc/version.
 */
size_t get_version(char **buf)
{
    PR_MALLOC(*buf, 64);
    ksprintf(*buf, 64, "%s %s %s\n", ostype, osrelease, version);

    return strlen(*buf);
}


/*
 * Read /proc/vmstat.
 */
size_t get_vmstat(char **buf)
{
    size_t memfree = pmmngr_get_free_block_count();
    size_t ptables = used_pagetable_count();
    size_t kstacks = get_kstack_count();
    size_t shms = get_shm_page_count();
    
    PR_MALLOC(*buf, 128);
    ksprintf(*buf, 128, "nr_free_pages %lu\n"
                  "nr_page_table_pages %lu\n"
                  "nr_kernel_stack %lu\n"
                  "nr_shmem %lu\n",
                  memfree, ptables, kstacks, shms);

    return strlen(*buf);
}


static void get_mapped_pagecount(size_t *mapped, size_t *anon)
{
    *mapped = 0;
    *anon = 0;

    elevated_priority_lock(&task_table_lock);

    for_each_taskptr(t)
    {
        if(*t)
        {
            (*mapped) += memregion_data_pagecount(*t);
            (*mapped) += memregion_text_pagecount(*t);
            (*anon) += memregion_anon_pagecount(*t);
        }
    }
    
    elevated_priority_unlock(&task_table_lock);
}


/*
 * Read /proc/meminfo.
 */
size_t get_meminfo(char **buf)
{
    char *p;
    size_t memtotal = PAGES_TO_KBS(pmmngr_get_memory_size());
    size_t memfree = PAGES_TO_KBS(pmmngr_get_free_block_count());
    size_t memavail = PAGES_TO_KBS(pmmngr_get_available_block_count());
    //size_t bufs = PAGES_TO_KBS(NR_DISK_BUFFERS);
    size_t bufs = PAGES_TO_KBS(get_cached_block_count());
    size_t ptables = PAGES_TO_KBS(used_pagetable_count());
    size_t cached = PAGES_TO_KBS(get_cached_page_count());
    size_t kstacks = PAGES_TO_KBS(get_kstack_count());
    size_t mapped, anon;

    get_mapped_pagecount(&mapped, &anon);

    mapped = PAGES_TO_KBS(mapped);
    anon = PAGES_TO_KBS(anon);
    
    /*
     * TODO: fill the fields for system load, shared ram, total/free swap,
     *       total/free high memory.
     */
    PR_MALLOC(*buf, 1024);
    p = *buf;

    ksprintf(p, 1024, "MemTotal:      %lu kB\n"
                      "MemFree:       %lu kB\n"
                      "MemAvailable:  %lu kB\n"
                      "Buffers:       %lu kB\n"
                      "Cached:        %lu kB\n",
                      memtotal, memfree, memavail, bufs, cached);
    p += strlen(p);

    ksprintf(p, 1024, "SwapTotal:     %lu kB\n"
                      "SwapFree:      %lu kB\n"
                      "KernelStack:   %lu kB\n"
                      "PageTables:    %lu kB\n",
                      (size_t)0, (size_t)0, kstacks, ptables);
    p += strlen(p);

    ksprintf(p, 1024, "AnonPages:     %lu kB\n"
                      "Mapped:        %lu kB\n",
                      anon, mapped);
    p += strlen(p);

    //return strlen(*buf);
    return (p - *buf);
}


/*
 * Read /proc/modules.
 */
size_t get_modules(char **buf)
{
    struct kmodule_t *mod;
    size_t len, count = 0, bufsz = 512;
    char tmp[512];
    char *p;

    PR_MALLOC(*buf, bufsz);
    p = *buf;
    *p = '\0';

    //kernel_mutex_lock(&kmod_list_mutex);

    for(mod = modules_head.next; mod != NULL; mod = mod->next)
    {
        ksprintf(tmp, 512, "%s\t%s\t%s\t%s\n", mod->modinfo.name,
                                         mod->modinfo.author,
                                         mod->modinfo.desc,
                                         mod->modinfo.deps ?
                                            mod->modinfo.deps : "[NULL]");
        len = strlen(tmp);
        
        if(count + len >= bufsz)
        {
            PR_REALLOC(*buf, bufsz, count);
            p = *buf + count;
        }

        count += len;
        strcpy(p, tmp);
        p += len;
    }

    //kernel_mutex_unlock(&kmod_list_mutex);

    return count;
}


/*
 * Read /proc/mounts.
 */
size_t get_mounts(char **buf)
{
    volatile size_t count = 0, bytes = 0;
    volatile struct dirent *devent;
    volatile char *fsname, *fsmount, *fsopts;
    volatile dev_t dev;
    struct fs_node_t *mpoint;
    struct dentry_t *dent = NULL;
    struct mount_info_t *d = mounttab;
    struct mount_info_t *ld = &mounttab[NR_SUPER];
    size_t bufsz = 4096;
    char *p;

    PR_MALLOC(*buf, bufsz);
    p = *buf;
    *p = '\0';
    
    kernel_mutex_lock(&mount_table_mutex);

    for( ; d < ld; d++)
    {
        if(!d->dev || !d->fs || !d->mpoint)
        {
            continue;
        }
        
        if(strcmp(d->fs->name, "devfs") == 0 ||
           strcmp(d->fs->name, "tmpfs") == 0 ||
           strcmp(d->fs->name, "procfs") == 0 ||
           strcmp(d->fs->name, "devpts") == 0)
        {
            //special_type = 1;
            fsname = d->fs->name;
            devent = NULL;
        }
        else
        {
            if(devfs_find_deventry(d->dev, 1, (struct dirent **)&devent) != 0)
            {
                continue;
            }
            
            fsname = devent->d_name;
        }
        
        // avoid locking ourselves when we call get_dentry()
        dev = d->dev;
        mpoint = d->mpoint;
        bytes = 0;
        INC_NODE_REFS(d->mpoint);
        kernel_mutex_unlock(&mount_table_mutex);

        
        if(get_dentry(/* d-> */ mpoint, &dent) < 0)
        {
            if(devent)
            {
                kfree((void *)devent);
            }

            release_node(/* d-> */ mpoint);
            kernel_mutex_lock(&mount_table_mutex);
            continue;
        }
        
        if(!(fsmount = dent->path))
        {
            goto cont;
        }
        
        
        // check the device wasn't unmounted while we chased its dentry
        if(d->dev != dev)
        {
            goto cont;
        }


        fsopts = d->mountopts ? d->mountopts : "defaults";

        // estimate the needed space make sure we have enough buffer space
        bytes = strlen((char *)fsname) + strlen((char *)fsmount) +
                strlen(d->fs->name) +
                strlen((char *)fsopts) +
                6 /* spaces */ + 4 /* 2 x 2-digit numbers */ +
                5 /* potential '/dev/' prefix */;

        if((bytes + count) > bufsz)
        {
            //kpanic("Insufficient buffer space (in get_mounts())");
            PR_REALLOC(*buf, bufsz, count);
            p = *buf + count;
        }

        /*
        if(count >= PAGE_SIZE)
        {
            break;
        }
        */

        /*
         * For now, we pass 0 for the dump & fsck passno fields.
         * FIXME:
         */
        if(devent)
        {
            //sprintf(p, "/dev/%s %s %s %s %d %d\n",
            ksprintf(p, (bufsz - count), "/dev/%s %s %s %s %d %d\n",
                     fsname, fsmount, d->fs->name, fsopts, 0, 0);
        }
        else
        {
            //sprintf(p, "%s %s %s %s %d %d\n",
            ksprintf(p, (bufsz - count), "%s %s %s %s %d %d\n",
                     fsname, fsmount, d->fs->name, fsopts, 0, 0);

            //kfree((void *)devent);
        }
        
        bytes = strlen(p);

cont:
        if(devent)
        {
            kfree((void *)devent);
        }

        //kfree((void *)fsmount);
        release_dentry(dent);
        count += bytes;
        p += bytes;

        release_node(/* d-> */ mpoint);
        kernel_mutex_lock(&mount_table_mutex);
    }
    
    kernel_mutex_unlock(&mount_table_mutex);
    //printk("get_mounts: done\n");

    return count;
}


/*
 * Read /proc/stat.
 */
size_t get_sysstat(char **buf)
{
    struct task_t *idle_task = get_idle_task();
    unsigned long user = 0, sys = 0, idle = 0;
    unsigned long irq_hits = 0, irq_ticks = 0, softirq = 0;
    unsigned int running = 0, blocked = 0;
    int i;
    char *p;

    PR_MALLOC(*buf, 2048);
    p = *buf;
    *p = '\0';
    
    /*
     * TODO: Collect the rest of info for /proc/stat.
     *       See: https://man7.org/linux/man-pages/man5/proc.5.html
     */
    
    elevated_priority_lock(&task_table_lock);

    // get task stats
    for_each_taskptr(t)
    {
        if(!*t)
        {
            continue;
        }
        
        if((*t)->state == TASK_RUNNING || (*t)->state == TASK_READY)
        {
            running++;
        }
        else if((*t)->state == TASK_WAITING || (*t)->state == TASK_SLEEPING)
        {
            blocked++;
        }

        if(*t == idle_task)
        {
            idle += (*t)->user_time + (*t)->children_user_time;
        }
        else if(*t == softsleep_task || *t == softitimer_task)
        //else if(*t == softint_task)
        {
            softirq += (*t)->user_time + (*t)->children_user_time;
        }
        else
        {
            user += (*t)->user_time + (*t)->children_user_time;
            sys += (*t)->sys_time + (*t)->children_sys_time;
        }
    }
    
    elevated_priority_unlock(&task_table_lock);

    // get IRQ stats
    for(i = 32; i < (32 + 16); i++)
    {
        if(interrupt_handlers[i] != NULL)
        {
            irq_ticks += interrupt_handlers[i]->ticks;
            irq_hits += interrupt_handlers[i]->hits;
        }
    }

    // now print to the buffer
    ksprintf(p, 1024, "cpu %lu %lu %lu %lu %lu\n",
                      user, sys, idle, irq_ticks, softirq);
    p += strlen(p);

    ksprintf(p, 1024, "intr %lu\n"
                      "ctxt %lu\n"
                      "btime %ld\n"
                      "processes %lu\n"
                      "procs_running %u\n"
                      "procs_blocked %u\n",
                      irq_hits,
                      system_context_switches,
                      (long int)startup_time,
                      system_forks, running, blocked);

    return strlen(*buf);
}


/*
 * Read /proc/bus/pci/devices.
 */
size_t get_pci_device_list(char **_buf)
{
    int i, j;
    size_t len, count = 0;
    size_t bufsz = 1024;
    char tmp[64];
    char *buses, *buf, *p;
    int bus_count;
    struct pci_dev_t *pci;

    *_buf = NULL;

    if(active_pci_buses(&buses, &bus_count) != 0)
    {
        return 0;
    }

    PR_MALLOC(buf, bufsz);
    p = buf;
    *p = '\0';

    for(i = 0, j = 0; i < bus_count; i++)
    {
        for(pci = first_pci; pci != NULL; pci = pci->next)
        {
            if(pci->bus != buses[i])
            {
                continue;
            }

            ksprintf(tmp, sizeof(tmp),
                    "%04x\t%04x\t%02x\t%04x\t%02x\t%04x\t%04x\t%02x\n",
                    pci->base_class, pci->sub_class,
                    pci->bus, pci->dev, pci->function,
                    pci->vendor, pci->dev_id, pci->rev);

            len = strlen(tmp);

            if(count + len >= bufsz)
            {
                *_buf = buf;
                PR_REALLOC(buf, bufsz, count);
                p = buf + count;
            }

            count += len;

            /*
            if(count >= buflen)
            {
                break;
            }
            */

            strcpy(p, tmp);
            p += len;
            j++;
        }

        /*
        if(count >= buflen)
        {
            break;
        }
        */
    }
    
    kfree(buses);
    
    *_buf = buf;
    return count;
}


/*
 * Read /proc/bus/pci/XX/YY.ZZ.
 */
size_t get_pci_device_config_space(struct pci_dev_t *pci, char **_buf)
{
    size_t i, bufsz = 256, words = bufsz / sizeof(uint32_t);
    char *buf;
    uint32_t *p;

    *_buf = NULL;
    PR_MALLOC(buf, bufsz);
    p = (uint32_t *)buf;

    for(i = 0; i < words; i++)
    {
        *p++ = pci_config_read_long(pci->bus, pci->dev, pci->function,
                                    i * sizeof(uint32_t));
    }
    
    *_buf = buf;
    return bufsz;
}


/*
 * Read /proc/net/resolv.conf.
 */
size_t get_dns_list(char **buf)
{
    struct dhcp_client_cookie_t *dhcpc;
    size_t len, count = 0, bufsz = 1024;
    char tmp[64];
    char *p;
    int i;

    PR_MALLOC(*buf, bufsz);
    p = *buf;
    *p = '\0';

    ksprintf(p, 256, "# Dynamic resolv.conf file for connecting local\n"
                     "# clients to DNS servers.\n#\n"
                     "# This file is maintained by the kernel.\n#\n"
                     "# DO NOT edit, as your changes WILL NOT be saved!\n\n");
    len = strlen(p);
    count += len;
    p += len;
    
    //kernel_mutex_lock(&dhcp_cookie_lock);

#define ADDR_BYTE(addr, shift)      (((addr) >> (shift)) & 0xff)

    for(dhcpc = dhcp_cookies; dhcpc != NULL; dhcpc = dhcpc->next)
    {
        for(i = 0; i < 2; i++)
        {
            if(dhcpc->dns[i].s_addr)
            {
                ksprintf(tmp, 64, "nameserver %u.%u.%u.%u\n",
                              ADDR_BYTE(dhcpc->dns[i].s_addr, 0 ),
                              ADDR_BYTE(dhcpc->dns[i].s_addr, 8 ),
                              ADDR_BYTE(dhcpc->dns[i].s_addr, 16),
                              ADDR_BYTE(dhcpc->dns[i].s_addr, 24));
                len = strlen(tmp);
                
                if(count + len >= bufsz)
                {
                    PR_REALLOC(*buf, bufsz, count);
                    p = *buf + count;
                }

                count += len;
                strcpy(p, tmp);
                p += len;
            }
        }
    }

#undef ADDR_BYTE

    //kernel_mutex_unlock(&dhcp_cookie_lock);

    return count;
}


/*
 * Read /proc/ksyms.
 */
size_t get_ksyms(char **buf)
{
    struct hashtab_item_t *hitem;
    size_t len, count = 0, bufsz = 2048;
    char tmp[64];
    char *p;
    int i;

    if(!ksymtab)
    {
        return 0;
    }

    PR_MALLOC(*buf, bufsz);
    p = *buf;
    *p = '\0';

    for(i = 0; i < ksymtab->count; i++)
    {
        hitem = ksymtab->items[i];
        
        while(hitem)
        {
            ksprintf(tmp, 64, "%lx  %s\n", hitem->val, hitem->key);
            len = strlen(tmp);
            
            if(count + len >= bufsz)
            {
                PR_REALLOC(*buf, bufsz, count);
                p = *buf + count;
            }

            count += len;
            strcpy(p, tmp);
            p += len;

            hitem = hitem->next;
        }
    }

    return count;
}

