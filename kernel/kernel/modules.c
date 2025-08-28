/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: modules.c
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
 *  \file modules.c
 *
 *  Initialise kernel modules at boot time.
 */

#define __DEBUG

#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/modules.h>
#include <kernel/dev.h>
#include <kernel/ksymtab.h>
#include <mm/kstack.h>
#include "../../vdso/vdso.h"

// count of loaded boot modules
int boot_module_count = 0;

// boot modules list
struct boot_module_t boot_module[MAX_BOOT_MODULES];

// loaded modules list and the lock to access the list
struct kmodule_t modules_head;
volatile struct kernel_mutex_t kmod_list_mutex;

#define ZERO_MODULE_INDEXES(i)          \
{                                       \
    boot_module[i].vstart = 0;          \
    boot_module[i].vend = 0;            \
    boot_module[i].pstart = 0;          \
    boot_module[i].pend = 0;            \
}

/*
 * Initialize kernel modules. The modules MUST be loaded as follows:
 *     Module[0] => initrd image
 *     Module[1] => the kernel's symbol table (/boot/System.map)
 *     Module[2..n] => rest of boot modules
 *
 * This function also loads and decompresses the initial ramdisk (initrd),
 * which we'll mount later as our filesystem root. It also loads the kernel
 * symbol table, needed to load other kernel modules.
 */
void boot_module_init(void)
{
    int i;
    int found_initrd = 0, found_symtab = 0;
    
    init_kernel_mutex(&kmod_list_mutex);
    memset(&modules_head, 0, sizeof(struct kmodule_t));
    
    // if no loaded modules, bail out
    if(!boot_module_count)
    {
        return;
    }
    
    // note that the start & end members of our module struct currently point
    // to physical memory addresses (where the modules where loaded by the
    // bootloader), and in order to make those meaningful, we have to either
    // identity-map them, or map them to higher memory.
    
    // here we map each module's physical memory to temporary virtual memory
    // in the address range reserved for kernel modules, that is, between
    // KMODULE_START and KMODULE_END.

    printk("  Looking for boot modules..\n");

    for(i = 0; i < boot_module_count; i++)
    {
        if(!(boot_module[i].vstart = phys_to_virt_off(boot_module[i].pstart,
                                                      boot_module[i].pend,
                                                      PTE_FLAGS_PW, 
                                                      REGION_KMODULE)))
        {
            kpanic("Failed to map boot module to memory\n");
            empty_loop();
        }

        boot_module[i].vend = boot_module[i].vstart +
                                (boot_module[i].pend - boot_module[i].pstart);
        
        printk("    [%d] mapped " _XPTR_ "-" _XPTR_ " at " 
               _XPTR_ "-" _XPTR_ " (cmdline %s)..\n",
                    i, boot_module[i].pstart, boot_module[i].pend,
                       boot_module[i].vstart, boot_module[i].vend,
                       boot_module[i].cmdline);
    }

    for(i = 0; i < boot_module_count; i++)
    {
        size_t sz;

        sz = boot_module[i].vend - boot_module[i].vstart;

        if(memcmp(boot_module[i].cmdline, "INITRD", 6) == 0)
        {
            printk("  Found initramdisk..\n");
            found_initrd = 1;

            if(ramdisk_init(boot_module[i].vstart, boot_module[i].vend) != 0)
            {
                kpanic("Failed to decompress initrd\n");
            }
        }
        else if(memcmp(boot_module[i].cmdline, "SYSTEM.MAP", 10) == 0)
        {
            printk("  Found kernel symbol table..\n");
            found_symtab = 1;

            if(ksymtab_init(boot_module[i].vstart, boot_module[i].vend) != 0)
            {
                kpanic("Failed to load kernel's symbol table\n");
            }
        }
        else if(memcmp(boot_module[i].cmdline, "VDSO", 4) == 0)
        {
            printk("  Found virtual dynamic shared object (vdso)..\n");

            if(vdso_stub_init(boot_module[i].vstart, boot_module[i].vend) != 0)
            {
                kpanic("Failed to load the vdso\n");
            }
        }
        else
        {
            if(init_module_internal((void *)boot_module[i].vstart, sz,
                                    boot_module[i].cmdline, 1) != 0)
            {
                kpanic("Failed to load boot module\n");
            }
        }

        vmmngr_free_pages(boot_module[i].vstart, sz);
        ZERO_MODULE_INDEXES(i);
    }

    if(!found_initrd)
    {
        kpanic("Kernel was loaded without initrd\n");
    }

    if(!found_symtab)
    {
        kpanic("Kernel was loaded without a symbol table\n");
    }
    
    printk("Finished loading modules\n");

    boot_module_count = 0;
}

