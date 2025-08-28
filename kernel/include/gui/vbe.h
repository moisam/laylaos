/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: vbe.h
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
 *  \file vbe.h
 *
 *  Functions and macros for working with the framebuffer device.
 */

#ifndef __VBE_H__
#define __VBE_H__

#ifdef KERNEL
#ifdef USE_MULTIBOOT2
#include <kernel/multiboot2.h>
#else
#include <kernel/multiboot.h>
#endif
#endif      /* KERNEL */


#ifndef __RGBA_COLOR_STRUCT_DEFINED__
#define __RGBA_COLOR_STRUCT_DEFINED__   1

struct rgba_color_t
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} __attribute__((packed));

#endif      /* __RGBA_COLOR_STRUCT_DEFINED__ */


/**
 * @struct vbe_control_info_t
 * @brief The vbe_control_info_t structure.
 *
 * A structure containing VBE control info (what the bootloader got from 
 * the BIOS).
 */
struct vbe_control_info_t
{
    uint8_t signature[4];    /**< must be "VESA" to indicate VBE support */
    uint16_t version;        /**< VBE version; high byte is major version, low
                                  byte is minor version */
    uint16_t oem[2];         /**< segment:offset pointer to OEM */
    uint32_t capabilities;   /**< bitfield that describes card capabilities */
    uint16_t video_modes[2]; /**< segment:offset pointer to list of supported
                                  video modes */
    uint16_t video_memory;   /**< amount of video memory in 64KB blocks */
    uint16_t software_rev;   /**< software revision */
    uint32_t vendor;         /**< segment:offset to card vendor string */
    uint32_t product_name;   /**< segment:offset to card model name */
    uint32_t product_rev;    /**< segment:offset pointer to product revision */
    uint8_t reserved[222];   /**< reserved for future expansion */
    uint8_t oem_data[256];   /**< OEM BIOSes store their strs in this area */
} __attribute__((packed));


/**
 * @struct vbe_mode_info_t
 * @brief The vbe_mode_info_t structure.
 *
 * A structure containing VBE mode info (what the bootloader got from 
 * the BIOS).
 */
struct vbe_mode_info_t
{
    uint16_t attributes;    /**< bit 7 indicates the mode supports a linear
                                 frame buffer */
    uint8_t window_a;       /**< deprecated */
    uint8_t window_b;       /**< deprecated */
    uint16_t granularity;   /**< deprecated; used while calculating
                                 bank numbers */
    uint16_t window_size;
    uint16_t segment_a;
    uint16_t segment_b;
    uint32_t win_func_ptr;  /**< deprecated; used to switch banks from 
                                 protected mode without returning to 
                                 real mode */
    uint16_t pitch;         /**< number of bytes per horizontal line */
    uint16_t width;         /**< width in pixels */
    uint16_t height;        /**< height in pixels */
    uint8_t w_char;         /**< unused */
    uint8_t y_char;         /**< unused */
    uint8_t planes;
    uint8_t bpp;            /**< bits per pixel in this mode */
    uint8_t banks;          /**< deprecated; total number of banks in 
                                 this mode */
    uint8_t memory_model;
    uint8_t bank_size;      /**< deprecated; size of a bank, almost always 
                                 64 KB but may be 16 KB */
    uint8_t image_pages;
    uint8_t reserved0;
 
    uint8_t red_mask;       /**< red color mask */
    uint8_t red_position;   /**< red color position in multibyte colors */
    uint8_t green_mask;     /**< green color mask */
    uint8_t green_position; /**< green color position in multibyte colors */
    uint8_t blue_mask;      /**< blue color mask */
    uint8_t blue_position;  /**< blue color position in multibyte colors */
    uint8_t reserved_mask;      /**< reserved bits mask */
    uint8_t reserved_position;  /**< reserved bits position in multibyte
                                     colors */
    uint8_t direct_color_attributes;
 
    uint32_t framebuffer;   /**< physical address of the linear frame buffer;
                                 write here to draw to the screen */
    uint32_t off_screen_mem_off;
    uint16_t off_screen_mem_size;   /**< size of memory in the framebuffer but
                                         not being displayed on the screen */
    uint8_t reserved1[206];         /**< reserved */
} __attribute__((packed));


/**
 * @struct framebuffer_t
 * @brief The framebuffer_t structure.
 *
 * A structure containing all the needed framebuffer info. Most of the fields
 * are filled from values passed to us by GRUB. In addition, there are some
 * extra fields we set and use for housekeeping and ease of calculations.
 */
