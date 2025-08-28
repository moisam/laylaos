/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024, 2025 (c)
 * 
 *    file: init_module.c
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
 *  \file init_module.c
 *
 *  Initialise and load kernel modules.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <kernel/laylaos.h>
#include <kernel/modules.h>
#include <kernel/ksymtab.h>
#include <kernel/user.h>
#include <mm/mmap.h>
#include <mm/kheap.h>
#include <mm/kstack.h>


#define DEFAULT_MODPATH         "/lib/modules"

#define INIT_HASHSZ             100

#define EHDR(mod)       ((elf_ehdr *)(mod)->module_image)
#define VADDR(mod)      ((virtual_addr)(mod)->module_image)
#define PHDRS(mod)      (elf_phdr *)(VADDR(mod) + EHDR(mod)->e_phoff)
#define SHDRS(mod)      (elf_shdr *)(VADDR(mod) + EHDR(mod)->e_shoff)
#define PHNUM(mod)      (EHDR(mod)->e_phnum)
#define SHNUM(mod)      (EHDR(mod)->e_shnum)
#define PHENTSIZE(mod)  (EHDR(mod)->e_phentsize)
#define SHENTSIZE(mod)  (EHDR(mod)->e_shentsize)


static struct kmodule_t *alloc_mod_obj(void);
static long load_module(struct kmodule_t *mod, int print_info);
static long load_module_sections(struct kmodule_t *mod);
static long read_module_dyntab(struct kmodule_t *mod);
static void get_module_info(struct kmodule_t *mod);
static long read_module_symbols(struct kmodule_t *mod);
static long load_module_list(char *depslist, int print_info);

static char *find_module_file(char *name, struct fs_node_t **node);
static struct kmodule_t *find_loaded_module(char *name);

void object_read_copy_relocs(elf_ehdr *hdr, elf_shdr *shdr, elf_sym *symtab,
                             char *strtab, struct hashtab_t *global_symbols,
                             uintptr_t mempos);

void object_relocate(elf_ehdr *hdr, elf_shdr *shdr, elf_sym *symtab,
                     char *strtab,
                     struct hashtab_t *global_symbols,
                     struct hashtab_t *symbols,
                     struct hashtab_t *tls_symbols,
                     size_t *tls_off, uintptr_t mempos, int print_err);


/*
 * Handler for syscall init_module().
 */
long syscall_init_module(void *module_image, unsigned long len, 
                         char *param_values)
{
    char *params = NULL;
    size_t paramslen = 0;
    long res;

    if(!suser(this_core->cur_task))
    {
        return -EPERM;
    }

    // check validity of user addresses first
    if(!valid_addr(this_core->cur_task, (virtual_addr)module_image,
                   (virtual_addr)module_image + len))
    {
        add_task_segv_signal(this_core->cur_task, SEGV_MAPERR, (void *)module_image);
        return -EFAULT;
    }
    
    // copy args
    if(param_values &&
       copy_str_from_user(param_values, &params, &paramslen) != 0)
    {
        return -EFAULT;
    }

    res = init_module_internal(module_image, len, params, 0);
    
    if(params)
    {
        kfree(params);
    }
    
    return res;
}


/*
 * Initialize a kernel module.
 */
