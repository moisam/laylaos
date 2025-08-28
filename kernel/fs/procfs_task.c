/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: procfs_task.c
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
 *  \file procfs_task.c
 *
 *  This file implements some procfs filesystem functions (mainly the ones
 *  used to read from files under /proc/[pid] where pid is a process id).
 *  Functions implementing filesystem operations are exported to the rest of
 *  the kernel via the \ref procfs_ops structure, which is defined in procfs.c.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/mutex.h>
#include <kernel/vfs.h>
#include <kernel/timer.h>
#include <kernel/clock.h>
#include <kernel/ksignal.h>
#include <kernel/tty.h>
#include <fs/tmpfs.h>
#include <fs/ext2.h>
#include <fs/procfs.h>
#include <fs/dentry.h>
#include <mm/kheap.h>
#include <mm/mmap.h>            // align_down()


int copy_task_dirpath(dev_t dev, ino_t ino,
                      char *buf, size_t bufsz, int kernel)
{
    int res;
    char *path = NULL;
    struct dentry_t *dent = NULL;
    struct fs_node_t *dir = NULL;

    if((dir = get_node(dev, ino, GETNODE_FOLLOW_MPOINTS)) == NULL)
    {
        return -ENOENT;
    }
    
    if((res = get_dentry(dir, &dent)) < 0)
    {
        release_node(dir);
        return res;
    }
    
    if(!dent->path)
    {
        release_dentry(dent);
        release_node(dir);
        return -ENOENT;
    }
    
    path = dent->path;
    res = copy_string_internal(buf, path, bufsz, kernel);
    release_dentry(dent);
    release_node(dir);

    return res;
}


/*
 * Read from another task's memory space (used mainly in ptracing and when
 * reading/writing to files under /procfs/[pid]).
 */
size_t read_other_taskmem(struct task_t *task, off_t pos,
                          virtual_addr memstart, virtual_addr memend,
                          char *buf, size_t count)
{
    volatile size_t left = count;
    size_t i, j;
    char *p;
    virtual_addr page, last_page = 0;
    pt_entry *e, *tmp = NULL;
    virtual_addr addr;
    
    KDEBUG("read_other_taskmem: buf 0x%x, count %d\n", buf, count);

    get_tmp_virt_addr(&addr, &tmp, PTE_FLAGS_PW);
    
    if(!tmp)
    {
        return 0;
    }
    
    while(left)
    {
        KDEBUG("read_other_taskmem: left %d, count %d\n", left, count);

        virtual_addr mempos = pos + memstart;
        physical_addr phys;
        
        page = align_down(mempos);

        KDEBUG("read_other_taskmem: page 0x%x, last_page 0x%x\n", page, last_page);
        
        if(page != last_page)
        {
            if(!(e = get_page_entry_pd((pdirectory *)task->pd_virt,
                                        (void *)page)) ||
               // !(phys = pt_entry_get_frame(e)))
               !(phys = PTE_FRAME(*e)))
            {
                break;
            }
            
            PTE_SET_FRAME(tmp, phys);
            vmmngr_flush_tlb_entry(addr);
            last_page = page;
        }

        i = mempos % PAGE_SIZE;

        if((page + PAGE_SIZE) <= memend)
        {
            j = MIN((PAGE_SIZE - i), left);
        }
        else
        {
            j = MIN((memend - page), left);
        }

        KDEBUG("read_other_taskmem: i %d, j %d\n", i, j);
        
        if(j == 0)
        {
            break;
        }

        pos += j;
        left -= j;

        p = (char *)addr + i;
        copy_internal(buf, p, j, j, 1);
        buf += j;
    }
    
    *tmp = 0;
    vmmngr_flush_tlb_entry(addr);
    
    return count - left;
}


/*
 * Write to another task's memory space (used mainly in ptracing and when
 * reading/writing to files under /procfs/[pid]).
 */
