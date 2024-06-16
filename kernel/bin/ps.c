/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: ps.c
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
 *  \file ps.c
 *
 *  A utility program to list the running tasks on the system using different
 *  filters.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>


char *ver = "1.0";
uid_t my_euid = 0;
int clock_ticks = 100;
int page_size = 4096;
char buf[4096];

// selection options
int show_all = 0;
int show_running_only = 0;
int show_leaders = 0;
int show_noterm = 0;
int show_threads = 0;
int negate = 0;

// output options
int show_pid = 1;
int show_ppid = 0;
int show_tname = 1;
int show_time = 1;
int show_cmd = 1;
int show_state = 0;
int show_cmd_args = 0;
int show_uid = 0;
int show_nice = 0;
int show_prio = 0;
int show_wchan = 0;
int show_majflt = 0;
int show_rss = 0;
int show_vmsize = 0;

// struct to represent a process
struct proc_t
{
    pid_t pid, tgid, /* tid, */ ppid, pgid, sid;
    uid_t euid;
    char *user, *ttyname;
    int ctty, prio, nice, threads;
    char *cmd;
    char state;
    long utime, stime;
    long unsigned int wchan, majflt, rss;
    size_t vmsize;
    struct proc_t *next;
};

struct proc_t *proc_head = NULL, *proc_tail = NULL;

// fields extracted from /proc/pid/stat
#define FIELD_TGID          0
#define FIELD_COMM          1
#define FIELD_PID           2
//#define FIELD_TID           3
#define FIELD_STATE         3
#define FIELD_PPID          4
#define FIELD_PGID          5
#define FIELD_SID           6
#define FIELD_CTTY          7
#define FIELD_TPGID         8

#define FIELD_MAJFLT        12
#define FIELD_CMAJFLT       13

#define FIELD_UTIME         14
#define FIELD_STIME         15
#define FIELD_CUTIME        16
#define FIELD_CSTIME        17
#define FIELD_PRIO          18
#define FIELD_NICE          19
#define FIELD_THREADS       20

#define FIELD_RSS           24

#define FIELD_WAITCHANNEL   35

// indices to the column width array
#define COL_STATE           0
#define COL_UID             1
#define COL_TGID            2
#define COL_TID             3
#define COL_THREADS         4
#define COL_PPID            5
#define COL_NICE            6
#define COL_PRIO            7
#define COL_WCHAN           8
#define COL_TTY             9
#define COL_TIME            10
#define COL_MAJFL           11
#define COL_VSZ             12
#define COL_RSS             13
#define COL_CMD             14

#define COL_LAST            14

// column widths
int colw[COL_LAST + 1];

// tty names buf
#define MAX_TTYS            1024
struct
{
    int id;
    char *name;
} ttys[MAX_TTYS];

// uids buf
#define MAX_UIDS            1024
struct
{
    uid_t euid;
    char *name;
} uids[MAX_UIDS];


char *get_user(uid_t euid)
{
    struct passwd *pwd;
    int i;
    char *user;
    
    for(i = 0; i < MAX_UIDS; i++)
    {
        if(!uids[i].name)
        {
            break;
        }
        
        if(uids[i].euid == euid)
        {
            return uids[i].name;
        }
    }
    
    if(i == MAX_UIDS)
    {
        fprintf(stderr, "ps: uid table is full!\n");
        return NULL;
    }

    user = NULL;

    if((pwd = getpwuid(euid)))
    {
        user = strdup(pwd->pw_name);
        uids[i].name = user;
        uids[i].euid = euid;
    }
    
    endpwent();

    return user;
}


