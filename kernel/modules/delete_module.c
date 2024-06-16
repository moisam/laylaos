/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: delete_module.c
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
 *  \file delete_module.c
 *
 *  Unload kernel modules.
 */

#include <errno.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/modules.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <mm/kheap.h>


/*
 * Handler for syscall delete_module().
 */
int syscall_delete_module(char *__name, unsigned int flags)
{
    UNUSED(flags);
    
    struct kmodule_t *mod, *me = NULL;
    char *name = NULL, *s;
    size_t namelen = 0;
    struct task_t *ct = cur_task;
    
    if(!suser(ct))
    {
        return -EPERM;
    }

    // copy arg
    if(copy_str_from_user(__name, &name, &namelen) != 0)
    {
        return -EFAULT;
    }
    
    if(!*name)
    {
        kfree(name);
        return -EINVAL;
    }
    
    kernel_mutex_lock(&kmod_list_mutex);

    /*
     * Check to see if other modules depend on this module.
     *
     * TODO: Perform a proper check. We currently simply search each module's
     *       dependency list (a char string) for this module's name, and take
     *       this as a sign whether the second module depends on this one. We
     *       should maintain reference counts and use that instead.
     * TODO: Check if this module depends on other modules, and remove those
     *       if they are not used.
     * TODO: Check if any tasks are using this module's functions, and fail
     *       to remove the module if so.
     *
     * See: https://man7.org/linux/man-pages/man2/delete_module.2.html
     */
    for(mod = modules_head.next; mod != NULL; mod = mod->next)
    {
        if(me == NULL && strcmp(mod->modinfo.name, name) == 0)
        {
            me = mod;
            continue;
        }
        
        if(!mod->modinfo.deps || mod->modinfo.deps[0] == '\0')
        {
            continue;
        }
        
        if((s = strstr(mod->modinfo.deps, name)) != NULL)
        {
            char c = s[namelen];
            
            if(c == '\0' || c == ',')
            {
                kernel_mutex_unlock(&kmod_list_mutex);
                kfree(name);
                return -EWOULDBLOCK;
            }
        }
    }
    
    if(!me || !me->cleanup)
    {
        kernel_mutex_unlock(&kmod_list_mutex);
        return -ENOENT;
    }

    for(mod = modules_head.next; mod != NULL; mod = mod->next)
    {
        if(mod->next == me)
        {
            break;
        }
    }
    
    if(!mod)
    {
        modules_head.next = me->next;
    }
    else
    {
        mod->next = me->next;
    }

    kernel_mutex_unlock(&kmod_list_mutex);
    
    me->cleanup();
    me->state = MODULE_STATE_UNLOADED;
    vmmngr_free_pages(me->mempos, me->memsz);
    me->mempos = 0;
    me->memsz = 0;
    free_mod_obj(me);

    return 0;
}

