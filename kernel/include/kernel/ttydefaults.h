/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: ttydefaults.h
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
 *  \file ttydefaults.h
 *
 *  Default settings for newly opened terminal devices.
 */

#ifndef __KERNEL_TTY_DEFAULTS_H__
#define __KERNEL_TTY_DEFAULTS_H__

#ifndef TTYDEF_IFLAG
#define TTYDEF_IFLAG (BRKINT | ISTRIP | ICRNL | IMAXBEL | IXON | IXANY)
#endif

#ifndef TTYDEF_OFLAG
#define TTYDEF_OFLAG (OPOST | ONLCR | XTABS)
#endif

#ifndef TTYDEF_LFLAG
#define TTYDEF_LFLAG (ECHO | ICANON | ISIG | IEXTEN | ECHOE|ECHOKE|ECHOCTL)
#endif

#ifndef TTYDEF_CFLAG
#define TTYDEF_CFLAG (CREAD | CS7 | PARENB | HUPCL)
#endif

#ifndef TTYDEF_SPEED
#define TTYDEF_SPEED (B9600)
#endif

#ifndef __TTY_DEF_CHARS__
#define __TTY_DEF_CHARS__
/*
 * Control Character Defaults
 */
#define CTRL(x)             (x & 037)
#define CEOF                CTRL('d')
#define CEOL                '\0'
#define CERASE              0177
#define CINTR               CTRL('c')
#define CSTATUS             '\0'
#define CKILL               CTRL('u')
#define CMIN                1
#define CQUIT               034                /* ^| */
#define CSUSP               CTRL('z')
#define CTIME               0
#define CDSUSP              CTRL('y')
#define CSTART              CTRL('q')
#define CSTOP               CTRL('s')
#define CLNEXT              CTRL('v')
#define CDISCARD            CTRL('o')
#define CWERASE             CTRL('w')
#define CREPRINT            CTRL('r')
//#define CEOT                CEOF
/* compat */
//#define CBRK                CEOL
//#define CRPRNT              CREPRINT
//#define CFLUSH              CDISCARD

#ifdef __WANT_TTY_DEFCHARS_ARRAY__
/*
 * Array of default control characters.
 *	 intr=^C        quit=^|     erase=del   kill=^U
 *   eof=^D         vtime=\0    vmin=\1     sxtc=\0
 *   start=^Q       stop=^S     susp=^Y     eol=\0
 *   reprint=^R     discard=^O  werase=^W   lnext=^V
 *   eol2=\0
 */
static cc_t ttydefchars[NCCS] =
{
    CINTR, CQUIT, CERASE, CKILL, CEOF, CTIME, CMIN, '\0',
    CSTART, CSTOP, CSUSP, CEOL, CREPRINT, CDISCARD, CWERASE,
    CLNEXT, CEOL
    
    /*
    CEOF, CEOL, CEOL, CERASE, CWERASE, CKILL, CREPRINT,
    '\0', CINTR, CQUIT, CSUSP, CDSUSP, CSTART, CSTOP, CLNEXT,
    CDISCARD, CMIN, CTIME, CSTATUS, '\0'
    */
};
#endif
#endif

#endif      /* __KERNEL_TTY_DEFAULTS_H__ */
