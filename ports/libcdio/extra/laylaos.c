/*
  Copyright (C) 2025 Mohammed Isam <mohammed_isam1984@yahoo.com>
  Copyright (C) 2008, 2010-2012, 2017, 2018 Rocky Bernstein <rocky@gnu.org>
  Copyright (C) 2014 Robert Kausch <robert.kausch@freac.org>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Changes up to version 0.76 */
/*
 * Copyright (c) 2003
 *      Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * LaylaOS support for libcdio.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#endif

#include <cdio/sector.h>
#include <cdio/util.h>
#include "cdio_assert.h"
#include "cdio_private.h"

#ifndef USE_MMC_SUBCHANNEL
#define USE_MMC_SUBCHANNEL 0
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_LAYLAOS_CDROM
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <mntent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/cdio.h>
#include <sys/scsiio.h>
#include <sys/sysctl.h>

#define TOTAL_TRACKS (_obj->tochdr.ending_track \
                      - _obj->tochdr.starting_track + 1)
#define FIRST_TRACK_NUM (_obj->tochdr.starting_track)

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef enum {
  _AM_NONE,
  _AM_IOCTL,
  _AM_READ_CD,
  _AM_MMC_RDWR,
  _AM_MMC_RDWR_EXCL,
} access_mode_t;

typedef struct {
  /* Things common to all drivers like this.
     This must be first. */
  generic_img_private_t gen;

  access_mode_t access_mode;

  bool toc_valid;
  struct ioc_toc_header tochdr;
  struct cd_toc_entry tocent[100];

  bool sessionformat_valid;
  int sessionformat[100]; /* format of the session the track is in */
} _img_private_t;

/* Check a drive to see if it is a CD-ROM
   Return 1 if a CD-ROM. 0 if it exists but isn't a CD-ROM drive
   and -1 if no device exists .
*/
static bool
is_cdrom_laylaos(const char *drive, char *mnttype)
{
  bool is_cd=false;
  int cdfd;

  /* If it doesn't exist, return -1 */
  if ( !cdio_is_device_quiet_generic(drive) ) {
    return(false);
  }

  /* If it does exist, verify that it's an available CD-ROM */
  cdfd = open(drive, (O_RDONLY|O_NONBLOCK), 0);
  if ( cdfd >= 0 ) {
    /* TODO: linux's port does an ioctl here to check device capabilities,
     *       but we don't have a similar ioctl on LaylaOS.
     */
    is_cd = true;
    close(cdfd);
    }
  /* Even if we can't read it, it might be mounted */
  else if ( mnttype && (strcmp(mnttype, "iso9660") == 0) ) {
    is_cd = true;
  }
  return(is_cd);
}

static char *
check_mounts_laylaos(const char *mtab)
{
  FILE *mntfp;
  struct mntent *mntent;

  mntfp = setmntent(mtab, "r");
  if ( mntfp != NULL ) {
    char *tmp;
    char *mnt_type;
    char *mnt_dev;
    unsigned int i_mnt_type;
    unsigned int i_mnt_dev;

    while ( (mntent=getmntent(mntfp)) != NULL ) {
      i_mnt_type = strlen(mntent->mnt_type) + 1;
      mnt_type = calloc(1, i_mnt_type);
      if (mnt_type == NULL)
        continue;  /* maybe you'll get lucky next time. */

      i_mnt_dev = strlen(mntent->mnt_fsname) + 1;
      mnt_dev = calloc(1, i_mnt_dev);
      if (mnt_dev == NULL) {
        free(mnt_type);
        continue;
      }

      strncpy(mnt_type, mntent->mnt_type, i_mnt_type);
      strncpy(mnt_dev, mntent->mnt_fsname, i_mnt_dev);

      /* Handle "supermount" filesystem mounts */
      if ( strcmp(mnt_type, "supermount") == 0 ) {
        tmp = strstr(mntent->mnt_opts, "fs=");
        if ( tmp ) {
          free(mnt_type);
          mnt_type = strdup(tmp + strlen("fs="));
          if ( mnt_type ) {
            tmp = strchr(mnt_type, ',');
            if ( tmp ) {
              *tmp = '\0';
            }
          }
        }
        tmp = strstr(mntent->mnt_opts, "dev=");
        if ( tmp ) {
          free(mnt_dev);
          mnt_dev = strdup(tmp + strlen("dev="));
          if ( mnt_dev ) {
            tmp = strchr(mnt_dev, ',');
            if ( tmp ) {
              *tmp = '\0';
            }
          }
        }
      }
      if ( mnt_type && mnt_dev ) {
	if ( strcmp(mnt_type, "iso9660") == 0 ) {
	  if (is_cdrom_laylaos(mnt_dev, mnt_type) > 0) {
	    free(mnt_type);
	    endmntent(mntfp);
	    return mnt_dev;
	  }
        }
      }
      free(mnt_dev);
      free(mnt_type);
    }
    endmntent(mntfp);
  }
  return NULL;
}

