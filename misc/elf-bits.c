/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023 (c)
 * 
 *    file: elf-bits.c
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
 *  \file elf-bits.c
 *
 *  Helper functions for working with ELF executables.
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <kernel/elf.h>


#ifdef KERNEL
# include <mm/kheap.h>
# include <sys/hash.h>
# include <kernel/laylaos.h>
//# include "hash.h"
# define printf                 printk
# define malloc                 kmalloc
#endif


#ifdef USE_ALTERNATE_MALLOC
#undef malloc
#define malloc          USE_ALTERNATE_MALLOC
#endif

#ifdef USE_ALTERNATE_FREE
#undef free
#define free            USE_ALTERNATE_FREE
#endif


#define PRINTERR(s, a)          if(print_err) printf(s, a);


/*
 * Check ELF header.
 */
int check_elf_hdr(char *caller, elf_ehdr *hdr, int print_err)
{
    if(!hdr)
    {
        return 0;
    }
    
    if(hdr->e_ident[EI_MAG0] != ELFMAG0 || hdr->e_ident[EI_MAG1] != ELFMAG1 ||
       hdr->e_ident[EI_MAG2] != ELFMAG2 || hdr->e_ident[EI_MAG3] != ELFMAG3)
    {
        PRINTERR("%s: invalid ELF header magic field\n", caller);
        return 0;
    }

#ifdef __x86_64__

    if(hdr->e_ident[EI_CLASS] != ELFCLASS64)
    {
        PRINTERR("%s: unsupported ELF file class\n", caller);
        return 0;
    }
    
    if(hdr->e_machine != EM_X86_64)
    {
        PRINTERR("%s: unsupported ELF file target\n", caller);
        return 0;
    }

#else       /* !__x86_64__ */

    if(hdr->e_ident[EI_CLASS] != ELFCLASS32)
    {
        PRINTERR("%s: unsupported ELF file class\n", caller);
        return 0;
    }
    
    if(hdr->e_machine != EM_386)
    {
        PRINTERR("%s: unsupported ELF file target\n", caller);
        return 0;
    }

#endif      /* !__x86_64__ */
    
    if(hdr->e_ident[EI_DATA] != ELFDATA2LSB)
    {
        PRINTERR("%s: unsupported ELF file byte order\n", caller);
        return 0;
    }
    
    if(hdr->e_ident[EI_VERSION] != EV_CURRENT)
    {
        PRINTERR("%s: unsupported ELF file version\n", caller);
        return 0;
    }

/*
    if(hdr->e_type != ET_REL && hdr->e_type != ET_EXEC)
    {
        PRINTERR("Unsupported ELF file type\n");
        return 0;
    }
*/
    
    return 1;
}


/*
 * Calculate ELF memory size.
 */
int object_calc_memsz(elf_ehdr *ehdr, elf_phdr *phdr,
                      uintptr_t *membase, uintptr_t *memsz)
{
    uintptr_t brk = 0;
    uintptr_t base = (uintptr_t)-1;
    int i;

    for(i = 0; i < ehdr->e_phnum; i++)
    {
        switch(phdr->p_type)
        {
            case PT_LOAD:
                if(phdr->p_vaddr < base)
                {
                    base = phdr->p_vaddr;
                }
                
                if(phdr->p_vaddr + phdr->p_memsz > brk)
                {
                    brk = phdr->p_vaddr + phdr->p_memsz;
                }
                break;
                
            default:
                break;
        }
        
        phdr = (elf_phdr *)((char *)phdr + ehdr->e_phentsize);
    }
    
    if(base == (uintptr_t)-1)
    {
        return ENOEXEC;
    }
    
    if(memsz)
    {
        *memsz = brk - base;
    }

    if(membase)
    {
        *membase = base;
    }

    return 0;
}


/*
 * Check ELF program header.
 */
int check_phdr_sizes(elf_phdr *phdr)
{
    if(phdr->p_memsz < phdr->p_filesz)
    {
        return 0;
    }

    if(phdr->p_filesz &&
       (phdr->p_vaddr % phdr->p_align != phdr->p_offset % phdr->p_align))
    {
        return 0;
    }
    
    if(phdr->p_vaddr > USER_MEM_END ||
       (phdr->p_vaddr + phdr->p_memsz) > USER_MEM_END)
    {
        return 0;
    }
    
    return 1;
}


