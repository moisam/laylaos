/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025, 2026 (c)
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
    virtual_addr page, addr;
    pt_entry *e;

    while(left)
    {
        virtual_addr mempos = pos + memstart;
        physical_addr phys;

        page = align_down(mempos);

        if(!(e = get_page_entry_pd((pdirectory *)task->pd_virt, page)) ||
           !(phys = PTE_FRAME(*e)))
        {
            break;
        }

        addr = PHYS_TO_HIMEM(phys);
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
        copy_internal(buf, p, j, j, 1);
        buf += j;
    }
    
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
    virtual_addr page, addr;
    pt_entry *e;

    while(left)
    {
        virtual_addr mempos = pos + memstart;
        physical_addr phys;
        
        page = align_down(mempos);

        if(!(e = get_page_entry_pd((pdirectory *)task->pd_virt, page)) ||
           !(phys = PTE_FRAME(*e)))
        {
            break;
        }

        if(!PDE_WRITABLE(*e))
        {
            break;
        }

        addr = PHYS_TO_HIMEM(phys);
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


struct page_details_t
{
    int shclean;
    int shdirty;
    int prclean;
    int prdirty;
    int referenced;
    int anon;
    int anonhuge;
    int swap;
    int locked;
    int rss;
    int size;
};


static void __memregion_breakdown(struct task_t *task,
                                  struct memregion_t *memregion, 
                                  struct page_details_t *res)
{
    virtual_addr start = memregion->addr;
    virtual_addr end = start + (memregion->size * PAGE_SIZE);

    res->shclean = 0;
    res->shdirty = 0;
    res->prclean = 0;
    res->prdirty = 0;
    res->anonhuge = 0;
    res->swap = 0;
    res->rss = 0;
    res->referenced = 0;
    res->anon = 0;
    res->size = (memregion->size * PAGE_SIZE) / 1024;

    /*
     * XXX: Our memory lock process is currently a no-op
     */
    res->locked = (memregion->flags & MEMREGION_FLAG_STICKY_BIT) ? memregion->size : 0;

    if(memregion->type == MEMREGION_TYPE_KERNEL)
    {
        res->size = 0;
    }
    else if(!memregion->inode)
    {
        /*
         * TODO: return correct information for anonymous mappings
         */
        res->rss = memregion->size;
        res->referenced = res->rss;
        res->anon = res->rss;
    }
    else
    {
        pdirectory *pml4_src = (pdirectory *)task->pd_virt;
        volatile pt_entry *e;
        virtual_addr addr;

        for(addr = start; addr < end; addr += PAGE_SIZE)
        {
            if(!(e = __get_page_entry_pd(pml4_src, addr, 0)))
            {
                continue;
            }

            if(PTE_PRESENT(*e))
            {
                res->rss++;
            }

            if(PTE_ACCESSED(*e))
            {
                res->referenced++;
            }

            if(PTE_DIRTY(*e))
            {
                if(memregion->flags & MEMREGION_FLAG_PRIVATE)
                {
                    res->prdirty++;
                }
                else
                {
                    res->shdirty++;
                }
            }
            else
            {
                if(memregion->flags & MEMREGION_FLAG_PRIVATE)
                {
                    res->prclean++;
                }
                else
                {
                    res->shclean++;
                }
            }
        }
    }

    // convert values from multiples of PAGE_SIZE to kB
    size_t i = PAGE_SIZE / 1024;

    res->shclean *= i;
    res->shdirty *= i;
    res->prclean *= i;
    res->prdirty *= i;
    res->anonhuge *= i;
    res->swap *= i;
    res->rss *= i;
    res->referenced *= i;
    res->anon *= i;
}


#define APPEND_TWO_LETTERS(buf, c1, c2) \
    *buf++ = c1; *buf++ = c2; *buf++ = ' ';


static void __print_flags(struct memregion_t *memregion, char *buf)
{
    if(memregion->prot & PROT_READ)
    {
        APPEND_TWO_LETTERS(buf, 'r', 'd');
    }

    if(memregion->prot & PROT_WRITE)
    {
        APPEND_TWO_LETTERS(buf, 'w', 'r');
    }

    if(memregion->prot & PROT_EXEC)
    {
        APPEND_TWO_LETTERS(buf, 'e', 'x');
    }

    if(!(memregion->flags & MEMREGION_FLAG_PRIVATE))
    {
        APPEND_TWO_LETTERS(buf, 's', 'h');
    }

    if(memregion->type == MEMREGION_TYPE_STACK)
    {
        APPEND_TWO_LETTERS(buf, 'g', 'd');
    }

    if(memregion->flags & MEMREGION_FLAG_STICKY_BIT)
    {
        APPEND_TWO_LETTERS(buf, 'l', 'o');
    }

    *buf = '\0';
}


/*
 * Helper function to read task mmap files:
 *    /proc/[pid]/maps
 *    /proc/[pid]/smaps
 */
static size_t __get_task_mmaps(struct task_t *task, char **_buf, int extra_info)
{
    struct memregion_t *memregion;
    virtual_addr start;
    virtual_addr end;
    struct fs_node_t *node = NULL;
    char *path = NULL;
    struct dentry_t *dent = NULL;
    size_t buflen = 0, bufsz = 2048;
    volatile size_t len = 0;
    ino_t ino;
    dev_t dev;
    char tmp[128];
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

    /* 
     * No header for /proc/[pid]/smaps
     */
    if(!extra_info)
    {
    
#ifdef __x86_64__
        ksprintf(p, bufsz, "address                           perms offset"
                           "   dev   inode     pathname\n");
#else
        ksprintf(p, bufsz, "address           perms offset   dev   "
                           "inode     pathname\n");
#endif

        buflen = strlen(p);
        p += buflen;
    }

    for(memregion = task->mem->first_region;
        memregion != NULL;
        memregion = memregion->next)
    {
        // make sure we have enough space, otherwise expand the buffer
        if(buflen + _F1_ + _F2_ + _F3_ + _F4_ + _F5_ + _F6_ >= bufsz)
        {
            *_buf = buf;
            PR_REALLOC_OR_UNLOCK(buf, bufsz, buflen, &(task->mem->mutex));
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
                            PR_REALLOC_OR_UNLOCK(buf, bufsz, buflen, &(task->mem->mutex));
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

        /* 
         * Stuff for /proc/[pid]/smaps
         */
        if(extra_info)
        {
            struct page_details_t res;

            // 30 is the length of each line,
            // 16 is the number of lines plus 1 to account for the (possibly 
            // long) last line
            if(buflen + (30 * 16) + 1 >= bufsz)
            {
                *_buf = buf;
                PR_REALLOC_OR_UNLOCK(buf, bufsz, buflen, &(task->mem->mutex));
                *_buf = buf;
                p = buf + buflen;
            }

            __memregion_breakdown(task, memregion, &res);
            __print_flags(memregion, tmp);

            ksprintf(p, (bufsz - buflen), 
                     "Size:          %8d kB\n"
                     "Rss:           %8d kB\n"
                     "Pss:           %8d kB\n"
                     "Shared_Clean:  %8d kB\n"
                     "Shared_Dirty:  %8d kB\n"
                     , res.size,
                       res.rss, 
                       res.rss,    /* XXX: dummy value for Pss */
                       res.shclean, res.shdirty);

            tmpsz = strlen(p);
            buflen += tmpsz;
            p += tmpsz;

            ksprintf(p, (bufsz - buflen), 
                     "Private_Clean: %8d kB\n"
                     "Private_Dirty: %8d kB\n"
                     "Referenced:    %8d kB\n"
                     "Anonymous:     %8d kB\n"
                     "AnonHugePages: %8d kB\n"
                     , res.prclean, res.prdirty,
                       res.referenced, 
                       res.anon, res.anonhuge);

            tmpsz = strlen(p);
            buflen += tmpsz;
            p += tmpsz;

            ksprintf(p, (bufsz - buflen), 
                     "Swap:          %8d kB\n"
                     "KernelPageSize:%8d kB\n"
                     "MMUPageSie:    %8d kB\n"
                     "Locked:        %8d kB\n"
                     "VmFlags: %s\n\n"
                     , res.swap,
                       PAGE_SIZE, PAGE_SIZE,
                       res.locked,
                       tmp);

            tmpsz = strlen(p);
            buflen += tmpsz;
            p += tmpsz;
        }
    }

    kernel_mutex_unlock(&(task->mem->mutex));
    return buflen;
}


/*
 * Read /proc/[pid]/maps.
 */
size_t get_task_mmaps(struct task_t *task, char **_buf)
{
    return __get_task_mmaps(task, _buf, 0);
}


/*
 * Read /proc/[pid]/smaps.
 */
size_t get_task_smaps(struct task_t *task, char **_buf)
{
    return __get_task_mmaps(task, _buf, 1);
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

    ksprintf(p, 128, "rchar: %10lu\nwchar: %10lu\n",
                     task->read_count, task->write_count);
    p += strlen(p);

    ksprintf(p, 128, "syscr: %10u\nsyscw: %10u\n",
                     task->read_calls, task->write_calls);

    //switch_tty(1);
    //printk("*** %s\n", *buf);

    return strlen(*buf);
}


/*
 * Read /proc/[pid]/comm.
 */
size_t get_task_comm(struct task_t *task, char **buf)
{
    size_t len = strlen((char *)task->command) + 2;

    if(!task || !buf)
    {
        return 0;
    }

    PR_MALLOC(*buf, len);
    ksprintf(*buf, len, "%s\n", task->command);

    return strlen(*buf);
}


/*
 * Read /proc/[pid]/exe.
 */
size_t get_task_exe(struct task_t *task, char **buf)
{
    if(!task || !task->exe_path || !buf)
    {
        return 0;
    }

    PR_MALLOC(*buf, 2048);
    ksprintf(*buf, 2048, "%s", task->exe_path);

    return strlen(*buf);
}


/*
 * Read /proc/[pid]/cwd.
 */
size_t get_task_cwd(struct task_t *task, char **buf)
{
    if(!task || !task->fs || !task->fs->cwd || !buf)
    {
        return 0;
    }

    PR_MALLOC(*buf, 2048);

    return copy_task_dirpath(task->fs->cwd->dev,
                             task->fs->cwd->inode, *buf, 2048, 1);
}


/*
 * Read /proc/[pid]/root.
 */
size_t get_task_root(struct task_t *task, char **buf)
{
    if(!task || !task->fs || !task->fs->root || !buf)
    {
        return 0;
    }

    PR_MALLOC(*buf, 2048);

    return copy_task_dirpath(task->fs->root->dev,
                             task->fs->root->inode, *buf, 2048, 1);
}