size_t write_other_taskmem(struct task_t *task, off_t pos,
                           virtual_addr memstart, virtual_addr memend,
                           char *buf, size_t count)
{
    volatile size_t left = count;
    size_t i, j;
    char *p;
    virtual_addr page, last_page = 0;
    pt_entry *e, *tmp = NULL;
    virtual_addr addr, end = TMPFS_END;

    kernel_mutex_lock(&tmpfs_lock);
    
    for(addr = TMPFS_START; addr < end; addr += PAGE_SIZE)
    {
        pt_entry *pt = get_page_entry((void *)addr);

        if(PTE_FRAME(*pt) == 0)
        //if(pt_entry_get_frame(pt) == 0)
        {
            PTE_SET_FRAME(pt, 1);
            PTE_ADD_ATTRIB(pt, PTE_FLAGS_PW);
            tmp = pt;
            break;
        }
    }

    kernel_mutex_unlock(&tmpfs_lock);
    
    if(!tmp)
    {
        return 0;
    }
    
    while(left)
    {
        virtual_addr mempos = pos + memstart;
        physical_addr phys;
        
        page = align_down(mempos);
        
        if(page != last_page)
        {
            if(!(e = get_page_entry_pd((pdirectory *)task->pd_virt,
                                        (void *)page)) ||
               // !(phys = pt_entry_get_frame(e)))
               !(phys = PTE_FRAME(*e)))
            {
                break;
            }
            
            if(!PDE_WRITABLE(*e))
            //if(!pd_entry_is_writable(*e))
            {
                break;
            }
            
            PTE_SET_FRAME(tmp, phys);
            vmmngr_flush_tlb_entry(addr);
            last_page = page;
        }

        i = mempos % PAGE_SIZE;

        if((page + PAGE_SIZE) <= memend)
        {
            j = MIN((PAGE_SIZE - i), left);
        }
        else
        {
            j = MIN((memend - page), left);
        }
        
        if(j == 0)
        {
            break;
        }

        pos += j;
        left -= j;

        p = (char *)addr + i;
        copy_internal(p, buf, j, j, 1);
        buf += j;
    }
    
    *tmp = 0;
    vmmngr_flush_tlb_entry(addr);
    
    return count - left;
}


/*
 * Helper function.
 */
size_t print_rlimit(char *buf, size_t bufsz, char *name, char *slim,
                    char *rlim, char *units)
{
    size_t len, total = 0;
    
    ksprintf(buf, bufsz, "%s", name);
    len = strlen(name);
    buf += len;
    
    while(len < 28)
    {
        *buf++ = ' ';
        len++;
    }

    total += len;
    ksprintf(buf, bufsz, "%s", slim);
    len = strlen(slim);
    buf += len;
    
    while(len < 16)
    {
        *buf++ = ' ';
        len++;
    }

    total += len;
    ksprintf(buf, bufsz, "%s", rlim);
    len = strlen(rlim);
    buf += len;
    
    while(len < 16)
    {
        *buf++ = ' ';
        len++;
    }

    total += len;
    ksprintf(buf, bufsz, "%s\n", units);
    len = strlen(units) + 1;
    total += len;
    
    return total;
}


/*
 * Read /proc/[pid]/limits.
 */
size_t get_task_rlimits(struct task_t *task, char **_buf)
{

#define BUFSZ               2048
    
    int i;
    char slim[32];
    char rlim[32];
    char *buf;
    size_t buflen = 0;
    volatile size_t len = 0;
    
    if(!task || !_buf)
    {
        return 0;
    }

    *_buf = NULL;
    PR_MALLOC(buf, BUFSZ);
    *_buf = buf;

    len = print_rlimit(buf, 2048, "Limit", "Soft Limit", "Hard Limit", "Units");
    //KDEBUG("get_task_rlimits: buf = '%s'\n", buf);
    
    buf += len;
    buflen += len;
    
    for(i = 0; i < RLIMIT_NLIMITS; i++)
    {
        if(task->task_rlimits[i].rlim_cur == RLIM_INFINITY)
        {
            ksprintf(slim, sizeof(slim), "unlimited");
        }
        else
        {
            ksprintf(slim, sizeof(slim), "%ld", task->task_rlimits[i].rlim_cur);
        }
        
        if(task->task_rlimits[i].rlim_max == RLIM_INFINITY)
        {
            ksprintf(rlim, sizeof(rlim), "unlimited");
        }
        else
        {
            ksprintf(rlim, sizeof(rlim), "%ld", task->task_rlimits[i].rlim_max);
        }

        len = print_rlimit(buf, (BUFSZ - buflen), default_rlimits[i].name, slim, rlim,
                           default_rlimits[i].units);
        
        buf += len;
        buflen += len;
    }

#undef BUFSZ

    return buflen;
}


