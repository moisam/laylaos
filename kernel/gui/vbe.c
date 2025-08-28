/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: vbe.c
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
 *  \file vbe.c
 *
 *  This file provides functions for handling VBE (VESA BIOS Extensions).
 *  These functions are required in order to implement the framebuffer device.
 *  Our current implementation depends on the bootloader to pass us a
 *  multiboot struct that contains (what we assume is) valid VBE information.
 */

#define __DEBUG

#include <errno.h>
#include <string.h>
#include <kernel/asm.h>
#include <kernel/laylaos.h>
#include <kernel/tty.h>
#include <kernel/vga.h>
#include <mm/mmap.h>
#include <mm/kheap.h>
#include <mm/kstack.h>
#include <gui/vbe.h>
#include <gui/fb.h>

static int vbe_inited = 0;

uint16_t vbe_mode = 0;
struct vbe_control_info_t vbe_control_info = { 0, };
struct vbe_mode_info_t vbe_mode_info = { 0, };
struct framebuffer_t vbe_framebuffer = { 0, };

#ifndef MULTIBOOT2_BOOTLOADER_MAGIC

/*
 * See: https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 */
struct ext_multiboot_info
{
    multiboot_info_t mbd;
    multiboot_uint64_t framebuffer_addr;
    multiboot_uint32_t framebuffer_pitch;
    multiboot_uint32_t framebuffer_width;
    multiboot_uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t alignment;

    union
    {
        struct
        {
            multiboot_uint32_t framebuffer_palette_addr;
            multiboot_uint16_t framebuffer_palette_num_colors;
        };

        struct
        {
            uint8_t framebuffer_red_field_position;
            uint8_t framebuffer_red_mask_size;
            uint8_t framebuffer_green_field_position;
            uint8_t framebuffer_green_mask_size;
            uint8_t framebuffer_blue_field_position;
            uint8_t framebuffer_blue_mask_size;
        };
    };
} __attribute__((packed));

typedef struct ext_multiboot_info ext_multiboot_info_t;

#endif      /* !MULTIBOOT2_BOOTLOADER_MAGIC */


/*
 * Check if we are running in EGA mode.
 */
int using_ega(void)
{
    return (vbe_mode == 0 ||
            vbe_framebuffer.phys_addr == 0 ||
            vbe_framebuffer.phys_addr == (uint8_t *)VGA_MEMORY_PHYSICAL);
}


/*
 * Get bootloader VBE info.
 */
void get_vbe_info(unsigned long addr)
{

#ifdef MULTIBOOT2_BOOTLOADER_MAGIC

    struct multiboot_tag *tag;
    struct multiboot_tag_vbe *vbe;
    struct multiboot_tag_framebuffer *fb;

    if((tag = find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_VBE)))
    {
        vbe = (struct multiboot_tag_vbe *)tag;
        vbe_mode = vbe->vbe_mode;

        memcpy(&vbe_control_info, &vbe->vbe_control_info,
               sizeof(struct vbe_control_info_t));

        memcpy(&vbe_mode_info, &vbe->vbe_mode_info,
               sizeof(struct vbe_mode_info_t));
    }

    if((tag = find_tag_of_type(addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER)))
    {
        if(vbe_mode == 0)
        {
            vbe_mode = 1;
        }

        fb = (struct multiboot_tag_framebuffer *)tag;

        vbe_framebuffer.phys_addr = (uint8_t *)
                                    (unsigned long)
                                    fb->common.framebuffer_addr;

        vbe_framebuffer.pitch = fb->common.framebuffer_pitch;
        vbe_framebuffer.width = fb->common.framebuffer_width;
        vbe_framebuffer.height = fb->common.framebuffer_height;
        vbe_framebuffer.bpp = fb->common.framebuffer_bpp;
        vbe_framebuffer.type = fb->common.framebuffer_type;

        // we lie to ourselves and pretend that 15 bits-per-pixel is the
        // same as 16 bits-per-pixel, to ease our calculations later
        if(vbe_framebuffer.bpp == 15)
        {
            vbe_framebuffer.bpp = 16;
        }

        if(fb->common.framebuffer_type == 0)     // palette-indexed
        {
            vbe_framebuffer.palette_phys_addr = (uint8_t *)(uintptr_t)fb->framebuffer_palette;
            vbe_framebuffer.palette_num_colors = fb->framebuffer_palette_num_colors;
        }
        else if(fb->common.framebuffer_type == 1)    // rgb
        {
            vbe_framebuffer.red_pos = fb->framebuffer_red_field_position;
            vbe_framebuffer.red_mask_size = fb->framebuffer_red_mask_size;
            vbe_framebuffer.green_pos = fb->framebuffer_green_field_position;
            vbe_framebuffer.green_mask_size = fb->framebuffer_green_mask_size;
            vbe_framebuffer.blue_pos = fb->framebuffer_blue_field_position;
            vbe_framebuffer.blue_mask_size = fb->framebuffer_blue_mask_size;
        }
        else        // ega text
        {
            vbe_framebuffer.red_pos = 0;
            vbe_framebuffer.red_mask_size = 0;
            vbe_framebuffer.green_pos = 0;
            vbe_framebuffer.green_mask_size = 0;
            vbe_framebuffer.blue_pos = 0;
            vbe_framebuffer.blue_mask_size = 0;
        }
    }

