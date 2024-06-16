/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: mmngr_phys.c
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
 *  \file mmngr_phys.c
 *
 *  The Physical Memory Manager (PMM) implementation.
 */

//#define __DEBUG

#include <kernel/laylaos.h>
#include <kernel/mutex.h>
#include <kernel/modules.h>
#include <kernel/vga.h>
#include <mm/mmngr_phys.h>
#include <mm/mmngr_virtual.h>
#include <mm/mmap.h>
#include <gui/vbe.h>
#include <string.h>

struct kernel_mutex_t physmem_lock;

virtual_addr placement_address = (virtual_addr)&kernel_end;

//physical_addr zeropage_phys = 0;

// types of memory address ranges as returned by BIOS
static char *mem_type[] =
{
    "Undefined", 
    "Available", 
    "Reserved", 
    "ACPI reclaim", 
    "ACPI NVS", 
    "Bad mem"
};

// in case a frame is shared, this table shows the number of tasks sharing
// a single frame
volatile unsigned char *frame_shares;


/*
 * Most of the code below was adopted from BrokenThorn OS dev tutorial:
 *    http://www.brokenthorn.com/Resources/OSDev18.html
 *
 * (with many modifications, of course :)).
 */

// size of physical memory
static volatile size_t _mmngr_memory_size = 0;
static uintptr_t highest_usable_addr = 0;

// number of blocks currently in use
static volatile size_t _mmngr_used_blocks = 0;

// maximum number of available memory blocks
static volatile size_t _mmngr_max_blocks = 0;

// number of available memory blocks
static volatile size_t _mmngr_available_blocks = 0;

// memory map bit array. Each bit represents a memory block
static volatile uint32_t *_mmngr_memory_map = 0;

// How many items are in the memory map bit array
static volatile size_t _mmngr_memory_map_size = 0;


// set any bit (frame) within the memory map bit array
static void mmap_set(uintptr_t bit)
{
    volatile uintptr_t i = bit / 32;
    volatile uint32_t j = ((uint32_t)1 << (bit % 32));
    _mmngr_memory_map[i] |= j;
}

// unset any bit (frame) within the memory map bit array
static void mmap_unset(uintptr_t bit)
{
    volatile uintptr_t i = bit / 32;
    volatile uint32_t j = ((uint32_t)1 << (bit % 32));
    _mmngr_memory_map[i] &= ~j;
}

// test if any bit (frame) is set within the memory map bit array
static int mmap_test(uintptr_t bit)
{
    volatile uintptr_t i = bit / 32;
    volatile uint32_t j = ((uint32_t)1 << (bit % 32));
	return (_mmngr_memory_map[i] & j) ? 1 : 0;
}

// finds first free frame in the bit array and returns its index
static inline uintptr_t mmap_first_free(void)
{
    static volatile size_t last_index = 0;
    size_t i, count = _mmngr_memory_map_size;

    if(last_index >= count)
    {
        last_index = 0;
    }

loop:

	// find the first free bit
	for(i = last_index; i < count; i++)
	{
		if(_mmngr_memory_map[i] != 0xffffffff)
		{
			for(/* volatile */ uint32_t j = 0; j < 32; j++)
			{
			    // test each bit in the dword
				uint32_t bit = (uint32_t)1 << j;
				if(!(_mmngr_memory_map[i] & bit))
				{
					last_index = i;
					return i * 4 * 8 + j;
				}
			}
		}
	}

    if(count == _mmngr_memory_map_size && last_index != 0)
    {
        count = last_index;
        last_index = 0;
        goto loop;
    }

	return -1;
}

