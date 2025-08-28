/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2024 (c)
 * 
 *    file: nettimer.h
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
 *  \file nettimer.h
 *
 *  Functions and macros for working with network timers.
 */

#ifndef NET_TIMER_H
#define NET_TIMER_H

struct nettimer_t
{
    int refs;
    int cancelled;
    unsigned long long expires;
    void (*handler)(void *);
    void *arg;
    struct nettimer_t *next;
};


void nettimer_init(void);
struct nettimer_t *nettimer_add(uint32_t expire, void (*handler)(void *), void *arg);
void nettimer_oneshot(uint32_t expire, void (*handler)(void *), void *arg);
void nettimer_release(struct nettimer_t *t);

#endif      /* NET_TIMER_H */