long init_module_internal(void *module_image, unsigned long len, 
                          char *param_values, int print_info)
{
    UNUSED(len);
    UNUSED(param_values);

    struct kmodule_t *mod;
    long i;

    // alloc a new module object
    if(!(mod = alloc_mod_obj()))
    {
        printk("mod: insufficient memory\n");
        return -ENOMEM;
    }

    mod->module_image = module_image;

    // load the main executable and its dependencies
    if((i = load_module(mod, print_info)) != 0)
    {
        // not actually an error
        if(i == -EEXIST)
        {
            free_mod_obj(mod);
            return 0;
        }
        
        printk("mod: failed to load module: %s\n", strerror(-i));
        //printk("mod: failed to load module\n");
        free_mod_obj(mod);
        return i;
    }

    //get_module_info(mod);
    
    // check if the module is already loaded
    /*
    if(find_loaded_module(mod->modinfo.name))
    {
        printk("mod: module is already loaded\n");
        free_mod_obj(mod);
        return -EEXIST;
    }
    */

    // read symbols
    if(read_module_symbols(mod) != 0)
    {
        printk("mod: failed to resolve one or more symbols\n");
        free_mod_obj(mod);
        return -ENOEXEC;
    }

    // now do the relocations
    size_t tls_off = 0;
    object_relocate(EHDR(mod), SHDRS(mod), mod->symtab, mod->strtab,
                    mod->symbols, mod->symbols, mod->symbols,
                    &tls_off, mod->mempos, 1);

    // run init
    if(mod->init == NULL)
    {
        printk("mod: missing init_module() function\n");
        free_mod_obj(mod);
        return -ENOEXEC;
    }
    
    if(mod->init() != 0)
    {
        printk("mod: init_module() returned non-zero status\n");
        free_mod_obj(mod);
        return -EINVAL;
    }
    
    mod->module_image = 0;
    mod->state = MODULE_STATE_LOADED;

    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    
    kernel_mutex_lock(&kmod_list_mutex);
    mod->next = modules_head.next;
    modules_head.next = mod;
    kernel_mutex_unlock(&kmod_list_mutex);
    
    return 0;
}


static struct kmodule_t *alloc_mod_obj(void)
{
    struct kmodule_t *mod;
    
    if(!(mod = kmalloc(sizeof(struct kmodule_t))))
    {
        return NULL;
    }
    
    memset(mod, 0, sizeof(struct kmodule_t));

#define STRCMP          (int (*)(void *, void *))strcmp

    if(!(mod->symbols = hashtab_create(INIT_HASHSZ, 
                                       calc_hash_for_str, STRCMP)))
    {
        free_mod_obj(mod);
        return NULL;
    }

#undef STRCMP
    
    return mod;
}


/*
 * Free kernel module object.
 */
void free_mod_obj(struct kmodule_t *mod)
{
    if(!mod)
    {
        return;
    }

    /*
    if(mod->global_symbols)
    {
        hashtab_free(mod->global_symbols);
    }
    */

    if(mod->modinfo.name)
    {
        kfree(mod->modinfo.name);
    }

    if(mod->modinfo.author)
    {
        kfree(mod->modinfo.author);
    }

    if(mod->modinfo.desc)
    {
        kfree(mod->modinfo.desc);
    }

    if(mod->modinfo.deps)
    {
        kfree(mod->modinfo.deps);
    }
    
    if(mod->symbols)
    {
        hashtab_free(mod->symbols);
    }
    
    if(mod->mempos)
    {
        kernel_mutex_lock(&kmod_mem_mutex);
        vmmngr_free_pages(mod->mempos, mod->memsz);
        kernel_mutex_unlock(&kmod_mem_mutex);
    }
    
    kfree(mod);
}


#define INFO        if(print_info) printk

