/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: strace-sig.h
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
 *  \file strace-sig.h
 *
 *  Signal macros and declarations for the strace utility.
 */

//#include <sys/siglist.h>
#include <signal.h>

#define SEQSTR(n)           [n] = #n

#define SIGNULL     0

/*
 * Define the array in only one file.
 */
#ifdef DEFINE_SIG_NAMES

const char *sig_names[] =
{
    SEQSTR(SIGNULL),
    SEQSTR(SIGHUP),
    SEQSTR(SIGINT),
    SEQSTR(SIGQUIT),
    SEQSTR(SIGILL),
    SEQSTR(SIGTRAP),
    SEQSTR(SIGABRT),
    SEQSTR(SIGBUS),
    SEQSTR(SIGFPE),
    SEQSTR(SIGKILL),
    SEQSTR(SIGUSR1),
    SEQSTR(SIGSEGV),
    SEQSTR(SIGUSR2),
    SEQSTR(SIGPIPE),
    SEQSTR(SIGALRM),
    SEQSTR(SIGTERM),
    SEQSTR(SIGSTKFLT),
    SEQSTR(SIGCHLD),
    SEQSTR(SIGCONT),
    SEQSTR(SIGSTOP),
    SEQSTR(SIGTSTP),
    SEQSTR(SIGTTIN),
    SEQSTR(SIGTTOU),
    SEQSTR(SIGURG),
    SEQSTR(SIGXCPU),
    SEQSTR(SIGXFSZ),
    SEQSTR(SIGVTALRM),
    SEQSTR(SIGPROF),
    SEQSTR(SIGWINCH),
    SEQSTR(SIGIO),
    SEQSTR(SIGPWR),
    SEQSTR(SIGSYS),
};

int sig_name_count = (sizeof(sig_names) / sizeof(sig_names[0]));

#else   /* !DEFINE_SIG_NAMES */

extern char *sig_names[];
extern int sig_name_count;

#endif  /* DEFINE_SIG_NAMES */

/*
#define SIG_NAMES_COUNT             \
    (sizeof(sig_names) / sizeof(sig_names[0]))
*/


/*
 * Define the array in only one file.
 */
#ifdef DEFINE_SIG_MASK

char sig_mask[] =
{
    [SIGNULL    ] = 1,
    [SIGHUP     ] = 1,
    [SIGINT     ] = 1,
    [SIGQUIT    ] = 1,
    [SIGILL     ] = 1,
    [SIGTRAP    ] = 1,
    [SIGABRT    ] = 1,
    [SIGBUS     ] = 1,
    [SIGFPE     ] = 1,
    [SIGKILL    ] = 1,
    [SIGUSR1    ] = 1,
    [SIGSEGV    ] = 1,
    [SIGUSR2    ] = 1,
    [SIGPIPE    ] = 1,
    [SIGALRM    ] = 1,
    [SIGTERM    ] = 1,
    [SIGSTKFLT  ] = 1,
    [SIGCHLD    ] = 1,
    [SIGCONT    ] = 1,
    [SIGSTOP    ] = 1,
    [SIGTSTP    ] = 1,
    [SIGTTIN    ] = 1,
    [SIGTTOU    ] = 1,
    [SIGURG     ] = 1,
    [SIGXCPU    ] = 1,
    [SIGXFSZ    ] = 1,
    [SIGVTALRM  ] = 1,
    [SIGPROF    ] = 1,
    [SIGWINCH   ] = 1,
    [SIGIO      ] = 1,
    [SIGPWR     ] = 1,
    [SIGSYS     ] = 1,
};

int sig_mask_count = (sizeof(sig_mask) / sizeof(sig_mask[0]));

#else   /* !DEFINE_SIG_MASK */

extern char sig_mask[];
extern int sig_mask_count;

#endif  /* DEFINE_SIG_MASK */

/*
#define SIG_MASK_COUNT              \
    (sizeof(sig_mask) / sizeof(sig_mask[0]))
*/

