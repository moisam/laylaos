/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: ksymtab.c
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
 *  \file ksymtab.c
 *
 *  The kernel's symbol table implementation.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/modules.h>
#include <kernel/ksymtab.h>
#include <mm/kheap.h>

#define INIT_HASHSZ                 256

struct hashtab_t *ksymtab = NULL;


static inline virtual_addr hex(char *p, char *p2)
{
    virtual_addr v = 0;
    
    while(p < p2)
    {
        //KDEBUG("hex: *p %c, v " _XPTR_ "\n", *p, v);
        
        if(*p >= '0' && *p <= '9')
        {
            v = (v * 16) + (*p - '0');
        }
        else if(*p >= 'a' && *p <= 'f')
        {
            v = (v * 16) + ((*p - 'a') + 10);
        }
        else if(*p >= 'A' && *p <= 'F')
        {
            v = (v * 16) + ((*p - 'A') + 10);
        }
        else
        {
            kpanic("Invalid hex number in System.map\n");
        }
        
        p++;
    }
    
    return v;
}


#define CHECK_PTR()                                 \
    if(p2 >= pend)                                  \
        kpanic("Failed to parse System.map\n");

#define SKIP_NON_SPACES()                                   \
    while(*p2 && *p2 != ' ' && *p2 != '\t' && *p2 != '\n')  \
        p2++;                                               \
    CHECK_PTR()

#define SKIP_SPACES()                               \
    while(*p2 == ' ' || *p2 == '\t' || *p2 == '\n') \
        p2++;                                       \
    CHECK_PTR()


/*
 * Initialise ksymtab.
 */
int ksymtab_init(virtual_addr data_start, virtual_addr data_end)
{
    char *p, *p2, *pend;
    /* volatile */ void *key, *val;
    size_t len;
    
#define STRCMP          (int (*)(void *, void *))strcmp
    
    if(!(ksymtab = hashtab_create(INIT_HASHSZ, calc_hash_for_str, STRCMP)))
    {
        kpanic("Failed to initialise kernel symbol table\n");
        return -ENOMEM;
    }

#undef STRCMP
    
    // each line in the System.map file is formatted as 3 fields:
    //    Addr Type SymName
    
    p = (char *)data_start;
    pend = (char *)data_end;
    
    while(1)
    {
        if(p >= pend)
        {
            break;
        }
        
        // get field 0: the memory address
        p2 = p;
        SKIP_NON_SPACES();
        val = (void *)hex(p, p2);
        
        // skip spaces
        SKIP_SPACES();
        p = p2;

        // get field 1: the variable's type (not used currently)
        SKIP_NON_SPACES();

        // skip spaces
        SKIP_SPACES();
        p = p2;

        // get field 2: the variable's name
        SKIP_NON_SPACES();
        len = p2 - p;
        
        if(!(key = kmalloc(len + 1)))
        {
            kpanic("Insufficient memory for kernel symbol\n");
            empty_loop();
        }
        
        A_memcpy(key, p, len);
        *((char *)key + len) = '\0';
        hashtab_add(ksymtab, key, val);

        while(p2 < pend && (*p2 == ' ' || *p2 == '\t' || *p2 == '\n'))
        {
            p2++;
        }

        p = p2;
    }

    return 0;
}


/*
 * Get symbol value.
 */
void *ksym_value(char *name)
{
    struct hashtab_item_t *hitem;
    struct kmodule_t *mod;
    void *val;
    
    // First, try to find the symbol in one of the loaded modules.
    // This enables modules to override kernel symbol definitions.
    
    kernel_mutex_lock(&kmod_list_mutex);

    for(mod = modules_head.next; mod != NULL; mod = mod->next)
    {
        if(!(mod->state & MODULE_STATE_LOADED))
        {
            continue;
        }
        
        if((hitem = hashtab_lookup(mod->symbols, name)) != NULL)
        {
            val = hitem->val;
            kernel_mutex_unlock(&kmod_list_mutex);
            
            return val;
        }
    }
    
    kernel_mutex_unlock(&kmod_list_mutex);

    // The symbol is not defined in any loaded module, so try to find it
    // in the symbol table.
    
    if((hitem = hashtab_lookup(ksymtab, name)) != NULL)
    {
        return hitem->val;
    }
    
    return NULL;
}

