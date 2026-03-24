/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
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

#include "../kernel/task_funcs.c"

#define BUF_SPRINTF(msg)                                \
do {                                                    \
    ksprintf((char *)buf, 1024, msg);                   \
    len = strlen((char *)buf);                          \
    buf += len;                                         \
    buflen += len;                                      \
} while(0)

#define PAGES_TO_KBS(x)         ((x) * PAGE_SIZE / 1024)


/*
 * Read /proc/devices.
 */
size_t get_device_list(char **_buf)
{
    volatile size_t buflen = 0;
    volatile char *buf;
    size_t len = 0;

    *_buf = NULL;
    PR_MALLOC(buf, 1024);
    *_buf = (char *)buf;

    BUF_SPRINTF("Character devices:\n");
    BUF_SPRINTF("  1 mem\n");
    BUF_SPRINTF("  4 tty\n");
    BUF_SPRINTF("  5 /dev/tty\n");
    BUF_SPRINTF("  5 /dev/console\n");
    BUF_SPRINTF("  5 /dev/ptmx\n");
    BUF_SPRINTF(" 10 misc\n");
    BUF_SPRINTF(" 13 input\n");
    BUF_SPRINTF(" 14 audio\n");
    BUF_SPRINTF(" 29 fb\n");
    BUF_SPRINTF("136 pts\n");

    BUF_SPRINTF("\nBlock devices:\n");
    BUF_SPRINTF("  1 ram\n");
    BUF_SPRINTF("  3 hd\n");
    BUF_SPRINTF("  7 loop\n");
    BUF_SPRINTF("  8 sd\n");
    BUF_SPRINTF(" 22 hd\n");

    return buflen;

#if 0
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
#endif
}


#define PRINT_INTERRUPT(i)                          \
    if(count + rowlen >= bufsz) {                   \
        *_buf = buf;                                \
        PR_REALLOC(buf, bufsz, count);              \
        p = buf + count;                            \
    }                                               \
    if(i == 123) ksprintf(p, bufsz, "LOC: ");       \
    else if(i == 124) ksprintf(p, bufsz, "TLB: ");  \
    else if(i == 255) ksprintf(p, bufsz, "SPU: ");  \
    else ksprintf(p, bufsz, "%3d: ", i);            \
    p += 5;                                         \
    for(k = 0; k < processor_count; k++) {          \
        ksprintf(p, bufsz, "%10d ", processor_local_data[k].irq_count[i]); \
        p += 11;                                    \
    }                                               \
    ksprintf(p, bufsz, " %s\n", interrupt_handlers[i] ? \
             interrupt_handlers[i]->short_name : "--"); \
    len = strlen(p);                                \
    count += (5 + (processor_count * 11) + len);    \
    p += len;


/*
 * Read /proc/interrupts.
 */
size_t get_interrupt_info(char **_buf)
{
    size_t len, count = 0, bufsz = 2048;
    size_t rowlen;
    char *buf, *p;
    //char tmp[128];
    int i, k;

    PR_MALLOC(buf, bufsz);
    p = buf;
    *p = '\0';

    // maximum length of a single row
    rowlen = 5 + (processor_count * 11) + 
                sizeof(interrupt_handlers[0]->short_name) + 1;

    // print the header
    ksprintf(p, bufsz, " IRQ  ");
    count += 6;
    p += 6;

    for(i = 0; i < processor_count; i++)
    {
        ksprintf(p, bufsz, "     CPU%-2d ", i);
        count += 11;
        p += 11;
    }

    ksprintf(p, bufsz, " Name\n");
    count += 6;
    p += 6;

    // now print the rows
    for(i = 0; i < 256; i++)
    {
        PRINT_INTERRUPT(i);
    }

    *_buf = buf;
    return count;
}


STATIC_INLINE int is_special_fs(char *fsname)
{
    return (strcmp(fsname, "sysfs") == 0 ||
            strcmp(fsname, "tmpfs") == 0 ||
            strcmp(fsname, "procfs") == 0 ||
            strcmp(fsname, "sockfs") == 0 ||
            strcmp(fsname, "pipefs") == 0 ||
            strcmp(fsname, "ramfs") == 0 ||
            strcmp(fsname, "devpts") == 0 ||
            strcmp(fsname, "devfs") == 0 ||
            strcmp(fsname, "rootfs") == 0 ||
            strcmp(fsname, "efivarfs") == 0);
}


/*
 * Read /proc/filesystems.
 */
