/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: mmngr_phys.h
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
 *  \file mmngr_phys.h
 *
 *  Functions and macros for working with the Physical Memory Manager (PMM).
 */

#ifndef __MMNGR_PHYS_H__
#define __MMNGR_PHYS_H__

/*
 * Code adopted from BrokenThorn OS dev tutorial:
 *    http://www.brokenthorn.com/Resources/OSDev18.html
 */

#include <stdint.h>
#include <stddef.h>
//#include <sys/pagesize.h>
#include <kernel/pagesize.h>
#include <kernel/multiboot.h>

// block size (4k by default)
#define PMMNGR_BLOCK_SIZE	    PAGE_SIZE


// physical address
#ifdef __x86_64__
typedef	uint64_t physical_addr;     /**< 64-bit physical address */
#else
typedef	uint32_t physical_addr;     /**< 32-bit physical address */
#endif

/**
 * @var frame_shares
 * @brief frame shares.
 *
 * The frame shares array.
 */
extern volatile unsigned char *frame_shares;


// increment a frame's share count by 1
static inline void inc_frame_shares(physical_addr frame_addr)
{
   frame_shares[frame_addr / PAGE_SIZE] += 1;
}


// decrement a frame's share count by 1
static inline void dec_frame_shares(physical_addr frame_addr)
{
   frame_shares[frame_addr / PAGE_SIZE] -= 1;
}


// get a frame's share count
static inline unsigned char get_frame_shares(physical_addr frame_addr)
{
    return frame_shares[frame_addr / PAGE_SIZE];
}


/**********************************
 * Function prototypes
 **********************************/

/**
 * @brief Initialize the physical memory manager.
 *
 * This function is called early during boot with the multiboot info structure
 * that is passed to us by the bootloader (we assume GRUB, or whatever our
 * bootloader is, passes us a \a multiboot_info_t struct). The function 
 * initializes internal structs, marks used memory as such, and sets the
 * memory and frame share bitmaps as appropriate.
 *
 * @param   mbd     the multiboot info struct that is passed to us by the
 *                    bootloader (currently GRUB)
 * @param   bitmap  physical address of the frame bitmap
 *
 * @return  nothing.
 */
void pmmngr_init(multiboot_info_t *mbd, physical_addr bitmap);

/**
 * @brief Initialize physical memory region.
 *
 * Enable physical memory regions for use.
 *
 * @param   base    physical memory region base address
 * @param   size    physical memory region size
 *
 * @return  nothing.
 */
void pmmngr_init_region(physical_addr base, size_t size);

/**
 * @brief Deinitialize physical memory region.
 *
 * Disable physical memory regions (mark them as used/unusable).
 *
 * @param   base    physical memory region base address
 * @param   size    physical memory region size
 *
 * @return  nothing.
 */
void pmmngr_deinit_region(physical_addr base, size_t size);

/**
 * @brief Allocate physical memory page.
 *
 * Returns the physical address of the newly allocated page.
 *
 * @return  physical page address.
 */
void *pmmngr_alloc_block(void);

/**
 * @brief Free physical memory page.
 *
 * Decrements the physical page reference count. If the count reaches zero,
 * i.e. the last reference is released, the physical page is marked as free
 * and it can be reused.
 *
 * @param   p       physical page address
 *
 * @return  nothing.
 */
void pmmngr_free_block(void *p);

/**
 * @brief Allocate physical memory pages.
 *
 * Allocate \a size number of pages and return the physical address of the
 * first page in the newly allocated page region.
 *
 * @param   size    number of pages to allocate
 *
 * @return  physical page address.
 */
void *pmmngr_alloc_blocks(size_t size);

/**
 * @brief Allocate physical DMA memory pages.
 *
 * Allocate \a size number of pages and return the physical address of the
 * first page in the newly allocated page region. The pages are 64kb-aligned
 * to enable them to be used for DMA transfers.
 *
 * @param   size    number of pages to allocate
 *
 * @return  physical page address.
 */
void *pmmngr_alloc_dma_blocks(size_t size);

/**
 * @brief Free physical memory pages.
 *
 * For each page, decrement the physical page reference count. If the count 
 * reaches zero, i.e. the last reference is released, the physical page is
 * marked as free and it can be reused.
 *
 * @param   p       physical page address
 * @param   size    number of pages to free
 *
 * @return  nothing.
 */
void pmmngr_free_blocks(void *p, size_t size);

/**
 * @brief Get physical memory size.
 *
 * Return memory size in physical page granularity. This might be bigger than
 * the number returned by pmmngr_get_block_count(), e.g. if there are holes
 * in memory.
 *
 * @return  physical memory size.
 */
size_t pmmngr_get_memory_size(void);

/**
 * @brief Get physical page count.
 *
 * Return the number of physical pages in memory.
 *
 * @return  physical page count.
 */
size_t pmmngr_get_block_count(void);

/**
 * @brief Get available page count.
 *
 * Return the number of available physical pages.
 *
 * @return  available page count.
 */
size_t pmmngr_get_available_block_count(void);

/**
 * @brief Get free page count.
 *
 * Return the number of free physical pages.
 *
 * @return  free page count.
 */
size_t pmmngr_get_free_block_count(void);

/**
 * @brief Load the PDBR.
 *
 * Load the page directory base register (PDBR).
 *
 * @param   addr    physical address to load
 *
 * @return  nothing.
 */
void pmmngr_load_PDBR(physical_addr addr);

/**
 * @brief Increment page shares.
 *
 * Increment the share count for the given physical page.
 *
 * @param   addr    physical address
 *
 * @return  nothing.
 */
void inc_frame_shares(physical_addr addr);

/**
 * @brief Decrement page shares.
 *
 * Decrement the share count for the given physical page.
 *
 * @param   addr    physical address
 *
 * @return  nothing.
 */
void dec_frame_shares(physical_addr addr);

/**
 * @brief Get page shares.
 *
 * Get the share count for the given physical page.
 *
 * @param   addr    physical address
 *
 * @return  page share count (0 to 255).
 */
unsigned char get_frame_shares(physical_addr addr);

#endif      /* __MMNGR_PHYS_H__ */
