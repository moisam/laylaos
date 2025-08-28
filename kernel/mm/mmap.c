/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: mmap.c
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
 *  \file mmap.c
 *
 *  Memory map implementation.
 *
 *  See: https://man7.org/linux/man-pages/man2/mmap.2.html
 */

//#define __DEBUG

#define __USE_GNU
#define __USE_XOPEN_EXTENDED
#define _GNU_SOURCE
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <mm/mmap.h>
#include <mm/memregion.h>
#include <kernel/laylaos.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/vfs.h>
#include <kernel/mutex.h>
#include <kernel/user.h>
#include <kernel/ipc.h>
#include <kernel/user.h>

//#include <fs/dentry.h>


#define VALID_FLAGS         (MAP_SHARED | MAP_PRIVATE | MAP_ANONYMOUS | \
                             MAP_FIXED | MAP_GROWSDOWN | MAP_STACK | \
                             MAP_EXECUTABLE | MAP_NORESERVE | \
                             MAP_FIXED_NOREPLACE)


#define REGION_END(m)       ((m)->addr + ((m)->size * PAGE_SIZE))

/*
 * Reserve memory in userspace.
 */
virtual_addr get_user_addr(virtual_addr size, virtual_addr min, virtual_addr max)
{
    virtual_addr end, diff;
    struct memregion_t *memregion = this_core->cur_task->mem->first_region;

    while(memregion)
    {
        //printk("get_user_addr: addr 0x%lx\n", memregion->addr);

        if(memregion->addr < min)
        {
            memregion = memregion->next;
            continue;
        }

        /*
        if(memregion->addr > max)
        {
            return min;
        }
        */

        end = REGION_END(memregion);

        if(end > max)
        {
            if(memregion->prev)
            {
                return (REGION_END(memregion->prev) < min) ? min : 0;
            }
            else
            {
                return min;
            }
        }

        if(memregion->next->addr > max)
        {
            diff = max - end;
        }
        else
        {
            diff = memregion->next->addr - end;
        }

        if(diff >= size)
        {
            pt_entry *e = get_page_entry_pd((pdirectory *)this_core->cur_task->pd_virt, 
                                            (void *)end);

            if(e && PTE_FRAME(*e))
            {
                /*
                switch_tty(1);

                printk("Current process: pid %d, comm %s\n",
                       cur_task->pid, cur_task->command);

                for(struct memregion_t *tmp = cur_task->mem->first_region; tmp != NULL; tmp = tmp->next)
                {
                    char *path;
                    struct dentry_t *dent;
                
                    path = "*";
                
                    if(tmp->inode && get_dentry(tmp->inode, &dent) == 0)
                    {
                        if(dent->path)
                        {
                            path = dent->path;
                        }
                    }

                    printk("memregion: addr %lx - %lx (type %d, prot %x, fl %x, %s)\n", tmp->addr, tmp->addr + (tmp->size * PAGE_SIZE), tmp->type, tmp->prot, tmp->flags, path);
                }

                screen_refresh(NULL);
                */

                __asm__ __volatile__("xchg %%bx, %%bx"::);
                printk("mmap: addr %lx in use but not in a memregion\n", end);
                kpanic("mmap error\n");
            }

            //if(cur_task->pid >= 59) __asm__ __volatile__("xchg %%bx, %%bx"::);
            return end;
        }

        memregion = memregion->next;
    }

    return 0;
}


/*
 * Handler for syscall mmap().
 */
