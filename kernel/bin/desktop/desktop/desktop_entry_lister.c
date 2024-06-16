/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: desktop_entry_lister.c
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
 *  \file desktop_entry_lister.c
 *
 *  This file provides common functions that are used by the desktop and the
 *  top-panel Applications widget (and maybe more apps/widgets in the future).
 *  It must not be compiled alone, but included in one of the other executables.
 */

#define BUFSZ                   0x1000

char *app_categories[32];
int app_category_count = 0;


static void get_default_categories(void)
{
    app_categories[app_category_count++] = "Accessories";
    app_categories[app_category_count++] = "Games";
    app_categories[app_category_count++] = "Graphics";
    app_categories[app_category_count++] = "Internet";
    app_categories[app_category_count++] = "Office";
    app_categories[app_category_count++] = "Sound & Video";
    app_categories[app_category_count++] = "System Tools";
    app_categories[app_category_count++] = "Utilities";
}


void get_app_categories(char *path)
{
    FILE *file;
    char *buf, *p;
    size_t len;

    memset(app_categories, 0, sizeof(app_categories));
    app_category_count = 0;

    if(!(buf = malloc(BUFSZ)))
    {
        get_default_categories();
        return;
    }

    if(!(file = fopen(path ? path : DEFAULT_APP_CATEGORIES_PATH, "r")))
    {
        free(buf);
        get_default_categories();
        return;
    }

    while(fgets(buf, BUFSZ, file))
    {
        // skip empty spaces (and lines)
        p = buf;

        while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        {
            p++;
        }

        if(*p == '\0')
        {
            continue;
        }

        len = strlen(p);

        // remove trailing newline
        if(p[len - 1] == '\n')
        {
            p[len - 1] = '\0';
        }

        app_categories[app_category_count++] = strdup(p);
    }

    free(buf);
    fclose(file);

    if(app_category_count == 0)
    {
        get_default_categories();
    }
}


char *getval(volatile char *s)
{
    char *s2;

    while(*s == ' ' || *s == '\t')
    {
        s++;
    }
    
    if(*s++ != '=')
    {
        return NULL;
    }

    while(*s == ' ' || *s == '\t')
    {
        s++;
    }
    
    s2 = (char *)s + strlen((char *)s);
    
    while(--s2 > s)
    {
        if(*s2 == '\n' || *s2 == '\r')
        {
            *s2 = '\0';
        }
    }
    
    return (char *)s;
}


void free_tmp(char *name, char *command, char *iconpath, char *icon)
{
    if(name)
    {
        free(name);
    }

    if(command)
    {
        free(command);
    }

    if(iconpath)
    {
        free(iconpath);
    }

    if(icon)
    {
        free(icon);
    }
}


struct app_entry_t *do_entry(char *filename, char *buf, size_t bufsz)
{
    FILE *file;
    volatile char *b;
    volatile int inited, ob, kw;
    char *val;
    char *name = NULL, *command = NULL, *iconpath = NULL, *icon = NULL;
    int flags, category;
    struct app_entry_t *res;
    
    if(!(file = fopen(filename, "r")))
    {
        return NULL;
    }
    
    inited = 0;
    ob = 0;
    kw = 0;
    flags = 0;
    category = 0;