static driver_return_code_t
run_scsi_cmd_laylaos(void *p_user_data, unsigned int i_timeout_ms,
                     unsigned int i_cdb, const mmc_cdb_t *p_cdb,
                     cdio_mmc_direction_t e_direction,
                     unsigned int i_buf, void *p_buf)
{
        const _img_private_t *_obj = p_user_data;
        scsireq_t req;

        memset(&req, 0, sizeof(req));
        memcpy(&req.cmd[0], p_cdb, i_cdb);
        req.cmdlen = i_cdb;
        req.datalen = i_buf;
        req.databuf = p_buf;
        req.timeout = i_timeout_ms;
        req.flags = e_direction == SCSI_MMC_DATA_READ ? SCCMD_READ : SCCMD_WRITE;

        if (ioctl(_obj->gen.fd, SCIOCCOMMAND, &req) < 0) {
                cdio_info("SCIOCCOMMAND: %s", strerror(errno));
                return -1;
        }
        if (req.retsts != SCCMD_OK) {
                cdio_info("SCIOCCOMMAND cmd 0x%02x sts %d\n", req.cmd[0], req.retsts);
                return -1;
        }

        return 0;
}

static access_mode_t
str_to_access_mode_laylaos(const char *psz_access_mode)
{
  const access_mode_t default_access_mode = _AM_IOCTL;

  if (NULL==psz_access_mode) return default_access_mode;

  if (!strcmp(psz_access_mode, "IOCTL"))
    return _AM_IOCTL;
  else if (!strcmp(psz_access_mode, "READ_CD"))
    return _AM_READ_CD;
  else if (!strcmp(psz_access_mode, "MMC_RDWR"))
    return _AM_MMC_RDWR;
  else if (!strcmp(psz_access_mode, "MMC_RDWR_EXCL"))
    return _AM_MMC_RDWR_EXCL;
  else {
    cdio_warn ("unknown access type: %s. Default IOCTL used.",
               psz_access_mode);
    return default_access_mode;
  }
}

static int
read_audio_sectors_laylaos(void *user_data, void *data, lsn_t lsn,
                           unsigned int nblocks)
{
        scsireq_t req;
        _img_private_t *_obj = user_data;

        memset(&req, 0, sizeof(req));
        req.cmd[0] = 0xbe;
        req.cmd[1] = 0;
        req.cmd[2] = (lsn >> 24) & 0xff;
        req.cmd[3] = (lsn >> 16) & 0xff;
        req.cmd[4] = (lsn >> 8) & 0xff;
        req.cmd[5] = (lsn >> 0) & 0xff;
        req.cmd[6] = (nblocks >> 16) & 0xff;
        req.cmd[7] = (nblocks >> 8) & 0xff;
        req.cmd[8] = (nblocks >> 0) & 0xff;
        req.cmd[9] = 0x78;
        req.cmdlen = 10;

        req.datalen = nblocks * CDIO_CD_FRAMESIZE_RAW;
        req.databuf = data;
        req.timeout = 10000;
        req.flags = SCCMD_READ;

        if (ioctl(_obj->gen.fd, SCIOCCOMMAND, &req) < 0) {
                cdio_info("SCIOCCOMMAND: %s", strerror(errno));
                return 1;
        }
        if (req.retsts != SCCMD_OK) {
                cdio_info("SCIOCCOMMAND cmd 0xbe sts %d\n", req.retsts);
                return 1;
        }

        return 0;
}

/*!
   Reads a single mode1 sector from cd device into data starting
   from lsn. Returns 0 if no error.
 */
static driver_return_code_t
_read_mode1_sector_laylaos (void *p_user_data, void *p_data, lsn_t lsn,
                            bool b_form2)
{

  return cdio_generic_read_form1_sector(p_user_data, p_data, lsn);
}

/*!
   Reads i_blocks of mode2 sectors from cd device into data starting
   from lsn.
   Returns 0 if no error.
 */
