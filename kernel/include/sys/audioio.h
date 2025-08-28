/*    $OpenBSD: audioio.h,v 1.27 2016/09/14 06:12:20 ratchov Exp $    */
/*    $NetBSD: audioio.h,v 1.24 1998/08/13 06:28:41 mrg Exp $    */

/*
 * Copyright (c) 1991-1993 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the Computer Systems
 *    Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _SYS_AUDIOIO_H_
#define _SYS_AUDIOIO_H_


#include <sys/ioctl.h>


#define AUDIO_INITPAR(p)    \
    (void)memset((void *)(p), 0xff, sizeof(struct audio_swpar))

/*
 * argument to AUDIO_SETPAR and AUDIO_GETPAR ioctls
 */
struct audio_swpar {
    unsigned int sig;        /* if 1, encoding is signed */
    unsigned int le;        /* if 1, encoding is little-endian */
    unsigned int bits;        /* bits per sample */
    unsigned int bps;        /* bytes per sample */
    unsigned int msb;        /* if 1, bits are msb-aligned */
    unsigned int rate;        /* common play & rec sample rate */
    unsigned int pchan;        /* play channels */
    unsigned int rchan;        /* rec channels */
    unsigned int nblks;        /* number of blocks in play buffer */
    unsigned int round;        /* common frames per block */
    unsigned int _spare[6];
};

/*
 * argument to AUDIO_GETSTATUS
 */
struct audio_status {
    int mode;
    int pause;
    int active;
    int _spare[5];
};

/*
 * argument to AUDIO_GETINFO and AUDIO_SETINFO
 */
struct audio_prinfo {
    unsigned int sample_rate;    /* sample rate in samples/s */
    unsigned int channels;       /* number of channels, usually 1 or 2 */
    unsigned int precision;      /* number of bits/sample */
    unsigned int encoding;       /* data encoding (AUDIO_ENCODING_* below) */
#define AUDIO_ENCODING_ULAW         0x01 /* mu-law encoding, 8 bits/sample */
#define AUDIO_ENCODING_ALAW         0x02 /* A-law encoding, 8 bits/sample */
#define AUDIO_ENCODING_SLINEAR      0x03 /* two's complement signed linear
                                            encoding with the platform byte order */
#define AUDIO_ENCODING_ULINEAR      0x04 /* unsigned linear encoding with the
                                            platform byte order */
#define AUDIO_ENCODING_ADPCM        0x05 /* ADPCM encoding, 8 bits/sample */
#define AUDIO_ENCODING_SLINEAR_LE   0x06 /* two's complement signed linear
                                            encoding with little endian byte order */
#define AUDIO_ENCODING_SLINEAR_BE   0x07 /* two's complement signed linear
                                            encoding with big endian byte order */
#define AUDIO_ENCODING_ULINEAR_LE   0x08 /* unsigned linear encoding with little
                                            endian byte order */
#define AUDIO_ENCODING_ULINEAR_BE   0x09 /* unsigned linear encoding with big
                                            endian byte order */
    unsigned int gain;           /* volume level */
    unsigned int port;           /* selected I/O port */
    unsigned int seek;           /* BSD extension */
    unsigned int avail_ports;    /* available I/O ports */
    unsigned int buffer_size;    /* total size audio buffer */
    unsigned int _ispare[1];
    /* Current state of device: */
    unsigned int samples;        /* number of samples */
    unsigned int eof;            /* End Of File (zero-size writes) counter */
    unsigned char pause;          /* non-zero if paused, zero to resume */
    unsigned char error;          /* non-zero if underflow/overflow occurred */
    unsigned char waiting;        /* non-zero if another process hangs in open */
    unsigned char balance;        /* stereo channel balance */
    unsigned char cspare[2];
    unsigned char open;           /* non-zero if currently open */
    unsigned char active;         /* non-zero if I/O is currently active */
};

typedef struct audio_info {
    struct audio_prinfo play;   /* info for play (output) side */
    struct audio_prinfo record; /* info for record (input) side */
    unsigned int monitor_gain;        /* input to output mix */
    /* BSD extensions */
    unsigned int blocksize;      /* H/W read/write block size */
    unsigned int hiwat;          /* output high water mark */
    unsigned int lowat;          /* output low water mark */
    unsigned char output_muted;   /* toggle play mute */
    unsigned char cspare[3];
    unsigned int mode;           /* current device mode */
#define AUMODE_PLAY             0x01
#define AUMODE_RECORD           0x02
#define AUMODE_PLAY_ALL         0x04    /* do not do real-time correction */
} audio_info_t;

/*
 * Parameter for the AUDIO_GETDEV ioctl to determine current
 * audio devices.
 */
#define MAX_AUDIO_DEV_LEN       16
typedef struct audio_device {
    char name[MAX_AUDIO_DEV_LEN];
    char version[MAX_AUDIO_DEV_LEN];
    char config[MAX_AUDIO_DEV_LEN];
} audio_device_t;

struct audio_pos {
    unsigned int play_pos;    /* total bytes played */
    unsigned int play_xrun;    /* bytes of silence inserted */
    unsigned int rec_pos;    /* total bytes recorded */
    unsigned int rec_xrun;    /* bytes dropped */
};