    while(fgets(buf, bufsz, file))
    {
        b = buf;
        
        while(*b)
        {
            // skip commented lines
            if(*b == '#')
            {
                break;
            }
            else if(*b == ' ' || *b == '\t')
            {
                b++;
            }
            else if(*b == '\n' || *b == '\r')
            {
                ob = 0;
                b++;
            }
            else if(*b == '[')
            {
                ob = 1;
                b++;
            }
            else if(strncasecmp((char *)b, "Desktop Entry", 13) == 0)
            {
                if(ob)
                {
                    kw = 1;
                }

                b += 13;
            }
            else if(*b == ']')
            {
                if(ob && kw)
                {
                    inited = 1;
                    b++;
                    continue;
                }

                ob = 0;
                kw = 0;
                b++;
            }
            else
            {
                if(!inited)
                {
                    break;
                }
                
                if(strncasecmp((char *)b, "name", 4) == 0)
                {
                    if(!(val = getval(b + 4)))
                    {
                        name = strdup("Untitled");
                    }
                    else
                    {
                        name = strdup(val);
                    }
                }
                else if(strncasecmp((char *)b, "command", 7) == 0)
                {
                    if((val = getval(b + 7)))
                    {
                        command = strdup(val);
                    }
                }
                else if(strncasecmp((char *)b, "iconpath", 8) == 0)
                {
                    if(!(val = getval(b + 8)))
                    {
                        iconpath = strdup(DEFAULT_ICON_PATH);
                    }
                    else if(strcasecmp(val, "default") == 0)
                    {
                        iconpath = strdup(DEFAULT_ICON_PATH);
                    }
                    else
                    {
                        iconpath = strdup(val);
                    }
                }
                else if(strncasecmp((char *)b, "icon", 4) == 0)
                {
                    if((val = getval(b + 4)))
                    {
                        icon = strdup(val);
                    }
                }
                else if(strncasecmp((char *)b, "showondesktop", 13) == 0)
                {
                    if((val = getval(b + 13)))
                    {
                        if(strcasecmp(val, "yes") == 0 ||
                           strcasecmp(val, "true") == 0)
                        {
                            flags |= APPLICATION_FLAG_SHOW_ON_DESKTOP;
                        }
                    }
                }
                else if(strncasecmp((char *)b, "category", 8) == 0)
                {
                    if((val = getval(b + 8)))
                    {
                        // Compare the category name we found to the list of
                        // default names to convert the name to an index
                        for(category = 0;
                            category < app_category_count;
                            category++)
                        {
                            if(strcasecmp(val, app_categories[category]) == 0)
                            {
                                break;
                            }
                        }
                        
                        // Reached the end and couldn't find a match.
                        // Backoff one slot (to the Utilities category).
                        if(category == app_category_count)
                        {
                            category--;
                        }
                    }
                }

                break;
            }
        }
        
        if(ob || kw)
        {
            ob = 0;
            kw = 0;
        }
    }
    
    fclose(file);

    if(!name || !command || !iconpath || !icon)
    {
        free_tmp(name, command, iconpath, icon);
        return NULL;
    }

    if(!(res = malloc(sizeof(struct app_entry_t))))
    {
        free_tmp(name, command, iconpath, icon);
        return NULL;
    }

    //printf("%s, %s, %s, %s\n", name, command, iconpath, icon);
    
    A_memset(res, 0, sizeof(struct app_entry_t));
    res->name = name;
    res->command = command;
    res->iconpath = iconpath;
    res->icon = icon;
    res->flags = flags;
    res->category = category;
    
    return res;
}


static int ftree(char *path, char *p)
{
    char *buf, *ext;
    DIR *dirp;
    struct dirent *ent;
    struct stat st;
    struct app_entry_t *desktop_entry;

    // ensure we have a category list before we begin
    get_app_categories(NULL);

    if(!(buf = malloc(BUFSZ)))
    {
        return -1;
    }
    
    snprintf(p, PATH_MAX, "%s", path);
    
    if(stat(p, &st) == -1)
    {
        free(buf);
        return -1;
    }
    
    if((dirp = opendir(p)) == NULL)
    {
        free(buf);
        return -1;
    }
    
    for(;;)
    {
        errno = 0;
        
        if((ent = readdir(dirp)) == NULL)
        {
            closedir(dirp);
            free(buf);
            return errno ? -1 : 0;
        }
        
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }
        
        snprintf(p, PATH_MAX, "%s/%s", path, ent->d_name);
        
        if(stat(p, &st) == -1)
        {
            continue;
        }
        
        if(S_ISREG(st.st_mode))
        {
            if(!(ext = strstr(p, ".entry")))
            {
                continue;
            }
            
            if(ext[6] != '\0')
            {
                continue;
            }

            if((desktop_entry = do_entry(p, buf, BUFSZ)))
            {
                if(first_entry == NULL)
                {
                    first_entry = desktop_entry;
                    last_entry = desktop_entry;
                }
                else
                {
                    last_entry->next = desktop_entry;
                    desktop_entry->prev = last_entry;
                    last_entry = desktop_entry;
                }
            }
        }
    }
    
    closedir(dirp);
    free(buf);
    return 0;
}