static driver_return_code_t
_read_mode1_sectors_laylaos (void *p_user_data, void *p_data, lsn_t lsn,
                             bool b_form2, uint32_t i_blocks)
{
  _img_private_t *p_env = p_user_data;
  unsigned int i;
  int retval;
  unsigned int blocksize = b_form2 ? M2RAW_SECTOR_SIZE : CDIO_CD_FRAMESIZE;

  for (i = 0; i < i_blocks; i++) {
    if ( (retval = _read_mode1_sector_laylaos (p_env,
                                               ((char *)p_data) + (blocksize*i),
                                               lsn + i, b_form2)) )
      return retval;
  }
  return DRIVER_OP_SUCCESS;
}

static int
read_mode2_sector_laylaos(void *user_data, void *data, lsn_t lsn,
                          bool mode2_form2)
{
        scsireq_t req;
        _img_private_t *_obj = user_data;
        char buf[M2RAW_SECTOR_SIZE] = { 0, };

        memset(&req, 0, sizeof(req));
        req.cmd[0] = 0xbe;
        req.cmd[1] = 0;
        req.cmd[2] = (lsn >> 24) & 0xff;
        req.cmd[3] = (lsn >> 16) & 0xff;
        req.cmd[4] = (lsn >> 8) & 0xff;
        req.cmd[5] = (lsn >> 0) & 0xff;
        req.cmd[6] = 0;
        req.cmd[7] = 0;
        req.cmd[8] = 1;
        req.cmd[9] = 0x58; /* subheader + userdata + ECC */
        req.cmdlen = 10;

        req.datalen = M2RAW_SECTOR_SIZE;
        req.databuf = buf;
        req.timeout = 10000;
        req.flags = SCCMD_READ;

        if (ioctl(_obj->gen.fd, SCIOCCOMMAND, &req) < 0) {
                cdio_info("SCIOCCOMMAND: %s", strerror(errno));
                return 1;
        }
        if (req.retsts != SCCMD_OK) {
                cdio_info("SCIOCCOMMAND cmd 0xbe sts %d\n", req.retsts);
                return 1;
        }

        if (mode2_form2)
                memcpy(data, buf, M2RAW_SECTOR_SIZE);
        else
                memcpy(data, buf + CDIO_CD_SUBHEADER_SIZE, CDIO_CD_FRAMESIZE);

        return 0;
}

static int
read_mode2_sectors_laylaos(void *user_data, void *data, lsn_t lsn,
                           bool mode2_form2, unsigned int nblocks)
{
        int i, res;
        char *buf = data;

        for (i = 0; i < nblocks; i++) {
                res = read_mode2_sector_laylaos(user_data, buf, lsn, mode2_form2);
                if (res)
                        return res;

                buf += (mode2_form2 ? M2RAW_SECTOR_SIZE : CDIO_CD_FRAMESIZE);
                lsn++;
        }

        return 0;
}

static int
set_arg_laylaos(void *p_user_data, const char key[], const char value[])
{
  _img_private_t *p_env = p_user_data;

  if (!strcmp (key, "source"))
    {
      if (!value) return DRIVER_OP_ERROR;
      free (p_env->gen.source_name);
      p_env->gen.source_name = strdup (value);
    }
  else if (!strcmp (key, "access-mode"))
    {
      p_env->access_mode = str_to_access_mode_laylaos(key);
    }
  else return DRIVER_OP_ERROR;

  return DRIVER_OP_SUCCESS;
}

static bool
_cdio_read_toc(_img_private_t *_obj)
{
        int res;
        struct ioc_read_toc_entry req;

        res = ioctl(_obj->gen.fd, CDIOREADTOCHEADER, &_obj->tochdr);
        if (res < 0) {
                cdio_warn("error in ioctl(CDIOREADTOCHEADER): %s\n",
                           strerror(errno));
                return false;
        }

        req.address_format = CD_MSF_FORMAT;
        req.starting_track = FIRST_TRACK_NUM;
        req.data_len = (TOTAL_TRACKS + 1) /* leadout! */
                * sizeof(struct cd_toc_entry);
        req.data = _obj->tocent;

        res = ioctl(_obj->gen.fd, CDIOREADTOCENTRIES, &req);
        if (res < 0) {
                cdio_warn("error in ioctl(CDROMREADTOCENTRIES): %s\n",
                           strerror(errno));
                return false;
        }

        _obj->toc_valid = 1;
        _obj->gen.i_first_track = FIRST_TRACK_NUM;
        _obj->gen.i_tracks = TOTAL_TRACKS;
        _obj->gen.toc_init = true;
        return true;
}

static bool
read_toc_laylaos (void *p_user_data)
{

        return _cdio_read_toc(p_user_data);
}