void object_read_copy_relocs(elf_ehdr *ehdr, elf_shdr *shdr, elf_sym *symtab,
                             char *strtab, struct hashtab_t *global_symbols,
                             uintptr_t mempos)
{
    elf_rel *rel, *reltab;
    elf_rela *rela, *relatab;
    elf_word i, j;
    elf_half k;

    for(k = 0; k < ehdr->e_shnum; k++)
    {
        if(shdr->sh_type == SHT_REL)
        {
            reltab = (elf_rel *)(mempos + shdr->sh_addr);
            j = shdr->sh_size / sizeof(elf_rel);
        
            for(i = 0; i < j; i++)
            {
                rel = &reltab[i];

#ifdef __x86_64__
                if(ELF64_R_TYPE(rel->r_info) == R_x86_64_COPY)
                {
                    elf_sym *sym = &symtab[ELF64_R_SYM(rel->r_info)];
                    char *name = (char *)(strtab + sym->st_name);
                    hashtab_add(global_symbols, name, (void *)rel->r_offset);
                }
#else
                if(ELF32_R_TYPE(rel->r_info) == R_386_COPY)
                {
                    elf_sym *sym = &symtab[ELF32_R_SYM(rel->r_info)];
                    char *name = (char *)(strtab + sym->st_name);
                    hashtab_add(global_symbols, name, (void *)rel->r_offset);
                }
#endif

            }
        }
        else if(shdr->sh_type == SHT_RELA)
        {
            relatab = (elf_rela *)(mempos + shdr->sh_addr);
            j = shdr->sh_size / sizeof(elf_rela);
        
            for(i = 0; i < j; i++)
            {
                rela = &relatab[i];
            
#ifdef __x86_64__
                if(ELF64_R_TYPE(rela->r_info) == R_x86_64_COPY)
                {
                    elf_sym *sym = &symtab[ELF64_R_SYM(rela->r_info)];
                    char *name = (char *)(strtab + sym->st_name);
                    hashtab_add(global_symbols, name, (void *)rela->r_offset);
                }
#else
                if(ELF32_R_TYPE(rela->r_info) == R_386_COPY)
                {
                    elf_sym *sym = &symtab[ELF32_R_SYM(rela->r_info)];
                    char *name = (char *)(strtab + sym->st_name);
                    hashtab_add(global_symbols, name, (void *)rela->r_offset);
                }
#endif

            }
        }
        
        shdr = (elf_shdr *)((char *)shdr + ehdr->e_shentsize);
    }
}


