/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: syscall-defs.h
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
 *  \file syscall-defs.h
 *
 *  Syscall-related structure and macro definitions.
 */

#ifndef __SYSCALL_DEFS__
#define __SYSCALL_DEFS__

#ifdef KERNEL

// this is for syscalls that take 6 or more args
struct syscall_args
{
    uintptr_t args[8];
};

#define COPY_SYSCALL_ARGS(args, __args)         \
    if(!__args)                                 \
        return -EINVAL;                         \
    if((res = copy_from_user(&args, __args,     \
            sizeof(struct syscall_args))) != 0) \
        return res;

#define COPY_SYSCALL6_ARGS(args, __args)    COPY_SYSCALL_ARGS(args, __args)
#define COPY_SYSCALL7_ARGS(args, __args)    COPY_SYSCALL_ARGS(args, __args)


#define ERESTARTSYS                 512


#define SYSCALL_EFAULT(addr)                                \
        add_task_segv_signal(this_core->cur_task,           \
                             SEGV_MAPERR, (void *)addr);    \
        return -EFAULT;


#ifdef __x86_64__

# define GET_SYSCALL_NUMBER(r)       ((r)->rax)
# define SET_SYSCALL_NUMBER(r, i)    (r)->rax = i
# define GET_SYSCALL_RESULT(r)       ((r)->rax)
# define SET_SYSCALL_RESULT(r, i)    (r)->rax = i
# define GET_SYSCALL_ARG1(r)         ((r)->rdi)
# define GET_SYSCALL_ARG2(r)         ((r)->rsi)
# define GET_SYSCALL_ARG3(r)         ((r)->rdx)
# define GET_SYSCALL_ARG4(r)         ((r)->r10)
# define GET_SYSCALL_ARG5(r)         ((r)->r8)

#else       /* !__x86_64__ */

# define GET_SYSCALL_NUMBER(r)       ((r)->eax)
# define SET_SYSCALL_NUMBER(r, i)    (r)->eax = i
# define GET_SYSCALL_RESULT(r)       ((r)->eax)
# define SET_SYSCALL_RESULT(r, i)    (r)->eax = i
# define GET_SYSCALL_ARG1(r)         ((r)->ebx)
# define GET_SYSCALL_ARG2(r)         ((r)->ecx)
# define GET_SYSCALL_ARG3(r)         ((r)->edx)
# define GET_SYSCALL_ARG4(r)         ((r)->edi)
# define GET_SYSCALL_ARG5(r)         ((r)->esi)

#endif      /* !__x86_64__ */

#endif      /* KERNEL */

#endif      /* __SYSCALL_DEFS__ */