char *get_ttyname(int tty)
{
    static char *ttyname = NULL;
    static size_t ttynamelen = 0;
    static char *devpath = "/dev";
    static char *devptspath = "/dev/pts";
    DIR *devdir;
    struct dirent *dent;
    struct stat st;
    size_t len;
    
    /*
     * First try /dev/ttyX
     */
    if(!(devdir = opendir(devpath)))
    {
        return NULL;
    }
    
    while((dent = readdir(devdir)) != NULL)
    {
        if(strstr(dent->d_name, "tty") == dent->d_name)
        {
            len = strlen(dent->d_name) + 1;

            if(sizeof(devpath) + len > ttynamelen)
            {
                if(ttyname)
                {
                    free(ttyname);
                    ttyname = NULL;
                    ttynamelen = 0;
                }
                
                if(!(ttyname = malloc(sizeof(devpath) + len + 1)))
                {
                    closedir(devdir);
                    return NULL;
                }

                ttynamelen = sizeof(devpath) + len;
            }
                
            sprintf(ttyname, "%s/%s", devpath, dent->d_name);

            if(stat(ttyname, &st) == 0 && S_ISCHR(st.st_mode) &&
               st.st_rdev == tty)
            {
                closedir(devdir);
                return ttyname;
            }
        }
    }
    
    closedir(devdir);

    /*
     * Next try /dev/pts/X
     */
    if(!(devdir = opendir(devptspath)))
    {
        return NULL;
    }
    
    while((dent = readdir(devdir)) != NULL)
    {
        len = strlen(dent->d_name) + 1;

        if(sizeof(devptspath) + len > ttynamelen)
        {
            if(ttyname)
            {
                free(ttyname);
                ttyname = NULL;
                ttynamelen = 0;
            }
            
            if(!(ttyname = malloc(sizeof(devptspath) + len + 1)))
            {
                closedir(devdir);
                return NULL;
            }

            ttynamelen = sizeof(devptspath) + len;
        }
        
        sprintf(ttyname, "%s/%s", devptspath, dent->d_name);

        if(stat(ttyname, &st) == 0 && S_ISCHR(st.st_mode) &&
           st.st_rdev == tty)
        {
            closedir(devdir);
            return ttyname;
        }
    }
    
    closedir(devdir);

    return NULL;
}

char *tty_name(int tty)
{
    int i;
    char *ttyname;
    
    for(i = 0; i < MAX_TTYS; i++)
    {
        if(!ttys[i].name)
        {
            break;
        }

        if(ttys[i].id == tty)
        {
            return ttys[i].name;
        }
    }
    
    if(i == MAX_TTYS)
    {
        fprintf(stderr, "ps: tty table is full!\n");
        return NULL;
    }
    
    if(!(ttyname = get_ttyname(tty)))
    {
         return NULL;
    }
    
    ttys[i].name = strdup(ttyname);
    ttys[i].id = tty;
    
    return ttys[i].name;
}


#define UPDATE_FIELD_WIDTH(id, fmt, val)            \
    if((len = sprintf(buf, fmt, val)) > colw[id])   \
        colw[id] = len;


void update_col_widths(struct proc_t *proc)
{
    long hr, min, sec;
    size_t len;

    colw[COL_STATE] = 1;

    if(proc->user)
    {
        UPDATE_FIELD_WIDTH(COL_UID, "%s", proc->user);
    }
    else
    {
        UPDATE_FIELD_WIDTH(COL_UID, "%d", proc->euid);
    }

    UPDATE_FIELD_WIDTH(COL_TGID, "%d", proc->tgid);
    //UPDATE_FIELD_WIDTH(COL_TID, "%d", proc->tid);
    UPDATE_FIELD_WIDTH(COL_TID, "%d", proc->pid);
    UPDATE_FIELD_WIDTH(COL_THREADS, "%d", proc->threads);
    UPDATE_FIELD_WIDTH(COL_PPID, "%d", proc->ppid);
    UPDATE_FIELD_WIDTH(COL_NICE, "%d", proc->nice);
    UPDATE_FIELD_WIDTH(COL_PRIO, "%d", proc->prio);
    UPDATE_FIELD_WIDTH(COL_WCHAN, "%lx", proc->wchan);
    
    if(proc->ctty > 0 && (proc->ttyname = tty_name(proc->ctty)))
    {
        UPDATE_FIELD_WIDTH(COL_TTY, "%s", proc->ttyname);
    }
    else
    {
        proc->ttyname = NULL;
        //colw[COL_TTY] = 3;
    }

    sec = (proc->utime + proc->stime) / clock_ticks;
    hr = sec / (60 * 60);
    min = (sec / 60) % 60;
    sec = sec % 60;

    if((len = sprintf(buf, "%02lu:%02lu.%02lu", hr, min, sec)) > colw[COL_TIME])
    {
        colw[COL_TIME] = len;
    }

    UPDATE_FIELD_WIDTH(COL_MAJFL, "%lu", proc->majflt);
    UPDATE_FIELD_WIDTH(COL_VSZ, "%ld", proc->vmsize);
    UPDATE_FIELD_WIDTH(COL_RSS, "%lu", proc->rss);

    if(proc->cmd && (len = strlen(proc->cmd)) > colw[COL_CMD])
    {
        colw[COL_CMD] = len;
    }
    /*
    else
    {
        colw[COL_CMD] = 1;
    }
    */
}