void object_relocate(elf_ehdr *ehdr, elf_shdr *shdr, elf_sym *symtab,
                     char *strtab,
                     struct hashtab_t *global_symbols,
                     struct hashtab_t *symbols,
                     struct hashtab_t *tls_symbols,
                     size_t *tls_off, uintptr_t mempos, int print_err)
{
#ifndef __x86_64__
    elf_rel *rel, *reltab;
#endif
    elf_rela *rela, *relatab;
    elf_word i, j;
    elf_sym *sym;
    uintptr_t sym_loc;
    void *reloc_loc;
    char *sym_name;
    struct hashtab_item_t *hitem;
    elf_half k;
    
    for(k = 0; k < ehdr->e_shnum; k++)
    {
        if(shdr->sh_type != SHT_REL && shdr->sh_type != SHT_RELA)
        {
            shdr = (elf_shdr *)((char *)shdr + ehdr->e_shentsize);
            continue;
        }

        if(shdr->sh_type == SHT_REL)
        {

#ifdef __x86_64__

            if(print_err)
            {
                printf("REL section ignored in ELF64 executable!\n");
            }

            shdr = (elf_shdr *)((char *)shdr + ehdr->e_shentsize);
            continue;

#else

            reltab = (elf32_rel *)(mempos + shdr->sh_addr);
            j = shdr->sh_size / sizeof(elf32_rel);
        
            for(i = 0; i < j; i++)
            {
                rel = &reltab[i];
            
                uint8_t rel_type = ELF32_R_TYPE(rel->r_info);
                uint32_t rel_sym = ELF32_R_SYM(rel->r_info);
            
                if(rel_type == R_386_NONE)
                {
                    continue;
                }

                sym = &symtab[rel_sym];
                sym_loc = mempos + sym->st_value;
                sym_name = (char *)(strtab + sym->st_name);
            
                if(rel_type == R_386_32 ||rel_type == R_386_PC32 ||
                   rel_type == R_386_COPY || rel_type == R_386_GLOB_DAT ||
                   rel_type == R_386_JMP_SLOT)
                {
                    /*
                    if(sym_name)
                    {
                        if((hitem = hashtab_lookup(elf->symbols, sym_name)))
                        {
                            sym_loc = hitem->val;
                        }
                        else
                        {
                            sym_loc = 0;
                        }
                    }
                    */

                    if(sym_name && (hitem = hashtab_lookup(symbols, sym_name)))
                    {
                        sym_loc = (uintptr_t)hitem->val;
                    }
                    else
                    {
                        sym_loc = 0;
                    }
                }
            
                if(rel_type == R_386_GLOB_DAT)
                {
                    if(sym_name)
                    {
                        if((hitem = hashtab_lookup(global_symbols, sym_name)))
                        {
                            sym_loc = (uintptr_t)hitem->val;
                        }
                    }
                }
            
                reloc_loc = (void *)(mempos + rel->r_offset);
            
                switch(rel_type)
                {
                    case R_386_32:
                        sym_loc += *((ssize_t *)reloc_loc);
                        *((uintptr_t *)reloc_loc) = (uintptr_t)sym_loc;
                        break;

                    case R_386_PC32:
                        sym_loc += *((ssize_t *)reloc_loc);
                        sym_loc -= mempos + rel->r_offset;
                        *((uintptr_t *)reloc_loc) = (uintptr_t)sym_loc;
                        break;

                    case R_386_COPY:
                        memcpy(reloc_loc, (void *)sym_loc, sym->st_size);
                        break;

                    case R_386_GLOB_DAT:
                    case R_386_JMP_SLOT:
                        *((uintptr_t *)reloc_loc) = (uintptr_t)sym_loc;
                        break;

                    case R_386_RELATIVE:
                        sym_loc = mempos + *((ssize_t *)reloc_loc);
                        *((uintptr_t *)reloc_loc) = (uintptr_t)sym_loc;
                        break;
                
                    default:
                        break;
                }
            }

#endif

        }
        else    // if(shdr->sh_type == SHT_RELA)
        {
            relatab = (elf_rela *)(mempos + shdr->sh_addr);
            j = shdr->sh_size / sizeof(elf_rela);

            for(i = 0; i < j; i++)
            {
                rela = &relatab[i];

#ifdef __x86_64__
                unsigned int rel_type = ELF64_R_TYPE(rela->r_info);
                unsigned int rel_sym = ELF64_R_SYM(rela->r_info);
#else
                unsigned int rel_type = ELF32_R_TYPE(rela->r_info);
                unsigned int rel_sym = ELF32_R_SYM(rela->r_info);
#endif

                if(rel_type == 0 /* R_386_NONE */)
                {
                    continue;
                }

                sym = &symtab[rel_sym];
                sym_loc = mempos + sym->st_value;
                sym_name = (char *)(strtab + sym->st_name);
            
#ifdef __x86_64__
                if(rel_type == R_x86_64_64 ||rel_type == R_x86_64_PC32 ||
                   rel_type == R_x86_64_COPY || rel_type == R_x86_64_GLOB_DAT ||
                   rel_type == R_x86_64_JMP_SLOT || rel_type == R_x86_64_8 ||
                   rel_type == R_x86_64_TPOFF64 || rel_type == R_x86_64_32 ||
                   rel_type == R_x86_64_32S)
#else
                if(rel_type == R_386_32 ||rel_type == R_386_PC32 ||
                   rel_type == R_386_COPY || rel_type == R_386_GLOB_DAT ||
                   rel_type == R_386_JMP_SLOT)
#endif
                {
                    if(sym_name && (hitem = hashtab_lookup(symbols, sym_name)))
                    {
                        sym_loc = (uintptr_t)hitem->val;
                    }
                    else
                    {
                        sym_loc = 0;
                    }
                }
            
#ifdef __x86_64__
                if(rel_type == R_x86_64_GLOB_DAT)
#else
                if(rel_type == R_386_GLOB_DAT)
#endif
                {
                    if(sym_name)
                    {
                        if((hitem = hashtab_lookup(global_symbols, sym_name)))
                        {
                            sym_loc = (uintptr_t)hitem->val;
                        }
                    }
                }
            
                reloc_loc = (void *)(mempos + rela->r_offset);
            
                switch(rel_type)
                {
#ifdef __x86_64__
                    case R_x86_64_64:
                    case R_x86_64_32:
                    case R_x86_64_32S:
#else
                    case R_386_32:
                        sym_loc += *((ssize_t *)reloc_loc);
#endif
                        sym_loc += rela->r_addend;
                        *((uintptr_t *)reloc_loc) = (uintptr_t)sym_loc;
                        break;

#ifdef __x86_64__
                    /*
                     * TODO: process R_x86_64_PC32.
                     */
#else
                    case R_386_PC32:
                        sym_loc += *((ssize_t *)reloc_loc);
                        sym_loc -= mempos + rela->r_offset;
                        *((uintptr_t *)reloc_loc) = (uintptr_t)sym_loc;
                        break;
#endif

#ifdef __x86_64__
                    case R_x86_64_COPY:
#else
                    case R_386_COPY:
#endif
                        memcpy(reloc_loc, (void *)sym_loc, sym->st_size);
                        break;

#ifdef __x86_64__
                    case R_x86_64_GLOB_DAT:
                    case R_x86_64_JMP_SLOT:
#else
                    case R_386_GLOB_DAT:
                    case R_386_JMP_SLOT:
#endif
                        //if(strcmp(sym_name, "widget_create") == 0) __asm__ __volatile__("xchg %%bx, %%bx"::);

                        *((uintptr_t *)reloc_loc) = (uintptr_t)sym_loc;
                        break;

#ifdef __x86_64__
                    case R_x86_64_RELATIVE:
                        sym_loc = mempos;
#else
                    case R_386_RELATIVE:
                        sym_loc = mempos + *((ssize_t *)reloc_loc);
#endif
                        sym_loc += rela->r_addend;
                        *((uintptr_t *)reloc_loc) = (uintptr_t)sym_loc;
                        break;

#ifdef __x86_64__
                    case R_x86_64_TPOFF64:
                        sym_loc = *((ssize_t *)reloc_loc);

                        if(sym_name && (hitem = hashtab_lookup(tls_symbols, sym_name)))
                        {
                            sym_loc -= (size_t)hitem->val;
                        }
                        else
                        {
                            if(!sym->st_size)
                            {
                                if(print_err)
                                {
                                    printf("TLS symbol '%s' with 0 size!\n", sym_name);
                                }
                            }
                            
                            *tls_off += sym->st_size;
                            hashtab_add(tls_symbols, sym_name, (void *)(*tls_off));
                            sym_loc -= *tls_off;
                        }

                        *((uintptr_t *)reloc_loc) = (uintptr_t)sym_loc;
                        break;
#endif
                
                    default:
                        break;
                }
            }
        }

        shdr = (elf_shdr *)((char *)shdr + ehdr->e_shentsize);
    }
}


