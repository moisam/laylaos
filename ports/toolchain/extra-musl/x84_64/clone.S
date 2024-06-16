.text
.global __clone
.hidden __clone
.type   __clone,@function
__clone:

#ifdef __laylaos__

    // insert arguments onto the child's stack
	mov %rsi, %rax          // child stack pointer

    // the 7th arg is pushed on the stack, retrieve it and overwrite %r8
    // (the 5th arg) as we are not using it here
	mov 8(%rsp), %r8
	mov %r8, -8(%rax)       // child tid

	mov %r9, -16(%rax)      // child tls
	mov %rcx, -24(%rax)     // child args
	mov %rdi, -32(%rax)     // start or start_c11
	
	mov %rdx, %rdi          // flags
	sub $32, %rsi           // stack
	mov $120, %rax          // SYS_clone
	syscall

    //xchg %bx, %bx
	test %eax, %eax
	
	// parent or error
	jnz 1f
	
	// child
	mov 16(%rsp), %rdi
	
	// set the TLS if the 6th arg (tls) was non-NULL
	test %rdi, %rdi
	jz 3f
	call __set_thread_area

3:
	// gettid
	mov $224, %rax          // SYS_gettid
	syscall
	
	// store the new thread's tid if the 7th arg (child tid) was non-NULL
	mov 24(%rsp), %rdi
	test %rdi, %rdi
	jz 2f
	movl %eax, (%rdi)

2:
	xor %ebp, %ebp
	pop %r9                 // start or start_c11
	pop %rdi                // child args

	add $16, %rsp
	call *%r9

	mov %eax, %edi
	mov $1, %rax
	syscall
	hlt

1:	ret

#else       /* !__laylaos__ */

	xor %eax,%eax
	mov $56,%al
	mov %rdi,%r11
	mov %rdx,%rdi
	mov %r8,%rdx
	mov %r9,%r8
	mov 8(%rsp),%r10
	mov %r11,%r9
	and $-16,%rsi
	sub $8,%rsi
	mov %rcx,(%rsi)
	syscall
	test %eax,%eax
	jnz 1f
	xor %ebp,%ebp
	pop %rdi
	call *%r9
	mov %eax,%edi
	xor %eax,%eax
	mov $60,%al
	syscall
	hlt
1:	ret

#endif      /* !__laylaos__ */