// finds first free "size" number of frames and returns its index
static uintptr_t mmap_first_free_s(size_t size)
{
	if(size == 0)
	{
		return -1;
	}

	if(size == 1)
	{
		return mmap_first_free();
	}

    size_t count = _mmngr_memory_map_size;

	for(volatile size_t i = 0; i < count; i++)
	{
		if(_mmngr_memory_map[i] != 0xffffffff)
		{
			for(volatile uint32_t j = 0; j < 32; j++)
			{
			    // test each bit in the dword
				uint32_t bit = (uint32_t)1 << j;

				if(!(_mmngr_memory_map[i] & bit))
				{
					uintptr_t startingBit = i * 32;
					
					// get the free bit in the dword at index i
					startingBit += j;

					// loop through each bit to see if its enough space
					volatile size_t free = 0;
					
					for(volatile size_t count = 0; count <= size; count++)
					{
						if(mmap_test(startingBit + count))
						{
							break;
						}
						
						free++;	// this bit is clear (free frame)

						if(free == size)
						{
							// free count==size needed; return index
							return startingBit;
						}
					}
				}
			}
		}
	}

	return -1;
}


/*
 * Initialize the physical memory manager.
 */
void pmmngr_init(multiboot_info_t *mbd, physical_addr bitmap)
{
    uintptr_t highest_addr = 0;

    init_kernel_mutex(&physmem_lock);

    if(!BIT_SET(mbd->flags, 6))
    {
        kpanic("pmm: missing bootloader memory map\n");
        empty_loop();
    }

    multiboot_memory_map_t *mmap = 
                (multiboot_memory_map_t *)(uintptr_t)mbd->mmap_addr;

    while((uintptr_t)mmap < mbd->mmap_addr + mbd->mmap_length)
    {
	    if(/* mmap->type == 1 && */ mmap->len && 
	       ((uintptr_t)mmap->addr + mmap->len) > highest_addr)
	    {
            highest_addr = (uintptr_t)mmap->addr + mmap->len;
        }

	    if(mmap->type == 1 && mmap->len && 
	       ((uintptr_t)mmap->addr + mmap->len) > highest_usable_addr)
	    {
            highest_usable_addr = (uintptr_t)mmap->addr + mmap->len;
        }

        mmap = (multiboot_memory_map_t *)
                    ((uintptr_t)mmap + mmap->size + sizeof(mmap->size));
    }
    
    bitmap = align_up((virtual_addr)bitmap);

    _mmngr_memory_size = highest_addr / 1024;
	_mmngr_memory_map	=	(uint32_t *)bitmap;
	_mmngr_max_blocks	=	(_mmngr_memory_size * 1024) / PMMNGR_BLOCK_SIZE;
	_mmngr_used_blocks	=	_mmngr_max_blocks;
	
	_mmngr_memory_map_size = (_mmngr_max_blocks + 31) / 32;
	
	// account for the memory bitmap which might take 32 pages 
	// (for 4GB address space)
	placement_address = align_up((virtual_addr)bitmap + 
	                             (_mmngr_memory_map_size * 4));
	kernel_size += (placement_address - (virtual_addr)&kernel_end);

	// By default, all of memory is in use
	memset((void *)_mmngr_memory_map, 0xff, _mmngr_memory_map_size * 4);

    // get complete memory map
    printk("\nReading memory map:\n");
    mmap = (multiboot_memory_map_t *)(uintptr_t)mbd->mmap_addr;

    while((uintptr_t)mmap < mbd->mmap_addr + mbd->mmap_length)
    {
	    char *type = mem_type[0];

	    switch(mmap->type)
        {
	        case 0:
	        case 1:
	        case 2:
	        case 3:
	        case 4:
            case 5:
	            type = mem_type[mmap->type];
                break;

            default:
                type = mem_type[0];
                break;
	    }
	    
	    physical_addr start = mmap->addr;
	    size_t len = mmap->len;

        printk("    addr: " _XPTR_ ", len: " _XPTR_ ", type: %u [%s]\n", 
                   start, len,
    	           (unsigned)mmap->type, type);

	    if(mmap->type == 1)		// Available memory, mark it as such
	    {
            pmmngr_init_region(start, len);
            _mmngr_available_blocks += (align_up(len) / PMMNGR_BLOCK_SIZE);
        }

        mmap = (multiboot_memory_map_t *)((uintptr_t)mmap + 
                                           mmap->size + sizeof(mmap->size));
    }
    
    /*
     * De-init kernel memory (mark it as used).
     * Also, de-init the first 1Mib, as this contains important things like
     * the main BIOS area.
     */
    pmmngr_deinit_region(0, 0x100000 + kernel_size);

    printk("pmm: kernel memory (0x100000 - 0x%x), size 0x%x bytes..\n", 
            0x100000 + kernel_size, kernel_size);
    
    // mark VGA video memory area as used
    //pmmngr_deinit_region(VGA_MEMORY_PHYSICAL, VGA_MEMORY_SIZE);
    pmmngr_deinit_region(VGA_MEMORY_PHYSICAL, 
                            STANDARD_VGA_WIDTH * STANDARD_VGA_HEIGHT * 2);

    if(!using_ega())
    {
        // if we have VBE info, mark VBE video memory area as used
        pmmngr_deinit_region((physical_addr)vbe_framebuffer.phys_addr,
                                            vbe_framebuffer.memsize);
    }
    
    // de-init modules memory (mark it as used), so we won't override our
    // loaded modules when we allocate memory for the initial page directory
    // and page tables later when we init the virtual memory manager!
    printk("\nChecking loaded modules..\n");

    if(BIT_SET(mbd->flags, 3))
    {
        multiboot_module_t *mod;
        unsigned int i;
        boot_module_count = 0;

        memset(boot_module, 0, 
                sizeof(struct boot_module_t) * MAX_BOOT_MODULES);
        
        printk("    mods_count = %d, mods_addr = 0x%x\n",
                    (int) mbd->mods_count, (int) mbd->mods_addr);

        for(i = 0, mod = (multiboot_module_t *)(uintptr_t)mbd->mods_addr;
            i < mbd->mods_count;
            i++, mod++)
        {
            printk("      mod_start = " _XPTR_ ", mod_end = " _XPTR_ 
                   ", cmdline = '%s'\n",
                        (uintptr_t)mod->mod_start,
                        (uintptr_t)mod->mod_end,
                        (char *)(uintptr_t)mod->cmdline);
            
            uintptr_t aligned_start = mod->mod_start;

            if((aligned_start & 0x00000FFF))
            {
                // Align the start address;
                aligned_start &= ~0x0FFF;
            }

            pmmngr_deinit_region(aligned_start, 
                                 (mod->mod_end - aligned_start));
            
            // store the info in our modules array
            // we can only store upto MAX_BOOT_MODULES modules
            if(i >= MAX_BOOT_MODULES)
            {
                continue;
            }
            
            boot_module_count++;
            boot_module[i].pstart = mod->mod_start;
            boot_module[i].pend = mod->mod_end;
            
            // make sure we don't overflow our limited space!
            if(strlen((char *)(uintptr_t)mod->cmdline) >= MAX_MODULE_CMDLINE)
            {
                memcpy(boot_module[i].cmdline, 
                        (char *)(uintptr_t)mod->cmdline,
                        MAX_MODULE_CMDLINE - 1);
                boot_module[i].cmdline[MAX_MODULE_CMDLINE - 1] = '\0';
            }
            else
            {
                strcpy(boot_module[i].cmdline, 
                       (char *)(uintptr_t)mod->cmdline);
            }
        }
    }
    else
    {
        printk("    Nothing found!\n");
    }
}


