.global vfork
.type vfork,@function
vfork:
	pop %rdx
#ifdef __laylaos__
	mov $190,%eax
#else       /* !__laylaos__ */
	mov $58,%eax
#endif      /* !__laylaos__ */
	syscall
	push %rdx
	mov %rax,%rdi
	.hidden __syscall_ret
	jmp __syscall_ret