static int
_cdio_read_discinfo(_img_private_t *_obj)
{
        scsireq_t req;
#define FULLTOCBUF (4 + 1000*11)
        unsigned char buf[FULLTOCBUF] = { 0, };
        int i, j;

        memset(&req, 0, sizeof(req));
        req.cmd[0] = 0x43; /* READ TOC/PMA/ATIP */
        req.cmd[1] = 0x02;
        req.cmd[2] = 0x02; /* full TOC */
        req.cmd[3] = 0;
        req.cmd[4] = 0;
        req.cmd[5] = 0;
        req.cmd[6] = 0;
        req.cmd[7] = FULLTOCBUF / 256;
        req.cmd[8] = FULLTOCBUF % 256;
        req.cmd[9] = 0;
        req.cmdlen = 10;

        req.datalen = FULLTOCBUF;
        req.databuf = (caddr_t)buf;
        req.timeout = 10000;
        req.flags = SCCMD_READ;

        if (ioctl(_obj->gen.fd, SCIOCCOMMAND, &req) < 0) {
                cdio_info("SCIOCCOMMAND: %s", strerror(errno));
                return 1;
        }
        if (req.retsts != SCCMD_OK) {
                cdio_info("SCIOCCOMMAND cmd 0x43 sts %d\n", req.retsts);
                return 1;
        }
#if 1
        printf("discinfo:");
        for (i = 0; i < 4; i++)
                printf(" %02x", buf[i]);
        printf("\n");
        for (i = 0; i < buf[1] - 2; i++) {
                printf(" %02x", buf[i + 4]);
                if (!((i + 1) % 11))
                        printf("\n");
        }
#endif

        for (i = 4; i < req.datalen_used; i += 11) {
                if (buf[i + 3] == 0xa0) { /* POINT */
                        /* XXX: assume entry 0xa1 follows */
                        for (j = buf[i + 8] - 1; j <= buf[i + 11 + 8] - 1; j++)
                                _obj->sessionformat[j] = buf[i + 9];
                }
        }

        _obj->sessionformat_valid = true;
        return 0;
}

static int
eject_media_laylaos(void *user_data) {

        _img_private_t *_obj = user_data;
        int fd, res, ret = 0;

        fd = open(_obj->gen.source_name, O_RDONLY|O_NONBLOCK);
        if (fd < 0)
                return 2;

        res = ioctl(fd, CDIOCALLOW);
        if (res < 0) {
                cdio_warn("ioctl(fd, CDIOCALLOW) failed: %s\n",
                           strerror(errno));
                /* go on... */
        }
        res = ioctl(fd, CDIOCEJECT);
        if (res < 0) {
                cdio_warn("ioctl(CDIOCEJECT) failed: %s\n",
                           strerror(errno));
                ret = 1;
        }

        close(fd);
        return ret;
}

static bool
is_mmc_supported(void *user_data)
{
    _img_private_t *env = user_data;
    return (_AM_NONE == env->access_mode) ? false : true;
}

/*!
  Return the value associated with the key "arg".
*/
static const char *
get_arg_laylaos(void *env, const char key[])
{
  _img_private_t *_obj = env;

  if (!strcmp(key, "source")) {
    return _obj->gen.source_name;
  } else if (!strcmp(key, "access-mode")) {
    switch (_obj->access_mode) {
    case _AM_IOCTL:
      return "IOCTL";
    case _AM_READ_CD:
      return "READ_CD";
    case _AM_MMC_RDWR:
      return "MMC_RDWR";
    case _AM_MMC_RDWR_EXCL:
      return "MMC_RDWR_EXCL";
    case _AM_NONE:
      return "no access method";
    }
  } else if (!strcmp (key, "mmc-supported?")) {
      return is_mmc_supported(env) ? "true" : "false";
  }
  return NULL;
}

static track_t
get_first_track_num_laylaos(void *user_data)
{
        _img_private_t *_obj = user_data;
        int res;

        if (!_obj->toc_valid) {
                res = _cdio_read_toc(_obj);
                if (!res)
                        return CDIO_INVALID_TRACK;
        }

        return FIRST_TRACK_NUM;
}

static track_t
get_num_tracks_laylaos(void *user_data)
{
        _img_private_t *_obj = user_data;
        int res;

        if (!_obj->toc_valid) {
                res = _cdio_read_toc(_obj);
                if (!res)
                        return CDIO_INVALID_TRACK;
        }

        return TOTAL_TRACKS;
}