size_t get_fs_list(char **_buf)
{
    struct fs_info_t *f = fstab;
    struct fs_info_t *lf = &fstab[NR_FILESYSTEMS];
    size_t len, count = 0;
    // max fs name is 8 chars, plus 8 for the 'nodev' prefix and spaces
    size_t bufsz = (16 + 2) * NR_FILESYSTEMS;
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

        ksprintf(p, bufsz, "%s   %s\n", is_special_fs(f->name) ? "nodev" : "     ", f->name);

        len = strlen(p);
        count += len;
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
    volatile struct task_t *idle_task;
    time_t uptime = monotonic_time.tv_sec;  // now();
    time_t idle = 0;
    int i;

    for(i = 0; i < processor_count; i++)
    {
        if(processor_local_data[i].idle_task)
        {
            idle_task = processor_local_data[i].idle_task;
            idle += (idle_task->user_time + idle_task->sys_time) / PIT_FREQUENCY;
        }
    }

    PR_MALLOC(*buf, 32);
    ksprintf(*buf, 32, "%ld %ld\n", (long int)uptime, (long int)idle);

    return strlen(*buf);
}


/*
 * Read /proc/cmdline.
 */
size_t get_cmdline(char **buf)
{
    size_t len = strlen(kernel_cmdline) + 2;

    PR_MALLOC(*buf, len);
    ksprintf(*buf, len, "%s\n", kernel_cmdline);

    return strlen(*buf);
}


/*
 * Read /proc/self.
 */
size_t get_self(char **buf)
{
    PR_MALLOC(*buf, 16);
    ksprintf(*buf, 16, "/proc/%u", tgid(this_core->cur_task));

    return strlen(*buf);
}


/*
 * Read /proc/thread-self.
 */
