/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: ls.c
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
 *  \file ls.c
 *
 *  A simple file and directory listing program.
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <stdint.h>


/* output options */
int LIST_ALL = 0;
int IGNORE_BACKUPS = 0;
int LIST_DIRS = 1;
int PRINT_INODE = 0;
int PRINT_SIZE = 1;
int HUMAN_READABLE_SIZES = 1;
int multiple_args = 0;
int first_arg = 0;
char ls_ver[] = "1.0";

struct stat statbuf;
struct passwd *pwd;
struct group *grp;
struct tm *tm;
char datestring[128];

#define GREEN_FOREGROUND            32      // green fg
#define BLUE_FOREGROUND             34      // blue fg
#define DEFAULT_FOREGROUND          38      // default fg
#define DEFAULT_BACKGROUND          48      // default bg


void set_screen_colors(int FG, int BG) 
{
  fprintf(stdout, "\e[%d;%dm", FG, BG);    //control sequence to set screen color
}


void parse_line_args(int argc, char **argv) 
{
    int c;
    
    static struct option long_options[] =
    {
        {"help",            no_argument,         0,  'h'},
        {"all",             no_argument,         0,  'a'},
        {"ignore-backups",  no_argument,         0,  'B'},
        {"directory",       no_argument,         0,  'd'},
        {"inode",           no_argument,         0,  'i'},
        //{"human-readable",  no_argument,         0,  'h'},
        {"size",            no_argument,         0,  's'},
        {"version",         no_argument,         0,  'v'},
        {0, 0, 0, 0}
    };

    //optind = 1;
  
    while(1)
    {
        int option_index = 0;
        
        c = getopt_long(argc, argv, "haBdisv", long_options, &option_index);

        if(c == -1)
        {
            break;    //end of options
        }
    
        switch(c)
        {
            case 0:
                break;
            
            case 'a':
                LIST_ALL = 1;
                break;
            
            case 'B':
                IGNORE_BACKUPS = 1;
                break;
            
            case 'd':
                LIST_DIRS = 0;
                break;

            /*
            case 'h':
                HUMAN_READABLE_SIZES = 1;
                break;
            */
            
            case 'i':
                PRINT_INODE = 1;
                break;
            
            case 's':
                PRINT_SIZE = 1;
                break;
            
            case 'v':
                printf("%s\n", ls_ver);
                exit(0);

            case 'h':    //show program help
                printf("ls utility for Layla OS, Version %s\n\n", ls_ver);
                printf("Usage: %s [options] [file/dir ...]\n\n", argv[0]);
                printf("Options:\n");
                printf("  -a, --all               List all files & dirs, including hidden ones\n");
                printf("  -d, --directory         List the names of dirs, not their contents\n");
                printf("  -B, --ignore-backups    Ignore files ending in '~'\n");
                printf("  -i, --inode             Print inode number\n");
                printf("  -h, --help              Show help (this) and exit\n");
                printf("  -s, --size              Print file size\n");
                printf("  -v, --version           Print version and exit\n");
                exit(0);

            case '?':
                break;
      
            default:
                fprintf(stderr, "ls: unknown option: %c\n", c);
                abort();
        }
    }
}


char mbuf[16];

void print_mode(mode_t mode)
{
    int i, j;

    if(S_ISDIR(mode))
    {
        mbuf[0] = 'd';
    }
    else if(S_ISCHR(mode))
    {
        mbuf[0] = 'c';
    }
    else if(S_ISBLK(mode))
    {
        mbuf[0] = 'b';
    }
    else if(S_ISLNK(mode))
    {
        mbuf[0] = 'l';
    }
    else if(S_ISSOCK(mode))
    {
        mbuf[0] = 's';
    }
    else if(S_ISFIFO(mode))
    {
        mbuf[0] = 'p';
    }
    else
    {
        mbuf[0] = '-';
    }

    mode &= 0777;
    
    for(i = 0, j = 1; i < 3; i++)
    {
        mbuf[j++] = (mode & S_IRUSR) ? 'r' : '-';
        mbuf[j++] = (mode & S_IWUSR) ? 'w' : '-';
        mbuf[j++] = (mode & S_IXUSR) ? 'x' : '-';
        mode <<= 3;
    }
    
    mbuf[j] = '\0';
    printf("%s ", mbuf);
}