/*!
  Return the international standard recording code ISRC.

  Note: string is malloc'd so caller should free() then returned
  string when done with it.

 */
static char *
get_track_isrc_laylaos (const void *p_user_data, track_t i_track) {

  const _img_private_t *p_env = p_user_data;
  return mmc_get_track_isrc( p_env->gen.cdio, i_track );
}

static driver_return_code_t
audio_get_volume_laylaos(void *p_user_data, cdio_audio_volume_t *p_volume)
{
  const _img_private_t *p_env = p_user_data;
  return (ioctl(p_env->gen.fd, CDIOCGETVOL, p_volume));
}

static driver_return_code_t
audio_pause_laylaos(void *p_user_data)
{
  const _img_private_t *p_env = p_user_data;
  return (ioctl(p_env->gen.fd, CDIOCPAUSE));
}

static driver_return_code_t
audio_stop_laylaos(void *p_user_data)
{
  const _img_private_t *p_env = p_user_data;
  return (ioctl(p_env->gen.fd, CDIOCSTOP));
}

static driver_return_code_t
audio_resume_laylaos(void *p_user_data)
{
  const _img_private_t *p_env = p_user_data;
  return (ioctl(p_env->gen.fd, CDIOCRESUME));
}

static driver_return_code_t
audio_set_volume_laylaos(void *p_user_data, cdio_audio_volume_t *p_volume)
{
  const _img_private_t *p_env = p_user_data;
  return (ioctl(p_env->gen.fd, CDIOCSETVOL, p_volume));
}

/*!
  Get format of track.
*/
static track_format_t
get_track_format_laylaos(void *user_data, track_t track_num)
{
        _img_private_t *_obj = user_data;
        int res, first_track = 0, track_idx = 0;

        if (!_obj->toc_valid) {
                res = _cdio_read_toc(_obj);
                if (!res)
                        return TRACK_FORMAT_ERROR;
        }

        first_track = _obj->gen.i_first_track;

        if (!_obj->gen.toc_init ||
            track_num > (first_track + _obj->gen.i_tracks) ||
            track_num < first_track)
            return (CDIO_INVALID_TRACK);

        track_idx = track_num - first_track;

        if (_obj->tocent[track_idx].control & 0x04) {
                if (!_obj->sessionformat_valid) {
                        res = _cdio_read_discinfo(_obj);
                        if (res)
                                return TRACK_FORMAT_ERROR;
                }

                if (_obj->sessionformat[track_idx] == 0x10)
                        return TRACK_FORMAT_CDI;
                else if (_obj->sessionformat[track_idx] == 0x20)
                        return TRACK_FORMAT_XA;
                else
                        return TRACK_FORMAT_DATA;
        } else
                return TRACK_FORMAT_AUDIO;
}

/*!
  Return true if we have XA data (green, mode2 form1) or
  XA data (green, mode2 form2). That is track begins:
  sync - header - subheader
  12     4      -  8

  FIXME: there's gotta be a better design for this and get_track_format?
*/
static bool
get_track_green_laylaos(void *user_data, track_t track_num)
{

        return (get_track_format_laylaos(user_data, track_num)
                == TRACK_FORMAT_XA);
}

/*!
  Return the starting MSF (minutes/secs/frames) for track number
  track_num in obj.  Track numbers usually start at something
  greater than 0, usually 1.

  The "leadout" track is specified by passing i_track as either
  LEADOUT_TRACK or the track number of the last audio track plus one.

  False is returned if there is no track entry.
*/
static bool
get_track_msf_laylaos(void *user_data, track_t track_num, msf_t *msf)
{
        _img_private_t *_obj = user_data;
        int res, first_track = 0, track_idx = 0;

        if (!msf)
                return false;

        if (!_obj->toc_valid) {
                res = _cdio_read_toc(_obj);
                if (!res)
                        return false;
        }

        if (track_num == CDIO_CDROM_LEADOUT_TRACK)
                track_num = _obj->gen.i_tracks + _obj->gen.i_first_track;

        first_track = _obj->gen.i_first_track;

        if (!_obj->gen.toc_init ||
            track_num > (first_track + _obj->gen.i_tracks) ||
            track_num < first_track)
            return (CDIO_INVALID_TRACK);

        track_idx = track_num - first_track;
        msf->m = cdio_to_bcd8(_obj->tocent[track_idx].addr.msf.minute);
        msf->s = cdio_to_bcd8(_obj->tocent[track_idx].addr.msf.second);
        msf->f = cdio_to_bcd8(_obj->tocent[track_idx].addr.msf.frame);

        return true;
}