void pmmngr_init_region(physical_addr base, size_t size)
{
	volatile uintptr_t align = base / PMMNGR_BLOCK_SIZE;
	volatile size_t blocks = size / PMMNGR_BLOCK_SIZE;
	
	if(size % PMMNGR_BLOCK_SIZE)
	{
	    blocks++;
	}

	for( ; blocks > 0; blocks--)
	{
		mmap_unset(align++);
		_mmngr_used_blocks--;
	}

	// First block is always set. This insures allocs can't be 0
	mmap_set(0);
}


void pmmngr_deinit_region(physical_addr base, size_t size)
{
	volatile uintptr_t align = base / PMMNGR_BLOCK_SIZE;
	volatile size_t blocks = size / PMMNGR_BLOCK_SIZE;
	volatile int is_set;
	
	if(size % PMMNGR_BLOCK_SIZE)
	{
	    blocks++;
	}

	for( ; blocks > 0; blocks--)
	{
	    is_set = mmap_test(align);
		mmap_set(align++);

		if(!is_set)
		{
		    _mmngr_used_blocks++;
        }
	}
}

void *pmmngr_alloc_block(void)
{
	if(pmmngr_get_free_block_count() <= 0)
	{
		return 0;	//out of memory
	}

    int old_prio = 0, old_policy = 0;

    elevate_priority(cur_task, &old_prio, &old_policy);
    kernel_mutex_lock(&physmem_lock);
    
	volatile uintptr_t frame = mmap_first_free();

	if(frame == (uintptr_t)-1)
	{
        kpanic("pmm: out of memory!\n");
        kernel_mutex_unlock(&physmem_lock);
        restore_priority(cur_task, old_prio, old_policy);
		return 0;	//out of memory
	}

	mmap_set(frame);
	_mmngr_used_blocks++;

    kernel_mutex_unlock(&physmem_lock);
    restore_priority(cur_task, old_prio, old_policy);
    
	return (void *)(frame * PMMNGR_BLOCK_SIZE);
}