long syscall_mmap(struct syscall_args *__args)
{
    struct syscall_args a;
    long res;
    int type;
    virtual_addr aligned_addr, aligned_size, end;
    struct file_t *f = NULL;
    struct fs_node_t *node = NULL;
    
    // syscall args
    void *addr;
    size_t length;
    int prot;
    int flags;
    int fd;
    off_t offset;
    volatile void **res_addr;

    // get the args
    COPY_SYSCALL7_ARGS(a, __args);
    addr = (void *)(a.args[0]);
    length = (size_t)(a.args[1]);
    prot = (int)(a.args[2]);
    flags = (int)(a.args[3]);
    fd = (int)(a.args[4]);
    offset = (off_t)(a.args[5]);
    res_addr = (volatile void **)(a.args[6]);
    
    int overlaps;
    int fixed = FLAG_SET(flags, MAP_FIXED) | FLAG_SET(flags, MAP_FIXED_NOREPLACE);
    int anon = FLAG_SET(flags, MAP_ANONYMOUS);
	struct task_t *ct = (struct task_t *)this_core->cur_task;

    // check addr is aligned
    if(fixed && !PAGE_ALIGNED(addr))
    {
        return -EINVAL;
    }

    // check addr is valid (if MAP_FIXED is supplied)
    if(fixed && addr == 0)
    {
        return -EINVAL;
    }
    
    // check the length
    if(length == 0)
    {
        return -EINVAL;
    }

    // check we're not trying to map kernel memory
    if(((size_t)addr >= USER_MEM_END) ||
       (((size_t)addr + length) > USER_MEM_END))
    {
        return -EINVAL;
    }

    if((size_t)res_addr >= USER_MEM_END)
    {
        kpanic("invalid res_addr passed to syscall_mmap()\n");
        return -EINVAL;
    }
    
    // check prot is valid
    if(prot & ~VALID_PROT)
    {
        return -EINVAL;
    }
    
    // check for unknown flags
    if(flags & ~VALID_FLAGS)
    {
        return -EINVAL;
    }
    
    // check for conflicting flags - one of the two must be passed to us
    if(FLAG_SET(flags, MAP_PRIVATE) == FLAG_SET(flags, MAP_SHARED))
    {
        return -EINVAL;
    }
    
    // check fd and file offset are valid for file mapping
    if(!anon && 
       (fd < 0 || fd >= NR_OPEN || offset < 0 || !PAGE_ALIGNED(offset)))
    {
        return -EINVAL;
    }
    
    // page align address and size
    aligned_addr = align_down((virtual_addr)addr);
    aligned_size = align_up((virtual_addr)length);
    end = aligned_addr + aligned_size;
    
    // TODO: check offset + size doesn't overflow (we don't have OFF_MAX defined yet)
    /*
    if(!anon && (uintmax_t) (OFF_MAX - offset) < (uintmax_t) aligned_size)
    {
        return -EOVERFLOW;
    }
    */
    
    // check the mapped file fits this mapping
    if(!anon)
    {
        if(!(f = ct->ofiles->ofile[fd]) || !(node = f->node) ||
           // check we can seek the file
           (syscall_lseek(fd, 0, SEEK_CUR) < 0) ||
           // check we have read permission
           (has_access(f->node, READ, 0) != 0) ||
           // check we have write permission if needed
           ((prot & PROT_WRITE) && !FLAG_SET(flags, MAP_PRIVATE) &&
           (has_access(f->node, WRITE, 0) != 0)))
        {
            return -EACCES;
        }

        if(f->flags & O_PATH)
        {
            return -EBADF;
        }
    }
    
    // set the region's type
    type = ((flags & MAP_GROWSDOWN) || 
            (flags & MAP_STACK) || 
            (prot & PROT_GROWSDOWN)) ?
                MEMREGION_TYPE_STACK :
                    (((flags & MAP_EXECUTABLE) || (flags & PROT_EXEC)) ?
                        MEMREGION_TYPE_TEXT : MEMREGION_TYPE_DATA);

    // check if the underlying filesystem supports file execution
    if(node && type == MEMREGION_TYPE_TEXT)
    {
        struct mount_info_t *dinfo;

        if((dinfo = get_mount_info(node->dev)) &&
           (dinfo->mountflags & MS_NOEXEC))
        {
            return -ENOEXEC;
        }
    }

    // ensure no one changes the task memory map while we're fiddling with it
    kernel_mutex_lock(&(ct->mem->mutex));

    overlaps = !!(memregion_check_overlaps(ct, aligned_addr, end));

    if(FLAG_SET(flags, MAP_FIXED_NOREPLACE) && overlaps)
    {
        kernel_mutex_unlock(&(ct->mem->mutex));
        return -EEXIST;
    }

    // choose an address if no hint is given, or if the hint overlaps
    // existing memory regions
    if(!fixed && (!aligned_addr || overlaps))
    {
        if((aligned_addr = get_user_addr(aligned_size,
                                         USER_SHM_START, USER_SHM_END)) == 0)
        {
            kernel_mutex_unlock(&(ct->mem->mutex));
            return -ENOMEM;
        }
        
        end = aligned_addr + aligned_size;
    }

    //printk("mmap: task %d, s %lx, e %lx\n", ct->pid, aligned_addr, end);

    // allocate a new memregion struct
    if((res = memregion_alloc_and_attach(ct, node,
                               offset, (off_t)length,
                               aligned_addr, end,
                               prot, type,
                               (flags & (MAP_SHARED | MAP_PRIVATE)) | 
                                    MEMREGION_FLAG_USER,
                               fixed)) != 0)
    {
        //printk("mmap: ********** cannot attach memregion\n");
        //screen_refresh(NULL);

        kernel_mutex_unlock(&(ct->mem->mutex));
        return res;
    }

    // reserve memory for anonymous mapping and those with no file backing
    // this will make it easy to clone pages on fork/clone calls, as we
    // don't worry about the physical pages, as they will be automatically
    // shared between processes (NOTE: side effect is more memory consumption)
    if(anon || node == NULL)
    {
        if(!FLAG_SET(flags, MAP_PRIVATE))
        {
            int page_flags = 0;

            // prepare page flags
            if(prot != PROT_NONE)
            {
                page_flags = I86_PTE_PRESENT |
                        ((prot & PROT_WRITE) ? I86_PTE_WRITABLE : 0) |
                        (((aligned_addr < USER_MEM_END) &&
                            (end < USER_MEM_END)) ? I86_PTE_USER : 0);
            }
        
            /*
            if(FLAG_SET(flags, MAP_PRIVATE))
            {
                page_flags |= I86_PTE_PRIVATE;
            }
            */

            if(!vmmngr_alloc_pages(aligned_addr, (size_t)aligned_size, page_flags))
            {
                //printk("mmap: ********** removing memregion\n");
                //screen_refresh(NULL);

                kernel_mutex_unlock(&(ct->mem->mutex));
                memregion_detach(ct, memregion_containing(ct, aligned_addr), 1);
                return -ENOMEM;
            }
        
            A_memset((void *)aligned_addr, 0, aligned_size);
        }
    }

    /*
    printk("mmap: ********** new memregion map\n");

    for(struct memregion_t *tmp = ct->mem->first_region; tmp != NULL; tmp = tmp->next)
    {
        printk("mmap: tmp->addr %lx\n", tmp->addr);
    }
    
    screen_refresh(NULL);
    */
    
    memregion_consolidate(ct);
    kernel_mutex_unlock(&(ct->mem->mutex));
    
    addr = (void *)aligned_addr;
    COPY_TO_USER(res_addr, &addr, sizeof(void *));

    //printk("mmap: task %d, addr %lx\n", ct->pid, aligned_addr);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    return 0;
}