/*!
   Return the size of the CD in logical block address (LBA) units.
   @return the lsn. On error return CDIO_INVALID_LSN.

   Also note that in one at least one test the corresponding MMC gives
   a different answer, so there may be some disagreement about what is in
   fact the last lsn.
 */
static lsn_t
get_disc_last_lsn_laylaos(void *user_data)
{
        msf_t msf;

        get_track_msf_laylaos(user_data, CDIO_CDROM_LEADOUT_TRACK, &msf);

        return (((msf.m * 60) + msf.s) * CDIO_CD_FRAMES_PER_SEC + msf.f);
}


static driver_return_code_t
get_last_session_laylaos(void *p_user_data, lsn_t *i_last_session)
{
  const _img_private_t *p_env = p_user_data;
  int addr = 0;

  if (ioctl(p_env->gen.fd, CDIOREADMSADDR, &addr) == 0) {
    *i_last_session = addr;
    return (DRIVER_OP_SUCCESS);
  } else {
    cdio_warn("ioctl CDIOREADMSADDR failed: %s\n",
        strerror(errno));
    return (DRIVER_OP_ERROR);
  }
}

/* checklist: /dev/cdrom, /dev/dvd /dev/hd?, /dev/scd? */
static const char checklist1[][40] = {
  {"cdrom"}, {"dvd"}
};
static const int checklist1_size = sizeof(checklist1) / sizeof(checklist1[0]);

static const struct
  {
    char format[22];
    int num_min;
    int num_max;
  }
checklist2[] =
  {
    { "/dev/hd%c",  'a', 'z' },
    { "/dev/scd%d", 0,   25 },
  };
static const int checklist2_size = sizeof(checklist2) / sizeof(checklist2[0]);

#endif /* HAVE_LAYLAOS_CDROM */

/*!
  Return an array of strings giving possible CD devices.
 */
char **
cdio_get_devices_laylaos (void)
{
#ifndef HAVE_LAYLAOS_CDROM
  return NULL;
#else
  unsigned int i;
  char drive[40];
  char *ret_drive;
  char **drives = NULL;
  unsigned int num_drives=0;

  /* Scan the system for CD-ROM drives.
  */
  for ( i=0; i < checklist1_size; ++i ) {
    if (snprintf(drive, sizeof(drive), "/dev/%s", checklist1[i]) < 0)
      continue;
    if ( is_cdrom_laylaos(drive, NULL) > 0 ) {
      cdio_add_device_list(&drives, drive, &num_drives);
    }
  }

  /* Now check the currently mounted CD drives */
  if (NULL != (ret_drive = check_mounts_laylaos("/etc/mtab"))) {
    cdio_add_device_list(&drives, ret_drive, &num_drives);
    free(ret_drive);
  }

  /* Finally check possible mountable drives in /etc/fstab */
  if (NULL != (ret_drive = check_mounts_laylaos("/etc/fstab"))) {
    cdio_add_device_list(&drives, ret_drive, &num_drives);
    free(ret_drive);
  }

  /* Scan the system for CD-ROM drives.
     Not always 100% reliable, so use the USE_MNTENT code above first.
  */
  for ( i=0; i < checklist2_size; ++i ) {
    unsigned int j;
    for ( j=checklist2[i].num_min; j<=checklist2[i].num_max; ++j ) {
      if (snprintf(drive, sizeof(drive), checklist2[i].format, j) < 0)
        continue;
      if ( (is_cdrom_laylaos(drive, NULL)) > 0 ) {
        cdio_add_device_list(&drives, drive, &num_drives);
      }
    }
  }
  cdio_add_device_list(&drives, NULL, &num_drives);
  return drives;
#endif /* HAVE_LAYLAOS_CDROM */
}

/*!
  Return a string containing the default CD device.
 */
char *
cdio_get_default_device_laylaos(void)
{
#ifndef HAVE_LAYLAOS_CDROM
  return NULL;

#else
  unsigned int i;
  char drive[40];
  char *ret_drive;

  /* Scan the system for CD-ROM drives.
  */
  for ( i=0; i < checklist1_size; ++i ) {
    if (snprintf(drive, sizeof(drive), "/dev/%s", checklist1[i]) < 0)
      continue;
    if ( is_cdrom_laylaos(drive, NULL) > 0 ) {
      return strdup(drive);
    }
  }

  /* Now check the currently mounted CD drives */
  if (NULL != (ret_drive = check_mounts_laylaos("/etc/mtab")))
    return ret_drive;

  /* Finally check possible mountable drives in /etc/fstab */
  if (NULL != (ret_drive = check_mounts_laylaos("/etc/fstab")))
    return ret_drive;

  /* Scan the system for CD-ROM drives.
     Not always 100% reliable, so use the USE_MNTENT code above first.
  */
  for ( i=0; i < checklist2_size; ++i ) {
    unsigned int j;
    for ( j=checklist2[i].num_min; j<=checklist2[i].num_max; ++j ) {
      if (snprintf(drive, sizeof(drive), checklist2[i].format, j) < 0)
        continue;
      if ( is_cdrom_laylaos(drive, NULL) > 0 ) {
        return(strdup(drive));
      }
    }
  }
  return NULL;
#endif /* HAVE_LAYLAOS_CDROM */
}