/*
 * Audio device operations
 */
#define AUDIO_GETDEV            _IOR('A', 27, struct audio_device)
#define AUDIO_GETPOS            _IOR('A', 35, struct audio_pos)
#define AUDIO_GETPAR            _IOR('A', 36, struct audio_swpar)
#define AUDIO_SETPAR            _IOWR('A', 37, struct audio_swpar)
#define AUDIO_START             _IO('A', 38)
#define AUDIO_STOP              _IO('A', 39)
#define AUDIO_GETSTATUS         _IOR('A', 40, struct audio_status)

#define AUDIO_GETINFO           _IOR('A', 41, audio_info_t)
#define AUDIO_SETINFO           _IOWR('A', 42, audio_info_t)

#define AUDIO_FLUSH             _IO('A', 43)
#define AUDIO_DRAIN             _IO('A', 44)

#define AUDIO_INITINFO(a)       memset(a, 0, sizeof(audio_info_t))

/*
 * Mixer device
 */
#define AUDIO_MIN_GAIN          0
#define AUDIO_MAX_GAIN          255

typedef struct mixer_level {
    int num_channels;
    unsigned char level[8];    /* [num_channels] */
} mixer_level_t;
#define AUDIO_MIXER_LEVEL_MONO  0
#define AUDIO_MIXER_LEVEL_LEFT  0
#define AUDIO_MIXER_LEVEL_RIGHT 1

/*
 * Device operations
 */

typedef struct audio_mixer_name {
    char name[MAX_AUDIO_DEV_LEN];
    int msg_id;
} audio_mixer_name_t;

typedef struct mixer_devinfo {
    int index;
    audio_mixer_name_t label;
    int type;
#define AUDIO_MIXER_CLASS       0
#define AUDIO_MIXER_ENUM        1
#define AUDIO_MIXER_SET         2
#define AUDIO_MIXER_VALUE       3
    int mixer_class;
    int next, prev;
#define AUDIO_MIXER_LAST        -1
    union {
        struct audio_mixer_enum {
            int num_mem;
            struct {
                audio_mixer_name_t label;
                int ord;
            } member[32];
        } e;
        struct audio_mixer_set {
            int num_mem;
            struct {
                audio_mixer_name_t label;
                int mask;
            } member[32];
        } s;
        struct audio_mixer_value {
            audio_mixer_name_t units;
            int num_channels;
            int delta;
        } v;
    } un;
} mixer_devinfo_t;


typedef struct mixer_ctrl {
    int dev;
    int type;
    union {
        int ord;        /* enum */
        int mask;        /* set */
        mixer_level_t value;    /* value */
    } un;
} mixer_ctrl_t;

/*
 * Mixer operations
 */
#define AUDIO_MIXER_READ        _IOWR('M', 0, mixer_ctrl_t)
#define AUDIO_MIXER_WRITE       _IOWR('M', 1, mixer_ctrl_t)
#define AUDIO_MIXER_DEVINFO     _IOWR('M', 2, mixer_devinfo_t)

/*
 * Well known device names
 */
#define AudioNmicrophone        "mic"
#define AudioNline              "line"
#define AudioNcd                "cd"
#define AudioNdac               "dac"
#define AudioNaux               "aux"
#define AudioNrecord            "record"
#define AudioNvolume            "volume"
#define AudioNmonitor           "monitor"
#define AudioNtreble            "treble"
#define AudioNmid               "mid"
#define AudioNbass              "bass"
#define AudioNbassboost         "bassboost"
#define AudioNspeaker           "spkr"
#define AudioNheadphone         "hp"
#define AudioNoutput            "output"
#define AudioNinput             "input"
#define AudioNmaster            "master"
#define AudioNstereo            "stereo"
#define AudioNmono              "mono"
#define AudioNloudness          "loudness"
#define AudioNspatial           "spatial"
#define AudioNsurround          "surround"
#define AudioNpseudo            "pseudo"
#define AudioNmute              "mute"
#define AudioNenhanced          "enhanced"
#define AudioNpreamp            "preamp"
#define AudioNon                "on"
#define AudioNoff               "off"
#define AudioNmode              "mode"
#define AudioNsource            "source"
#define AudioNfmsynth           "fmsynth"
#define AudioNwave              "wave"
#define AudioNmidi              "midi"
#define AudioNmixerout          "mixerout"
#define AudioNswap              "swap"    /* swap left and right channels */
#define AudioNagc               "agc"
#define AudioNdelay             "delay"
#define AudioNselect            "select" /* select destination */
#define AudioNvideo             "video"
#define AudioNcenter            "center"
#define AudioNdepth             "depth"
#define AudioNlfe               "lfe"
#define AudioNextamp            "extamp"

#define AudioCinputs            "inputs"
#define AudioCoutputs           "outputs"
#define AudioCrecord            "record"
#define AudioCmonitor           "monitor"
#define AudioCequalization      "equalization"

#endif /* !_SYS_AUDIOIO_H_ */