void print_item(char *path)
{
    if(path[0] == '.' && !LIST_ALL) //ignore those starting with "."
    {
        return;
    }
    
    set_screen_colors(DEFAULT_FOREGROUND, DEFAULT_BACKGROUND);

    if(PRINT_INODE)
    {
        printf("%8ld ", statbuf.st_ino);
    }

    /* Print out type, permissions, and number of links. */
    print_mode(statbuf.st_mode);
    printf("%4ld", statbuf.st_nlink);

    /* Print out owner's name if it is found using getpwuid(). */
    if((pwd = getpwuid(statbuf.st_uid)) != NULL)
    {
        printf(" %-8.8s", pwd->pw_name);
    }
    else
    {
        printf(" %-8d", statbuf.st_uid);
    }
    
    /* Print out group name if it is found using getgrgid(). */
    if((grp = getgrgid(statbuf.st_gid)) != NULL)
    {
        printf(" %-8.8s", grp->gr_name);
    }
    else
    {
        printf(" %-8d", statbuf.st_gid);
    }
    
    /* Print size of file. */
    if(PRINT_SIZE)
    {
        if(HUMAN_READABLE_SIZES)
        {
            unsigned int sz = (statbuf.st_size >= 1024*1024) ?
                                (unsigned int)(statbuf.st_size/1024/1024) :
                                ((statbuf.st_size >= 1024) ?
                                    (unsigned int)(statbuf.st_size/1024) :
                                    (unsigned int)(statbuf.st_size));
            
            char *unit = (statbuf.st_size >= 1024*1024) ? "MB" :
                         ((statbuf.st_size >= 1024) ? "kB" : "B ");
            
            printf(" %6d%s", sz, unit);
        }
        else
        {
            printf(" %9jd", (intmax_t)statbuf.st_size);
        }
    }

    tm = localtime(&statbuf.st_mtime);

    /* Get localized date string. */
    strftime(datestring, sizeof(datestring), nl_langinfo(D_T_FMT), tm);
    printf(" %s", datestring);

    /* Print file name */
    if(S_ISDIR(statbuf.st_mode))
    {
        set_screen_colors(BLUE_FOREGROUND, DEFAULT_BACKGROUND);
    }
    else if(mbuf[3] == 'x' || mbuf[6] == 'x' || mbuf[9] == 'x')
    {
        set_screen_colors(GREEN_FOREGROUND, DEFAULT_BACKGROUND);
    }
    else 
    {
        set_screen_colors(DEFAULT_FOREGROUND, DEFAULT_BACKGROUND);
    }

    printf(" %s\n", path);


      /*
      DIR *ditem = (DIR *)item;
      time_t t = (time_t)ditem->mtime;
      struct tm *time = gmtime(&t);
      printf("\t");
      printf("%u", time->tm_mday);
      if(time->tm_mday < (unsigned)10) printf(" ");
      printf(" %s %u %u:%u:%u  ",     month_str[time->tm_mon],
                     time->tm_year+1900,
                     time->tm_hour,
                     time->tm_min,
                     time->tm_sec);
      printf("\n");
      */
}


int ls(char *path)
{
    DIR *dir;
    struct dirent *dp;

    /* Get entry's information. */
    if(stat(path, &statbuf) == -1)
    {
        fprintf(stderr, "ls: failed to open '%s': %s\n", path, strerror(errno));
        return -1;
    }
    
    if(!S_ISDIR(statbuf.st_mode) || !LIST_DIRS)
    {
        print_item(path);
        return 0;
    }
    
    if(multiple_args)
    {
        if(optind != first_arg)
        {
            putchar('\n');
        }
        
        printf("%s:\n", path);
    }
    
    errno = 0;
    
    if((dir = opendir(path)) == NULL)
    {
        fprintf(stderr, "ls: failed to open dir '%s': %s\n", path, strerror(errno));
        return -1;
    }

    printf("***\n");
    
    while((dp = readdir(dir)) != NULL)
    {
        printf("*** '%s'\n", dp->d_name);
        if(dp->d_name[0] == '.' && !LIST_ALL) //ignore those starting with "."
        {
            continue;
        }

        /* Get entry's information. */
        if(stat(dp->d_name, &statbuf) == -1)
        {
            fprintf(stderr, "ls: failed to open '%s': %s\n", dp->d_name, strerror(errno));
            continue;
        }
        
        print_item(dp->d_name);
    }

    closedir(dir);
    return 0;
}


int main(int argc, char **argv) 
{
    int exit_res = 0;
    
    //int i;
    //for(i = 0; i < argc; i++) printf("arg[%d] '%s'\n", i, argv[i]);
  
    parse_line_args(argc, argv);

    /*
     * No arguments. Read '.'
     */
    if(optind >= argc)
    {
        exit_res = ls(".") ? 1 : 0;
        set_screen_colors(DEFAULT_FOREGROUND, DEFAULT_BACKGROUND);
        return exit_res;
    }
  
    multiple_args = (optind < argc-1);
    first_arg = optind;
    
    while(optind < argc)
    {
        if(ls(argv[optind]) != 0)
        {
            exit_res = 1;
        }
        
        optind++;
    }
  
    set_screen_colors(DEFAULT_FOREGROUND, DEFAULT_BACKGROUND);
    return exit_res;
}
