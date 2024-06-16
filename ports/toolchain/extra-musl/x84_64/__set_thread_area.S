/* Copyright 2011-2012 Nicholas J. Kain, licensed under standard MIT license */
.text
.global __set_thread_area
.hidden __set_thread_area
.type __set_thread_area,@function

__set_thread_area:

#ifdef __laylaos__

    /*
     * This syscall (SYS_set_thread_area) expects a pointer to a structure like this:
     *
     * struct user_desc
     * {
     *     unsigned int    entry_number;            -1
     *     unsigned long   base_addr;               %rdi
     *     unsigned int    limit;                   0x1000
     *     unsigned int    seg_32bit:1;             1
     *     unsigned int    contents:2;              0
     *     unsigned int    read_exec_only:1;        0
     *     unsigned int    limit_in_pages:1;        1
     *     unsigned int    seg_not_present:1;       0
     *     unsigned int    useable:1;               1
     *     unsigned int    empty:25;                0
     * };
     */
	movq $0x100000000051, %rax
	pushq %rax
	pushq %rdi
	pushq $-1
	movq %rsp, %rdi
	movq $243, %rax
	syscall
	addq $24, %rsp
	ret

#else       /* !__laylaos__ */

	mov %rdi,%rsi           /* shift for syscall */
	movl $0x1002,%edi       /* SET_FS register */
	movl $158,%eax          /* set fs segment to */
	syscall                 /* arch_prctl(SET_FS, arg)*/
	ret

#endif      /* !__laylaos__ */