/*
 * Handler for syscall munmap().
 */
long syscall_munmap(void *addr, size_t length)
{
    long res;
	struct task_t *ct = (struct task_t *)this_core->cur_task;

    //printk("munmap: task %d, addr %lx, length %lx\n", ct->pid, addr, length);

    // check addr is aligned
    if(!PAGE_ALIGNED(addr))
    {
        return -EINVAL;
    }

    /*
     * We don't need to call msync here. The call to 
     * memregion_remove_overlaps() will call memregion_change_prot(), which
     * will call memregion_detach(), which in turn will call syscall_msync()
     * for us.
     */
    
    // ensure no one changes the task memory map while we're fiddling with it
    kernel_mutex_lock(&(ct->mem->mutex));
    res = memregion_remove_overlaps(ct, (virtual_addr)addr,
                        (virtual_addr)addr + align_up((virtual_addr)length));
    kernel_mutex_unlock(&(ct->mem->mutex));
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    /*
    printk("munmap: ********** new memregion map\n");

    for(struct memregion_t *tmp = ct->mem->first_region; tmp != NULL; tmp = tmp->next)
    {
        printk("munmap: tmp->addr %lx\n", tmp->addr);
    }
    
    screen_refresh(NULL);
    */
    
    return res;
}


/*
 * Handler for syscall mprotect().
 */
