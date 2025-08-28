#include <config-util.h>

#include <grub/disk.h>
#include <grub/partition.h>
#include <grub/msdos_partition.h>
#include <grub/types.h>
#include <grub/err.h>
#include <grub/emu/misc.h>
#include <grub/emu/hostdisk.h>
#include <grub/emu/getroot.h>
#include <grub/emu/exec.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

# include <sys/ioctl.h>         /* ioctl */
# include <sys/mount.h>
# include <sys/hdreg.h>


grub_int64_t
grub_util_get_fd_size_os (grub_util_fd_t fd, const char *name, unsigned *log_secsize)
{
  unsigned long long nr;
  unsigned sector_size, log_sector_size;

  if (ioctl (fd, BLKGETSIZE64, &nr))
    return -1;

  if (ioctl (fd, BLKSSZGET, &sector_size))
    return -1;

  if (sector_size & (sector_size - 1) || !sector_size)
    return -1;
  log_sector_size = grub_log2ull (sector_size);

  if (log_secsize)
    *log_secsize = log_sector_size;

  if (nr & ((1 << log_sector_size) - 1))
    grub_util_error ("%s", _("unaligned device size"));

  return nr;
}

grub_disk_addr_t
grub_util_find_partition_start_os (const char *dev)
{
  grub_util_fd_t fd;
  struct hd_geometry hdg;

  fd = open (dev, O_RDONLY);
  if (fd == -1)
    {
      grub_error (GRUB_ERR_BAD_DEVICE, N_("cannot open `%s': %s"),
		  dev, strerror (errno));
      return 0;
    }

  if (ioctl (fd, HDIO_GETGEO, &hdg))
    {
      grub_error (GRUB_ERR_BAD_DEVICE,
		  "cannot get disk geometry of `%s'", dev);
      close (fd);
      return 0;
    }

  close (fd);

  return hdg.start;
}

void
grub_hostdisk_flush_initial_buffer (const char *os_dev)
{
  grub_util_fd_t fd;
  struct stat st;

  fd = open (os_dev, O_RDONLY);
  if (fd >= 0 && fstat (fd, &st) >= 0 && S_ISBLK (st.st_mode))
    ioctl (fd, BLKFLSBUF, 0);
  if (fd >= 0)
    close (fd);
}

