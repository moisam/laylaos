/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: strace-eoptions.c
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
 *  \file strace-eoptions.c
 *
 *  Helper functions to process commandline options. This file is part of
 *  the trace utility program.
 */

#define DEFINE_QUIET_MASK
#define DEFINE_DECODE_FDS_MASK
#include "strace.h"

#define DEFINE_SIG_NAMES
#define DEFINE_SIG_MASK
#include "strace-sig.h"

#define DEFINE_SYSCALL_NAMES
#define DEFINE_SYSCALL_MASK
#define DEFINE_SYSCALL_STATUS_MASK
#include "strace-syscall.h"

#define DEFINE_ERRNO_NAMES
#include "strace-errno.h"

#include <stdlib.h>
#include <string.h>


struct inject_t *syscall_inject_mask = NULL;


#define MAY_NEGATE(negate, count, mask)     \
    if(negate)                              \
        for(n = 0; n < count; n++)          \
            mask[n] = !mask[n];


#define HANDLE_ALL_AND_NONE_OPTIONS(mask)   \
        if(strcmp(tok, "all") == 0)         \
        {                                   \
            memset(mask, 1, sizeof(mask));  \
            tok = strtok(NULL, comma);      \
            continue;                       \
        }                                   \
        else if(strcmp(tok, "none") == 0)   \
        {                                   \
            memset(mask, 0, sizeof(mask));  \
            tok = strtok(NULL, comma);      \
            continue;                       \
        }


static inline int find_name_and_set(const char **names, char *mask,
                                    char *which, size_t name_count)
{
    size_t n;

    for(n = 0; n < name_count; n++)
    {
        if(names[n] && strcmp(names[n], which) == 0)
        {
            mask[n] = 1;
            return 1;
        }
    }
    
    return 0;
}


void process_eoption_trace(char *myname, char *_optstr)
{
    char *optstr, *tok;
    static char comma[] = ",";
    size_t n;
    int negate = (*_optstr == '!');

    if(!(optstr = strdup(negate ? (_optstr + 1) : _optstr)))
    {
        ERR_EXIT("%s: insufficient memory to parse '--trace' option\n", myname);
    }

    memset(syscall_mask, 0, sizeof(syscall_mask));

    tok = strtok(optstr, comma);

    while(tok)
    {
        HANDLE_ALL_AND_NONE_OPTIONS(syscall_mask);

        if(*tok == '%')
        {
            tok++;
            
            if(strcmp(tok, "creds") == 0)
            {
                // %creds
                SET_SYSCALL_MASK(CREDS_SYSCALL_LIST);
            }
            else if(strcmp(tok, "clock") == 0)
            {
                // %clock
                SET_SYSCALL_MASK(CLOCK_SYSCALL_LIST);
            }
            else if(strcmp(tok, "desc") == 0)
            {
                // %desc
                SET_SYSCALL_MASK(DESC_SYSCALL_LIST);
            }
            else if(strcmp(tok, "file") == 0)
            {
                // %file
                SET_SYSCALL_MASK(FILE_SYSCALL_LIST);
            }
            else if(strcmp(tok, "fstatfs") == 0)
            {
                // %fstatfs
                SET_SYSCALL_MASK(FSTATFS_SYSCALL_LIST);
            }
            else if(strcmp(tok, "fstat") == 0)
            {
                // %fstat
                SET_SYSCALL_MASK(FSTAT_SYSCALL_LIST);
            }
            else if(strcmp(tok, "ipc") == 0)
            {
                // %ipc
                SET_SYSCALL_MASK(IPC_SYSCALL_LIST);
            }
            else if(strcmp(tok, "lstat") == 0)
            {
                // %lstat
                SET_SYSCALL_MASK(LSTAT_SYSCALL_LIST);
            }
            else if(strcmp(tok, "memory") == 0)
            {
                // %memory
                SET_SYSCALL_MASK(MEMORY_SYSCALL_LIST);
            }
            else if(strcmp(tok, "net") == 0 || strcmp(tok, "network") == 0)
            {
                // %net and %network
                SET_SYSCALL_MASK(NETWORK_SYSCALL_LIST);
            }
            else if(strcmp(tok, "process") == 0)
            {
                // %process
                SET_SYSCALL_MASK(PROCESS_SYSCALL_LIST);
            }
            else if(strcmp(tok, "pure") == 0)
            {
                // %pure
                SET_SYSCALL_MASK(PURE_SYSCALL_LIST);
            }
            else if(strcmp(tok, "signal") == 0)
            {
                // %signal
                SET_SYSCALL_MASK(SIGNAL_SYSCALL_LIST);
            }
            else if(strcmp(tok, "statfs") == 0)
            {
                // %statfs
                SET_SYSCALL_MASK(STATFS_SYSCALL_LIST);
            }
            else if(strcmp(tok, "stat") == 0)
            {
                // %stat
                SET_SYSCALL_MASK(STAT_SYSCALL_LIST);
            }
            else if(strcmp(tok, "%%statfs") == 0)
            {
                // %%statfs
                SET_SYSCALL_MASK(STATFS_SYSCALL_LIST);
                SET_SYSCALL_MASK(FSTATFS_SYSCALL_LIST);
            }
            else if(strcmp(tok, "%%stat") == 0)
            {
                // %%stat
                SET_SYSCALL_MASK(STAT_SYSCALL_LIST);
                SET_SYSCALL_MASK(LSTAT_SYSCALL_LIST);
                SET_SYSCALL_MASK(FSTAT_SYSCALL_LIST);
            }
            else
            {
                ERR_EXIT("%s: unknown syscall group: %s\n", myname, tok);
            }
        }
        else
        {
            if(!find_name_and_set(syscall_names, syscall_mask,
                                  tok, syscall_name_count))
            {
                ERR_EXIT("%s: unknown syscall name: %s\n", myname, tok);
            }
        }

        tok = strtok(NULL, comma);
    }
    
    free(optstr);
    MAY_NEGATE(negate, syscall_mask_count, syscall_mask);
}


