#ifdef __laylaos__

#include <stddef.h>
#include "syscall.h"

/*
 * Definition taken from Linux glibc source file include/linux/sysctl.h.
 */

struct __sysctl_args
{
	int *name;
	int nlen;
	void *oldval;
	size_t *oldlenp;
	void *newval;
	size_t newlen;
	unsigned long unused[4];
};


int sysctl(int *name, int nlen, void *oldval, size_t *oldlenp,
           void *newval, size_t newlen)
{
    struct __sysctl_args args =
    {
        .name = name,
        .nlen = nlen,
        .oldval = oldval,
        .oldlenp = oldlenp,
        .newval = newval,
        .newlen = newlen
    };

    return syscall(SYS_sysctl, &args);
}

#endif      /* __laylaos__ */
