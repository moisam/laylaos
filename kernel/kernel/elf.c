/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: elf.c
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
 *  \file elf.c
 *
 *  Load an ELF executable to memory.
 */

//#define __DEBUG
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/elf.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/pcache.h>
#include <mm/kheap.h>
#include <mm/mmap.h>
#include "../../vdso/vdso.h"

static int elf_load_exec(struct fs_node_t *node, struct cached_page_t *block0,
                         size_t *auxv, int flags);


/*
 * Load ELF file.
 */
int elf_load_file(struct fs_node_t *node, struct cached_page_t *block0,
                  size_t *auxv, int flags)
{
    if(!node || !block0 || !auxv)
    {
        return -EINVAL;
    }
    
    elf_ehdr *hdr = (elf_ehdr *)block0->virt;
  
    if(!check_elf_hdr("elf", hdr, 1))
    {
        printk("Invalid ELF file header\n");
        return -ENOEXEC;
    }

    if(hdr->e_type != ET_REL && hdr->e_type != ET_EXEC &&
       hdr->e_type != ET_DYN)
    {
        printk("Unsupported ELF file type\n");
        return -ENOEXEC;
    }
    
    return elf_load_exec(node, block0, auxv, flags);

#if 0
    switch(hdr->e_type)
    {
        case ET_EXEC:
            return elf_load_exec(node, block0, auxv, flags);
        
        /*
        case ET_REL:
            //return elf_load_rel(node, hdr_data);
            return -ENOSYS;
        */
    }
    
    return -ENOEXEC;
#endif
}


static int elf_load_segment(struct fs_node_t *node,
                            struct cached_page_t *block0,
                            unsigned char *buf, size_t pos, size_t count)
{
    volatile size_t left = count;
    size_t offset, i, j;
    char *p;
    struct cached_page_t *dbuf;
    
    while(left)
    {
        dbuf = NULL;
        offset = pos / PAGE_SIZE;

        if(offset == 0)
        {
            /*
             * If the block we're looking for is the same as blocks[0], no need
             * to read it again (actually, if we try to read it, we'll lock
             * ourselves, as the block is locked by execve()!).
             */
            dbuf = block0;
        }
        else if(!(dbuf = get_cached_page(node, offset, 0)))
        {
            return -EIO;
        }

        i = pos % PAGE_SIZE;
        j = MIN((PAGE_SIZE - i), left);
        pos += j;
        left -= j;

        p = (char *)(dbuf->virt + i);
        A_memcpy(buf, p, j);
        buf += j;

        if(dbuf != block0)
        {
            release_cached_page(dbuf);
        }
    }

    return 0;
}


#if 0
/*
 * Reserve memory in userspace. Similar to what we do in mmap.c, except we
 * use different offsets here as we are using a different memory region (the
 * region we reserve for shared libraries).
 */
static uintptr_t elf_get_user_addr(size_t size)
{
    uintptr_t addr;
    struct task_t *ct = cur_task;
    
    // search for an address in the 1GB to 3GB address range
    for(addr = LIB_ADDR_START; addr < LIB_ADDR_END; addr += PAGE_SIZE)
    {
        if(memregion_check_overlaps(ct, addr, addr + size) == 0)
        {
            //printk("elf_get_user_addr: s %lx, e %lx\n", addr, addr + size);
            return addr;
        }
    }
    
    return 0;
}
#endif


static void calc_elf_limits(struct task_t *ct, 
                            elf_ehdr *hdr, elf_phdr *phdr, 
                            uintptr_t offset)
{
    elf_phdr *lphdr = &phdr[hdr->e_phnum];
    size_t mempos, memend;
    uint32_t imgsz = 0;

    ct->end_data = 0;
    ct->image_base = 0;

    for( ; phdr < lphdr; phdr++)
    {
        if(phdr->p_type == PT_LOAD)
        {
            mempos = align_down(phdr->p_vaddr);
            memend = align_up(phdr->p_vaddr + phdr->p_memsz);
            
            mempos += offset;
            memend += offset;

            if(memend > ct->end_data)
            {
                ct->end_data = memend;
            }
      
            imgsz += (memend - mempos);
            
            if(ct->image_base == 0 || ct->image_base > mempos)
            {
	            ct->image_base = mempos;
	        }
        }
    }

    ct->image_size = align_up(imgsz) / PAGE_SIZE;
}