void process_eoption_signal(char *myname, char *_optstr)
{
    char *optstr, *tok;
    static char comma[] = ",";
    size_t n;
    int negate = (*_optstr == '!');

    if(!(optstr = strdup(negate ? (_optstr + 1) : _optstr)))
    {
        ERR_EXIT("%s: insufficient memory to parse '--signal' option\n", myname);
    }

    memset(sig_mask, 0, sizeof(sig_mask));

    tok = strtok(optstr, comma);

    while(tok)
    {
        HANDLE_ALL_AND_NONE_OPTIONS(sig_mask);

        if(!find_name_and_set(sig_names, sig_mask,
                                  tok, sig_name_count))
        {
            ERR_EXIT("%s: unknown signal name: %s\n", myname, tok);
        }

        tok = strtok(NULL, comma);
    }
    
    free(optstr);
    MAY_NEGATE(negate, sig_name_count, sig_mask);
}


void process_eoption_status(char *myname, char *_optstr)
{
    char *optstr, *tok;
    static char comma[] = ",";
    size_t n;
    int negate = (*_optstr == '!');

    if(!(optstr = strdup(negate ? (_optstr + 1) : _optstr)))
    {
        ERR_EXIT("%s: insufficient memory to parse '--status' option\n", myname);
    }

    memset(syscall_status_mask, 0, sizeof(syscall_status_mask));

    tok = strtok(optstr, comma);

    while(tok)
    {
        HANDLE_ALL_AND_NONE_OPTIONS(syscall_status_mask);

        if(strcmp(tok, "successful") == 0)
        {
            syscall_status_mask[SYSCAL_STATUS_SUCCESSFUL] = 1;
        }
        else if(strcmp(tok, "failed") == 0)
        {
            syscall_status_mask[SYSCAL_STATUS_FAILED] = 1;
        }
        else
        {
            ERR_EXIT("%s: unknown status: %s\n", myname, tok);
        }

        tok = strtok(NULL, comma);
    }
    
    free(optstr);
    MAY_NEGATE(negate, 2, syscall_status_mask);
}


