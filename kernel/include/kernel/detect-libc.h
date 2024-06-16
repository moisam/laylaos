

// musl libc does not defined a macro like __MUSL__, see their reasoning here:
//    https://wiki.musl-libc.org/faq
//
// This method of detecting musl libc is based on the Stack Overflow answer at:
//    https://stackoverflow.com/questions/58177815/how-to-actually-detect-musl-libc

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
    #include <features.h>
    #ifndef __USE_GNU
        #define __MUSL__
    #endif
    #undef _GNU_SOURCE /* don't contaminate other includes unnecessarily */
#else
    #include <features.h>
    #ifndef __USE_GNU
        #define __MUSL__
    #endif
#endif

#ifdef _NEWLIB_VERSION
    #define __NEWLIB__
#endif

