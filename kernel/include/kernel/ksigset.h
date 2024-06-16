/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: ksigset.h
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
 *  \file ksigset.h
 *
 *  Helper functions to work with signal sets within the kernel.
 */

#ifndef __KERNEL_SIGSET_H__
#define __KERNEL_SIGSET_H__


/*
 * The following functions are inlined versions of musl sigset functions.
 * The original is in musl source tree under src/signal/.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <signal.h>
#include <string.h>

#define SST_SIZE (_NSIG/8/sizeof(long))

static inline int ksigisemptyset(const sigset_t *set)
{
	for (size_t i=0; i<_NSIG/8/sizeof *set->__bits; i++)
		if (set->__bits[i]) return 0;
	return 1;
}

static inline void ksigandset(sigset_t *dest, const sigset_t *left, const sigset_t *right)
{
	unsigned long i = 0, *d = (void*) dest, *l = (void*) left, *r = (void*) right;
	for(; i < SST_SIZE; i++) d[i] = l[i] & r[i];
}

static inline void ksigorset(sigset_t *dest, const sigset_t *left, const sigset_t *right)
{
	unsigned long i = 0, *d = (void*) dest, *l = (void*) left, *r = (void*) right;
	for(; i < SST_SIZE; i++) d[i] = l[i] | r[i];
}

static inline void ksignotset(sigset_t *dest, const sigset_t *src)
{
	unsigned long i = 0, *d = (void*) dest, *s = (void*) src;
	for(; i < SST_SIZE; i++) d[i] = ~(s[i]);
}

static inline void ksigemptyset(sigset_t *set)
{
	set->__bits[0] = 0;
	if (sizeof(long)==4 || _NSIG > 65) set->__bits[1] = 0;
	if (sizeof(long)==4 && _NSIG > 65) {
		set->__bits[2] = 0;
		set->__bits[3] = 0;
	}
}

static inline void ksigfillset(sigset_t *set)
{
#if ULONG_MAX == 0xffffffff
	set->__bits[0] = 0x7ffffffful;
	set->__bits[1] = 0xfffffffcul;
	if (_NSIG > 65) {
		set->__bits[2] = 0xfffffffful;
		set->__bits[3] = 0xfffffffful;
	}
#else
	set->__bits[0] = 0xfffffffc7ffffffful;
	if (_NSIG > 65) set->__bits[1] = 0xfffffffffffffffful;
#endif
}

static inline void ksigaddset(sigset_t *set, int sig)
{
	unsigned s = sig-1;
	if (s >= _NSIG-1 || sig-32U < 3) {
		return;
	}
	set->__bits[s/8/sizeof *set->__bits] |= 1UL<<(s&(8*sizeof *set->__bits-1));
}

static inline void ksigdelset(sigset_t *set, int sig)
{
	unsigned s = sig-1;
	if (s >= _NSIG-1 || sig-32U < 3) {
		return;
	}
	set->__bits[s/8/sizeof *set->__bits] &=~(1UL<<(s&(8*sizeof *set->__bits-1)));
}

static inline int ksigismember(const sigset_t *set, int sig)
{
	unsigned s = sig-1;
	if (s >= _NSIG-1) return 0;
	return !!(set->__bits[s/8/sizeof *set->__bits] & 1UL<<(s&(8*sizeof *set->__bits-1)));
}


/*
 * Uncomment the following code if using newlib. The implementation
 * of sigset_t is different in musl, for which we use the code above.
 *
 * TODO: find a better way to implement this switch.
 */

#if 0

static inline int ksigisemptyset(const sigset_t *set)
{
    return (*set == (sigset_t)0);
    
    /*
    size_t i;

    for(i = 0; i < NSIG; i++)
    {
        if(*set & (1 << i))
        {
            return 0;
        }
    }

    return 1;
    */
}


static inline void ksigandset(sigset_t *set, const sigset_t *left, 
                                             const sigset_t *right)
{
    *set = (*left) & (*right);
}


static inline void ksignotset(sigset_t *dest, const sigset_t *src)
{
    *dest = ~(*src);
}


static inline void ksigorset(sigset_t *set, const sigset_t *left, 
                                            const sigset_t *right)
{
    *set = (*left) | (*right);
}


static inline void ksigemptyset(sigset_t *set)
{
    *set = (sigset_t) 0;
}


static inline void ksigfillset(sigset_t *set)
{
    *set = ~((sigset_t) 0);
}


static inline void ksigaddset(sigset_t *set, int signo)
{
    if(signo >= NSIG || signo <= 0)
    {
        return;
    }

    *set |= 1 << (signo - 1);
}


static inline void ksigdelset(sigset_t *set, int signo)
{
    if(signo >= NSIG || signo <= 0)
    {
        return;
    }

    *set &= ~(1 << (signo - 1));
}


static inline int ksigismember(const sigset_t *set, int signo)
{
    if(signo >= NSIG || signo <= 0)
    {
        return 0;
    }

    return (*set & (1 << (signo - 1))) ? 1 : 0;
}

#endif


#endif      /* __KERNEL_SIGSET_H__ */