void process_eoption_silent(char *myname, char *_optstr)
{
    char *optstr, *tok;
    static char comma[] = ",";
    size_t n;
    int negate = (*_optstr == '!');

    if(!(optstr = strdup(negate ? (_optstr + 1) : _optstr)))
    {
        ERR_EXIT("%s: insufficient memory to parse '--quiet' option\n", myname);
    }

    memset(quiet_mask, 0, sizeof(quiet_mask));

    tok = strtok(optstr, comma);

    while(tok)
    {
        HANDLE_ALL_AND_NONE_OPTIONS(quiet_mask);

        if(strcmp(tok, "attach") == 0)
        {
            quiet_mask[QUIET_ATTACH] = 1;
        }
        else if(strcmp(tok, "exit") == 0)
        {
            quiet_mask[QUIET_EXIT] = 1;
        }
        else
        {
            ERR_EXIT("%s: unknown quiet option: %s\n", myname, tok);
        }

        tok = strtok(NULL, comma);
    }
    
    free(optstr);
    MAY_NEGATE(negate, 2, quiet_mask);
}


void process_eoption_decode_fds(char *myname, char *_optstr)
{
    char *optstr, *tok;
    static char comma[] = ",";
    size_t n;
    int negate = (*_optstr == '!');

    if(!(optstr = strdup(negate ? (_optstr + 1) : _optstr)))
    {
        ERR_EXIT("%s: insufficient memory to parse '--decode-fds' option\n", myname);
    }

    memset(decode_fds_mask, 0, sizeof(decode_fds_mask));

    tok = strtok(optstr, comma);

    while(tok)
    {
        HANDLE_ALL_AND_NONE_OPTIONS(decode_fds_mask);

        if(strcmp(tok, "path") == 0)
        {
            decode_fds_mask[DECODE_FDS_PATH] = 1;
        }
        else
        {
            ERR_EXIT("%s: unknown decode-fds option: %s\n", myname, tok);
        }

        tok = strtok(NULL, comma);
    }
    
    free(optstr);
    MAY_NEGATE(negate, 1, decode_fds_mask);
}


#define SET_INJECT(list)                        \
{                                               \
    int *i, syscalls[] = { list, 0 };           \
    for(i = syscalls; *i; i++)                  \
        syscall_inject_mask[*i].inject = 1;     \
}


#define SET_INJECT_FIELD(which, val)            \
{                                               \
    for(i = 0; i < syscall_mask_count; i++)     \
        if(syscall_inject_mask[i].inject)       \
            syscall_inject_mask[i].which = val; \
}