static long load_module(struct kmodule_t *mod, int print_info)
{
    long i;

    if(!mod)
    {
        return EINVAL;
    }
    
    // don't load an already loaded module
    if(mod->state == MODULE_STATE_LOADED)
    {
        return 0;
    }
    
    // read file header
    if(!check_elf_hdr("mod", EHDR(mod), 0))
    {
        return -ENOEXEC;
    }
    
    if(EHDR(mod)->e_type != ET_DYN)
    {
        return -ENOEXEC;
    }

    // calculate object size in memory
    if(object_calc_memsz(EHDR(mod), PHDRS(mod), NULL, &mod->memsz) != 0)
    {
        return -ENOEXEC;
    }
    
    // allocate memory
    if(!(mod->mempos = vmmngr_alloc_and_map(mod->memsz, 0, PTE_FLAGS_PW,
                                            NULL, REGION_KMODULE)))
    {
        //kernel_mutex_unlock(&kmod_mem_mutex);
        printk("mod: failed to alloc memory\n");
        return -ENOMEM;
    }

    INFO("Loading module to " _XPTR_ "\n", mod->mempos);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    
    // load object sections
    INFO("Loading object sections\n");
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    if(load_module_sections(mod) != 0)
    {
        printk("mod: failed to read sections\n");
        return -ENOMEM;
    }
    
    // read the dynamic table and find the needed dependencies
    INFO("Reading dynamic symbol table\n");
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    if(read_module_dyntab(mod) != 0)
    {
        printk("mod: failed to read dynamic table\n");
        return -ENOEXEC;
    }

    INFO("Getting module info\n");
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    // now that we have the symbol table, we can get module info and
    // validate it
    get_module_info(mod);
    
    if(!mod->modinfo.name || !mod->modinfo.author || !mod->modinfo.desc)
    {
        printk("mod: missing module name, author or description\n");
        return -ENOEXEC;
    }

    INFO("Checking if the module is already loaded\n");
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    // check if the module is already loaded
    if(find_loaded_module(mod->modinfo.name))
    {
        printk("mod: module is already loaded\n");
        return -EEXIST;
    }
    
    // read copy relocations
    INFO("Reading copy relocations\n");
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    object_read_copy_relocs(EHDR(mod), SHDRS(mod), mod->symtab,
                             mod->strtab, mod->symbols, mod->mempos);

    INFO("Loading dependencies\n");
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    // load the required dependencies
    if((i = load_module_list(mod->modinfo.deps, print_info)) != 0)
    {
        printk("mod: failed to load module dependencies: %s\n", strerror(-i));
        //printk("mod: failed to load module dependencies\n");
        return i;
    }

    INFO("Finished loading module '%s'\n", mod->modinfo.name);
    
    return 0;
}

#undef INFO


static long load_module_sections(struct kmodule_t *mod)
{
    long i;
    virtual_addr mempos, memsize, filepos, filesize;
    elf_phdr *phdr = PHDRS(mod);

    for(i = 0; i < PHNUM(mod); i++)
    {
        KDEBUG("%s: %d/%d\n", __func__, i, PHNUM(mod) - 1);

        switch(phdr->p_type)
        {
            case PT_LOAD:
                // some sanity checks first
                if(!check_phdr_sizes(phdr))
                {
                    return -ENOEXEC;
                }

                filepos = align_down(phdr->p_offset);
                filesize = phdr->p_filesz + (phdr->p_offset - filepos);
                mempos = align_down(mod->mempos + phdr->p_vaddr);
                memsize = phdr->p_memsz + 
                            ((mod->mempos + phdr->p_vaddr) - mempos);

                KDEBUG("Loading section: fp " _XPTR_ ", fs " _XPTR_
                       ", mp " _XPTR_ ", ms " _XPTR_ " (R%c%c)\n",
                       filepos, filesize, mempos, memsize,
                       (phdr->p_flags & PT_W) ? 'W' : '-',
                       (phdr->p_flags & PT_X) ? 'X' : '-');
                //__asm__ __volatile__("xchg %%bx, %%bx"::);

                if(filesize == 0)
                {
                    memset((void *)mempos, 0, memsize);
                }
                else
                {
                    memcpy((void *)mempos,
                           (void *)(VADDR(mod) + filepos), filesize);

                    if(memsize > filesize)
                    {
                        memset((void *)(mempos + filesize), 0, 
                                            (memsize - filesize));
                    }
                }
                
                if(!(phdr->p_flags & PT_W))
                {
                    KDEBUG("%s: changing flags\n", __func__);
                    vmmngr_change_page_flags(mempos, memsize, I86_PDE_PRESENT);
                }

                break;
        }
        
        KDEBUG("%s: next\n", __func__);
        phdr = (elf_phdr *)((char *)phdr + PHENTSIZE(mod));
    }

    KDEBUG("%s: done\n", __func__);
    return 0;
}


