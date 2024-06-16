	nop
.global __restore_rt
.hidden __restore_rt
.type __restore_rt,@function
__restore_rt:
#ifdef __laylaos__
	movq $119, %rax
	movq %rsp, %rdi
#else
	mov $15, %rax
#endif
	syscall
.size __restore_rt,.-__restore_rt