void pmmngr_free_block(void *p)
{
	volatile uintptr_t frame = (uintptr_t)p / PMMNGR_BLOCK_SIZE;
    int old_prio = 0, old_policy = 0;

    elevate_priority(cur_task, &old_prio, &old_policy);
    kernel_mutex_lock(&physmem_lock);
    
    if(frame_shares[frame] == 0)
    {
    	mmap_unset(frame);
    	_mmngr_used_blocks--;
    }
    else
    {
        /* frame is shared. don't release it yet */
        frame_shares[frame]--;
    }

    kernel_mutex_unlock(&physmem_lock);
    restore_priority(cur_task, old_prio, old_policy);
}


void *pmmngr_alloc_blocks(size_t size)
{
	if(pmmngr_get_free_block_count() <= size)
	{
		return 0;	//not enough space
	}

    int old_prio = 0, old_policy = 0;

    elevate_priority(cur_task, &old_prio, &old_policy);
    kernel_mutex_lock(&physmem_lock);

	uintptr_t frame = mmap_first_free_s(size);

	if(frame == (uintptr_t)-1)
	{
        kernel_mutex_unlock(&physmem_lock);
        restore_priority(cur_task, old_prio, old_policy);
		return 0;	//not enough space
	}

	for(size_t i = 0; i < size; i++)
	{
		mmap_set(frame + i);
	}

	_mmngr_used_blocks += size;

    kernel_mutex_unlock(&physmem_lock);
    restore_priority(cur_task, old_prio, old_policy);

	return (void*)(frame * PMMNGR_BLOCK_SIZE);
}


