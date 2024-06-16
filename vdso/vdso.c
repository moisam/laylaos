#include <limits.h>
#include <stdint.h>
#include <time.h>
#include <syscall.h>
#include "vdso.h"

//uintptr_t vdso_base = 0;


int __vdso_clock_gettime(uintptr_t vdso_base, clockid_t clock_id, struct timespec *tp)
{
    unsigned long res;
    struct timespec *vdso_monotonic;

    /*
    if(vdso_base == 0)
    {
        __asm__ __volatile__ ("syscall" : 
                              "=a"(res) :
                              "a"(__NR_vdso_shared_page), "D"(&vdso_base) :
                              "rcx", "r9", "r11", "memory");

        if(res < 0)
        {
            goto do_syscall;
        }
    }
    */
    vdso_base += VDSO_STATIC_CODE_SIZE;

    vdso_monotonic = (struct timespec *)(vdso_base + VDSO_OFFSET_CLOCK_GETTIME);

    /*
     * NOTE: CLOCK_REALTIME: Its time represents seconds and nanoseconds
     *       since the Epoch.  When its time is changed, timers for a
     *       relative interval are unaffected, but timers for an absolute
     *       point in time are affected.
     */
    if(clock_id == CLOCK_REALTIME)
    {
        time_t startup_time = *(time_t *)(vdso_base + VDSO_OFFSET_STARTUP_TIME);

        tp->tv_sec  = vdso_monotonic->tv_sec + startup_time;
        tp->tv_nsec = vdso_monotonic->tv_nsec;
        return 0;
    }
    else if(clock_id == CLOCK_MONOTONIC)
    {
        tp->tv_sec  = vdso_monotonic->tv_sec;
        tp->tv_nsec = vdso_monotonic->tv_nsec;
        return 0;
    }

//do_syscall:

    __asm__ __volatile__ ("syscall" : 
                          "=a"(res) :
                          "a"(__NR_clock_gettime), "D"(clock_id), "S"(tp) :
                          "rcx", "r9", "r11", "memory");
    return res;
}