void process_eoption_inject(char *myname, char *_optstr)
{
    char *optstr, *tok, *colon;
    static char comma_separator[] = ",", colon_separator[] = ":";
    size_t n, sz = syscall_mask_count * sizeof(struct inject_t);
    int negate = (*_optstr == '!');
    int i, at_least_one;
    int error_set = 0, retval_set = 0, signum_set = 0, syscall_set = 0;

    if(!(optstr = strdup(negate ? (_optstr + 1) : _optstr)))
    {
        ERR_EXIT("%s: insufficient memory to parse '--inject' option\n", myname);
    }
    
    if((colon = strchr(optstr, ':')))
    {
        *colon = '\0';
    }
    
    if(!(syscall_inject_mask = (struct inject_t *)malloc(sz)))
    {
        ERR_EXIT("%s: insufficient memory to parse '--inject' option\n", myname);
    }

    memset(syscall_inject_mask, 0, sz);

    tok = strtok(optstr, comma_separator);

    while(tok)
    {
        if(strcmp(tok, "all") == 0)
        {
            for(i = 0; i < syscall_mask_count; i++)
            {
                syscall_inject_mask[i].inject = 1;
            }
        }
        else if(strcmp(tok, "none") == 0)
        {
            for(i = 0; i < syscall_mask_count; i++)
            {
                syscall_inject_mask[i].inject = 0;
            }
        }
        else if(*tok == '%')
        {
            tok++;
            
            if(strcmp(tok, "creds") == 0)
            {
                // %creds
                SET_INJECT(CREDS_SYSCALL_LIST);
            }
            else if(strcmp(tok, "clock") == 0)
            {
                // %clock
                SET_INJECT(CLOCK_SYSCALL_LIST);
            }
            else if(strcmp(tok, "desc") == 0)
            {
                // %desc
                SET_INJECT(DESC_SYSCALL_LIST);
            }
            else if(strcmp(tok, "file") == 0)
            {
                // %file
                SET_INJECT(FILE_SYSCALL_LIST);
            }
            else if(strcmp(tok, "fstatfs") == 0)
            {
                // %fstatfs
                SET_INJECT(FSTATFS_SYSCALL_LIST);
            }
            else if(strcmp(tok, "fstat") == 0)
            {
                // %fstat
                SET_INJECT(FSTAT_SYSCALL_LIST);
            }
            else if(strcmp(tok, "ipc") == 0)
            {
                // %ipc
                SET_INJECT(IPC_SYSCALL_LIST);
            }
            else if(strcmp(tok, "lstat") == 0)
            {
                // %lstat
                SET_INJECT(LSTAT_SYSCALL_LIST);
            }
            else if(strcmp(tok, "memory") == 0)
            {
                // %memory
                SET_INJECT(MEMORY_SYSCALL_LIST);
            }
            else if(strcmp(tok, "net") == 0 || strcmp(tok, "network") == 0)
            {
                // %net and %network
                SET_INJECT(NETWORK_SYSCALL_LIST);
            }
            else if(strcmp(tok, "process") == 0)
            {
                // %process
                SET_INJECT(PROCESS_SYSCALL_LIST);
            }
            else if(strcmp(tok, "pure") == 0)
            {
                // %pure
                SET_INJECT(PURE_SYSCALL_LIST);
            }
            else if(strcmp(tok, "signal") == 0)
            {
                // %signal
                SET_INJECT(SIGNAL_SYSCALL_LIST);
            }
            else if(strcmp(tok, "statfs") == 0)
            {
                // %statfs
                SET_INJECT(STATFS_SYSCALL_LIST);
            }
            else if(strcmp(tok, "stat") == 0)
            {
                // %stat
                SET_INJECT(STAT_SYSCALL_LIST);
            }
            else if(strcmp(tok, "%%statfs") == 0)
            {
                // %%statfs
                SET_INJECT(STATFS_SYSCALL_LIST);
                SET_INJECT(FSTATFS_SYSCALL_LIST);
            }
            else if(strcmp(tok, "%%stat") == 0)
            {
                // %%stat
                SET_INJECT(STAT_SYSCALL_LIST);
                SET_INJECT(LSTAT_SYSCALL_LIST);
                SET_INJECT(FSTAT_SYSCALL_LIST);
            }
            else
            {
                ERR_EXIT("%s: unknown syscall group: %s\n", myname, tok);
            }
        }
        else
        {
            for(i = 0; i < syscall_name_count; i++)
            {
                if(syscall_names[i] && strcmp(syscall_names[i], tok) == 0)
                {
                    syscall_inject_mask[i].inject = 1;
                    break;
                }
            }

            if(i == syscall_name_count)
            {
                ERR_EXIT("%s: unknown syscall name: %s\n", myname, tok);
            }
        }

        tok = strtok(NULL, comma_separator);
    }

    if(negate)
    {
        for(i = 0; i < syscall_mask_count; i++)
        {
            syscall_inject_mask[i].inject = !syscall_inject_mask[i].inject;
        }
    }

    for(at_least_one = 0, i = 0; i < syscall_mask_count; i++)
    {
        if(syscall_inject_mask[i].inject == 1)
        {
            at_least_one = 1;
            break;
        }
    }
    
    if((colon && !at_least_one) || (!colon && at_least_one))
    {
        ERR_EXIT("%s: invalid use of the '--inject' option\n"
                 "See %s --help for the syntax\n", myname, myname);
    }
    
    if(!colon)
    {
        free(optstr);
        return;
    }
    
    // process the options

    tok = strtok(colon + 1, colon_separator);

    while(tok)
    {
        if(strstr(tok, "error=") == tok)
        {
            if(tok[6] >= '0' && tok[6] <= '9')
            {
                int err = atoi(tok + 6);
                
                if(err < 1 || err > 4095)
                {
                    ERR_EXIT("%s: invalid errno: %s\n", myname, tok + 6);
                }
                
                SET_INJECT_FIELD(error, err);
            }
            else
            {
                for(n = 0; n < errno_count; n++)
                {
                    if(strcmp(errno_names[n], tok + 6) == 0)
                    {
                        SET_INJECT_FIELD(error, n);
                        break;
                    }
                }
                
                if(n == errno_count)
                {
                    ERR_EXIT("%s: invalid errno: %s\n", myname, tok + 6);
                }
            }
            
            error_set = 1;
        }
        else if(strstr(tok, "retval=") == tok)
        {
            if(tok[7] >= '0' && tok[7] <= '9')
            {
                SET_INJECT_FIELD(retval, atoi(tok + 7));
            }
            else
            {
                ERR_EXIT("%s: invalid retval: %s\n", myname, tok + 7);
            }
            
            retval_set = 1;
        }
        else if(strstr(tok, "signal=") == tok)
        {
            if(tok[7] >= '0' && tok[7] <= '9')
            {
                int sig = atoi(tok + 7);
                
                if(sig < 1 || sig > SIGRTMAX)
                {
                    ERR_EXIT("%s: invalid signal: %s\n", myname, tok + 7);
                }
                
                SET_INJECT_FIELD(signum, sig);
            }
            else
            {
                for(i = 0; i < sig_name_count; i++)
                {
                    if(strcmp(sig_names[i], tok + 7) == 0)
                    {
                        SET_INJECT_FIELD(signum, i);
                        break;
                    }
                }
                
                if(i == sig_name_count)
                {
                    ERR_EXIT("%s: invalid signal: %s\n", myname, tok + 7);
                }
            }
            
            signum_set = 1;
        }
        else if(strstr(tok, "syscall=") == tok)
        {
            int syscall = 0;

            if(tok[8] >= '0' && tok[8] <= '9')
            {
                syscall = atoi(tok + 8);
            }
            else
            {
                for(i = 0; i < syscall_name_count; i++)
                {
                    if(syscall_names[i] && strcmp(syscall_names[i], tok + 8) == 0)
                    {
                        syscall = i;
                        break;
                    }
                }

                if(i == syscall_name_count)
                {
                    ERR_EXIT("%s: unknown syscall name: %s\n", myname, tok + 8);
                }
            }
            
            // only %pure syscalls are permitted with this option
            int *x, syscalls[] = { PURE_SYSCALL_LIST, 0 };

            for(x = syscalls; *x; x++)
            {
                if(syscall == *x)
                {
                    break;
                }
            }
            
            if(!*x)
            {
                ERR_EXIT("%s: invalid syscall specified: %s\n"
                         "See %s --help for the syntax\n", myname, tok + 8, myname);
            }
            
            SET_INJECT_FIELD(syscall, syscall);
            syscall_set = 1;
        }
        else
        {
        }

        tok = strtok(NULL, colon_separator);
    }
    
    free(optstr);
    
    if(!error_set && !retval_set && !signum_set && !syscall_set)
    {
        ERR_EXIT("%s: you must supply error, retval, signal or syscall with '--inject'\n"
                 "See %s --help for the syntax\n", myname, myname);
    }
    
    if(error_set && retval_set)
    {
        ERR_EXIT("%s: error and retval are mutually exclusive when using '--inject'\n"
                 "See %s --help for the syntax\n", myname, myname);
    }
}


void process_eoption_fault(char *myname, char *_optstr)
{
    if(strchr(_optstr, ':'))
    {
        process_eoption_inject(myname, _optstr);
        return;
    }
    
    // add the [:error=errno] bit
    char buf[strlen(_optstr) + 16];
    
    sprintf(buf, "%s:error=ENOSYS", _optstr);
    process_eoption_inject(myname, buf);
}