long syscall_mprotect(void *addr, size_t length, int prot)
{
    virtual_addr aligned_size, end;
    long res;
	struct task_t *ct = (struct task_t *)this_core->cur_task;

    //printk("mprotect: addr %lx, length %lx\n", addr, length);

    // check addr is aligned
    if(!PAGE_ALIGNED(addr))
    {
        return -EINVAL;
    }

    // check prot is valid
    if(prot & ~VALID_PROT)
    {
        return -EINVAL;
    }

    aligned_size = align_up((virtual_addr)length);
    end = (virtual_addr)addr + aligned_size;

    // check we're not trying to map kernel memory
    if(((size_t)addr >= USER_MEM_END) ||
                (end > USER_MEM_END))
    {
        return -EINVAL;
    }

    // ensure no one changes the task memory map while we're fiddling with it
    kernel_mutex_lock(&(ct->mem->mutex));
    res = memregion_change_prot(ct, (virtual_addr)addr, end, prot, 0);
    kernel_mutex_unlock(&(ct->mem->mutex));

    /*
    printk("mprotect: ********** new memregion map\n");

    for(struct memregion_t *tmp = ct->mem->first_region; tmp != NULL; tmp = tmp->next)
    {
        printk("mprotect: tmp->addr %lx\n", tmp->addr);
    }
    
    screen_refresh(NULL);
    */
    
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    return res;
}


static void remap_pages(virtual_addr dest, virtual_addr src, size_t memsz)
{
    virtual_addr dest_end = dest + memsz;
    pt_entry *de, *se;

    while(dest < dest_end)
    {
        if(!(de = get_page_entry((void *)dest)) ||
           !(se = get_page_entry((void *)src)))
        {
            break;
        }

        *de = *se;

        // Temporarily increase frame shares. A later call to 
        // memregion_remove_overlaps() will decrement this value
        inc_frame_shares(PTE_FRAME(*se));
        vmmngr_flush_tlb_entry(dest);
        dest += PAGE_SIZE;
        src += PAGE_SIZE;
    }
}


#define OVERLAPS(oa, os, na, ns)                                \
    (((virtual_addr)na >= (virtual_addr)oa &&                   \
        ((virtual_addr)na <= ((virtual_addr)oa + os))) ||       \
    (((virtual_addr)na + ns) >= (virtual_addr)oa &&             \
        (((virtual_addr)na + ns) <= ((virtual_addr)oa + os))))


/*
 * Handler for syscall mremap().
 */
