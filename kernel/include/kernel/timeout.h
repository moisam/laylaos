#ifndef __KERNEL_TIMEOUT__
#define __KERNEL_TIMEOUT__

void	timeout (void (*func)(void *), void *arg, int ticks);
void	untimeout (void (*func)(void *), void *arg);

#endif      /* __KERNEL_TIMEOUT__ */