#ifdef HAVE_LAYLAOS_CDROM
static driver_return_code_t
audio_play_msf_laylaos(void *p_user_data, msf_t *p_start_msf, msf_t *p_end_msf)
{
  const _img_private_t *p_env = p_user_data;
  struct ioc_play_msf a;

  a.start_m = cdio_from_bcd8(p_start_msf->m);
  a.start_s = cdio_from_bcd8(p_start_msf->s);
  a.start_f = cdio_from_bcd8(p_start_msf->f);
  a.end_m = cdio_from_bcd8(p_end_msf->m);
  a.end_s = cdio_from_bcd8(p_end_msf->s);
  a.end_f = cdio_from_bcd8(p_end_msf->f);

  return (ioctl(p_env->gen.fd, CDIOCPLAYMSF, (char *)&a));
}

#if !USE_MMC_SUBCHANNEL
static driver_return_code_t
audio_read_subchannel_laylaos(void *p_user_data, cdio_subchannel_t *subchannel)
{
  const _img_private_t *p_env = p_user_data;
  struct ioc_read_subchannel s;
  struct cd_sub_channel_info data;

  bzero(&s, sizeof(s));
  s.data = &data;
  s.data_len = sizeof(data);
  s.address_format = CD_MSF_FORMAT;
  s.data_format = CD_CURRENT_POSITION;

  if (ioctl(p_env->gen.fd, CDIOCREADSUBCHANNEL, &s) != -1) {
    subchannel->control = s.data->what.position.control;
    subchannel->track = s.data->what.position.track_number;
    subchannel->index = s.data->what.position.index_number;

    subchannel->abs_addr.m =
        cdio_to_bcd8(s.data->what.position.absaddr.msf.minute);
    subchannel->abs_addr.s =
        cdio_to_bcd8(s.data->what.position.absaddr.msf.second);
    subchannel->abs_addr.f =
        cdio_to_bcd8(s.data->what.position.absaddr.msf.frame);
    subchannel->rel_addr.m =
        cdio_to_bcd8(s.data->what.position.reladdr.msf.minute);
    subchannel->rel_addr.s =
        cdio_to_bcd8(s.data->what.position.reladdr.msf.second);
    subchannel->rel_addr.f =
        cdio_to_bcd8(s.data->what.position.reladdr.msf.frame);
    subchannel->audio_status = s.data->header.audio_status;

    return (DRIVER_OP_SUCCESS);
  } else {
    cdio_warn("ioctl CDIOCREADSUBCHANNEL failed: %s\n",
        strerror(errno));
    return (DRIVER_OP_ERROR);
  }
}
#endif
#endif /* HAVE_LAYLAOS_CDROM */

/*!
  Close tray on CD-ROM.

  @param psz_device the CD-ROM drive to be closed.

*/
driver_return_code_t
close_tray_laylaos (const char *psz_device)
{
#ifdef HAVE_LAYLAOS_CDROM
  return DRIVER_OP_UNSUPPORTED;
#else
  return DRIVER_OP_NO_DRIVER;
#endif
}