#define FREE(c)             if(c) free(c)

#define SKIP_SPACES(p, buf)         { p = buf; while(*p == ' ') p++; }
#define SKIP_LETTERS(p, buf)        { p = buf; while(*p && *p != '\t') p++; }


struct proc_t *do_entry(struct dirent *dent)
{
    FILE *f;
    char space[] = " ";
    char *p, *nl, *tok;
    int field = 0;
    struct proc_t *proc;
    
    pid_t pid = -1, tgid = -1, ppid = -1, pgid = -1, sid = -1;
    uid_t euid = -1;
    int ctty = -1, prio = 0, nice = 0, threads = 1;
    char *cmd = NULL, *user = NULL, *cmdline = NULL;
    char state = '-';
    long utime = 0, stime = 0;
    long unsigned int wchan = 0, majflt = 0, rss = 0;
    size_t vmsize = 0;
    
    /*
     * Read most of the process's info from /proc/pid/stat
     */
    sprintf(buf, "/proc/%s/stat", dent->d_name);
    
    if(!(f = fopen(buf, "r")))
    {
        return NULL;
    }
    
    if(fgets(buf, sizeof(buf), f) != NULL)
    {
        if((nl = strchr(buf, '\n')))
        {
            *nl = '\0';
        }
        
        tok = strtok(buf, space);
        
        while(tok)
        {
            switch(field)
            {
                case FIELD_TGID:
                    tgid = atoi(tok);
                    break;

                case FIELD_COMM:
                    // skip the leading '('
                    if((cmd = strdup(tok + 1)))
                    {
                        // and remove the trailing ')'
                        cmd[strlen(cmd) - 1] = '\0';
                    }
                    break;

                case FIELD_PID:
                    pid = atoi(tok);
                    break;

                /*
                case FIELD_TID:
                    tid = atoi(tok);
                    break;
                */

                case FIELD_STATE:
                    state = *tok;
                    break;

                case FIELD_PPID:
                    ppid = atoi(tok);
                    break;

                case FIELD_PGID:
                    pgid = atoi(tok);
                    break;

                case FIELD_SID:
                    sid = atoi(tok);
                    break;

                case FIELD_CTTY:
                    ctty = atoi(tok) & 0xffff;
                    break;

                /*
                case FIELD_TPGID:
                    tpgid = atoi(tok);
                    break;
                */

                case FIELD_MAJFLT:
                    majflt = atol(tok);
                    break;

                case FIELD_CMAJFLT:
                    majflt += atol(tok);
                    break;

                case FIELD_UTIME:
                    utime += atol(tok);
                    break;

                case FIELD_STIME:
                    stime += atol(tok);
                    break;

                case FIELD_CUTIME:
                    utime += atol(tok);
                    break;

                case FIELD_CSTIME:
                    stime += atol(tok);
                    break;

                case FIELD_PRIO:
                    prio = atoi(tok);
                    break;

                case FIELD_NICE:
                    nice = atoi(tok);
                    break;

                case FIELD_THREADS:
                    threads = atoi(tok);
                    break;

                case FIELD_RSS:
                    // the kernel reports this in pagesize granularity, here we
                    // convert to kilobytes
                    rss = atol(tok);
                    rss = (rss * page_size) / 1024;
                    break;

                case FIELD_WAITCHANNEL:
                    wchan = atol(tok);
                    break;
            }
            
            field++;
            tok = strtok(NULL, space);
        }
        
        /*
        if(strstr(buf, "Pid:") == buf)
        {
            pid = atoi(SKIP_SPACES(p, buf + 4));
        }
        else if(strstr(buf, "Pgid:") == buf)
        {
            pgid = atoi(SKIP_SPACES(p, buf + 5));
        }
        else if(strstr(buf, "Tgid:") == buf)
        {
            tgid = atoi(SKIP_SPACES(p, buf + 5));
        }
        else if(strstr(buf, "Uid:") == buf)
        {
            SKIP_SPACES(p, buf + 4);
            // skip the 'uid' field
            SKIP_LETTERS(p, p);
            // the 'uid' field is followed by '\t', then the 'euid' field,
            // which is what we want
            euid = atoi(++p);
        }
        else if(strstr(buf, "Name:") == buf)
        {
            name = strdup(SKIP_SPACES(p, buf + 5));
        }
        */
    }
    
    fclose(f);
    
    /*
     * Read euid and vmsize from /proc/pid/status
     */
    euid = 0;
    sprintf(buf, "/proc/%s/status", dent->d_name);

