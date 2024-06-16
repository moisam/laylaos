/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: gdt.c
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
 *  \file gdt.c
 *
 * Define Global Descriptor Table [GDT] for x86 and x64 processors to
 * handle basic memory map and permission levels.
 *
 * See: https://wiki.osdev.org/GDT
 */

#include <errno.h>
#include <string.h>
#include <kernel/gdt.h>
#include <kernel/tss.h>
#include <kernel/user.h>
#include <kernel/task.h>
#include <kernel/msr.h>

struct gdt_descriptor_s GDT[MAX_DESCRIPTORS];
uint16_t gdt_descriptor_count;

/* GDTR data */
struct gdtr GDTR;

/* defined in gdt_install.S */
extern void _gdt_install();

/* defined in boot.S */
extern unsigned int stack_top;

#define SEGMENT_PRESENT(n)      (GDT[(n)].access & 0x80)

/*
 * Set a Descriptor in the GDT
 */
void gdt_add_descriptor(uint32_t no, uint32_t base,
                        uint32_t limit, uint8_t type)
{
    if(no >= MAX_DESCRIPTORS)
    {
     return;
    }
  
    /* NULL out the Descriptor */
    memset((void *) &GDT[no], 0, sizeof(struct gdt_descriptor_s));

#ifdef __x86_64__

    if(limit > 65536)
    {
        limit >>= 12;
        GDT[no].flags = 0xA0;
    }
    else
    {
        GDT[no].flags = 0x20;
    }

#else

    if(limit > 65536)
    {
        limit >>= 12;
        GDT[no].flags = 0xC0;
    }
    else
    {
        GDT[no].flags = 0x40;
    }

#endif
  
    GDT[no].limit = (uint16_t)(limit & 0xFFFF);
    GDT[no].base_low = (uint16_t)(base & 0xFFFF);
    GDT[no].base_mid = (uint8_t)((base >> 16) & 0xFF);
    GDT[no].access = type;
    GDT[no].flags |= (uint8_t)(limit >> 16) & 0xF;
    GDT[no].base_hi  = (uint8_t)((base >> 24) & 0xFF);
}


/*
void gdt_clear_descriptor(uint32_t no)
{
    if(!no)
    {
        return;
    }
    
    gdt_add_descriptor(no, 0, 0, 0);
}
*/


#ifdef __x86_64__

static void gdt_add_descriptor64(uint32_t no, uint64_t base,
                                 uint32_t limit, uint8_t type)
{
    struct gdt_descriptor64_s *desc;

    if(no >= MAX_DESCRIPTORS) return;

    desc = (struct gdt_descriptor64_s *)&GDT[no];
    gdt_add_descriptor(no, (base & 0xFFFFFFFF), limit, type);
    memset((void *) &GDT[no + 1], 0, sizeof(struct gdt_descriptor_s));
    desc->base_very_hi = (base >> 32) & 0xFFFFFFFF;
}


static void set_gs_base(uintptr_t base)
{
    wrmsr(IA32_GS_BASE, base);
    wrmsr(IA32_KERNGS_BASE, base);
	__asm__ __volatile__("swapgs");
}

#endif      /* __x86_64__ */


/*
 * Initialise the GDT.
 * See: https://wiki.osdev.org/GDT_Tutorial#Flat_Setup
 */