void *pmmngr_alloc_dma_blocks(size_t size)
{
	if(pmmngr_get_free_block_count() <= size)
	{
		return 0;	//not enough space
	}

    int old_prio = 0, old_policy = 0;

    elevate_priority(cur_task, &old_prio, &old_policy);
    kernel_mutex_lock(&physmem_lock);

	uintptr_t frame = -1;
    size_t count = _mmngr_memory_map_size;

    if(size == 1)
    {
    	// find the first free bit
    	for(volatile size_t i = 0; i < count; i++)
    	{
    	    /*
    	     * DMA requires memory buffers to be 64kb-aligned. This means
    	     * we should only accept frames at offsets 0, 16, 32, 48, ...
    	     * Hence we only test bits 0 and 4 (offsets 0 and 16) of every
    	     * 32-bit dword.
    	     */
			if(!(_mmngr_memory_map[i] & (1 << 0)))
			{
				frame = (i * 4 * 8) + 0;
				goto done;
			}

			if(!(_mmngr_memory_map[i] & (1 << 16)))
			{
				frame = (i * 4 * 8) + 16;
				goto done;
    		}
    	}

	    goto done;
	}

	for(volatile size_t i = 0; i < count; i++)
	{
        volatile uint32_t j;
        uintptr_t startingBit;
        
   	    /*
   	     * DMA requires memory buffers to be 64kb-aligned. This means
   	     * we should only accept frames at offsets 0, 16, 32, 48, ...
   	     * Hence we only test bits 0 and 5 (offsets 0 and 16) of every
   	     * 32-bit dword.
   	     */
		if(!(_mmngr_memory_map[i] & (1 << 0)))
		{
		    j = 0;
		}
		else if(!(_mmngr_memory_map[i] & (1 << 16)))
		{
		    j = 16;
   		}
   		else
   		{
   		    continue;
   		}

try:
		startingBit = i * 32;
		// get the free bit in the dword at index i
		startingBit += j;

		// loop through each bit to see if its enough space
		volatile size_t free = 0;
					
		for(volatile size_t count = 0; count <= size; count++)
		{
			if(mmap_test(startingBit + count))
			{
			    if(j == 0 && !(_mmngr_memory_map[i] & (1 << 16)))
			    {
			        j = 16;
			        goto try;
			    }
			    
			    break;
			}

			free++;	// this bit is clear (free frame)

			if(free == size)
			{
				frame = startingBit;
				break;
			}
		}
		
		// have we found anything?
		if(frame != (uintptr_t)-1)
		{
		    break;
		}
	}

done:
	if(frame == (uintptr_t)-1)
	{
        kernel_mutex_unlock(&physmem_lock);
        restore_priority(cur_task, old_prio, old_policy);
		return 0;	//not enough space
	}

	for(size_t i = 0; i < size; i++)
	{
		mmap_set(frame + i);
	}

	_mmngr_used_blocks += size;

    kernel_mutex_unlock(&physmem_lock);
    restore_priority(cur_task, old_prio, old_policy);

	return (void *)(frame * PMMNGR_BLOCK_SIZE);
}


void pmmngr_free_blocks(void *p, size_t size)
{
	uintptr_t frame = (uintptr_t)p / PMMNGR_BLOCK_SIZE;

    int old_prio = 0, old_policy = 0;

    elevate_priority(cur_task, &old_prio, &old_policy);
    kernel_mutex_lock(&physmem_lock);

	for(size_t i = 0; i < size; i++)
	{
        if(frame_shares[frame + i] == 0)
        {
    	    mmap_unset(frame + i);
    	    _mmngr_used_blocks--;
        }
        else
        {
            /* frame is shared. don't release it yet */
            frame_shares[frame + i]--;
        }
    }

    kernel_mutex_unlock(&physmem_lock);
    restore_priority(cur_task, old_prio, old_policy);
}


size_t pmmngr_get_memory_size(void)
{
    return (size_t)(highest_usable_addr / PAGE_SIZE);
}


size_t pmmngr_get_block_count(void)
{
	return _mmngr_max_blocks;
}

size_t pmmngr_get_available_block_count(void)
{
	return _mmngr_available_blocks;
}

size_t pmmngr_get_free_block_count(void)
{
	return _mmngr_max_blocks - _mmngr_used_blocks;
}


void pmmngr_load_PDBR(physical_addr addr)
{

#ifdef __x86_64__

    __asm__("mov	%0, %%rax\n\t"
		    "mov	%%rax, %%cr3"
            ::"m"(addr));

#else

    __asm__("mov	%0, %%eax\n\t"
		    "mov	%%eax, %%cr3		# PDBR is cr3 register in i86"
            ::"m"(addr));

#endif

}