    if(!(f = fopen(buf, "r")))
    {
        return NULL;
    }

    while(fgets(buf, sizeof(buf), f) != NULL)
    {
        if(strstr(buf, "Uid:") == buf)
        {
            SKIP_SPACES(p, buf + 4);
            // skip the 'uid' field
            SKIP_LETTERS(p, p);
            // the 'uid' field is followed by '\t', then the 'euid' field,
            // which is what we want
            euid = atoi(++p);
        }
        else if(strstr(buf, "VmSize:") == buf)
        {
            SKIP_SPACES(p, buf + 7);
            // the kernel reports this in pagesize granularity, here we
            // convert to kilobytes
            vmsize = atol(p);
            vmsize = (vmsize * page_size) / 1024;
        }
    }

    fclose(f);

    // filter user processes if not -a,e,A,d
    if(!show_all)
    {
        if(euid != my_euid)
        {
            FREE(cmd);
            return NULL;
        }
    }
    
    if(!show_leaders && pid == pgid)
    {
        FREE(cmd);
        return NULL;
    }

    //printf("do_entry: show_noterm %d, ctty %d\n", show_noterm, ctty);

    if(!show_noterm && ctty <= 0)
    {
        FREE(cmd);
        return NULL;
    }

    if(show_running_only && state != 'R')
    {
        FREE(cmd);
        return NULL;
    }
    
    if(!show_threads && pid != tgid)
    {
        for(proc = proc_head; proc != NULL; proc = proc->next)
        {
            if(proc->tgid == tgid)
            {
                proc->utime += utime;
                proc->stime += stime;
                break;
            }
        }
        
        FREE(cmd);
        return NULL;
    }

    /*
     * Get the user's name
     */
    user = get_user(euid);

    /*
     * Get the process's commandline
     */
    if(show_cmd_args)
    {
        sprintf(buf, "/proc/%s/cmdline", dent->d_name);

        if(!(f = fopen(buf, "r")))
        {
            FREE(cmd);
            //FREE(user);
            return NULL;
        }
    
        if(fgets(buf, sizeof(buf), f) != NULL)
        {
            if((nl = strchr(buf, '\n')))
            {
                *nl = '\0';
            }
            
            if(*buf)
            {
                cmdline = strdup(buf);
                free(cmd);
                cmd = cmdline;
            }
        }

        fclose(f);
    }
    
    // now allocate and return the result
    if(!(proc = malloc(sizeof(struct proc_t))))
    {
        FREE(cmd);
        //FREE(user);
        return NULL;
    }
    
    proc->tgid = tgid;
    proc->cmd = cmd;
    proc->pid = pid;
    //proc->tid = tid;
    proc->state = state;
    proc->ppid = ppid;
    proc->pgid = pgid;
    proc->sid = sid;
    proc->euid = euid;
    proc->ctty = ctty;
    //proc->tpgid = tpgid;
    proc->utime = utime;
    proc->stime = stime;
    proc->prio = prio;
    proc->nice = nice;
    proc->threads = threads;
    proc->user = user;
    proc->wchan = wchan;
    proc->majflt = majflt;
    proc->rss = rss;
    proc->vmsize = vmsize;
    proc->next = NULL;
    
    update_col_widths(proc);
    
    return proc;
}


void print_header(void)
{
    if(show_state)
    {
        printf("%*s ", colw[COL_STATE], "S");
    }

    if(show_uid)
    {
        printf("%-*s ", colw[COL_UID], "UID");
    }

    if(show_pid)
    {
        printf("%*s ", colw[COL_TGID], "PID");
    }

    if(show_threads)
    {
        printf("%*s ", colw[COL_TID], "TID");
        printf("%*s ", colw[COL_THREADS], "NTID");
    }

    if(show_ppid)
    {
        printf("%*s ", colw[COL_PPID], "PPID");
    }

    if(show_nice)
    {
        printf("%*s ", colw[COL_NICE], "NI");
    }

    if(show_prio)
    {
        printf("%*s ", colw[COL_PRIO], "PR");
    }

    if(show_wchan)
    {
        printf("%-*s ", colw[COL_WCHAN], "WCHAN");
    }

    if(show_tname)
    {
        printf("%-*s ", colw[COL_TTY], "TTY");
    }

    if(show_time)
    {
        printf("%-*s ", colw[COL_TIME], "TIME");
    }

    if(show_majflt)
    {
        printf("%*s ", colw[COL_MAJFL], "MAJFL");
    }

    if(show_vmsize)
    {
        printf("%*s ", colw[COL_VSZ], "VSZ");
    }

    if(show_rss)
    {
        printf("%*s ", colw[COL_RSS], "RSS");
    }

    //printf("%-*s\n", colw[COL_CMD], "CMD");
    printf("CMD\n");
}


