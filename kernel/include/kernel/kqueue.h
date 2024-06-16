/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: kqueue.h
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
 *  \file kqueue.h
 *
 *  Functions and macros to work with tty and keyboard buffers.
 */

#ifndef __KQUEUE_H__
#define __KQUEUE_H__

#include <kernel/laylaos.h>

/**
 * @struct kqueue_t
 * @brief The kqueue_t structure.
 *
 * A structure to represent a tty or keyboard buffer.
 */
struct kqueue_t
{
    void *buf;      /**< pointer to the queue's buffer */

    int head,       /**< index of queue head */
        tail;       /**< index of queue tail */
    
    int extra;      /**< implementation-specific (mainly used by ttys) */
};


#ifdef KQUEUE_DEFINE_INLINES

#ifndef KQUEUE_SIZE
#error You must define KQUEUE_SIZE to your queue size
#endif

/*
 * TTY buffer functions.
 */

static inline void __buf_init(struct kqueue_t *q, void *buf)
{
    if(!q || (KQUEUE_SIZE % 2))
    {
        kpanic("Initializing queue with invalid value(s)");
    }
    
    q->head = 0;
    q->tail = 0;
    q->buf = buf;
    q->extra = 0;
}

static inline int __buf_used(struct kqueue_t *q)
{
    return (q->tail + KQUEUE_SIZE - q->head) & (KQUEUE_SIZE - 1);
}

static inline int __buf_is_empty(struct kqueue_t *q)
{
    return (q->head == q->tail);
}

static inline int __buf_is_full(struct kqueue_t *q)
{
    return (((q->tail + 1) & (KQUEUE_SIZE - 1)) == q->head);
}

static inline int __buf_has_space_for(struct kqueue_t *q, int n)
{
    return (KQUEUE_SIZE - __buf_used(q)) >= n;
}

static inline void __buf_clear(struct kqueue_t *q)
{
    q->head = 0;
    q->tail = 0;
    q->extra = 0;
}

#define kbdbuf_init             __buf_init
#define kbdbuf_used             __buf_used
#define kbdbuf_is_empty         __buf_is_empty
#define kbdbuf_is_full          __buf_is_full
#define kbdbuf_has_space_for    __buf_has_space_for
#define kbdbuf_clear            __buf_clear

#define ttybuf_init             __buf_init
#define ttybuf_used             __buf_used
#define ttybuf_is_empty         __buf_is_empty
#define ttybuf_is_full          __buf_is_full
#define ttybuf_has_space_for    __buf_has_space_for
#define ttybuf_clear            __buf_clear

#define __buf_enqueue(type, q, v)                   \
{                                                   \
    if(__buf_is_full(q))                            \
    {                                               \
        printk("Tried to enqueue a full queue\n");  \
        return;                                     \
    }                                               \
    ((type *)q->buf)[q->tail] = v;                  \
    q->tail = (q->tail + 1) & (KQUEUE_SIZE - 1);    \
}

static inline void kbdbuf_enqueue(struct kqueue_t *q, uint16_t v)
{
    __buf_enqueue(uint16_t, q, v);
}

static inline void ttybuf_enqueue(struct kqueue_t *q, char v)
{
    __buf_enqueue(char, q, v);
}

#define __buf_dequeue(type, q)                          \
{                                                       \
    if(__buf_is_empty(q))                               \
    {                                                   \
        printk("Tried to dequeue an empty queue\n");    \
        return 0;                                       \
    }                                                   \
    type res = ((type *)q->buf)[q->head];               \
    q->head = (q->head + 1) & (KQUEUE_SIZE - 1);        \
    return res;                                         \
}

static inline uint16_t kbdbuf_dequeue(struct kqueue_t *q)
{
    __buf_dequeue(uint16_t, q);
}

static inline char ttybuf_dequeue(struct kqueue_t *q)
{
    __buf_dequeue(char, q);
}

#endif      /* KQUEUE_DEFINE_INLINES */

#endif      /* __KQUEUE_H__ */
