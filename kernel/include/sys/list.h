/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: list.h
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
 *  \file list.h
 *
 *  General doubly linked list implementation.
 */

#ifndef LIST_H
#define LIST_H

struct list_item_t
{
    void *val;
    struct list_item_t *next, *prev;
};

struct list_t
{
    struct list_item_t *head, *tail;
    int count;
};


#define __PASTEL(p, f)          p ## _ ## f
#define __EVALL(p, f)           __PASTEL(p, f)

#ifdef USE_LIST_FUNC_PREFIX
#define __FNL(f)                __EVALL(USE_LIST_FUNC_PREFIX, f)
#else
#define __FNL(f)                __EVALL(_, f)
#endif

#define list_create             __FNL(list_create)
#define list_free               __FNL(list_free)
#define list_add                __FNL(list_add)
#define list_lookup             __FNL(list_lookup)
#define list_remove             __FNL(list_remove)
#define list_free_objs          __FNL(list_free_objs)


struct list_t *list_create(void);
void list_free(struct list_t *list);
void list_add(struct list_t *list, void *val);
struct list_item_t *list_lookup(struct list_t *list, void *val);
void list_remove(struct list_t *list, void *val);

/*
 * We only need this function in the dynamic linker.
 */
#ifdef DEFINE_LIST_FREE_OBJS
void list_free_objs(struct list_t *list);
#endif      /* DEFINE_LIST_FREE_OBJS */

#endif      /* LIST_H */