void print_processes(void)
{
    long hr, min, sec;
    struct proc_t *proc;

    for(proc = proc_head; proc != NULL; proc = proc->next)
    {
        if(show_state)
        {
            sprintf(buf, "%c", proc->state);
            printf("%*s ", colw[COL_STATE], buf);
        }

        if(show_uid)
        {
            if(proc->user)
            {
                printf("%-*s ", colw[COL_UID], proc->user);
            }
            else
            {
                printf("%-*d ", colw[COL_UID], proc->euid);
            }
        }

        if(show_pid)
        {
            printf("%*d ", colw[COL_TGID], proc->tgid);
        }

        if(show_threads)
        {
            //printf("%*d ", colw[COL_TID], proc->tid);
            printf("%*d ", colw[COL_TID], proc->pid);
            printf("%*d ", colw[COL_THREADS], proc->threads);
        }

        if(show_ppid)
        {
            printf("%*d ", colw[COL_PPID], proc->ppid);
        }

        if(show_nice)
        {
            printf("%*d ", colw[COL_NICE], proc->nice);
        }

        if(show_prio)
        {
            printf("%*d ", colw[COL_PRIO], proc->prio);
        }

        if(show_wchan)
        {
            if(proc->state == 'S')
            {
                printf("%-*lx ", colw[COL_WCHAN], proc->wchan);
            }
            else
            {
                printf("%-*s ", colw[COL_WCHAN], "-");
            }
        }

        if(show_tname)
        {
            if(proc->ttyname)
            {
                printf("%-*s ", colw[COL_TTY], proc->ttyname);
            }
            else
            {
                printf("%-*s ", colw[COL_TTY], "?");
            }
        }

        if(show_time)
        {
            sec = (proc->utime + proc->stime) / clock_ticks;
            hr = sec / (60 * 60);
            min = (sec / 60) % 60;
            sec = sec % 60;

            sprintf(buf, "%02lu:%02lu.%02lu", hr, min, sec);
            printf("%*s ", colw[COL_TIME], buf);
        }

        if(show_majflt)
        {
            printf("%*lu ", colw[COL_MAJFL], proc->majflt);
        }

        if(show_vmsize)
        {
            printf("%*ld ", colw[COL_VSZ], proc->vmsize);
        }

        if(show_rss)
        {
            printf("%*lu ", colw[COL_RSS], proc->rss);
        }

        if(proc->cmd)
        {
            //printf("%-*s\n", colw[COL_CMD], proc->cmd);
            printf("%s\n", proc->cmd);
        }
        else
        {
            //printf("%-*s\n", colw[COL_CMD], "?");
            printf("%s\n", "?");
        }
    }
}