static long read_module_dyntab(struct kmodule_t *mod)
{
    int i, found = 0;
    int count = 0 /* , depcount = 0 */;
    //char *buf = NULL;
    elf_dyn *d, *ld, *dyntab = NULL;
    elf_phdr *phdr = PHDRS(mod);
    virtual_addr mempos = mod->mempos ? mod->mempos : VADDR(mod);

    KDEBUG("  total phdr entries %d\n", PHNUM(mod));

    for(i = 0; i < PHNUM(mod); i++)
    {
        KDEBUG("  phdr[%d].type = 0x%x\n", i, phdr->p_type);
        
        if(phdr->p_type != PT_DYNAMIC)
        {
            phdr = (elf_phdr *)((char *)phdr + PHENTSIZE(mod));
            continue;
        }
        
        // read the dynamic table
        found = 1;
        count = phdr->p_filesz / sizeof(elf_dyn);
        //depcount = 0;
        
        // walk through the table entries
        dyntab = (elf_dyn *)(VADDR(mod) + phdr->p_offset);
        ld = &dyntab[count];
        KDEBUG("    table entries = %d\n", count);
        
        for(d = dyntab; d < ld; d++)
        {
            KDEBUG("    entry.d_tag = 0x%x\n", d->d_tag);

            if(d->d_tag == DT_NULL)
            {
                break;
            }
            
            switch(d->d_tag)
            {
                case DT_HASH:
                    mod->hash = (elf_word *)(mempos + d->d_un.d_ptr);
                    KDEBUG("      mod->hash @ %p\n", (void *)mod->hash);
                    mod->symtab_size = mod->hash[1];
                    KDEBUG("      mod->symtab_size = " _XPTR_ "\n", mod->symtab_size);
                    break;

                case DT_STRTAB:
                    mod->strtab = (char *)(mempos + d->d_un.d_ptr);
                    KDEBUG("      mod->strtab @ %p\n", (void *)mod->strtab);
                    break;

                case DT_SYMTAB:
                    mod->symtab = (elf_sym *)(mempos + d->d_un.d_ptr);
                    KDEBUG("      mod->symtab @ %p\n", (void *)mod->symtab);
                    break;

                case DT_STRSZ:
                    mod->strtab_size = d->d_un.d_val;
                    KDEBUG("      mod->strtab_size = " _XPTR_ "\n", mod->strtab_size);
                    break;
                
                /*
                case DT_NEEDED:
                    depcount++;
                    break;
                */
                
                case DT_TEXTREL:
                    KDEBUG("      non-writable segment relocs\n");
                    //__asm__ __volatile__("xchg %%bx, %%bx"::);
                    return -ENOEXEC;
                    break;
            }
        }

        // walk through the table again to find library dependencies
        // this is done separate to the loop above to make sure the dynamic
        // string tables are loaded first as they are needed in order to
        // resolve dependency names
        /*
        KDEBUG("    checking dependencies\n");
        KDEBUG("    table entries = %d\n", count);

        for(d = dyntab; d < ld; d++)
        {
            if(d->d_tag == DT_NEEDED)
            {
                list_add(elf->dependencies, elf->strtab + d->d_un.d_val);
            }
        }
        */
        
        phdr = (elf_phdr *)((char *)phdr + PHENTSIZE(mod));
    }
    
    if(!found)
    {
        return -ENOEXEC;
    }

    return 0;
}


static char *getstr(char *s)
{
    KDEBUG("getstr: s @ 0x%x\n", s);
    
    char *d;
    size_t len = strlen(s);
    
    if(!(d = kmalloc(len + 1)))
    {
        return NULL;
    }
    
    strcpy(d, s);
    return d;
}