static int elf_load_exec(struct fs_node_t *node, struct cached_page_t *block0,
                         size_t *auxv, int flags)
{
    elf_ehdr *hdr = (elf_ehdr *)block0->virt;
    uintptr_t dyn_base = 0;
    int res;
    int load_now = (flags & ELF_FLAG_LOAD_NOW);
    virtual_addr vdso;

    // load header table
    size_t bufsz = hdr->e_phnum * sizeof(elf_phdr);
    unsigned char *buf = (unsigned char *)kmalloc(bufsz);
    
    if(!buf)
    {
        return -ENOMEM;
    }

    if(elf_load_segment(node, block0, buf, hdr->e_phoff, bufsz) != 0)
    {
        kfree(buf);
        return -EIO;
    }
    
    elf_phdr *phdr = (elf_phdr *)buf;
    elf_phdr *lphdr = &phdr[hdr->e_phnum];
    struct task_t *ct = cur_task;
    
    kernel_mutex_lock(&(ct->mem->mutex));

    for( ; phdr < lphdr; phdr++)
    {
        KDEBUG("elf_load_exec: segment (type 0x%x)\n", phdr->p_type);

        if(phdr->p_type == PT_INTERP)
        {
            kernel_mutex_unlock(&(ct->mem->mutex));
            res = ldso_load(auxv);
            KDEBUG("elf_load_exec: res %d\n", res);

            if(res == 0)
            {
                if(hdr->e_type != ET_DYN)
                {
                    calc_elf_limits(ct, hdr, (elf_phdr *)buf, 0);
                }
                
                ct->properties |= PROPERTY_DYNAMICALLY_LOADED;
            }

            kfree(buf);

            return res;
        }
    }

    for(phdr = (elf_phdr *)buf; phdr < lphdr; phdr++)
    {
        KDEBUG("elf_load_exec: loading segment (type 0x%x)\n", phdr->p_type);
        //__asm__ __volatile__("xchg %%bx, %%bx"::);

        if(phdr->p_type == PT_LOAD)
        {
            size_t mempos, memend, filepos, filesize, z;
            
            // some sanity checks first
            if(phdr->p_memsz < phdr->p_filesz)
            {
                goto err_inval;
            }
            
            if(phdr->p_filesz &&
               (phdr->p_vaddr % phdr->p_align !=
                                phdr->p_offset % phdr->p_align))
            {
                goto err_inval;
            }
            
            if(phdr->p_vaddr > USER_MEM_END ||
               (phdr->p_vaddr + phdr->p_memsz) > USER_MEM_END)
            {
                goto err_inval;
            }

            filepos = align_down(phdr->p_offset);
            filesize = phdr->p_filesz + (phdr->p_offset - filepos);
            mempos = align_down(phdr->p_vaddr);
            memend = align_up(phdr->p_vaddr + phdr->p_memsz);
            
            // set the base address for the dynamic loader
            if(hdr->e_type == ET_DYN)
            {
                if(dyn_base == 0)
                {
                    if((dyn_base = get_user_addr(memend - mempos, 
                                        LIB_ADDR_START, LIB_ADDR_END)) == 0)
                    //if((dyn_base = elf_get_user_addr(memend - mempos)) == 0)
                    {
                        goto err_inval;
                    }
                }
                
                mempos += dyn_base;
                memend += dyn_base;
            }
            
            // add the segment to the task's memory map
            int writeable = (phdr->p_flags & PT_W);
            int prot = PROT_READ | (writeable ? PROT_WRITE : 0) |
                                   ((phdr->p_flags & PT_X) ? PROT_EXEC : 0);
            int type = writeable ? MEMREGION_TYPE_DATA : MEMREGION_TYPE_TEXT;

            if((res = memregion_alloc_and_attach(ct,
                                  node, filepos, filesize,
                                  mempos, memend, prot, type,
                                  MAP_PRIVATE, 0)) != 0)
            {
                kernel_mutex_unlock(&(ct->mem->mutex));
                kfree(buf);
                return res;
            }

            if(load_now)
            {
                for(z = mempos; z < memend; z += PAGE_SIZE)
                {
                    pt_entry *pt = get_page_entry((void *)z);
                
                    if(!pt)
                    {
                        kfree(buf);
                        return -ENOMEM;
                    }

                    vmmngr_alloc_page(pt, (PTE_FLAGS_PWU | I86_PTE_PRIVATE));
                    vmmngr_flush_tlb_entry(z);
                }

                if(elf_load_segment(node, block0,
                              (unsigned char *)mempos, filepos, filesize) != 0)
                {
                    kfree(buf);
                    return -EIO;
                }

                /* zero out the rest of the image space in memory */
                size_t memsize = (memend - mempos);
                
                if(filesize < memsize)
                {
    	            A_memset((void *)(mempos + filesize), 0,
    	                                        memsize - filesize);
	            }
	        }

            z = memend;

            if(!writeable)
            {
                if(load_now)
                {
                    for(z = mempos; z < memend; z += PAGE_SIZE)
                    {
                        pt_entry *pt = get_page_entry((void *)z);
                        PTE_DEL_ATTRIB(pt, I86_PTE_WRITABLE);
                    }
                }
            }
        }
        /*
        else if(phdr->p_type == PT_DYNAMIC)
        {
        }
        else if(phdr->p_type == PT_INTERP)
        {
        }
        */
    }
    
    calc_elf_limits(ct, hdr, (elf_phdr *)buf, dyn_base);
    
    /*
     * We rely on the C library to load dynamically linked executables.
     * Therefore, we fill the auxiliary vector with information related
     * to the dynamic linker, which will then do the heavy work of loading
     * the executable.
     */
    A_memset(auxv, 0, AUXV_SIZE * sizeof(size_t) * 2);
    res = 0;
    auxv[res++    ] = AT_PHDR;
    auxv[res++    ] = ct->image_base + hdr->e_phoff;
    auxv[res++    ] = AT_PHENT;
    auxv[res++    ] = hdr->e_phentsize;
    auxv[res++    ] = AT_PHNUM;
    auxv[res++    ] = hdr->e_phnum;
    auxv[res++    ] = AT_PAGESZ;
    auxv[res++    ] = PAGE_SIZE;
    auxv[res++    ] = AT_BASE;
    auxv[res++    ] = ct->image_base;
    auxv[res++    ] = AT_ENTRY;
    auxv[res++    ] = dyn_base + hdr->e_entry;
    auxv[res++    ] = AT_UID;
    auxv[res++    ] = ct->uid;
    auxv[res++    ] = AT_EUID;
    auxv[res++    ] = ct->euid;
    auxv[res++    ] = AT_GID;
    auxv[res++    ] = ct->gid;
    auxv[res++    ] = AT_EGID;
    auxv[res++    ] = ct->egid;
    auxv[res++    ] = AT_HWCAP;
    auxv[res++    ] = 0;     // TODO
    auxv[res++    ] = AT_CLKTCK;
    auxv[res++    ] = PIT_FREQUENCY;
    //auxv[res++    ] = AT_RANDOM;
    //auxv[res++    ] = 0;     // TODO

    kernel_mutex_unlock(&(ct->mem->mutex));
    kfree(buf);

    // Try to map the vdso
    if(map_vdso(&vdso) == 0)
    {
        auxv[res++    ] = AT_SYSINFO_EHDR;
        auxv[res++    ] = vdso;
    }

    KDEBUG("elf_load_exec: end_data 0x%x\n", ct->end_data);

    return 0;

err_inval:
    kernel_mutex_unlock(&(ct->mem->mutex));
    kfree(buf);
    return -EINVAL;
}