/*
 * The following function's code is taken from the file 'strings.h' in
 * Layla shell's source code.
 *
 * Create a pathname by concatenating the next colon entry and the given 
 * filename. If 'use_dot' is non-zero, we prepend './' to the pathname if 
 * the next colon entry is an empty string.
 * 
 * Returns the malloc'd pathname, or NULL if we reached the end of the string,
 * or an error occurs.
 */
char *next_path_entry(char **colon_list, char *filename, int use_dot)
{
    char *s1 = colon_list ? *colon_list : NULL;
    char *s2 = s1;
    
    if(!s2 || !*s2)
    {
        return NULL;
    }
    
    /* skip to the next colon */
    while(*s2 && *s2 != ':')
    {
        s2++;
    }
    
    /* get path lengths */
    size_t flen = strlen(filename);
    size_t plen = s2-s1;
    /*
     * we add three for \0 terminator, possible /, and leading dot
     * (in case the path is NULL and we need to append ./)
     */
    int tlen = plen+flen+3;
    char *path = malloc(tlen);
    if(!path)
    {
        return NULL;
    }

    /* empty colon entry */
    if(!plen)
    {
        if(use_dot)
        {
            strcpy(path, "./");
        }
        else
        {
            path[0] = '\0';
        }
    }
    /* non-empty colon entry */
    else
    {
        //strncpy(path, s1, plen);
        memcpy(path, s1, plen);
        if(path[plen-1] != '/')
        {
            path[plen  ] = '/';
            path[plen+1] = '\0';
        }
        else
        {
            path[plen  ] = '\0';
        }
    }

    /* form the new path as the path segment plus the filename */
    strcat(path, filename);
    
    /* skip the colons */
    while(*s2 && *s2 == ':')
    {
        s2++;
    }
    (*colon_list) = s2;
    
    /* return the result */
    return path;
}