static void get_module_info(struct kmodule_t *mod)
{
    elf_half k;
    elf_sym *symtab = mod->symtab;
    char *s, *v;

    for(k = 0; k < mod->symtab_size; k++)
    {
        if(symtab->st_name > (unsigned int)mod->strtab_size)
        {
            break;
        }
        
        s = mod->strtab + symtab->st_name;
        
        KDEBUG("get_module_info: 0x%x\n", symtab->st_name);
        KDEBUG("get_module_info: %s\n", s);
        
        // in order to get module info variable values (author, name, ...),
        // we need to:
        //    - calculate the variable's address
        //    - get the value at that address, which is a char pointer
        //    - this pointer is an offset to the variable's value in file
        //    - add the pointer value to the base address (where the module's
        //      image is loaded in memory)
        //    - read the string at the final address
        
        if(strcmp(s, "module_name") == 0)
        {
            v = *(char **)(mod->mempos + symtab->st_value);
            v += VADDR(mod);

            if(!(mod->modinfo.name = getstr(v)))
            {
                return;
            }

            KDEBUG("%s: 0x%x, %s\n", s, symtab->st_value, mod->modinfo.name);
            //mod->modinfo.name = (char *)(mod->mempos + symtab->st_value);
        }
        else if(strcmp(s, "module_author") == 0)
        {
            v = *(char **)(mod->mempos + symtab->st_value);
            v += VADDR(mod);

            if(!(mod->modinfo.author = getstr(v)))
            {
                return;
            }

            KDEBUG("%s: 0x%x, %s\n", s, symtab->st_value, mod->modinfo.author);
            //mod->modinfo.author = (char *)(mod->mempos + symtab->st_value);
        }
        else if(strcmp(s, "module_description") == 0)
        {
            v = *(char **)(mod->mempos + symtab->st_value);
            v += VADDR(mod);

            if(!(mod->modinfo.desc = getstr(v)))
            {
                return;
            }

            KDEBUG("%s: 0x%x, %s\n", s, symtab->st_value, mod->modinfo.desc);
            //mod->modinfo.desc = (char *)(mod->mempos + symtab->st_value);
        }
        else if(strcmp(s, "module_dependencies") == 0)
        {
            KDEBUG("%s: 0x%x, 0x%x\n", s, symtab->st_value, VADDR(mod));
            v = *(char **)(mod->mempos + symtab->st_value);
            KDEBUG("%s: 0x%x\n", s, v);
            v += VADDR(mod);
            KDEBUG("%s: 0x%x\n", s, v);

            if(!(mod->modinfo.deps = getstr(v)))
            {
                return;
            }

            KDEBUG("%s: 0x%x, %s\n", s, symtab->st_value, mod->modinfo.deps);
            //mod->modinfo.deps = (char *)(mod->mempos + symtab->st_value);
        }
        else if(strcmp(s, "init_module") == 0)
        {
            KDEBUG("init_module: 0x%x\n", mod->mempos + symtab->st_value);
            mod->init = (int (*)(void))(mod->mempos + symtab->st_value);
        }
        else if(strcmp(s, "cleanup_module") == 0)
        {
            KDEBUG("cleanup_module: 0x%x\n", mod->mempos + symtab->st_value);
            mod->cleanup = (void (*)(void))(mod->mempos + symtab->st_value);
        }

        //var = mod->mempos + symtab->st_value;
        
        symtab++;
    }
    
    KDEBUG("get_module_info: end\n");
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
}


static long read_module_symbols(struct kmodule_t *mod)
{
    size_t i;
    elf_sym *sym;
    char *name;
    void *val;
    
    if(mod == NULL || mod->symtab == NULL || mod->strtab == NULL)
    {
        return - EINVAL;
    }
    
    for(i = 0; i < mod->symtab_size; i++)
    {
        sym = &mod->symtab[i];
        name = mod->strtab + sym->st_name;
        
        if(sym->st_shndx)
        {
            if(hashtab_lookup(mod->symbols, name) == NULL)
            {
                hashtab_add(mod->symbols, name, 
                            (void *)(sym->st_value + mod->mempos));
            }
        }
        else if(*name)
        {
            KDEBUG("read_module_symbols: looking up symbol '%s'\n", name);
            if((val = ksym_value(name)) != 0)
            {
                hashtab_add(mod->symbols, name, val);
            }
            else
            {
                return -EINVAL;
            }
        }
    }
    
    return 0;
}