#else       /* !MULTIBOOT2_BOOTLOADER_MAGIC */

    multiboot_info_t *mbd = (multiboot_info_t *)addr;

    vbe_mode = mbd->vbe_mode;

    printk("  VBE info block at 0x%x\n", mbd->vbe_control_info);
    printk("  VBE mode info at 0x%x\n", mbd->vbe_mode_info);

    memcpy(&vbe_control_info, (void *)(uintptr_t)mbd->vbe_control_info,
           sizeof(struct vbe_control_info_t));

    memcpy(&vbe_mode_info, (void *)(uintptr_t)mbd->vbe_mode_info,
           sizeof(struct vbe_mode_info_t));

    if(BIT_SET(mbd->flags, 12))
    {
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        ext_multiboot_info_t *mbde = (ext_multiboot_info_t *)addr;

        vbe_framebuffer.phys_addr = (uint8_t *)
                                    (unsigned long)
                                    mbde->framebuffer_addr;

        vbe_framebuffer.pitch = mbde->framebuffer_pitch;
        vbe_framebuffer.width = mbde->framebuffer_width;
        vbe_framebuffer.height = mbde->framebuffer_height;
        vbe_framebuffer.bpp = mbde->framebuffer_bpp;
        vbe_framebuffer.type = mbde->framebuffer_type;

        // we lie to ourselves and pretend that 15 bits-per-pixel is the
        // same as 16 bits-per-pixel, to ease our calculations later
        if(vbe_framebuffer.bpp == 15)
        {
            vbe_framebuffer.bpp = 16;
        }

        if(mbde->framebuffer_type == 0)     // palette-indexed
        {
            vbe_framebuffer.palette_phys_addr = (uint8_t *)(uintptr_t)mbde->framebuffer_palette_addr;
            vbe_framebuffer.palette_num_colors = mbde->framebuffer_palette_num_colors;
        }
        else if(mbde->framebuffer_type == 1)    // rgb
        {
            vbe_framebuffer.red_pos = mbde->framebuffer_red_field_position;
            vbe_framebuffer.red_mask_size = mbde->framebuffer_red_mask_size;
            vbe_framebuffer.green_pos = mbde->framebuffer_green_field_position;
            vbe_framebuffer.green_mask_size = mbde->framebuffer_green_mask_size;
            vbe_framebuffer.blue_pos = mbde->framebuffer_blue_field_position;
            vbe_framebuffer.blue_mask_size = mbde->framebuffer_blue_mask_size;
        }
        else        // ega text
        {
            vbe_framebuffer.red_pos = 0;
            vbe_framebuffer.red_mask_size = 0;
            vbe_framebuffer.green_pos = 0;
            vbe_framebuffer.green_mask_size = 0;
            vbe_framebuffer.blue_pos = 0;
            vbe_framebuffer.blue_mask_size = 0;
        }
    }

