#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <kernel/elf.h>
#include <sys/hash.h>

#ifdef KERNEL
#include <mm/kheap.h>
#include <kernel/laylaos.h>
#undef malloc
#undef free
#undef printf
#define malloc  kmalloc
#define free    kfree
#define printf  printk
#endif


#ifdef USE_ALTERNATE_MALLOC
#undef malloc
#define malloc          USE_ALTERNATE_MALLOC
#endif

#ifdef USE_ALTERNATE_FREE
#undef free
#define free            USE_ALTERNATE_FREE
#endif


struct hashtab_t *hashtab_create(int count,
                                 uint32_t (*hash_func)(struct hashtab_t *, void *),
                                 int (*compare_func)(void *, void *))
{
    struct hashtab_t *h;
    size_t sz = count * sizeof(struct hashtab_item_t);
    
    if(!(h = (struct hashtab_t *)malloc(sizeof(struct hashtab_t))))
    {
        return NULL;
    }
    
    memset(h, 0, sizeof(struct hashtab_t));
    h->count = count;
    h->hash_func = hash_func;
    h->compare_func = compare_func;
    
    if(!(h->items = (struct hashtab_item_t **)malloc(sz)))
    {
        free(h);
        return NULL;
    }

    memset(h->items, 0, sz);

    return h;
}


void hashtab_free(struct hashtab_t *h)
{
    int i;
    struct hashtab_item_t *hitem, *tmp;
    
    if(!h)
    {
        return;
    }
    
    for(i = 0; i < h->count; i++)
    {
        hitem = h->items[i];
        
        while(hitem)
        {
            tmp = hitem;
            hitem = hitem->next;
            
            /*
            free(tmp->key);
            free(tmp->val);
            */
            
            free(tmp);
        }
        
        h->items[i] = NULL;
    }
    
    free(h->items);
    free(h);
}


struct hashtab_item_t *hashtab_lookup(struct hashtab_t *h, void *key)
{
    struct hashtab_item_t *hitem;
    unsigned int i;

    if(!h)
    {
        return NULL;
    }
    
    i = h->hash_func(h, key);
    //i = calc_hash(h, (char *)key);
    hitem = h->items[i];
    
    while(hitem)
    {
        if(h->compare_func(hitem->key, key) == 0)
        //if(strcmp((char *)hitem->key, (char *)key) == 0)
        {
            return hitem;
        }
        
        hitem = hitem->next;
    }
    
    return NULL;
}


/* static inline */ struct hashtab_item_t *alloc_hitem(void *key, void *val)
{
    struct hashtab_item_t *hitem;
    
    if(!(hitem = (struct hashtab_item_t *)malloc(sizeof(struct hashtab_item_t))))
    {
        return NULL;
    }

    memset(hitem, 0, sizeof(struct hashtab_item_t));
    hitem->key = key;
    hitem->val = val;
    
    return hitem;
}


void hashtab_add(struct hashtab_t *h, void *key, void *val)
{
    struct hashtab_item_t *hitem, *prev;
    unsigned int i;

    if(!h || !key)
    {
        return;
    }
    
    i = h->hash_func(h, key);
    //i = calc_hash(h, (char *)key);
    hitem = h->items[i];
    
    if(!hitem)
    {
        if(!(hitem = alloc_hitem(key, val)))
        {
            printf("Failed to alloc hash item: insufficient memory\n");
            return;
        }
        
        h->items[i] = hitem;
        return;
    }
    
    prev = NULL;
    
    while(hitem)
    {
        if(h->compare_func(hitem->key, key) == 0)
        //if(strcmp((char *)hitem->key, (char *)key) == 0)
        {
            hitem->val = val;
            return;
        }
        
        prev = hitem;
        hitem = hitem->next;
    }
    
    if(!(hitem = alloc_hitem(key, val)))
    {
        printf("Failed to alloc hash item: insufficient memory\n");
        return;
    }
    
    prev->next = hitem;
}


void hashtab_add_hitem(struct hashtab_t *h, void *key, struct hashtab_item_t *new_hitem)
{
    struct hashtab_item_t *hitem, *prev;
    unsigned int i;

    if(!h || !key)
    {
        return;
    }
    
    i = h->hash_func(h, key);
    hitem = h->items[i];
    
    if(!hitem)
    {
        h->items[i] = new_hitem;
        return;
    }
    
    prev = NULL;
    
    while(hitem)
    {
        if(h->compare_func(hitem->key, key) == 0)
        {
            new_hitem->next = hitem->next;
            
            if(prev)
            {
                prev->next = new_hitem;
            }
            else
            {
                h->items[i] = new_hitem;
            }
            
            free(hitem);
            return;
        }
        
        prev = hitem;
        hitem = hitem->next;
    }
    
    prev->next = new_hitem;
}


void hashtab_remove(struct hashtab_t *h, void *key)
{
    struct hashtab_item_t *hitem, *prev;
    unsigned int i;

    if(!h || !key)
    {
        return;
    }
    
    i = h->hash_func(h, key);
    //i = calc_hash(h, (char *)key);
    hitem = h->items[i];
    
    if(!hitem)
    {
        return;
    }
    
    prev = NULL;
    
    while(hitem)
    {
        if(h->compare_func(hitem->key, key) == 0)
        //if(strcmp((char *)hitem->key, (char *)key) == 0)
        {
            if(prev)
            {
                prev->next = hitem->next;
            }
            else
            {
                h->items[i] = hitem->next;
            }

            free(hitem);
            return;
        }
        
        prev = hitem;
        hitem = hitem->next;
    }
}

