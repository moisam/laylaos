#ifndef _KERNEL_DIRENT_H_
#define	_KERNEL_DIRENT_H_

#include <sys/types.h>       // ino_t
#include <kernel/vfs.h>

// struct returned by finddir() and readdir()
// See: https://www.man7.org/linux/man-pages/man3/readdir.3.html
struct dirent
{
   ino_t          d_ino;       /* Inode number */
   off_t          d_off;       /* Not an offset; see manpages */
   unsigned short d_reclen;    /* Length of this record */
/* types for d_type (similar to GLibC values) */
#define DT_UNKNOWN	0
#define DT_FIFO		1
#define DT_CHR		2
#define DT_DIR		4
#define DT_BLK		6
#define DT_REG		8
#define DT_LNK		10
#define DT_SOCK		12
#define DT_WHT		14
   unsigned char  d_type;      /* Type of file; not supported
                                  by all filesystem types */
   //char           d_name[NAME_MAX + 1]; /* Null-terminated filename */
   char           d_name[]; /* Null-terminated filename */
};


typedef struct
{
    struct fs_node_t *dd_node;		/* directory file */
    off_t dd_fpos;		/* position in file */
    int dd_loc;		/* position in buffer */
    int dd_seek;
    char *dd_buf;	/* buffer */
    int dd_len;		/* buffer length */
    int dd_size;	/* amount of data in buffer */
    //_LOCK_RECURSIVE_T dd_lock;
} DIR;


DIR	*opendir(const char *);
struct dirent *readdir(DIR *);
int	 closedir(DIR *);
int getdents(DIR *dirp);

#endif /* _KERNEL_DIRENT_H_ */