#endif      /* MULTIBOOT2_BOOTLOADER_MAGIC */

    else
    {
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        vbe_framebuffer.phys_addr =
                        (uint8_t *)(uintptr_t)vbe_mode_info.framebuffer;
        vbe_framebuffer.pitch = vbe_mode_info.pitch;
        vbe_framebuffer.width = vbe_mode_info.width;
        vbe_framebuffer.height = vbe_mode_info.height;
        vbe_framebuffer.bpp = vbe_mode_info.bpp;
        vbe_framebuffer.type = 1;
        
        vbe_framebuffer.red_pos = vbe_mode_info.red_position;
        vbe_framebuffer.red_mask_size = vbe_mode_info.red_mask;
        vbe_framebuffer.green_pos = vbe_mode_info.green_position;
        vbe_framebuffer.green_mask_size = vbe_mode_info.green_mask;
        vbe_framebuffer.blue_pos = vbe_mode_info.blue_position;
        vbe_framebuffer.blue_mask_size = vbe_mode_info.blue_mask;
    }

    vbe_framebuffer.memsize = vbe_framebuffer.pitch *
                                    vbe_framebuffer.height;

    printk("Found VBE info:\n");
    printk("  VBE mode 0x%x\n", vbe_mode);
    printk("  %c%c%c%c ", vbe_control_info.signature[0],
                          vbe_control_info.signature[1],
                          vbe_control_info.signature[2],
                          vbe_control_info.signature[3]);
    printk("ver %d (total memory: %ukB)\n",
                          VBE_VERSION(vbe_control_info.version),
                          (vbe_control_info.video_memory * 64));

    printk("  Resolution %u x %u, bpp %u, phys base 0x%x\n",
           vbe_framebuffer.width, vbe_framebuffer.height,
           vbe_framebuffer.bpp, vbe_framebuffer.phys_addr);

    //printf("\nR [%u, %u]\n", vbe_driver.red_position, vbe_driver.red_mask);
    //printf("G [%u, %u]\n", vbe_driver.green_position, vbe_driver.green_mask);
    //printf("B [%u, %u]\n", vbe_driver.blue_position, vbe_driver.blue_mask);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    //empty_loop();
}


static inline void vbe_map_backbuf(uint8_t **addr)
{
    if(!(*addr = (uint8_t *)
            vmmngr_alloc_and_map(vbe_framebuffer.memsize, 0,
                                 PTE_FLAGS_PWU, NULL, REGION_VBE_BACKBUF)))
    {
        printk("  Failed to alloc VBE back buffer\n");
    }
    else
    {
        A_memset(*addr, 0, vbe_framebuffer.memsize);

        virtual_addr start = (virtual_addr)*addr;
        virtual_addr end = start + vbe_framebuffer.memsize;
        virtual_addr addr;
        volatile pt_entry *e;

        for(addr = start; addr < end; addr += PAGE_SIZE)
        {
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            e = get_page_entry((void *)addr);
            inc_frame_shares(PTE_FRAME(*e));
        }
    }
}


/*
 * Initialize the VBE driver.
 */
void vbe_init(void)
{
    if(using_ega())
    {
        printk("  Using EGA mode\n");
        return;
    }

    //printk("  VBE mode 0x%x, phys 0x%lx\n", vbe_mode, vbe_framebuffer.phys_addr);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    if(!(vbe_framebuffer.virt_addr = (uint8_t *)
            phys_to_virt_off((physical_addr)vbe_framebuffer.phys_addr,
                             (physical_addr)vbe_framebuffer.phys_addr +
                                            vbe_framebuffer.memsize,
                              PTE_FLAGS_PW, REGION_VBE_FRONTBUF)))
    {
        printk("  Failed to map virtual VBE memory\n");
        return;
    }

    /*
    // mark video memory area as used
    pmmngr_deinit_region((physical_addr)vbe_framebuffer.phys_addr,
                                        vbe_framebuffer.memsize);
    */

    vbe_map_backbuf(&fb_backbuf_text);
    vbe_map_backbuf(&fb_backbuf_gui);
    fb_cur_backbuf = vbe_framebuffer.virt_addr;

    // If VBE is palette-indexed, map the palette to a virtual address
    // we can use. We will convert the packed palette data (in the form of
    // red, green, blue, 3 bytes for each color) to a more easily-readable
    // format (red, green, blue, alpha, 4 bytes for each color).
    if(vbe_framebuffer.type == 0)
    {
        struct rgba_color_t *palette;
        uint8_t i, *temp_addr;
        size_t sz = sizeof(struct rgba_color_t) * 
                        vbe_framebuffer.palette_num_colors;

        temp_addr = (uint8_t *)
            phys_to_virt_off((physical_addr)vbe_framebuffer.palette_phys_addr,
                             (physical_addr)vbe_framebuffer.palette_phys_addr +
                                (vbe_framebuffer.palette_num_colors * 4),
                             PTE_FLAGS_PW, REGION_VBE_FRONTBUF);

        vbe_framebuffer.palette_virt_addr = kmalloc(sz);
        palette = vbe_framebuffer.palette_virt_addr;

        for(i = 0; i < vbe_framebuffer.palette_num_colors; i++, palette++)
        {
            palette->red = *temp_addr++;
            palette->green = *temp_addr++;
            palette->blue = *temp_addr++;
            palette->alpha = 0xff;
        }
    }

    vbe_framebuffer.pixel_width = vbe_framebuffer.bpp / 8;
    vbe_inited = 1;

    fb_init();
}