int main(int argc, char **argv)
{
    int c;
    char *arg;
    DIR *procdir;
    struct dirent *dent;
    struct proc_t *proc;
    
    my_euid = geteuid();

    /*
     * Process standard arguments (those preceded by '-').
     */
    while((c = getopt(argc, argv, "adefhlrvAFNTV")) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case 'A':
            case 'e':
                show_all = 1;
                show_leaders = 1;
                show_noterm = 1;
                show_uid = 1;
                break;

            case 'a':
                show_all = 1;
                //show_leaders = 0;
                //show_noterm = 0;
                show_uid = 1;
                break;

            case 'd':
                show_all = 1;
                //show_leaders = 0;
                show_noterm = 1;
                show_uid = 1;
                break;

            case 'F':
            case 'f':
                show_uid = 1;
                show_cmd_args = 1;
                show_ppid = 1;
                break;

            case 'l':
                show_state = 1;
                show_uid = 1;
                show_cmd_args = 1;
                show_ppid = 1;
                show_nice = 1;
                show_prio = 1;
                show_wchan = 1;
                break;

            case 'r':
                show_running_only = 1;
                break;

            case 'v':
                show_state = 1;
                show_cmd_args = 1;
                show_majflt = 1;
                show_rss = 1;
                break;

            case 'N':
                negate = 1;
                break;
            
            case 'T':
                show_threads = 1;
                break;

            case 'V':
                printf("%s\n", ver);
                exit(0);
                break;

            case 'h':
                printf("ps utility for LaylaOS, Version %s\n\n", ver);
                printf("Usage: %s [options]\n\n"
                       "Options:\n"
                       "  -A        Show all processes\n"
                       "  -a        Show all processes except session leaders and\n"
                       "              processes not associated with a terminal\n"
                       "  -d        Show all processes except session leaders\n"
                       "  -e        Show all processes (same as -A)\n"
                       "  -F        Full format listing\n"
                       "  -f        Same as -F\n"
                       "  -h        Show this help and exit\n"
                       "  -l        Show long format listing\n"
                       "  -N        Negate the selection\n"
                       "  -r        Show running processes only\n"
                       "  -T        Show thread info\n"
                       "  -V        Print version and exit\n"
                       "  -v        Verbose output\n"
                       "  a         Show all processes with a terminal, or all processes\n"
                       "              if used with the x option\n"
                       "  g         Show all processes (same as -a)\n"
                       "  r         Show running processes only\n"
                       "  x         Show user's processes, even if not associated with\n"
                       "              a terminal, or all processes if used with the a option\n"
                       "\n", argv[0]);
                exit(0);
                break;

            case '?':
                break;

            default:
                abort();
        }
    }
  
    /*
     * Now process BSD arguments (those NOT preceded by '-').
     */
    while(optind < argc)
    {
        arg = argv[optind];
        
        while(*arg)
        {
            switch(*arg)
            {
                case 'a':
                case 'g':
                    show_all = 1;
                    show_uid = 1;
                    show_cmd_args = 1;
                    break;

                case 'l':
                    show_state = 1;
                    show_uid = 1;
                    show_cmd_args = 1;
                    show_ppid = 1;
                    show_nice = 1;
                    show_prio = 1;
                    show_rss = 1;
                    show_wchan = 1;
                    break;

                case 'r':
                    show_running_only = 1;
                    break;

                case 'x':
                    show_noterm = 1;
                    show_state = 1;
                    show_cmd_args = 1;
                    break;
            }
            
            arg++;
        }
        
        optind++;
    }
    
    if(negate)
    {
        show_all = !show_all;
        show_leaders = !show_leaders;
        show_noterm = !show_noterm;
        show_running_only = !show_running_only;
        show_threads = !show_threads;
        show_uid = !show_uid;

        show_ppid = !show_ppid;
        show_tname = !show_tname;
        show_time = !show_time;
        show_state = !show_state;
        show_cmd_args = !show_cmd_args;
        show_nice = !show_nice;
        show_prio = !show_prio;
        show_wchan = !show_wchan;
        show_majflt = !show_majflt;
        show_rss = !show_rss;
    }
    
    colw[COL_STATE  ] = 1;
    colw[COL_UID    ] = 3;
    colw[COL_TGID   ] = 3;
    colw[COL_TID    ] = 3;
    colw[COL_THREADS] = 4;
    colw[COL_PPID   ] = 4;
    colw[COL_NICE   ] = 2;
    colw[COL_PRIO   ] = 2;
    colw[COL_WCHAN  ] = 5;
    colw[COL_TTY    ] = 3;
    colw[COL_TIME   ] = 4;
    colw[COL_MAJFL  ] = 5;
    colw[COL_VSZ    ] = 3;
    colw[COL_RSS    ] = 3;
    colw[COL_CMD    ] = 3;
    //memset(colw, 0, sizeof(colw));
    memset(ttys, 0, sizeof(ttys));
    memset(uids, 0, sizeof(uids));
    
    if((clock_ticks = sysconf(_SC_CLK_TCK)) < 0)
    {
        clock_ticks = 100;
    }

    if((page_size = sysconf(_SC_PAGE_SIZE)) < 0)
    {
        page_size = 4096;
    }
    
    if(!(procdir = opendir("/proc")))
    {
        fprintf(stderr, "%s: failed to read /proc: %s\n", argv[0], strerror(errno));
        exit(1);
    }
    
    while((dent = readdir(procdir)) != NULL)
    {
        if(dent->d_name[0] >= '0' && dent->d_name[0] <= '9')
        {
            if((proc = do_entry(dent)))
            {
                if(proc_head == NULL)
                {
                    proc_head = proc;
                    proc_tail = proc;
                }
                else
                {
                    proc_tail->next = proc;
                    proc_tail = proc;
                }
            }
        }
    }
    
    closedir(procdir);
    
    if(proc_head)
    {
        print_header();
        print_processes();
    }
    
    exit(0);
}