struct framebuffer_t
{
    uint8_t *phys_addr;     /**< front buffer physical memory address */
    uint8_t *virt_addr;     /**< front buffer virtual memory address */
    uint8_t *back_buffer;   /**< back buffer virtual memory address */
    uint32_t memsize;       /**< front/back buffer memory length */
    uint32_t pitch;         /**< number of bytes per horizontal line */
    uint32_t width;         /**< width in pixels */
    uint32_t height;        /**< height in pixelss */
    uint8_t bpp;            /**< bits per pixel in this modes */
    uint8_t type;           /**< framebuffer type as passed to us by GRUBs */
    uint8_t pixel_width;    /**< bytes per pixels */
    uint32_t line_height;   /**< bytes per line = pitch * char_heights */

    union
    {
        struct               // for type == 0
        {
            void *palette_phys_addr;    /**< palette physical memory address */
            void *palette_virt_addr;    /**< palette virtual memory address */
            uint16_t palette_num_colors;/**< number of colors in palette */
        } indexed;

        struct               // for type == 1
        {
            uint8_t red_pos;         /**< red color position */
            uint8_t red_mask_size;   /**< red color mask size */
            uint8_t green_pos;       /**< green color position */
            uint8_t green_mask_size; /**< green color mask size */
            uint8_t blue_pos;        /**< blue color position */
            uint8_t blue_mask_size;  /**< blue color mask size */
        } rgb;
    } color_info;
};


#ifdef KERNEL

#define palette_phys_addr   color_info.indexed.palette_phys_addr
#define palette_virt_addr   color_info.indexed.palette_virt_addr
#define palette_num_colors  color_info.indexed.palette_num_colors
#define red_pos             color_info.rgb.red_pos
#define red_mask_size       color_info.rgb.red_mask_size
#define green_pos           color_info.rgb.green_pos
#define green_mask_size     color_info.rgb.green_mask_size
#define blue_pos            color_info.rgb.blue_pos
#define blue_mask_size      color_info.rgb.blue_mask_size

/**
 * \def VBE_VERSION
 *
 * macro to extract numeric VBE version
 */
#define VBE_VERSION(v)              \
    (((v) == 0x0300) ? 3 : ((v) == 0x0200) ? 2 : ((v) == 0x0100) ? 1 : (v))


/**
 * @brief Get bootloader VBE info.
 *
 * This function is called early during boot with the multiboot info structure
 * that is passed to us by the bootloader (we assume GRUB, or whatever our
 * bootloader is, passes us a \a multiboot_info_t struct). The function saves
 * the VBE control info and initialises the global \ref vbe_framebuffer
 * struct, which is used by the framebuffer device.
 *
 * @param   mbd     the multiboot info struct that is passed to us by the
 *                    bootloader (currently GRUB)
 *
 * @return  nothing.
 *
 * @see fb_init, fb_init_screen
 */
void get_vbe_info(unsigned long mbd);
//void get_vbe_info(multiboot_info_t *mbd);

/**
 * @brief Initialize the VBE driver.
 *
 * Allocate memory for the VBE front buffer and initialize the framebuffer
 * device (by calling fb_init()).
 *
 * @return  nothing.
 *
 * @see fb_init
 */
void vbe_init(void);

/**
 * @brief Remap the VBE backbuffer.
 *
 * Called during exec() to remap the VBE backbuffer as the original task's
 * page directory and page tables are destroyed before loading the new
 * executable into the new, empty memory space.
 *
 * @param   resaddr     mapped address will be returned here
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see syscall_execve, syscall_execveat
 */
int map_vbe_backbuf(virtual_addr *resaddr);

/**
 * @brief Refresh the screen.
 *
 * After the framebuffer device is initialized, this function can be called
 * to refresh the screen. It copies whatever is there in the back buffer to
 * the front buffer.
 *
 * @param   arg     unused (all kernel callback functions require a single
 *                          argument of type void pointer)
 *
 * @return  nothing.
 */
void screen_refresh(void *arg);

/**
 * @brief Check if EGA is used.
 *
 * This function returns non-zero if EGA (i.e. text mode) is in use, zero if
 * VBE if in use.
 *
 * @return  zero if in VBE mode, non-zero if in EGA mode.
 */
int using_ega(void);

/**
 * @var vbe_framebuffer
 * @brief framebuffer device info.
 *
 * This variable holds information about the framebuffer device, such as
 * the VBE BIOS info, screen size, color depth, etc. This is the information
 * returned when performing an ioctl() operation on the framebuffer device.
 */
extern struct framebuffer_t vbe_framebuffer;

/**
 * @var repaint_screen
 * @brief automatically repaint the screen.
 *
 * If the value is non-zero, the screen function automatically repaints the
 * screen at set intervals. If zero, the screen is not updated unless
 * specifically requested (by performing an ioctl() on the framebuffer
 * device).
 */
extern volatile int repaint_screen;

#endif      /* KERNEL */

#endif      /* __VBE_H__ */