long syscall_mremap(struct syscall_args *__args)
{
    struct syscall_args a;
    long res, i;
    virtual_addr addr = 0, aligned_size, end;
    size_t memsz;
    struct memregion_t *memregion = NULL;
	struct task_t *ct = (struct task_t *)this_core->cur_task;
    
    // syscall args
    void *old_address;
    size_t old_size;
    size_t new_size;
    int flags;
    void *new_address;
    volatile void **res_address;

    // get the args
    COPY_SYSCALL6_ARGS(a, __args);
    old_address = (void *)(a.args[0]);
    old_size = (size_t)(a.args[1]);
    new_size = (size_t)(a.args[2]);
    flags = (int)(a.args[3]);
    new_address = (void *)(a.args[4]);
    res_address = (volatile void **)(a.args[5]);
    
    if(!PAGE_ALIGNED(old_address))
    {
        return -EINVAL;
    }

    aligned_size = align_up((virtual_addr)old_size);
    end = (virtual_addr)old_address + aligned_size;

    //printk("syscall_mremap: addr 0x%lx, end 0x%lx\n", old_address, end);

    // check we're not trying to unmap kernel memory
    if(((size_t)old_address >= USER_MEM_END) ||
                (end > USER_MEM_END))
    {
        return -EINVAL;
    }

    if((memregion = memregion_containing(ct, 
                                         (virtual_addr)old_address)) == NULL)
    {
        add_task_segv_signal(ct, SEGV_MAPERR, old_address);
        return -EFAULT;
    }
    
    memsz = memregion->size * PAGE_SIZE;
    //printk("syscall_mremap: memsz 0x%lx\n", memsz);
    
    if(memregion->addr + memsz < (virtual_addr)old_address + old_size)
    {
        add_task_segv_signal(ct, SEGV_MAPERR,
                             (void *)((virtual_addr)old_address + old_size));
        return -EFAULT;
    }

    /*
     * 1 - Handle shared memory regions (shmem).
     */
    if(memregion->type == MEMREGION_TYPE_SHMEM)
    {
        /*
         * Currently we only allow remapping on the whole region, i.e. no
         * fragmentation. If new_size is 0, we take it as the whole region.
         * Also, old_size should be 0 or the size of the shared memory region.
         */
        if(old_size != 0 && old_size != memsz)
        {
            return -EINVAL;
        }
        
        if(new_size == 0)
        {
            new_size = memsz;
        }
        else if(new_size != memsz)
        {
            return -EINVAL;
        }
        
        /* Get the id of the shared memory region backing this memregion */
        if((i = memregion_to_shmid((void *)memregion->addr, memregion)) < 0)
        {
            return i;
        }
        
        /*
         * If the MREMAP_FIXED and MREMAP_MAYMOVE flags are specified, map
         * the shared memory at new_address, removing any overlapping memory
         * regions.
         */
        if(flags & MREMAP_FIXED)
        {
            /* This flag must be specified with MREMAP_FIXED */
            if(!(flags & MREMAP_MAYMOVE))
            {
                return -EINVAL;
            }

            if(!new_address || !PAGE_ALIGNED(new_address) ||
               OVERLAPS(old_address, old_size, new_address, new_size))
            {
                return -EINVAL;
            }
            
            if((res = syscall_shmat(i, new_address, SHM_REMAP, res_address)) != 0)
            {
                return res;
            }

            /* remove the old mapping */
            return memregion_remove_overlaps(ct, memregion->addr,
                                                 memregion->addr + memsz);
        }

        /*
         * If not MREMAP_FIXED, check if there is an overlapping memory region.
         * If there is, select a new address and map into it. Otherwise, try to
         * map into new_address (the call might fail if there is a memory
         * region that partially overlaps with us).
         */
        //printk("mremap: s %lx, e %lx\n", new_address, (virtual_addr)new_address + new_size);

        if(memregion_check_overlaps(ct, (virtual_addr)new_address,
                                    (virtual_addr)new_address + new_size) == 0)
        {
            if((res = syscall_shmat(i, new_address, 0, res_address)) != 0)
            {
                return res;
            }

            /* remove the old mapping */
            return memregion_remove_overlaps(ct, memregion->addr,
                                                 memregion->addr + memsz);
        }

        if(!(flags & MREMAP_MAYMOVE))
        {
            return -ENOMEM;
        }

        if((res = syscall_shmat(i, NULL, 0, res_address)) != 0)
        {
            return res;
        }

        /* remove the old mapping */
        return memregion_remove_overlaps(ct, memregion->addr,
                                             memregion->addr + memsz);
    }

    /*
     * 2 - Handle other types of memory regions.
     */

    if(new_size == 0)
    {
        return -EINVAL;
    }

    new_size = align_up((virtual_addr)new_size);

    kernel_mutex_lock(&(ct->mem->mutex));

    if(flags & MREMAP_FIXED)
    {
        printk("syscall_mremap: 4\n");
        /* This flag must be specified with MREMAP_FIXED */
        if(!(flags & MREMAP_MAYMOVE))
        {
            kernel_mutex_unlock(&(ct->mem->mutex));
            return -EINVAL;
        }

        if(!new_address || !PAGE_ALIGNED(new_address) ||
           OVERLAPS(old_address, old_size, new_address, new_size))
        {
            kernel_mutex_unlock(&(ct->mem->mutex));
            return -EINVAL;
        }

        if((res = memregion_alloc_and_attach(ct, memregion->inode,
                               memregion->fpos,
                               memregion->inode ?
                                    memregion->flen + (new_size - memsz) : 0,
                               (virtual_addr)new_address,
                               (virtual_addr)new_address + new_size,
                               memregion->prot, memregion->type,
                               memregion->flags, 1)) != 0)
        {
            kernel_mutex_unlock(&(ct->mem->mutex));
            return res;
        }


        if(new_size >= aligned_size)
        {
            remap_pages((virtual_addr)new_address, 
                            (virtual_addr)old_address, aligned_size);
        }
        else
        {
            remap_pages((virtual_addr)new_address, 
                            (virtual_addr)old_address, new_size);
        }

        addr = (virtual_addr)new_address;
        res = memregion_remove_overlaps(ct, (virtual_addr)old_address, end);
        goto fin;
    }
    
    if(new_size == aligned_size)
    {
        addr = (virtual_addr)old_address;
        res = 0;
        goto fin;
    }

    /*
     * If shrinking, do it. If expanding, check for (and remove) any
     * overlapping memory regions, before doing the expansion.
     */
    if(new_size < aligned_size)
    {
        addr = (virtual_addr)old_address;
        res = memregion_remove_overlaps(ct,
                                        (virtual_addr)old_address + new_size,
                                        (virtual_addr)old_address + aligned_size);
        goto fin;
    }

   	if(pmmngr_get_free_block_count() <= ((new_size - aligned_size) / PAGE_SIZE))
   	{
        res = -ENOMEM;
        goto fin;
   	}

    // expanding with no overlaps
    if(memregion_check_overlaps(ct, (virtual_addr)old_address + aligned_size,
                                    (virtual_addr)old_address + new_size) == 0)
    {
        if((virtual_addr)old_address + new_size > memregion->addr + memsz)
        {
            new_size = ((virtual_addr)old_address + new_size) - memregion->addr;
            memregion->size = new_size / PAGE_SIZE;

            if(memregion->inode)
            {
                memregion->flen = new_size;
            }
        }

        addr = (virtual_addr)old_address;
        res = 0;
        goto fin;
    }
    
    // expanding with overlap, MREMAP_MAYMOVE not set
    if(!(flags & MREMAP_MAYMOVE))
    {
        printk("mremap: 2a\n");
        res = -ENOMEM;
        goto fin;
    }

    if(!(new_address = 
            (void *)get_user_addr(new_size, USER_SHM_START, USER_SHM_END)))
    {
        printk("mremap: 2b\n");
        res = -ENOMEM;
        goto fin;
    }

    if((res = memregion_alloc_and_attach(ct, memregion->inode,
                               memregion->fpos,
                               memregion->inode ?
                                    memregion->flen + (new_size - memsz) : 0,
                               (virtual_addr)new_address,
                               (virtual_addr)new_address + new_size,
                               memregion->prot, memregion->type,
                               memregion->flags, 1)) != 0)
    {
        goto fin;
    }

    remap_pages((virtual_addr)new_address, (virtual_addr)old_address, aligned_size);

    addr = (virtual_addr)new_address;
    res = memregion_remove_overlaps(ct, (virtual_addr)old_address, end);

fin:

    if(res == 0)
    {
        memregion_consolidate(ct);
    }

    kernel_mutex_unlock(&(ct->mem->mutex));
    *res_address = (void *)addr;
    return res;
}