/*
 * Remap the VBE backbuffer.
 */
int map_vbe_backbuf(virtual_addr *resaddr)
{
    *resaddr = 0;

    if(using_ega())
    {
        return -ENOENT;
    }

    virtual_addr vbestart = (virtual_addr)fb_cur_backbuf;
    virtual_addr vbeend = vbestart + vbe_framebuffer.memsize;
    virtual_addr src, dest, mapaddr;
    virtual_addr mapsz = align_up(vbe_framebuffer.memsize);
    pdirectory *pml4_dest = (pdirectory *)this_core->cur_task->pd_virt;
    pdirectory *pml4_src = (pdirectory *)get_idle_task()->pd_virt;
    volatile pt_entry *esrc, *edest;
    int res;

    // ensure no one changes the task memory map while we're fiddling with it
    kernel_mutex_lock(&(this_core->cur_task->mem->mutex));
    
    // choose an address
    if((mapaddr = get_user_addr(mapsz, USER_SHM_START, USER_SHM_END)) == 0)
    {
        kernel_mutex_unlock(&(this_core->cur_task->mem->mutex));
        return -ENOMEM;
    }

    if((res = memregion_alloc_and_attach((struct task_t *)this_core->cur_task, 
                                         NULL, 0, 0,
                                         mapaddr, mapaddr + mapsz,
                                         PROT_READ | PROT_WRITE,
                                         MEMREGION_TYPE_DATA,
                                         MAP_SHARED | MEMREGION_FLAG_USER,
                                         0)) != 0)
    {
        kernel_mutex_unlock(&(this_core->cur_task->mem->mutex));
        return res;
    }

    kernel_mutex_unlock(&(this_core->cur_task->mem->mutex));
    
    for(dest = mapaddr, src = vbestart;
        src < vbeend;
        dest += PAGE_SIZE, src += PAGE_SIZE)
    {
        if(!(esrc = get_page_entry_pd(pml4_src, (void *)src)) ||
           !(edest = get_page_entry_pd(pml4_dest, (void *)dest)))
        {
            return -ENOMEM;
        }
        
        *edest = *esrc;
        inc_frame_shares(PTE_FRAME(*esrc));
        vmmngr_flush_tlb_entry(dest);
    }

    *resaddr = mapaddr;
    
    return 0;
}


volatile int repaint_screen = 1;

//#define WAIT_FOR_VERTICAL_RETRACE   1

void screen_refresh(void *arg)
{
    UNUSED(arg);

    if(using_ega())
    {
        return;
    }

    if(repaint_screen)
    {

#ifdef WAIT_FOR_VERTICAL_RETRACE

        while((inb(0x3DA) & 0x08))
        {
            ;
        }

        while(!(inb(0x3DA) & 0x08))
        {
            ;
        }

#endif

        repaint_screen = 0;
        sti();
        A_memcpy((void *)vbe_framebuffer.virt_addr,
                    fb_cur_backbuf, vbe_framebuffer.memsize);
    }
}

