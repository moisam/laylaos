/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: memregion.h
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
 *  \file memregion.h
 *
 *  Functions and macros for working with task memory regions.
 */

#ifndef __MEMREGION_H__
#define __MEMREGION_H__

#include <mm/mmngr_virtual.h>
#include <mm/vmmngr_pte.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/mutex.h>


/**********************************
 * Macro definitions
 **********************************/

/* memory region flag options */
#define MEMREGION_FLAG_SHARED       MAP_SHARED      // 0x01
#define MEMREGION_FLAG_PRIVATE      MAP_PRIVATE     // 0x02
#define MEMREGION_FLAG_NORESERVE    MAP_NORESERVE   // currently no-op
#define MEMREGION_FLAG_USER         0x04
#define MEMREGION_FLAG_STICKY_BIT   0x08        // for shared memory regions,
                                                // always keep them in memory
#define MEMREGION_FLAG_VDSO         0x10

#define ACCEPTED_MEMREGION_FLAGS    (MEMREGION_FLAG_PRIVATE |   \
                                     MEMREGION_FLAG_SHARED |    \
                                     MEMREGION_FLAG_USER |      \
                                     MEMREGION_FLAG_STICKY_BIT |\
                                     MEMREGION_FLAG_NORESERVE | \
                                     MEMREGION_FLAG_VDSO)

/* memory region types */
#define MEMREGION_TYPE_TEXT         1
#define MEMREGION_TYPE_DATA         2
#define MEMREGION_TYPE_SHMEM        3
#define MEMREGION_TYPE_STACK        4
#define MEMREGION_TYPE_KERNEL       5

#define MEMREGION_TYPE_LOWEST       MEMREGION_TYPE_TEXT
#define MEMREGION_TYPE_HIGHEST      MEMREGION_TYPE_KERNEL


/**********************************
 * Structure definitions
 **********************************/

/**
 * @struct memregion_t
 * @brief The memregion_t structure.
 *
 * A structure to represent a memory region mapped in a task's virtual
 * address space.
 */
struct memregion_t
{
    struct fs_node_t *inode;    /**< backing file (if not mapped anonymous) */
    off_t  fpos,                /**< start of mapping in file */
           flen;                /**< size of mapping in file */
    int    prot,                /**< mapping protection bits */
           type,                /**< mapping type */
           flags;               /**< mapping flags */
    size_t size;                /**< mapping size in pages, not bytes */
    int    refs;                /**< mapping reference count */
    virtual_addr addr;          /**< mapping virtual address */
    struct kernel_mutex_t mutex;    /**< struct lock */
    struct memregion_t *next_free;  /**< next region in the free list */
    struct memregion_t *next;       /**< next region in task mappings */
    struct memregion_t *prev;       /**< previous region in task mappings */
};


/**
 * @struct task_vm_t
 * @brief The task_vm_t structure.
 *
 * A structure to represent a task's virtual address space.
 */
struct task_vm_t
{
    struct memregion_t *first_region;   /**< pointer to first memory region */
    struct memregion_t *last_region;    /**< pointer to last memory region */
    struct kernel_mutex_t mutex;        /**< struct lock */

    uintptr_t vdso_code_start;          /**< start of vdso code */

    size_t    __image_size;             /**< task size in pages, not bytes */
    uintptr_t __end_data,               /**< end of data segment */
              __end_stack,              /**< end of stack segment */
              __base_addr;              /**< base address */

#define image_base      mem->__base_addr
#define image_size      mem->__image_size
#define end_data        mem->__end_data
#define end_stack       mem->__end_stack
};


#include "../mm/mmngr_inlines.h"


/**********************************
 * Function prototypes
 **********************************/

/**
 * @brief Check for memregion overlaps.
 *
 * Check if the given address range from 'start' to 'end-1' overlaps with
 * other memory regions.
 *
 * This is a helper function. The task's memory struct should be locked by
 * the caller.
 *
 * @param   task        pointer to task
 * @param   start       start address
 * @param   end         end address
 *
 * @return  zero if no overlaps, or -EEXIST if there are one or more overlaps.
 */
int memregion_check_overlaps(struct task_t *task,
                             virtual_addr start, virtual_addr end);