/*
 * Handler for syscall mincore().
 */
long syscall_mincore(void *__addr, size_t length, unsigned char *vec)
{
    int i;
    size_t arrsz;
    virtual_addr addr = (virtual_addr)__addr, aligned_size, end;
    struct memregion_t *memregion = NULL;
	struct task_t *ct = (struct task_t *)this_core->cur_task;
    
    if(!addr || !vec)
    {
        add_task_segv_signal(ct, SEGV_MAPERR, addr ? vec : __addr);
        return -EFAULT;
    }
    
    if(!PAGE_ALIGNED(addr))
    {
        return -EINVAL;
    }
    
    if(!length)
    {
        return -EINVAL;
    }

    if((memregion = memregion_containing(ct, addr)) == NULL)
    {
        return -ENOMEM;
    }
    
    aligned_size = align_up(length);
    end = addr + aligned_size;
    arrsz = aligned_size / PAGE_SIZE;
    
    unsigned char arr[arrsz];

    kernel_mutex_lock(&(ct->mem->mutex));

    for(i = 0; addr < end; addr += PAGE_SIZE, i++)
    {
        pt_entry *page = get_page_entry((void *) addr);

        if(page && PTE_PRESENT(*page))
        {
            arr[i] = 1;
        }
        else
        {
            arr[i] = 0;
        }
    }

    kernel_mutex_unlock(&(ct->mem->mutex));
    
    return copy_to_user(vec, arr, arrsz);
}

