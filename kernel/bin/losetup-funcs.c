/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: losetup-funcs.c
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
 *  \file losetup-funcs.c
 *
 *  Internal functions used by the losetup utility.
 */

#define LODEV_FULL_OVERLAP      1
#define LODEV_PARTIAL_OVERLAP   2
#define LODEV_NO_OVERLAP        0

#undef major
#define major(d)            (((unsigned)d >> 8) & 0xff)

#undef minor
#define minor(d)            (((unsigned)d) & 0xff)


static int number_from_devname(char *lodev)
{
    int res = 0, i;

    if(strncmp(lodev, "/dev/loop", 9) == 0)
    {
        i = 9;
    }
    else if(strncmp(lodev, "loop", 4) == 0)
    {
        i = 4;
    }
    else
    {
        return -1;
    }

    while(lodev[i])
    {
        if(lodev[i] < '0' || lodev[i] > '9')
        {
            return -1;
        }

        res = (res * 10) + (lodev[i] - '0');
        i++;
    }

    return res;
}


static int lodev_get_info(char *path, struct loop_info64 *loinfo)
{
    int fd, res;

    if((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0)
    {
        fprintf(stderr, "%s: failed to open %s: %s\n", myname, path, strerror(errno));
        return fd;
    }

    if((res = ioctl(fd, LOOP_GET_STATUS64, loinfo)) < 0)
    {
        fprintf(stderr, "%s: failed ioctl(LOOP_GET_STATUS64) on %s: %s\n", myname, path, strerror(errno));
    }

    close(fd);

    return res;
}


static int lodev_get_info_and_blocksz(char *path, struct loop_info64 *loinfo, int *blocksz)
{
    int fd, res;

    if((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0)
    {
        fprintf(stderr, "%s: failed to open %s: %s\n", myname, path, strerror(errno));
        return fd;
    }

    if((res = ioctl(fd, LOOP_GET_STATUS64, loinfo)) < 0)
    {
        fprintf(stderr, "%s: failed ioctl(LOOP_GET_STATUS64) on %s: %s\n", myname, path, strerror(errno));
    }
    else if((res = ioctl(fd, BLKSSZGET, blocksz)) < 0)
    {
        fprintf(stderr, "%s: failed ioctl(BLKSSZGET) on %s: %s\n", myname, path, strerror(errno));
    }

    close(fd);

    return res;
}


static int lodev_set_info(char *path, struct loop_info64 *loinfo)
{
    int fd, res;

    if((fd = open(path, O_RDWR | O_CLOEXEC)) < 0)
    {
        fprintf(stderr, "%s: failed to open %s: %s\n", myname, path, strerror(errno));
        return fd;
    }

    res = ioctl(fd, LOOP_SET_STATUS64, loinfo);
    close(fd);

    return res;
}


static int find_overlap(char *filename, struct loop_info64 *loinfo, char *lodev_path)
{
    DIR *dir;
    struct dirent *d;
    struct stat st;
    char *name, buf[264];

    if(!filename || !*filename)
    {
        fprintf(stderr, "%s: empty/invalid filename\n", myname);
        errno = EINVAL;
        return -1;
    }

    if(stat(filename, &st) < 0)
    {
        fprintf(stderr, "%s: failed to stat %s: %s\n", myname, filename, strerror(errno));
        return -1;
    }

    if(!(dir = opendir("/dev")))
    {
        fprintf(stderr, "%s: failed to open /dev: %s\n", myname, strerror(errno));
        return -1;
    }

    while((d = readdir(dir)))
    {
        name = d->d_name;

        if(name[0] == '.' &&
           (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
        {
            continue;
        }

        if(name[0] != 'l' || name[1] != 'o' || name[2] != 'o' || name[3] != 'p')
        {
            continue;
        }

        sprintf(buf, "/dev/%s", name);

        if(lodev_get_info(buf, loinfo) < 0)
        {
            closedir(dir);
            return -1;
        }

        if(loinfo->lo_inode == st.st_ino && loinfo->lo_device == st.st_dev)
        {
            // full match in terms of offset and sizelimit
            if(loinfo->lo_sizelimit == sizelimit && loinfo->lo_offset == offset)
            {
                fprintf(stderr, "%s: overlapping device %s (full match)\n",
                                myname, buf);
                closedir(dir);
                strcpy(lodev_path, buf);
                return LODEV_FULL_OVERLAP;
            }

            // check for overlap
            if(loinfo->lo_sizelimit != 0 &&
               offset >= loinfo->lo_offset + loinfo->lo_sizelimit)
            {
                continue;
            }

            if(sizelimit != 0 && offset + sizelimit <= loinfo->lo_offset)
            {
                continue;
            }

            fprintf(stderr, "%s: overlapping device %s\n", myname, buf);
            closedir(dir);
            strcpy(lodev_path, buf);
            return LODEV_PARTIAL_OVERLAP;
        }
    }

    closedir(dir);
    return LODEV_NO_OVERLAP;
}


static void set_refname(struct loop_info64 *loinfo, char *refname)
{
    memset(loinfo->lo_file_name, 0, sizeof(loinfo->lo_file_name));

    if(strlen(refname) < LO_NAME_SIZE)
    {
        strcpy((char *)loinfo->lo_file_name, refname);
    }
    else
    {
        strncpy((char *)loinfo->lo_file_name, refname, LO_NAME_SIZE - 1);
        loinfo->lo_file_name[LO_NAME_SIZE - 1] = '\0';
    }
}


static int lodev_first_free(void)
{
    int fd, n;

    if((fd = open("/dev/loop-control", O_RDWR | O_CLOEXEC)) < 0)
    {
        err_exit_add_device("failed to open /dev/loop-control");
    }

    if((n = ioctl(fd, LOOP_CTL_GET_FREE)) < 0)
    {
        close(fd);
        err_exit_add_device("failed ioctl on /dev/loop-control");
    }

    close(fd);

    return n;
}


static void create_lodev(void)
{
    int res, fd, n;
    mode_t flags, mode;
    char lodev_path[32];
    struct loop_config config;

    // --find --noverlap filename
    if(loopname == NULL && nooverlap)
    {
        res = find_overlap(filename, &config.info, lodev_path);

        if(res < 0)
        {
            err_and_exit("failed to list loop devices");
        }
        else if(res == LODEV_PARTIAL_OVERLAP)
        {
            err_and_exit("overlapping device exists");
        }
        else if(res == LODEV_FULL_OVERLAP)
        {
            // try to reuse loop device
            // if it is initialised as read-only, we cannot change any params
            // so we fail here
            if((config.info.lo_flags & LO_FLAGS_READ_ONLY) && !(loflags & LO_FLAGS_READ_ONLY))
            {
                err_and_exit("overlapping read-only device exists");
            }

            config.info.lo_flags &= ~LO_FLAGS_AUTOCLEAR;

            if(lodev_set_info(lodev_path, &config.info) != 0)
            {
                err_and_exit("failed to reuse device");
            }

            if(showdev)
            {
                printf("%s\n", lodev_path);
            }

            return;
        }
    }

    // add the new device
    if(loopname)
    {
        n = number_from_devname(loopname);

        if(n < 0)
        {
            err_exit_add_device("invalid loop device name");
        }

        if((fd = open("/dev/loop-control", O_RDWR | O_CLOEXEC)) < 0)
        {
            err_exit_add_device("failed to open /dev/loop-control");
        }

        if(ioctl(fd, LOOP_CTL_ADD, n) < 0)
        {
            close(fd);
            err_exit_add_device("failed ioctl on /dev/loop-control");
        }

        close(fd);
    }

    // --noverlap /dev/loopX filename
    if(loopname && nooverlap)
    {
        res = find_overlap(filename, &config.info, lodev_path);

        if(res < 0)
        {
            err_and_exit("failed to list loop devices");
        }
        else if(res != LODEV_NO_OVERLAP)
        {
            err_and_exit("overlapping device exists");
        }
    }

    // add and configure device
    if(loopname == NULL)
    {
        n = lodev_first_free();
    }

    memset(&config, 0, sizeof(struct loop_config));

    if(set_offset)
    {
        config.info.lo_offset = offset;
    }

    if(set_sizelimit)
    {
        config.info.lo_sizelimit = sizelimit;
    }

    if(loflags)
    {
        config.info.lo_flags = loflags;
    }

    if(refname)
    {
        set_refname(&config.info, refname);
    }

    // TODO: we should canonicalize the pathname here
    if(config.info.lo_file_name[0] == 0)
    {
        set_refname(&config.info, filename);
    }

    // do the setup
    mode = (config.info.lo_flags & LO_FLAGS_READ_ONLY) ? O_RDONLY : O_RDWR;
    flags = O_CLOEXEC | (config.info.lo_flags & LO_FLAGS_DIRECT_IO ? O_DIRECT : 0);
    sprintf(lodev_path, "/dev/loop%d", n);
    errno = 0;

    if((fd = open(filename, mode | flags)) < 0)
    {
        if(mode != O_RDONLY && (errno == EROFS || errno == EACCES))
        {
            mode = O_RDONLY;
            fd = open(filename, mode | flags);
        }

        if(fd < 0)
        {
            err_exit_add_device("failed to open backing file");
        }
    }

    if(mode == O_RDONLY)
    {
        config.info.lo_flags |= LO_FLAGS_READ_ONLY;
    }
    else
    {
        config.info.lo_flags &= ~LO_FLAGS_READ_ONLY;
    }

    if((res = open(lodev_path, O_RDWR | O_CLOEXEC)) < 0)
    {
        close(fd);
        err_exit_add_device("failed to open loop device");
    }

    config.fd = fd;

    if(set_blocksz)
    {
        config.block_size = blocksz;
    }

    if(ioctl(res, LOOP_CONFIGURE, &config) < 0)
    {
        close(fd);
        close(res);
        err_exit_add_device("failed to configure loop device");
    }

    if(showdev)
    {
        printf("%s\n", lodev_path);
    }

    close(fd);
    close(res);
}


static int delete_lodev(char *loopname)
{
    int fd, n = number_from_devname(loopname);
    char lodev_path[32];

    if(n < 0)
    {
        fprintf(stderr, "%s: invalid loop device: %s\n", myname, loopname);
        return -1;
    }

    sprintf(lodev_path, "/dev/loop%d", n);

    if((fd = open(lodev_path, O_RDWR | O_CLOEXEC)) < 0)
    {
        fprintf(stderr, "%s: failed to open loop device: %s\n", myname, strerror(errno));
        return -1;
    }

    if(ioctl(fd, LOOP_CLR_FD, 0) < 0)
    {
        close(fd);
        fprintf(stderr, "%s: failed to remove loop device: %s\n", myname, strerror(errno));
        return -1;
    }

    close(fd);
    return 0;
}


static int delete_all_lodevs(void)
{
    DIR *dir;
    struct dirent *d;
    char *name, buf[264];
    int res = 0;

    if(!(dir = opendir("/dev")))
    {
        fprintf(stderr, "%s: failed to open /dev: %s\n", myname, strerror(errno));
        return -1;
    }

    while((d = readdir(dir)))
    {
        name = d->d_name;

        if(name[0] == '.' &&
           (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
        {
            continue;
        }

        if(name[0] != 'l' || name[1] != 'o' || name[2] != 'o' || name[3] != 'p')
        {
            continue;
        }

        sprintf(buf, "/dev/%s", name);
        res |= delete_lodev(buf);
    }

    closedir(dir);
    return res;
}


static int set_lodev_capacity(char *loopname)
{
    int fd, n = number_from_devname(loopname);
    char lodev_path[32];

    if(n < 0)
    {
        fprintf(stderr, "%s: invalid loop device: %s\n", myname, loopname);
        return -1;
    }

    sprintf(lodev_path, "/dev/loop%d", n);

    if((fd = open(lodev_path, O_RDWR | O_CLOEXEC)) < 0)
    {
        fprintf(stderr, "%s: failed to open loop device: %s\n", myname, strerror(errno));
        return -1;
    }

    if(ioctl(fd, LOOP_SET_CAPACITY, 0) < 0)
    {
        close(fd);
        fprintf(stderr, "%s: failed to configure loop device: %s\n", myname, strerror(errno));
        return -1;
    }

    close(fd);
    return 0;
}


static int set_lodev_directio(char *loopname)
{
    int fd, n = number_from_devname(loopname);
    char lodev_path[32];

    if(n < 0)
    {
        fprintf(stderr, "%s: invalid loop device: %s\n", myname, loopname);
        return -1;
    }

    sprintf(lodev_path, "/dev/loop%d", n);

    if((fd = open(lodev_path, O_RDWR | O_CLOEXEC)) < 0)
    {
        fprintf(stderr, "%s: failed to open loop device: %s\n", myname, strerror(errno));
        return -1;
    }

    if(ioctl(fd, LOOP_SET_DIRECT_IO, directio) < 0)
    {
        close(fd);
        fprintf(stderr, "%s: failed to configure loop device: %s\n", myname, strerror(errno));
        return -1;
    }

    close(fd);
    return 0;
}


static int set_lodev_blocksz(char *loopname)
{
    int fd, n = number_from_devname(loopname);
    char lodev_path[32];

    if(n < 0)
    {
        fprintf(stderr, "%s: invalid loop device: %s\n", myname, loopname);
        return -1;
    }

    sprintf(lodev_path, "/dev/loop%d", n);

    if((fd = open(lodev_path, O_RDWR | O_CLOEXEC)) < 0)
    {
        fprintf(stderr, "%s: failed to open loop device: %s\n", myname, strerror(errno));
        return -1;
    }

    if(ioctl(fd, LOOP_SET_BLOCK_SIZE, blocksz) < 0)
    {
        close(fd);
        fprintf(stderr, "%s: failed to configure loop device: %s\n", myname, strerror(errno));
        return -1;
    }

    close(fd);
    return 0;
}


static void print_table_header(void)
{
    int i;

    for(i = 0; i < colcount; i++)
    {
        switch(colhdrs[i])
        {
            case COL_NAME:          printf("NAME         "); break;
            case COL_AUTOCLEAR:     printf("AUTOCLEAR "); break;
            case COL_BACK_FILE:     printf("BACK-FILE%*s", 56, " "); break;
            case COL_BACK_INO:      printf("BACK-INO    "); break;
            case COL_BACK_MAJMIN:   printf("BACK-MAJ:MIN "); break;
            case COL_MAJMIN:        printf("MAJ:MIN "); break;
            case COL_OFFSET:        printf("OFFSET      "); break;
            case COL_PARTSCAN:      printf("PARTSCAN "); break;
            case COL_RO:            printf("RO "); break;
            case COL_SIZELIMIT:     printf("SIZELIMIT   "); break;
            case COL_DIO:           printf("DIO "); break;
            case COL_LOGSEC:        printf("LOG-SEC "); break;
        }
    }

    printf("\n");
}


static void print_table_row(struct loop_info64 *loinfo, int n, int blocksz)
{
    int i;

    for(i = 0; i < colcount; i++)
    {
        switch(colhdrs[i])
        {
            case COL_NAME:
                printf("/dev/loop%-3d ", n);
                break;

            case COL_AUTOCLEAR:
                printf("%9d ", !!(loinfo->lo_flags & LO_FLAGS_AUTOCLEAR));
                break;

            case COL_BACK_FILE:
                loinfo->lo_file_name[LO_NAME_SIZE - 2] = '*';
                loinfo->lo_file_name[LO_NAME_SIZE - 1] = '\0';
                printf("%-64s", (char *)loinfo->lo_file_name);
                break;

            case COL_BACK_INO:
                printf("%11lu ", loinfo->lo_inode);
                break;

            case COL_BACK_MAJMIN:
                printf("%8u:%-3u ", major(loinfo->lo_device), minor(loinfo->lo_device));
                break;

            case COL_MAJMIN:
                printf("%3u:%-3u ", LODEV_MAJ, n);
                break;

            case COL_OFFSET:
                printf("%11lu ", loinfo->lo_offset);
                break;

            case COL_PARTSCAN:
                printf("%8d ", !!(loinfo->lo_flags & LO_FLAGS_PARTSCAN));
                break;

            case COL_RO:
                printf("%2d ", !!(loinfo->lo_flags & LO_FLAGS_READ_ONLY));
                break;

            case COL_SIZELIMIT:
                printf("%11lu ", loinfo->lo_sizelimit);
                break;

            case COL_DIO:
                printf("%3d ", !!(loinfo->lo_flags & LO_FLAGS_DIRECT_IO));
                break;

            case COL_LOGSEC:
                printf("%7u ", blocksz);
                break;
        }
    }

    printf("\n");
}


static void print_loopdev(struct loop_info64 *loinfo, int n)
{
    loinfo->lo_file_name[LO_NAME_SIZE - 2] = '*';
    loinfo->lo_file_name[LO_NAME_SIZE - 1] = '\0';

    printf("/dev/loop%d: [%04d]:%u (%s)", 
            n, major(loinfo->lo_device), minor(loinfo->lo_device),
            (char *)loinfo->lo_file_name);

    if(loinfo->lo_offset != 0)
    {
        printf(", offset %ju", loinfo->lo_offset);
    }

    if(loinfo->lo_sizelimit != 0)
    {
        printf(", sizelimit %ju", loinfo->lo_sizelimit);
    }

    printf("\n");
}


static int show_list(char *loopname, char *filename)
{
    struct loop_info64 loinfo;
    char *name, buf[264];
    int n, blocksz, res = 0;

    if(loopname)
    {
        // list only the given loop device
        n = number_from_devname(loopname);
        sprintf(buf, "/dev/loop%d", n);

        if(n < 0)
        {
            fprintf(stderr, "%s: invalid loop device: %s\n", myname, loopname);
            return -1;
        }

        if(lodev_get_info_and_blocksz(buf, &loinfo, &blocksz) < 0)
        {
            return -1;
        }

        if(list)
        {
            print_table_header();
            print_table_row(&loinfo, n, blocksz);
        }
        else
        {
            print_loopdev(&loinfo, n);
        }
    }
    else
    {
        // list all devices
        struct stat __st, *st = &__st;
        DIR *dir;
        struct dirent *d;

        if(!filename || !*filename || stat(filename, st) < 0)
        {
            st = NULL;
        }

        if(!(dir = opendir("/dev")))
        {
            fprintf(stderr, "%s: failed to open /dev: %s\n", myname, strerror(errno));
            return -1;
        }

        if(list)
        {
            print_table_header();
        }

        while((d = readdir(dir)))
        {
            name = d->d_name;

            if(name[0] == '.' &&
               (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
            {
                continue;
            }

            if(name[0] != 'l' || name[1] != 'o' || name[2] != 'o' || name[3] != 'p')
            {
                continue;
            }

            sprintf(buf, "/dev/%s", name);
            n = number_from_devname(name);

            if(n < 0)
            {
                fprintf(stderr, "%s: invalid loop device: %s\n", myname, name);
                res = -1;
                continue;
            }

            if(lodev_get_info_and_blocksz(buf, &loinfo, &blocksz) < 0)
            {
                res = -1;
                continue;
            }

            if(st)
            {
                if(loinfo.lo_inode != st->st_ino || loinfo.lo_device != st->st_dev)
                {
                    continue;
                }
            }

            if(list)
            {
                print_table_row(&loinfo, n, blocksz);
            }
            else
            {
                print_loopdev(&loinfo, n);
            }
        }

        closedir(dir);
    }

    return res;
}