#ifdef __x86_64__
// format specifier for addresses
# define _LX_                   "%016lx"
// field 1 length
# define _F1_                   34
#else       /* !__x86_64__ */
# define _LX_                   "%08lx"
# define _F1_                   18
#endif      /* !__x86_64__ */

// rest of field lengths
# define _F2_                   6
# define _F3_                   9
# define _F4_                   6
# define _F5_                   10
# define _F6_                   10


/*
 * Read /proc/[pid]/maps.
 */
size_t get_task_mmaps(struct task_t *task, char **_buf)
{
    struct memregion_t *memregion;
    virtual_addr start;
    virtual_addr end;
    //virtual_addr data_end;
    struct fs_node_t *node = NULL;
    char *path = NULL;
    struct dentry_t *dent = NULL;
    size_t buflen = 0, bufsz = 2048;
    volatile size_t len = 0;
    ino_t ino;
    dev_t dev;
    char tmp[16];
    char *buf, *p;
    
    if(!task || !task->mem || !_buf)
    {
        return 0;
    }

    kernel_mutex_lock(&(task->mem->mutex));
    
    //data_end = task_get_data_end(task);

    *_buf = NULL;
    PR_MALLOC(buf, bufsz);
    *_buf = buf;
    p = buf;
    
#ifdef __x86_64__
    ksprintf(p, bufsz, "address                           perms offset"
                       "   dev   inode     pathname\n");
#else
    ksprintf(p, bufsz, "address           perms offset   dev   "
                        "inode     pathname\n");
#endif

    buflen = strlen(p);
    p += buflen;

    for(memregion = task->mem->first_region;
        memregion != NULL;
        memregion = memregion->next)
    {
        // make sure we have enough space, otherwise expand the buffer
        if(buflen + _F1_ + _F2_ + _F3_ + _F4_ + _F5_ + _F6_ >= bufsz)
        {
            *_buf = buf;
            PR_REALLOC(buf, bufsz, buflen);
            *_buf = buf;
            p = buf + buflen;
        }

        size_t tmpsz = (bufsz - buflen);
        start = memregion->addr;
        end = start + (memregion->size * PAGE_SIZE);
        node = memregion->inode;
        
        ksprintf(p, tmpsz, _LX_ "-" _LX_ " ", start, end);
        p += _F1_;
        tmpsz -= _F1_;

        *p++ = (memregion->prot & PROT_READ) ? 'r' : '-';
        *p++ = (memregion->prot & PROT_WRITE) ? 'w' : '-';
        *p++ = (memregion->prot & PROT_EXEC) ? 'x' : '-';
        *p++ = (memregion->flags & MEMREGION_FLAG_PRIVATE) ? 'p' : 's';
        *p++ = ' ';
        *p++ = ' ';

        ksprintf(p, tmpsz, "%08lx ", memregion->fpos);
        p += _F3_;
        tmpsz -= _F3_;
        
        if(node)
        {
            dev = node->dev;
            ino = node->inode;
        }
        else
        {
            dev = 0;
            ino = 0;
        }

        ksprintf(p, tmpsz, "%02x:%02x ",
                    (unsigned int)MAJOR(dev), (unsigned int)MINOR(dev));
        p += _F4_;
        tmpsz -= _F4_;
        
        ksprintf(tmp, tmpsz, "%lu", ino);
        strcpy(p, tmp);
        len = strlen(tmp);
        p += len;

        while(len < _F5_)
        {
            *p++ = ' ';
            len++;
        }
        
        if(memregion->type == MEMREGION_TYPE_STACK)
        {
            strcpy(p, "[stack]");
            len += 7;
            p += 7;
        }
        else if(memregion->type == MEMREGION_TYPE_KERNEL)
        {
            strcpy(p, "[kernel]");
            len += 8;
            p += 8;
        }
        else if(memregion->type == MEMREGION_TYPE_DATA && !node)
        {
            if(end <= task->end_data)
            //if(end <= data_end)
            {
                strcpy(p, "[heap]");
                len += 6;
                p += 6;
            }
            else if(start == task->mem->vdso_code_start)
            {
                strcpy(p, "[vdso]");
                len += 6;
                p += 6;
            }
        }
        else if(node)
        {
            if((node = get_node(node->dev, node->inode, GETNODE_FOLLOW_MPOINTS)) != NULL)
            {
                dent = NULL;
                
                if(get_dentry(node, &dent) == 0)
                {
                    if(dent->path)
                    {
                        path = dent->path;
                        size_t x = strlen(path);

                        // make sure we have enough space, otherwise expand 
                        // the buffer
                        if(buflen + x + 1 >= bufsz)
                        {
                            *_buf = buf;
                            PR_REALLOC(buf, bufsz, buflen);
                            *_buf = buf;
                            p = buf + buflen;
                        }

                        ksprintf(p, (bufsz - buflen), "%s", path);
                        len += x;
                        p += x;
                    }
                    
                    release_dentry(dent);
                }
                
                release_node(node);
            }
        }

        *p++ = '\n';
        len++;

        len += (_F1_ + _F2_ + _F3_ + _F4_);
        buflen += len;
    }

    kernel_mutex_unlock(&(task->mem->mutex));
    return buflen;
}