/**
 * @brief Allocate and attach a memregion.
 *
 * Allocate a new memregion struct with the given address range, protection,
 * type, flags and inode. The new struct is inserted into the task's memregion
 * list. If \a remove_overlaps is non-zero, overlapping memory maps are 
 * removed automatically.
 *
 * NOTE: The task's mem struct need not be locked by the caller, as we only get
 *       called by syscall_execve() and ELF loader, as well as when initialising
 *       tasking on system startup.
 *
 * @param   task                pointer to task
 * @param   inode               file node (NULL if anonymous mapping)
 * @param   fpos                start of mapping in file (if \a inode != NULL)
 * @param   flen                length of mapping in file (if \a inode != NULL)
 * @param   start               start address
 * @param   end                 end address
 * @param   prot                mapping protection bits
 * @param   type                mapping type
 * @param   flags               mapping flags
 * @param   remove_overlaps     if non-zero, remove overlaps automatically
 *
 * @return  zero on success, -(errno) on failure.
 */
int memregion_alloc_and_attach(struct task_t *task, struct fs_node_t *inode,
                               off_t fpos, off_t flen,
                               virtual_addr start, virtual_addr end,
                               int prot, int type, int flags,
                               int remove_overlaps);

/**
 * @brief Change the protection bits of a memory address range.
 *
 * The target address range could be part of a wider memory region, in which
 * case we split the region into smaller regions and change the protection
 * bits of the desired address range only. If \a detach is set, the
 * address range is actually detached from the task's memory map instead
 * of changing its protection bits.
 *
 * NOTE: The task's mem struct should be locked by the caller.
 *       This function is called by syscall_unmap() and syscall_mprotect().
 *
 * @param   task                pointer to task
 * @param   start               start address
 * @param   end                 end address
 * @param   prot                mapping protection bits
 * @param   detach              if non-zero, mapping is detached from task
 *
 * @return  zero on success, -(errno) on failure.
 */
int memregion_change_prot(struct task_t *task,
                              virtual_addr start, virtual_addr end,
                              int prot, int detach);

/**
 * @brief Check for, and remove, overlapping memory mapped regions.
 *
 * The target address range could be part of a wider memory region, in which
 * case we split the region into smaller regions and remove the desired 
 * address range only. The address range is detached from the task's memory
 * map.
 *
 * NOTE: The task's mem struct should be locked by the caller.
 *       This function is called by syscall_unmap().
 *
 * @param   task                pointer to task
 * @param   start               start address
 * @param   end                 end address
 *
 * @return  zero on success, -(errno) on failure.
 */
int memregion_remove_overlaps(struct task_t *task,
                              virtual_addr start, virtual_addr end);

/**
 * @brief Allocate a new memory region struct.
 *
 * We try to get a memregion from the free region list. If the list is empty,
 * we try to allocate a new struct (this is all done by calling
 * memregion_first_free()).
 *
 * NOTES:
 *   - prot and type are as defined in mm/mmap.h.
 *   - falgs are as defined in mm/memregion.h.
 *   - The caller must have locked task->mem->mutex before calling us.
 *
 * @param   inode           file node (NULL if anonymous mapping)
 * @param   prot            mapping protection bits
 * @param   type            mapping type
 * @param   flags           mapping flags
 * @param   res             the newly allocated memregion is returned here
 *
 * @return  zero on success, -(errno) on failure.
 */
int memregion_alloc(struct fs_node_t *inode,
                    int prot, int type, int flags,
                    struct memregion_t **res);

/**
 * @brief Attach a memory region to a task.
 *
 * Called during fork, exec, mmap and shmget syscalls.
 *
 * NOTES:
 *   - The caller should have alloc'd memregion by calling memregion_alloc().
 *   - The size argument should be in PAGE_SIZE multiples, not in bytes.
 *   - The caller must have locked task->mem->mutex before calling us.
 *
 * @param   task                pointer to task
 * @param   memregion           memory region to attach
 * @param   attachat            attach \a memregion at this virtual address
 * @param   size                memregion size
 * @param   remove_overlaps     if non-zero, remove overlaps automatically
 *
 * @return  zero on success, -(errno) on failure.
 */
int memregion_attach(struct task_t *task, struct memregion_t *memregion,
                     virtual_addr attachat, size_t size, int remove_overlaps);

/**
 * @brief Detach a memory region from a task.
 *
 * Detach a memory region from a task and add it to the free region list.
 * If the region was mmap-ed from a file, dirty pages are written back to
 * the file. If \a free_pages is non-zero, the physical memory pages are
 * released.
 *
 * NOTES:
 *   - The caller must have locked task->mem->mutex before calling us.
 *
 * @param   task                pointer to task
 * @param   memregion           memory region to attach
 * @param   free_pages          if non-zero, free physical memory pages
 *
 * @return  zero on success, -(errno) on failure.
 */
