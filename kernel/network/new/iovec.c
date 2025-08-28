/*
 * iovec.c
 *
 * Scatter/gather utility routines
 *
 * Copyright (C) 2022, 2023, 2024 Mohammed Isam [mohammed_isam1984@yahoo.com].
 * Copyright (C) 2002 Michael Ringgaard. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.  
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.  
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */ 

/**
 *  \file iovec.c
 *
 *  Helper scatter/gather input & output functions.
 *
 * Code in this file has been largely modified to work with LaylaOS.
 * Formatting was also changed to fit with the rest of LaylaOS's code base.
 */

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <mm/kheap.h>
#include <kernel/laylaos.h>
#include <kernel/user.h>
#include <kernel/task.h>

#undef INLINE
#define INLINE      static inline __attribute__((always_inline))


INLINE size_t get_iovec_size(struct iovec *iov, int iovlen)
{
    size_t size = 0;
    
    while(iovlen > 0)
    {
        size += iov->iov_len;
        iov++;
        iovlen--;
    }

    return size;
}


INLINE struct iovec *dup_iovec(struct iovec *iov, int iovlen)
{
    struct iovec *newiov;

    newiov = kmalloc(iovlen * sizeof(struct iovec));
  
    if(!newiov)
    {
        return NULL;
    }
    
    A_memcpy(newiov, iov, iovlen * sizeof(struct iovec));

    return newiov;
}


INLINE size_t read_iovec(struct iovec *iov, int iovlen, 
                         uint8_t *buf, size_t count, int kernel)
{
    size_t read = 0;
    size_t len;

    while(count > 0 && iovlen > 0)
    {
        if(iov->iov_len > 0)
        {
            len = iov->iov_len;
      
            if(count < iov->iov_len)
            {
                len = count;
            }

            if(kernel)
            {
                A_memcpy(buf, iov->iov_base, len);
            }
            else
            {
                copy_from_user(buf, iov->iov_base, len);
            }
            
            read += len;

    		iov->iov_base = (void *)((uintptr_t)iov->iov_base + len);
            iov->iov_len -= len;

            buf += len;
            count -= len;
        }

        iov++;
        iovlen--;
    }

    return read;
}


INLINE size_t write_iovec(struct iovec *iov, int iovlen, 
                          uint8_t *buf, size_t count, int kernel)
{
    size_t written = 0;
    size_t len;

    while(count > 0 && iovlen > 0)
    {
        if(iov->iov_len > 0)
        {
            len = iov->iov_len;
      
            if(count < iov->iov_len)
            {
                len = count;
            }

            if(kernel)
            {
                A_memcpy(iov->iov_base, buf, len);
            }
            else
            {
                copy_to_user(iov->iov_base, buf, len);
            }

            written += len;

    		iov->iov_base = (void *)((uintptr_t)iov->iov_base + len);
            iov->iov_len -= len;

            buf += len;
            count -= len;
        }

        iov++;
        iovlen--;
    }

    return written;
}

