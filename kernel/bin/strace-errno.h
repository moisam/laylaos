/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: strace-errno.h
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
 *  \file strace-errno.h
 *
 *  Errno macros and declarations for the strace utility.
 */

#define __LINUX_ERRNO_EXTENSIONS__
#define __CYGWIN__
#include <errno.h>
#undef __LINUX_ERRNO_EXTENSIONS__
#undef __CYGWIN__


#ifdef DEFINE_ERRNO_NAMES

#define NEQSTR(n)           [n] = #n

const char *errno_names[] =
{
    NEQSTR(EPERM),
    NEQSTR(ENOENT),
    NEQSTR(ESRCH),
    NEQSTR(EINTR),
    NEQSTR(EIO),
    NEQSTR(ENXIO),
    NEQSTR(E2BIG),
    NEQSTR(ENOEXEC),
    NEQSTR(EBADF),
    NEQSTR(ECHILD),
    NEQSTR(EAGAIN),
    NEQSTR(ENOMEM),
    NEQSTR(EACCES),
    NEQSTR(EFAULT),
    NEQSTR(ENOTBLK),
    NEQSTR(EBUSY),
    NEQSTR(EEXIST),
    NEQSTR(EXDEV),
    NEQSTR(ENODEV),
    NEQSTR(ENOTDIR),
    NEQSTR(EISDIR),
    NEQSTR(EINVAL),
    NEQSTR(ENFILE),
    NEQSTR(EMFILE),
    NEQSTR(ENOTTY),
    NEQSTR(ETXTBSY),
    NEQSTR(EFBIG),
    NEQSTR(ENOSPC),
    NEQSTR(ESPIPE),
    NEQSTR(EROFS),
    NEQSTR(EMLINK),
    NEQSTR(EPIPE),
    NEQSTR(EDOM),
    NEQSTR(ERANGE),
    NEQSTR(EDEADLK),
    NEQSTR(ENAMETOOLONG),
    NEQSTR(ENOLCK),
    NEQSTR(ENOSYS),
    NEQSTR(ENOTEMPTY),
    NEQSTR(ELOOP),
    NEQSTR(ENOMSG),
    NEQSTR(EIDRM),
    NEQSTR(ECHRNG),
    NEQSTR(EL2NSYNC),
    NEQSTR(EL3HLT),
    NEQSTR(EL3RST),
    NEQSTR(ELNRNG),
    NEQSTR(EUNATCH),
    NEQSTR(ENOCSI),
    NEQSTR(EL2HLT),
    NEQSTR(EBADE),
    NEQSTR(EBADR),
    NEQSTR(EXFULL),
    NEQSTR(ENOANO),
    NEQSTR(EBADRQC),
    NEQSTR(EBADSLT),
    //NEQSTR(EDEADLOCK),
    NEQSTR(EBFONT),
    NEQSTR(ENOSTR),
    NEQSTR(ENODATA),
    NEQSTR(ETIME),
    NEQSTR(ENOSR),
    NEQSTR(ENONET),
    NEQSTR(ENOPKG),
    NEQSTR(EREMOTE),
    NEQSTR(ENOLINK),
    NEQSTR(EADV),
    NEQSTR(ESRMNT),
    NEQSTR(ECOMM),
    NEQSTR(EPROTO),
    NEQSTR(EMULTIHOP),
    NEQSTR(EDOTDOT),
    NEQSTR(EBADMSG),
    NEQSTR(EOVERFLOW),
    NEQSTR(ENOTUNIQ),
    NEQSTR(EBADFD),
    NEQSTR(EREMCHG),
    NEQSTR(ELIBACC),
    NEQSTR(ELIBBAD),
    NEQSTR(ELIBSCN),
    NEQSTR(ELIBMAX),
    NEQSTR(ELIBEXEC),
    NEQSTR(EILSEQ),
    NEQSTR(ERESTART),
    NEQSTR(ESTRPIPE),
    NEQSTR(EUSERS),
    NEQSTR(ENOTSOCK),
    NEQSTR(EDESTADDRREQ),
    NEQSTR(EMSGSIZE),
    NEQSTR(EPROTOTYPE),
    NEQSTR(ENOPROTOOPT),
    NEQSTR(EPROTONOSUPPORT),
    NEQSTR(ESOCKTNOSUPPORT),
    NEQSTR(EOPNOTSUPP),
    //NEQSTR(ENOTSUP),
    NEQSTR(EPFNOSUPPORT),
    NEQSTR(EAFNOSUPPORT),
    NEQSTR(EADDRINUSE),
    NEQSTR(EADDRNOTAVAIL),
    NEQSTR(ENETDOWN),
    NEQSTR(ENETUNREACH),
    NEQSTR(ENETRESET),
    NEQSTR(ECONNABORTED),
    NEQSTR(ECONNRESET),
    NEQSTR(ENOBUFS),
    NEQSTR(EISCONN),
    NEQSTR(ENOTCONN),
    NEQSTR(ESHUTDOWN),
    NEQSTR(ETOOMANYREFS),
    NEQSTR(ETIMEDOUT),
    NEQSTR(ECONNREFUSED),
    NEQSTR(EHOSTDOWN),
    NEQSTR(EHOSTUNREACH),
    NEQSTR(EALREADY),
    NEQSTR(EINPROGRESS),
    NEQSTR(ESTALE),
    NEQSTR(EUCLEAN),
    NEQSTR(ENOTNAM),
    NEQSTR(ENAVAIL),
    NEQSTR(EISNAM),
    NEQSTR(EREMOTEIO),
    NEQSTR(EDQUOT),
    NEQSTR(ENOMEDIUM),
    NEQSTR(EMEDIUMTYPE),
    NEQSTR(ECANCELED),
    NEQSTR(ENOKEY),
    NEQSTR(EKEYEXPIRED),
    NEQSTR(EKEYREVOKED),
    NEQSTR(EKEYREJECTED),
    NEQSTR(EOWNERDEAD),
    NEQSTR(ENOTRECOVERABLE),
    NEQSTR(ERFKILL),
    NEQSTR(EHWPOISON),

    /*
    NEQSTR(ELBIN),
    NEQSTR(EFTYPE),
    NEQSTR(ENMFILE),
    NEQSTR(EPROCLIM),
    NEQSTR(ENOSHARE),
    NEQSTR(ECASECLASH),
    */
};

int errno_count = (sizeof(errno_names) / sizeof(errno_names[0]));

#else       /* !DEFINE_ERRNO_NAMES */

extern const char *errno_names[];
extern int errno_count;

#endif      /* DEFINE_ERRNO_NAMES */

//#define ERRNO_COUNT         (sizeof(errno_names) / sizeof(errno_names[0]))