void gdt_init(void)
{
    gdt_descriptor_count = MAX_DESCRIPTORS;
    /* set GDTR */
    GDTR.limit = (uint16_t)((sizeof(struct gdt_descriptor_s) * 
                                        gdt_descriptor_count) - 1);
    GDTR.base = (uintptr_t)&GDT;
  
    gdt_add_descriptor(0, 0, 0,          0   );	//0x00 - NULL Descriptor
    gdt_add_descriptor(1, 0, 0xFFFFFFFF, 0x9A);	//0x08 - Ring 0 CODE Descriptor
    gdt_add_descriptor(2, 0, 0xFFFFFFFF, 0x92);	//0x10 - Ring 0 DATA Descriptor
    gdt_add_descriptor(3, 0, 0xFFFFFFFF, 0xFA);	//0x18 - Ring 3 CODE Descriptor
    gdt_add_descriptor(4, 0, 0xFFFFFFFF, 0xF2);	//0x20 - Ring 3 DATA Descriptor

#ifdef __x86_64__

    // 0x28 - Repeat CODE Descriptor for Ring 3 to satisfy 
    // SYSCALL/SYSRET requirements
    gdt_add_descriptor(5, 0, 0xFFFFFFFF, 0xFA);

#endif      /* __x86_64__ */

    tss_install(0x10, (uintptr_t)&stack_top);

#ifdef __x86_64__

    // 0x28   - TSS  Descriptor
    gdt_add_descriptor64(6, (uint64_t)&tss_entry, sizeof(tss_entry), 0x89);

    /*
     * TODO: load our processor local data into GS.
     */
    set_gs_base(0);

#else

    // 0x28   - TSS  Descriptor
    gdt_add_descriptor(5, (uint32_t)&tss_entry, sizeof(tss_entry), 0x89);
    // 0x30 - DATA Descriptor for TLS
    gdt_add_descriptor(GDT_TLS_DESCRIPTOR, 0, 0xFFFFFFFF, 0xF2);

#endif      /* __x86_64__ */

    _gdt_install();
    tss_flush();
}


/*
 * See:
 *   https://man7.org/linux/man-pages/man2/set_thread_area.2.html
 */
int syscall_set_thread_area(struct user_desc *u_info)
{
    uint32_t n;
    struct user_desc tmp;
    int res;
    struct task_t *ct = cur_task;
    
    //printk("syscall_set_thread_area: u_info 0x%lx\n", u_info);
    
    if(!u_info)
    {
        return -EINVAL;
    }
    
    if((res = copy_from_user(&tmp, u_info, sizeof(struct user_desc))))
    {
        return res;
    }


    n = GDT_TLS_DESCRIPTOR;

#ifdef __x86_64__

    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    wrmsr(IA32_FS_BASE, tmp.base_addr);

#else

    // 0x30 - DATA Descriptor for TLS
    gdt_add_descriptor(n, tmp.base_addr, tmp.limit, 0xF2);

#endif      /* __x86_64__ */

    tmp.entry_number = n;
    ct->ldt.base = tmp.base_addr;
    ct->ldt.limit = tmp.limit;
    
    return copy_to_user(u_info, &tmp, sizeof(struct user_desc));
}


int syscall_get_thread_area(struct user_desc *u_info)
{
    uint32_t n;
    struct user_desc tmp;
    int res;
    
    if(!u_info)
    {
        return -EINVAL;
    }
    
    if((res = copy_from_user(&tmp, u_info, sizeof(struct user_desc))))
    {
        return res;
    }
    
    n = tmp.entry_number;
    
    if(n < 6 || n >= MAX_DESCRIPTORS)
    {
        return -EINVAL;
    }

#ifdef __x86_64__

    struct task_t *ct = cur_task;
    
    tmp.base_addr = ct->ldt.base;
    tmp.limit = ct->ldt.limit;
    tmp.useable = (tmp.base_addr && tmp.limit) ? 1 : 0;

#else      /* !__x86_64__ */

    tmp.base_addr = (uint32_t)GDT[n].base_low |
                    ((uint32_t)GDT[n].base_mid << 16) |
                    ((uint32_t)GDT[n].base_hi << 24);

    tmp.limit = (uint32_t)GDT[n].limit |
                        ((uint32_t)(GDT[n].flags & 0xF) << 16);
    
    tmp.useable = (tmp.base_addr && tmp.limit) ? 1 : 0;
    tmp.read_exec_only = (GDT[n].access & 0x08) ? 1 : 0;
    tmp.seg_not_present = SEGMENT_PRESENT(n) ? 0 : 1;

    // check size
    tmp.seg_32bit = (GDT[n].flags & 0x40) ? 1 : 0;
    
    // check granularity
    tmp.limit_in_pages = (GDT[n].flags & 0x80) ? 1 : 0;

#endif      /* !__x86_64__ */

    return copy_to_user(u_info, &tmp, sizeof(struct user_desc));
}

