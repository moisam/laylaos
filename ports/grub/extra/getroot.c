#include <config-util.h>
#include <config.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include <grub/util/misc.h>

#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/disk.h>
#include <grub/emu/misc.h>
#include <grub/emu/hostdisk.h>
#include <grub/emu/getroot.h>


char *
grub_util_part_to_disk (const char *os_dev, struct stat *st,
			int *is_part)
{
  char *path;

  if (! S_ISBLK (st->st_mode))
    {
      *is_part = 0;
      return xstrdup (os_dev);
    }

  path = xmalloc (PATH_MAX);

  if (! realpath (os_dev, path))
    return NULL;

  if (strncmp ("/dev/", path, 5) == 0)
    {
      char *p = path + 5;

      /* If this is an IDE disk.  */
      if (strncmp ("ide/", p, 4) == 0)
	{
	  p = strstr (p, "part");
	  if (p)
	    {
	      *is_part = 1;
	      strcpy (p, "disc");
	    }

	  return path;
	}

      /* If this is a SCSI disk.  */
      if (strncmp ("scsi/", p, 5) == 0)
	{
	  p = strstr (p, "part");
	  if (p)
	    {
	      *is_part = 1;
	      strcpy (p, "disc");
	    }

	  return path;
	}

      /* If this is an IDE, SCSI or Virtio disk.  */
      if ((strncmp ("hd", p, 2) == 0
	   || strncmp ("vd", p, 2) == 0
	   || strncmp ("sd", p, 2) == 0)
	  && p[2] >= 'a' && p[2] <= 'z')
	{
	  char *pp = p + 2;
	  while (*pp >= 'a' && *pp <= 'z')
	    pp++;
	  if (*pp)
	    *is_part = 1;
	  /* /dev/[hsv]d[a-z]+[0-9]* */
	  *pp = '\0';
	  return path;
	}

      /* If this is a loop device */
      if ((strncmp ("loop", p, 4) == 0) && p[4] >= '0' && p[4] <= '9')
	{
	  char *pp = p + 4;
	  while (*pp >= '0' && *pp <= '9')
	    pp++;
	  if (*pp == 'p')
	    *is_part = 1;
	  /* /dev/loop[0-9]+p[0-9]* */
	  *pp = '\0';
	  return path;
	}
    }

  return path;
}

enum grub_dev_abstraction_types
grub_util_get_dev_abstraction_os (const char *os_dev __attribute__((unused)))
{
  return GRUB_DEV_ABSTRACTION_NONE;
}

int
grub_util_pull_device_os (const char *os_dev __attribute__ ((unused)),
			  enum grub_dev_abstraction_types ab __attribute__ ((unused)))
{
  return 0;
}

char *
grub_util_get_grub_dev_os (const char *os_dev __attribute__ ((unused)))
{
  return NULL;
}

