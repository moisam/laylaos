/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: signal_funcs.c
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
 *  \file signal_funcs.c
 *
 *  Inlined functions for the kernel signal dispatcher use.
 */

#include <kernel/detect-libc.h>
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <kernel/user.h>

#undef INLINE
#define INLINE      static inline __attribute__((always_inline))


#ifdef __MUSL__

#define SST_SIZE (_NSIG/8/sizeof(long))

INLINE void copy_sigset(sigset_t *dest, sigset_t *src)
{
    unsigned long i = 0, *d = (void *)dest, *s = (void *)src;

    for( ; i < SST_SIZE; i++)
    {
        d[i] = s[i];
    }
}

INLINE int copy_sigset_to_user(sigset_t *dest, sigset_t *src)
{
    unsigned long i = 0, *d = (void *)dest, *s = (void *)src;

    for( ; i < SST_SIZE; i++)
    {
        COPY_VAL_TO_USER(&d[i], &s[i]);
    }

    return 0;
}

INLINE int copy_sigset_from_user(sigset_t *dest, sigset_t *src)
{
    unsigned long i = 0, *d = (void *)dest, *s = (void *)src;

    for( ; i < SST_SIZE; i++)
    {
        COPY_VAL_FROM_USER(&d[i], &s[i]);
    }

    return 0;
}

#elif defined(__NEWLIB__)

// sigset_t is unsigned long on newlib

INLINE void copy_sigset(sigset_t *dest, sigset_t *src)
{
    *dest = *src;
}

INLINE int copy_sigset_to_user(sigset_t *dest, sigset_t *src)
{
    COPY_VAL_TO_USER(dest, src);
    return 0;
}

INLINE int copy_sigset_from_user(sigset_t *dest, sigset_t *src)
{
    COPY_VAL_FROM_USER(dest, src);
    return 0;
}

#else

// fallbacks for other libc implementations

INLINE void copy_sigset(sigset_t *dest, sigset_t *src)
{
    A_memcpy(dest, src, sizeof(sigset_t));
}

INLINE int copy_sigset_to_user(sigset_t *dest, sigset_t *src)
{
    return copy_to_user(dest, src, sizeof(sigset_t));
}

INLINE int copy_sigset_from_user(sigset_t *dest, sigset_t *src)
{
    return copy_from_user(dest, src, sizeof(sigset_t));
}

#endif