/*
 * Read /proc/[pid]/timers.
 */
size_t get_task_posix_timers(struct task_t *task, char **_buf)
{
    struct posix_timer_t *timer;
    char *buf, *p;
    char tmp[256];
    size_t len, count = 0, bufsz = 512;
    
    if(!task || !_buf)
    {
        return 0;
    }

    *_buf = NULL;
    PR_MALLOC(buf, bufsz);
    *_buf = buf;
    p = buf;

    for(timer = task->posix_timers; timer != NULL; timer = timer->next)
    {
        volatile int signal = 0;
        volatile void *sigval = NULL;
        volatile int bysig = 0;
        
        if(timer->sigev.sigev_notify == SIGEV_SIGNAL)
        {
            bysig = 1;
            signal = timer->sigev.sigev_signo;
            sigval = timer->sigev.sigev_value.sival_ptr;
        }
        
        //sprintf(tmp, "ID: %ld\n"
        ksprintf(tmp, sizeof(tmp), "ID: %ld\n"
#ifdef __x86_64__
                     "signal: %d/%0lx\n"
#else
                     "signal: %d/%0x\n"
#endif
                     "notify: %s/pid.%d\n"
                     "ClockID: %ld\n",
                timer->timerid, signal, (uintptr_t)sigval,
                bysig ? "signal" : "none", task->pid,
                timer->clockid);

        len = strlen(tmp);

        // make sure we have enough space, otherwise expand the buffer
        if(count + len >= bufsz)
        {
            *_buf = buf;
            PR_REALLOC(buf, bufsz, count);
            *_buf = buf;
            p = buf + count;
        }

        strcpy(p, tmp);
        count += len;
        p += len;
    }
    
    return count;
}


/*
 * Read /proc/[pid]/io.
 */
size_t get_task_io(struct task_t *task, char **buf)
{
    char *p;

    if(!task || !buf)
    {
        return 0;
    }

    PR_MALLOC(*buf, 128);
    p = *buf;

    ksprintf(p, 128, "rchar: %lu\nwchar: %lu\n",
                     task->read_count, task->write_count);
    p += strlen(p);

    ksprintf(p, 128, "syscr: %u\nsyscw: %u\n",
                     task->read_calls, task->write_calls);

    //printk("*** %s\n", *buf);

    return strlen(*buf);
}