static long load_module_list(char *depslist, int print_info)
{
    char *p = depslist, *p2;
    size_t len;
    struct fs_node_t *node;
    virtual_addr imageaddr;
    size_t imagesz;
    long res;
    ssize_t readsz;
    off_t fpos;

    if(!depslist || !*depslist)
    {
        return 0;
    }
    
    while(1)
    {
        if(!*p)
        {
            return -ENOENT;
        }
        
        // skip leading spaces
        while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        {
            p++;
        }
        
        p2 = p;
        
        while(*p2 && *p2 != ',')
        {
            p2++;
        }
        
        len = p2 - p;
        
        if(len == 0 || len >= MAX_MODULE_NAMELEN)
        {
            return -E2BIG;
        }

        char buf[len + 1];
        
        memcpy(buf, p, len);
        buf[len] = '\0';

        // remove trailing spaces
        p = buf + len - 1;
        
        while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        {
            *p = '\0';
            p--;
        }
        
        // check if the module is already loaded
        if(find_loaded_module(buf))
        {
            goto next;
        }
        
        if(!(p = find_module_file(buf, &node)))
        {
            return -ENOENT;
        }

        // allocate temp memory
        imagesz = node->size;

        if(!(imageaddr = vmmngr_alloc_and_map(imagesz, 0, PTE_FLAGS_PW,
                                              NULL, REGION_KMODULE)))
        {
        	release_node(node);
            kfree(p);
            return -ENOMEM;
        }

        fpos = 0;

        if((readsz = vfs_read_node(node, &fpos, (unsigned char *)imageaddr,
                                   imagesz, 1)) == 0)
        {
            readsz = -ENOEXEC;
        }

    	release_node(node);
        kfree(p);
        
        if(readsz < 0)
        {
            vmmngr_free_pages(imageaddr, imagesz);
            return (int)readsz;
        }

        if((res = init_module_internal((void *)imageaddr, imagesz, 
                                       NULL, print_info)) < 0)
        {
            vmmngr_free_pages(imageaddr, imagesz);
            return res;
        }
        
next:
        while(*p2 == ',')
        {
            p2++;
        }
        
        p = p2;
    }
}


static char *find_module_file(char *name, struct fs_node_t **node)
{
    char *modpath, *p;
    long res;
    //struct fs_node_t *node = NULL;
	int open_flags = OPEN_KERNEL_CALLER | /* OPEN_NO_TRAILING_SLASH | */
	                 OPEN_FOLLOW_SYMLINK;
    
    modpath = DEFAULT_MODPATH;
    *node = NULL;
    
    KDEBUG("find_module_file: name %s, path %s\n", name, modpath);
    
    while((p = next_path_entry(&modpath, name, 1)))
    {
        KDEBUG("find_module_file: p %s\n", p);

        // check if file exists and move along if not
    	if((res = vfs_open_internal(p, AT_FDCWD, node, open_flags)) < 0)
	    {
            KDEBUG("find_module_file: res %d\n", res);
            kfree(p);
            continue;
	    }

        KDEBUG("find_module_file: success\n");
    	//release_node(*node);
        return p;
    }
    
    KDEBUG("find_module_file: done\n");
    
    return NULL;
}


static struct kmodule_t *find_loaded_module(char *name)
{
    struct kmodule_t *mod;
    
    kernel_mutex_lock(&kmod_list_mutex);

    for(mod = modules_head.next; mod != NULL; mod = mod->next)
    {
        if(strcmp(mod->modinfo.name, name) == 0)
        {
            kernel_mutex_unlock(&kmod_list_mutex);
            return mod;
        }
    }
    
    kernel_mutex_unlock(&kmod_list_mutex);
    
    return NULL;
}