int memregion_detach(struct task_t *task, struct memregion_t *memregion,
                     int free_pages);

/**
 * @brief Detach user memory regions.
 *
 * Detach (and possibly free pages used by) a user-allocated memory regions.
 * Called during exec(), as well when a task terminates (if all threads
 * are dead). If \a free_pages is non-zero, the physical memory pages are
 * released.
 *
 * @param   task                pointer to task
 * @param   free_pages          if non-zero, free physical memory pages
 *
 * @return  nothing.
 */
void memregion_detach_user(struct task_t *task, int free_pages);

/**
 * @brief Free a memory region.
 *
 * Release an alloc'd memregion struct and release its inode (if != NULL).
 *
 * @param   memregion           memory region to attach
 *
 * @return  nothing.
 */
void memregion_free(struct memregion_t *memregion);

/**
 * @brief Handler for syscall msync().
 *
 * Synchronise a file with a memory map.
 *
 * @param   addr        address of attachment
 * @param   length      size to synchronise
 * @param   flags       either MS_SYNC or MS_ASYNC, optionally OR'd with 
 *                        MS_INVALIDATE (see sys/mman.h)
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_msync(void *addr, size_t length, int flags);

/**
 * @brief Duplicate task memory map.
 *
 * Duplicate task memory map, making a copy of all its memory regions.
 * Called during fork().
 *
 * NOTES:
 *   - The caller must have locked mem->mutex before calling us.
 *
 * @param   mem         memory map to duplicate
 *
 * @return  the memory map copy on success, NULL on failure.
 */
struct task_vm_t *task_mem_dup(struct task_vm_t *mem);

/**
 * @brief Free task memory map.
 *
 * Called during task termination.
 *
 * @param   mem         memory map to free
 *
 * @return  nothing.
 */
void task_mem_free(struct task_vm_t *mem);

/**
 * @brief Load page into memory region.
 *
 * Load a memory page from the file node referenced in the given memregion,
 * or zero-out the page is the memregion has no file backing. The function
 * allocates a new physical memory page and sets its protection according
 * to the memregion's prot field.
 * This function is called from the page fault handler. The sought address
 * need not be page-aligned, as the function automatically aligns it down to
 * the nearest page boundary.
 * If the memregion is shared by more than one task (i.e. the tasklist
 * field != NULL), the other tasks' page tables are updated to advertise
 * the newly allocated physical memory page.
 *
 * TODO: there is a possible race condition if one of the other tasks page
 *       faults and allocates physical memory page for the same address
 *       before we advertise our physical memory page.
 *
 * NOTES:
 *   - The caller must have locked mem->mutex before calling us.
 *
 * @param   memregion           memory region
 * @param   pd                  task page directory
 * @param   __addr              address to load
 *
 * @return  zero on success, -(errno) on failure.
 */
int memregion_load_page(struct memregion_t *memregion, pdirectory *pd, 
                        volatile virtual_addr __addr);

/**
 * @brief Consolidate memory regions.
 *
 * For the given task, check all memory regions and piece together adjacent
 * regions that seem to be continuous. This function is called by mmap() and
 * mremap() after a new region has been attached to a task.
 *
 * @param   task        pointer to task
 *
 * @return  nothing.
 */
void memregion_consolidate(struct task_t *task);

/**
 * @brief Get shared page count.
 *
 * Get the number of shared memory pages.
 *
 * @param   task        pointer to task
 *
 * @return  memory usage in pages (not bytes).
 */
size_t memregion_shared_pagecount(struct task_t *task);

/**
 * @brief Get anonymous page count.
 *
 * Get the number of anonymous memory pages (ones with no file-backing).
 *
 * @param   task        pointer to task
 *
 * @return  memory usage in pages (not bytes).
 */
size_t memregion_anon_pagecount(struct task_t *task);

/**
 * @brief Get code page count.
 *
 * Get the number of text (code) memory pages.
 *
 * @param   task        pointer to task
 *
 * @return  memory usage in pages (not bytes).
 */
size_t memregion_text_pagecount(struct task_t *task);

/**
 * @brief Get data page count.
 *
 * Get the number of data memory pages.
 *
 * @param   task        pointer to task
 *
 * @return  memory usage in pages (not bytes).
 */
size_t memregion_data_pagecount(struct task_t *task);

/**
 * @brief Get stack page count.
 *
 * Get the number of stack memory pages.
 *
 * @param   task        pointer to task
 *
 * @return  memory usage in pages (not bytes).
 */
size_t memregion_stack_pagecount(struct task_t *task);

#endif      /* __MEMREGION_H__ */