size_t get_thread_self(char **buf)
{
    PR_MALLOC(*buf, 32);
    ksprintf(*buf, 32, "/proc/%u/task/%u",
                       tgid(this_core->cur_task), this_core->cur_task->pid);

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


/*
 * Read /proc/loadavg.
 */
size_t get_loadavg(char **buf)
{
    char *p;
    int running = get_running_task_count();
    int total = total_tasks /* get_total_task_count() */;
    unsigned long avg[3];

    PR_MALLOC(*buf, 256);
    p = *buf;

    avg[0] = avenrun[0] + (FIXED_1 / 200);
    avg[1] = avenrun[1] + (FIXED_1 / 200);
    avg[2] = avenrun[2] + (FIXED_1 / 200);

    ksprintf(p, 256, "%d.%d ", LOAD_INT(avg[0]), LOAD_FRAC(avg[0]));
    p += strlen(p);
    ksprintf(p, 256, "%d.%d ", LOAD_INT(avg[1]), LOAD_FRAC(avg[1]));
    p += strlen(p);
    ksprintf(p, 256, "%d.%d ", LOAD_INT(avg[2]), LOAD_FRAC(avg[2]));
    p += strlen(p);

    ksprintf(p, 256, "%d/%d %u\n", running, total, next_pid);
    p += strlen(p);

    return (p - *buf);
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
    size_t dirty = PAGES_TO_KBS(get_dirty_cached_block_count());
    size_t tmpfs = PAGES_TO_KBS(get_tmpfs_pagecount());
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

    ksprintf(p, 1024, "Dirty:         %lu kB\n"
                      "AnonPages:     %lu kB\n"
                      "Mapped:        %lu kB\n"
                      "Shmem:         %lu kB\n",
                      dirty, anon, mapped, tmpfs);
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
        ksprintf(tmp, 512, "%s\t%u\t%s\t%s\t" _XPTR_ "\n",
                            mod->modinfo.name,
                            mod->memsz,
                            mod->modinfo.deps ? mod->modinfo.deps : "-",
                            (mod->state & MODULE_STATE_LOADED) ?
                                "Loaded" : "Unloaded",
                            mod->mempos - KERNEL_MEM_START);
        /*
        ksprintf(tmp, 512, "%s\t%s\t%s\t%s\n", mod->modinfo.name,
                                         mod->modinfo.author,
                                         mod->modinfo.desc,
                                         mod->modinfo.deps ?
                                            mod->modinfo.deps : "[NULL]");
        */
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


#define FORMAT_FOR_MOUNTS           1
#define FORMAT_FOR_MOUNTINFO        2
#define FORMAT_FOR_MOUNTSTATS       3

/*
 * Helper function to read mount-related files:
 *    /proc/mounts
 *    /proc/mountinfo
 *    /proc/mounstats
 */
static size_t __get_mounts(char **buf, int format)
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

        if(format == FORMAT_FOR_MOUNTS)
        {
            /*
             * For now, we pass 0 for the dump & fsck passno fields.
             * FIXME: see https://man.he.net/man5/procfs
             */
            if(devent)
            {
                ksprintf(p, (bufsz - count), "/dev/%s %s %s %s %d %d\n",
                         fsname, fsmount, d->fs->name, fsopts, 0, 0);
            }
            else
            {
                ksprintf(p, (bufsz - count), "%s %s %s %s %d %d\n",
                         fsname, fsmount, d->fs->name, fsopts, 0, 0);
            }
        }
        else if(format == FORMAT_FOR_MOUNTINFO)
        {
            /*
             * For now, we pass dummy values for some fields below.
             * FIXME: see https://man.he.net/man5/procfs
             */
            ksprintf(p, (bufsz - count), "%d %d %u:%u %s %s %s - %s %s %s\n",
                         (d - mounttab) + 1, (d - mounttab) + 1,
                         MAJOR(d->dev),     /* dummy mount ID */
                         MINOR(d->dev),     /* dummy parent ID */
                         "/",   /* assume this for in-mount root dir */
                         fsmount, fsopts,
                         d->fs->name,       /* dummy filesys-specific info */
                         d->fs->name,       /* dummy per-superblock options */
                         (d->mountflags & MS_RDONLY) ? "ro" : "rw");
        }
        else if(format == FORMAT_FOR_MOUNTSTATS)
        {
            /*
             * For now, we do not report any actual statistics.
             * FIXME: see https://man.he.net/man5/procfs
             */
            if(devent)
            {
                ksprintf(p, (bufsz - count), "device /dev/%s mounted on %s with fstype %s\n",
                         fsname, fsmount, d->fs->name);
            }
            else
            {
                ksprintf(p, (bufsz - count), "device %s mounted on %s with fstype %s\n",
                         fsname, fsmount, d->fs->name);
            }
        }
        else
        {
            kpanic("invalid format passed to __get_mounts()\n");
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
 * Read /proc/mounts.
 */
size_t get_mounts(char **buf)
{
    return __get_mounts(buf, FORMAT_FOR_MOUNTS);
}


/*
 * Read /proc/mountinfo.
 */
size_t get_mountinfo(char **buf)
{
    return __get_mounts(buf, FORMAT_FOR_MOUNTINFO);
}


/*
 * Read /proc/mountstats.
 */
size_t get_mountstats(char **buf)
{
    return __get_mounts(buf, FORMAT_FOR_MOUNTSTATS);
}


/*
 * Read /proc/stat.
 */
size_t get_sysstat(char **buf)
{
    //struct task_t *idle_task = get_idle_task();
    unsigned long tmp;
    unsigned long user = 0, sys = 0, idle = 0;
    unsigned long *irq_hits, total_irq_hits = 0;
    unsigned long irq_ticks = 0, softirq = 0;
    unsigned int running = 0, blocked = 0;
    int i, j, state;
    char *p;

    PR_MALLOC(*buf, 8192);
    p = *buf;
    *p = '\0';

    if(!(irq_hits = kmalloc(256 * sizeof(unsigned long))))
    {
        kfree(*buf);
        *buf = NULL;
        return 0;
    }

    A_memset(irq_hits, 0, 256 * sizeof(unsigned long));

    /*
     * TODO: Collect the rest of info for /proc/stat.
     *       See: https://man7.org/linux/man-pages/man5/proc.5.html
     *            https://man.he.net/man5/procfs
     */
    
    elevated_priority_lock(&task_table_lock);

    // get task stats
    for_each_taskptr(t)
    {
        if(!*t)
        {
            continue;
        }

        state = get_task_state(*t);
        
        if(state == TASK_RUNNING || state == TASK_READY)
        {
            running++;
        }
        else if(state == TASK_WAITING || state == TASK_SLEEPING)
        {
            blocked++;
        }
    }
    
    elevated_priority_unlock(&task_table_lock);

    for(i = 0; i < processor_count; i++)
    {
        sys += processor_local_data[i].sys_time;
        user += processor_local_data[i].user_time;
        softirq += processor_local_data[i].softirq_ticks;

        if(processor_local_data[i].idle_task)
        {
            idle += processor_local_data[i].idle_task->sys_time;
            idle += processor_local_data[i].idle_task->user_time;
        }

        for(j = 0; j < 256; j++)
        {
            irq_ticks += processor_local_data[i].irq_ticks[j];
            total_irq_hits += processor_local_data[i].irq_count[j];
            irq_hits[j] += processor_local_data[i].irq_count[j];
        }
    }

    // now print to the buffer
    ksprintf(p, 8192, "cpu %lu %lu %lu %lu %lu %lu\n",
                      user, // time spent in user mode
                      0,    // TODO: time spent in user mode with low priority (nice)
                      sys,  // time spent in system mode
                      idle, // time spent in idle task
                      irq_ticks, // time servicing IRQs
                      softirq   // time seriving soft IRQs
                      );
    p += strlen(p);

    for(i = 0; i < processor_count; i++)
    {
        if(processor_local_data[i].idle_task)
        {
            tmp  = processor_local_data[i].idle_task->sys_time;
            tmp += processor_local_data[i].idle_task->user_time;
        }
        else
        {
            tmp = 0;
        }

        ksprintf(p, 8192, "cpu%d %lu %lu %lu %lu ",
                 i,
                 processor_local_data[i].user_time,
                 0,    // TODO: time spent in user mode with low priority (nice)
                 processor_local_data[i].sys_time,
                 tmp);
        p += strlen(p);

        tmp = 0;

        for(j = 0; j < 256; j++)
        {
            tmp += processor_local_data[i].irq_ticks[j];
        }

        ksprintf(p, 8192, "%lu %lu\n",
                 tmp,
                 processor_local_data[i].softirq_ticks);
        p += strlen(p);
    }

    ksprintf(p, 8192, "intr %lu ", total_irq_hits);
    p += strlen(p);

    for(j = 0; j < 256; j++)
    {
        ksprintf(p, 8192, "%lu%c", irq_hits[j], (j == 255) ? '\n' : ' ');
        p += strlen(p);
    }

    ksprintf(p, 8192, "swap %u %u\n"
                      "ctxt %lu\n"
                      "btime %ld\n"
                      "processes %lu\n"
                      "procs_running %u\n"
                      "procs_blocked %u\n",
                      0, 0,     // TODO: fix when we implement swapping
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
        *p++ = pci_config_read_long(pci, i * sizeof(uint32_t));
    }
    
    *_buf = buf;
    return bufsz;
}


/*
 * Read /proc/net/resolv.conf.
 */
size_t get_dns_list(char **buf)
{
    struct dhcp_binding_t *binding;
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

#define ADDR_BYTE(addr, shift)      (((addr) >> (shift)) & 0xff)

    for(binding = dhcp_bindings; binding != NULL; binding = binding->next)
    {
        for(i = 0; i < 2; i++)
        {
            if(binding->dns[i])
            {
                ksprintf(tmp, 64, "nameserver %u.%u.%u.%u\n",
                              ADDR_BYTE(binding->dns[i], 0 ),
                              ADDR_BYTE(binding->dns[i], 8 ),
                              ADDR_BYTE(binding->dns[i], 16),
                              ADDR_BYTE(binding->dns[i], 24));
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

    return count;
}


/*
 * Read /proc/ksyms.
 */
size_t get_ksyms(char **buf)
{
    struct hashtab_item_t *hitem;
    struct kmodule_t *mod;
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

    // first print kernel symbols
    for(i = 0; i < ksymtab->count; i++)
    {
        hitem = ksymtab->items[i];
        
        while(hitem)
        {
            /*
             * TODO: report symbol types
             */
            ksprintf(tmp, 64, "%16lx  %s\n", hitem->val, hitem->key);
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

    // next print module symbols
    kernel_mutex_lock(&kmod_list_mutex);

    for(mod = modules_head.next; mod != NULL; mod = mod->next)
    {
        if(!(mod->state & MODULE_STATE_LOADED))
        {
            continue;
        }
        
        for(i = 0; i < mod->symbols->count; i++)
        {
            hitem = mod->symbols->items[i];

            while(hitem)
            {
                /*
                 * TODO: report symbol types
                 */
                ksprintf(tmp, 64, "%16lx  %s\t[%s]\n", 
                              hitem->val, hitem->key,
                              mod->modinfo.name ? mod->modinfo.name : "??");
                len = strlen(tmp);
            
                if(count + len >= bufsz)
                {
                    PR_REALLOC_OR_UNLOCK(*buf, bufsz, count, &kmod_list_mutex);
                    p = *buf + count;
                }

                count += len;
                strcpy(p, tmp);
                p += len;

                hitem = hitem->next;
            }
        }
    }

    kernel_mutex_unlock(&kmod_list_mutex);

    return count;
}

