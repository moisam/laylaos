#ifndef KERNEL_VDSO_H
#define KERNEL_VDSO_H

#define VDSO_STATIC_CODE_SIZE           (2 * PAGE_SIZE)

#define VDSO_OFFSET_STARTUP_TIME        0
#define VDSO_OFFSET_CLOCK_GETTIME       16

#ifdef KERNEL

extern struct timespec *vdso_monotonic;
extern time_t *vdso_startup_time;

int vdso_stub_init(virtual_addr start, virtual_addr end);
int map_vdso(virtual_addr *resaddr);

#endif      /* KERNEL */

#endif      /* KERNEL_VDSO_H */
