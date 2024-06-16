/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: modules.h
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
 *  \file modules.h
 *
 *  Functions and macro definitions for working with kernel modules.
 */

#ifndef __KERNEL_MODULES__
#define __KERNEL_MODULES__

#ifdef MODULE

#define MODULE_INFO(tag, val)           \
    __attribute__((section(".modinfo"))) char *module_ ## tag = val;
    
#define MODULE_NAME(str)            MODULE_INFO(name, str)
#define MODULE_DESCRIPTION(str)     MODULE_INFO(description, str)
#define MODULE_AUTHOR(str)          MODULE_INFO(author, str)
#define MODULE_NEEDED(str)          MODULE_INFO(dependencies, str)

int init_module(void);
void cleanup_module(void);

#include <kernel/laylaos.h>

#else       /* !MODULE */

#define MAX_BOOT_MODULES        32
#define MAX_MODULE_CMDLINE      128
#define MAX_MODULE_NAMELEN      128


#include <mm/mmngr_virtual.h>
#include <mm/mmngr_phys.h>
#include <kernel/elf.h>
#include <kernel/mutex.h>
#include <sys/hash.h>


/**
 * @struct boot_module_t
 * @brief The boot_module_t structure.
 *
 * A structure to represent a boot module (loaded by GRUB).
 */
struct boot_module_t
{
    physical_addr pstart,   /**< start physical address */
                  pend;     /**< end physical address   */
    virtual_addr vstart,    /**< start virtual address  */
                 vend;      /**< end virtual address    */
    char cmdline[MAX_MODULE_CMDLINE];   /**< module command line */
};


/**
 * @struct kmodule_info_t
 * @brief The kmodule_info_t structure.
 *
 * A structure to represent module info.
 */
struct kmodule_info_t
{
    char *name;     /**< module name */
    char *author;   /**< module author */
    char *desc;     /**< module description */
    char *deps;     /**< module dependencies */
};


/**
 * @struct kmodule_t
 * @brief The kmodule_t structure.
 *
 * A structure to represent a kernel module.
 */
struct kmodule_t
{
    struct kmodule_info_t modinfo;  /**< module info */

#define MODULE_STATE_LOADED         0x01
#define MODULE_STATE_UNLOADED       0x02
    int state;              /**< module state */

    void *module_image;     /**< pointer to the module image */
    virtual_addr mempos,    /**< where to load module in memory */
                 memsz;     /**< module memory size */

    char *strtab;           /**< module string table */
    size_t strtab_size;     /**< module string table size */
    
    elf_sym *symtab;        /**< module symbol table */
    size_t symtab_size;     /**< module symbol table size */

    elf_word *hash;         /**< module hashtable */
    
    int (*init)(void);      /**< module init function */
    void (*cleanup)(void);  /**< module fini function */

    struct hashtab_t *symbols;  /**< module symbol hashtable */
    
    struct kmodule_t *next;     /**< next loaded module */
};


/********************************
 * externs defined in modules.c
 ********************************/

/**
 * @var boot_module_count
 * @brief boot module count.
 *
 * This number is set at boot time to the number of modules loaded by the 
 * bootloader (currently we use GRUB).
 */
extern int boot_module_count;

/**
 * @var boot_module
 * @brief boot modules.
 *
 * Pointers to the boot modules that were loaded by the bootloader
 * (currently we use GRUB).
 */
extern struct boot_module_t boot_module[];

/**
 * @var kmod_mem_mutex
 * @brief Module memory lock.
 *
 * Lock used to synchronize access to kernel module memory.
 */
extern struct kernel_mutex_t kmod_mem_mutex;

/**
 * @var kmod_list_mutex
 * @brief Module list lock.
 *
 * Lock used to synchronize access to kernel module list.
 */
extern struct kernel_mutex_t kmod_list_mutex;

/**
 * @var modules_head
 * @brief Modules head.
 *
 * Pointer to the first module in the loaded kernel module list.
 */
extern struct kmodule_t modules_head;


/***********************
 * Function prototypes
 ***********************/

/**
 * @brief Initialise boot modules.
 *
 * Initialize kernel modules. The modules MUST be loaded as follows:
 *  -  Module[0] => initrd image
 *  -  Module[1] => the kernel's symbol table (/boot/System.map)
 *  -  Module[2..n] => rest of boot modules
 *
 * This function also loads and decompresses the initial ramdisk (initrd),
 * which we'll mount later as our filesystem root. It also loads the kernel
 * symbol table, needed to load other kernel modules.
 *
 * @return  nothing.
 */
void boot_module_init(void);

/**
 * @brief Handler for syscall init_module().
 *
 * Initialize a kernel module that has been loaded to memory.
 *
 * @param   module_image    where the module has been loaded to memory
 * @param   len             size of module_image
 * @param   param_values    parameters to pass to the module's init function
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_init_module(void *module_image, unsigned long len,
                        char *param_values);

/**
 * @brief Initialize a kernel module.
 *
 * Helper function, called by syscall_init_module() and boot_module_init().
 *
 * @param   module_image    where the module has been loaded to memory
 * @param   len             size of module_image
 * @param   param_values    parameters to pass to the module's init function
 * @param   print_info      non-zero to print info as the module is initialised
 *
 * @return  zero on success, -(errno) on failure.
 */
int init_module_internal(void *module_image, unsigned long len, 
                         char *param_values, int print_info);

/**
 * @brief Free kernel module object.
 *
 * Release the memory used by the module info and the module object.
 *
 * @param   mod     struct of module to free
 *
 * @return  nothing.
 */
void free_mod_obj(struct kmodule_t *mod);


/**
 * @brief Handler for syscall delete_module().
 *
 * Unload kernel module.
 * This function is currently not fully implemented.
 *
 * @param   __name      module name
 * @param   flags       currently unused
 *
 * @return  zero on success, -(errno) on failure.
 */
int syscall_delete_module(char *__name, unsigned int flags);

#endif      /* !MODULE */

#endif      /* __KERNEL_MODULES__ */
