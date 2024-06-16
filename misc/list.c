#include <string.h>
#include <stdlib.h>
#include <kernel/elf.h>
//#include "list.h"
#include <sys/list.h>


#ifdef KERNEL
#include <mm/kheap.h>
#undef malloc
#undef free
#define malloc  kmalloc
#define free    kfree
#endif


#ifdef USE_ALTERNATE_MALLOC
#undef malloc
#define malloc          USE_ALTERNATE_MALLOC
#endif

#ifdef USE_ALTERNATE_FREE
#undef free
#define free            USE_ALTERNATE_FREE
#endif


struct list_t *list_create(void)
{
    struct list_t *list;
    
    if(!(list = malloc(sizeof(struct list_t))))
    {
        return NULL;
    }
    
    memset(list, 0, sizeof(struct list_t));

    return list;
}


void list_free(struct list_t *list)
{
    struct list_item_t *item, *next;
    
    if(!list)
    {
        return;
    }
    
    item = list->head;
    
    while(item)
    {
        next = item->next;
        free(item);
        item = next;
    }
    
    free(list);
}


/*
 * We only need this function in the dynamic linker.
 */
#ifdef DEFINE_LIST_FREE_OBJS

//#include "../ld/ld.h"

void list_free_objs(struct list_t *list)
{
    struct list_item_t *item, *next;
    
    if(!list)
    {
        return;
    }
    
    item = list->head;
    
    while(item)
    {
        next = item->next;
        object_free((struct elf_obj_t *)item->val);
        free(item);
        item = next;
    }
    
    free(list);
}

#endif      /* DEFINE_LIST_FREE_OBJS */

void list_add(struct list_t *list, void *val)
{
    struct list_item_t *item;
    
    if(!list)
    {
        return;
    }

    if(!(item = malloc(sizeof(struct list_item_t))))
    {
        return;
    }
    
    memset(item, 0, sizeof(struct list_item_t));
    item->val = val;
    
    if(list->count == 0)
    {
        list->head = item;
        list->tail = item;
        list->count++;
        return;
    }
    
    list->tail->next = item;
    item->prev = list->tail;
    list->tail = item;
    list->count++;
}


struct list_item_t *list_lookup(struct list_t *list, void *val)
{
    struct list_item_t *item;

    if(!list)
    {
        return NULL;
    }

    for(item = list->head; item != NULL; item = item->next)
    {
        if(item->val == val)
        {
            return item;
        }
    }
    
    return NULL;
}


void list_remove(struct list_t *list, void *val)
{
    struct list_item_t *item;

    if(!list)
    {
        return;
    }

    for(item = list->head; item != NULL; item = item->next)
    {
        if(item->val == val)
        {
            if(list->head == item)
            {
                list->head = item->next;
            }

            if(list->tail == item)
            {
                list->tail = item->prev;
            }
            
            if(item->prev)
            {
                item->prev->next = item->next;
            }
            
            if(item->next)
            {
                item->next->prev = item->prev;
            }
            
            list->count--;
            
            return;
        }
    }
}