#ifdef HAVE_LAYLAOS_CDROM
static cdio_funcs_t _funcs = {
  .audio_get_volume      = audio_get_volume_laylaos,
  .audio_pause           = audio_pause_laylaos,
  .audio_play_msf        = audio_play_msf_laylaos,
  .audio_play_track_index= NULL,
#if USE_MMC_SUBCHANNEL
  .audio_read_subchannel = audio_read_subchannel_mmc,
#else
  .audio_read_subchannel = audio_read_subchannel_laylaos,
#endif
  .audio_stop            = audio_stop_laylaos,
  .audio_resume          = audio_resume_laylaos,
  .audio_set_volume      = audio_set_volume_laylaos,
  .eject_media           = eject_media_laylaos,
  .free                  = cdio_generic_free,
  .get_arg               = get_arg_laylaos,
  .get_blocksize         = get_blocksize_mmc,
  .get_cdtext            = get_cdtext_generic,
  .get_cdtext_raw        = read_cdtext_generic,
  .get_default_device    = cdio_get_default_device_laylaos,
  .get_devices           = cdio_get_devices_laylaos,
  .get_disc_last_lsn     = get_disc_last_lsn_laylaos,
  .get_last_session      = get_last_session_laylaos,
  .get_media_changed     = get_media_changed_mmc,
  .get_discmode          = get_discmode_generic,
  .get_drive_cap         = get_drive_cap_mmc,
  .get_first_track_num   = get_first_track_num_laylaos,
  .get_hwinfo            = NULL,
  .get_mcn               = get_mcn_mmc,
  .get_num_tracks        = get_num_tracks_laylaos,
  .get_track_channels    = get_track_channels_generic,
  .get_track_copy_permit = get_track_copy_permit_generic,
  .get_track_format      = get_track_format_laylaos,
  .get_track_green       = get_track_green_laylaos,
   /* Not because we can't talk LBA, but the driver assumes MSF throughout */
  .get_track_lba         = NULL,
  .get_track_preemphasis = get_track_preemphasis_generic,
  .get_track_msf         = get_track_msf_laylaos,
  .get_track_isrc        = get_track_isrc_laylaos,
  .lseek                 = cdio_generic_lseek,
  .read                  = cdio_generic_read,
  .read_audio_sectors    = read_audio_sectors_laylaos,
  .read_data_sectors     = read_data_sectors_generic,
  .read_mode1_sector     = _read_mode1_sector_laylaos,
  .read_mode1_sectors    = _read_mode1_sectors_laylaos,
  .read_mode2_sector     = read_mode2_sector_laylaos,
  .read_mode2_sectors    = read_mode2_sectors_laylaos,
  .read_toc              = read_toc_laylaos,
  .run_mmc_cmd           = run_scsi_cmd_laylaos,
  .set_arg               = set_arg_laylaos,
};
#endif /* HAVE_LAYLAOS_CDROM */

/*!
  Initialization routine. This is the only thing that doesn't
  get called via a function pointer. In fact *we* are the
  ones to set that up.
 */
CdIo_t *
cdio_open_laylaos(const char *orig_source_name)
{
#ifdef HAVE_LAYLAOS_CDROM
    CdIo_t *ret;
    _img_private_t *_data;
    int open_access_mode;  /* Access mode passed to cdio_generic_init. */
    char *source_name;

    _data = calloc(1, sizeof(_img_private_t));

    _data->gen.init = false;
    _data->toc_valid = false;
    _data->sessionformat_valid = false;
    _data->gen.fd = -1;
    _data->gen.b_cdtext_error = false;

    if (NULL == orig_source_name) {
        source_name = cdio_get_default_device_laylaos();
        if (NULL == source_name) {
            goto err_exit;
        }
        set_arg_laylaos(_data, "source", source_name);
        free(source_name);
    } else {
        if (cdio_is_device_generic(orig_source_name))
            set_arg_laylaos(_data, "source", orig_source_name);
        else {
            goto err_exit;
        }
    }

    ret = cdio_new(&_data->gen, &_funcs);
    if (ret == NULL) {
        goto err_exit;
    }

    ret->driver_id = DRIVER_LAYLAOS;

  open_access_mode = O_NONBLOCK;
  if (_AM_MMC_RDWR == _data->access_mode)
    open_access_mode |= O_RDWR;
  else if (_AM_MMC_RDWR_EXCL == _data->access_mode)
    open_access_mode |= O_RDWR | O_EXCL;
  else
    open_access_mode |= O_RDONLY;
  if (cdio_generic_init(_data, open_access_mode)) {
    return ret;
  }
  free(ret);

 err_exit:
    cdio_generic_free(_data);
    return NULL;

#else
  return NULL;
#endif /* HAVE_LAYLAOS_CDROM */

}

/*!
  Initialization routine. This is the only thing that doesn't
  get called via a function pointer. In fact *we* are the
  ones to set that up.
 */
CdIo_t *
cdio_open_am_laylaos(const char *source_name, const char *am)
{
  return (cdio_open_laylaos(source_name));
}

bool
cdio_have_laylaos (void)
{
#ifdef HAVE_LAYLAOS_CDROM
  return true;
#else
  return false;
#endif /* HAVE_LAYLAOS_CDROM */
}


/*
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
