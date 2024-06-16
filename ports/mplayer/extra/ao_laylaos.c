/*
 * LaylaOS audio output driver for MPlayer
 * (c) 2024 Mohammed Isam
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/audioio.h>

#include "config.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "osdep/timer.h"
#include "libavutil/fifo.h"

static const ao_info_t info =
{
	"LaylaOS audio output",
	"laylaos",
	"Mohammed Isam (mohammed_isam1984@yahoo.com)",
	""
};

ao_functions_t audio_out_laylaos =
	{
		&info,
		control,
		init,
		uninit,
		reset,
		get_space,
		play,
		get_delay,
		audio_pause,
		audio_resume
	};



#define SAMPLESIZE 1024
#define CHUNK_SIZE 4096
#define NUM_CHUNKS 10
#define BUFFSIZE (NUM_CHUNKS * CHUNK_SIZE)

#ifndef AUDIO_ENCODING_NONE
#define AUDIO_ENCODING_NONE         (-1)
#endif

//static AVFifoBuffer *buffer;
static char *audio_dev = NULL;
static int audio_fd = -1;
static int queued_bursts = 0;
static int byte_per_sec = 0;

static void setup_device_paths(void)
{
    if (audio_dev == NULL) {
	if ((audio_dev = getenv("AUDIODEV")) == NULL)
	    audio_dev = "/dev/dsp";
    }
}


// convert an OSS audio format specification into our audio encoding
static int af2sysfmt(int format)
{
    switch (format){
    case AF_FORMAT_MU_LAW:
		return AUDIO_ENCODING_ULAW;
    case AF_FORMAT_A_LAW:
		return AUDIO_ENCODING_ALAW;
    case AF_FORMAT_S16_NE:
		return AUDIO_ENCODING_SLINEAR;
    case AF_FORMAT_U8:
		return AUDIO_ENCODING_ULINEAR;
    case AF_FORMAT_S8:
		return AUDIO_ENCODING_SLINEAR;
    default:
		return AUDIO_ENCODING_NONE;
  }
}

/*
static int write_buffer(unsigned char* data, int len)
{
	int free = av_fifo_space(buffer);
	if (len > free) len = free;
	return av_fifo_generic_write(buffer, data, len, NULL);
}


static int read_buffer(unsigned char* data, int len)
{
	int buffered = av_fifo_size(buffer);
	if (len > buffered) len = buffered;
	av_fifo_generic_read(buffer, data, len, NULL);
	return len;
}
*/

// to set/get/query special features/parameters
static int control(int cmd, void *arg)
{
	return CONTROL_UNKNOWN;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate, int channels, int format, int flags)
{
    audio_info_t info;

	//buffer = av_fifo_alloc(BUFFSIZE);

	setup_device_paths();

    mp_msg(MSGT_AO, MSGL_STATUS, "ao2_laylaos: %d Hz  %d chans  %s [0x%X]\n",
	   rate, channels, af_fmt2str_short(format), format);

    audio_fd = open(audio_dev, O_WRONLY);

    if(audio_fd < 0) {
		mp_msg(MSGT_AO, MSGL_ERR, "ao2_laylaos: cannot open device %s: %s\n",
		            audio_dev, strerror(errno));
		return 0;
    }

    if (af2sysfmt(format) == AUDIO_ENCODING_NONE)
      format = AF_FORMAT_S16_NE;

	AUDIO_INITINFO(&info);
	info.play.encoding = af2sysfmt(format);
	info.play.precision = (format == AF_FORMAT_S16_NE ? 16 : 8);
	info.play.channels = channels;
	info.play.sample_rate = rate;

	ao_data.channels = channels;
	ao_data.samplerate = rate;
	ao_data.buffersize = CHUNK_SIZE;
	ao_data.outburst = CHUNK_SIZE;
	ao_data.format = format;
	ao_data.bps = channels * rate * (af_fmt2bits(format) / 8);

	if (ioctl(audio_fd, AUDIO_SETINFO, &info) < 0) {
	    /* audio format not accepted by audio driver */
		char buf[128];
		mp_msg(MSGT_AO, MSGL_ERR,
		        "ao2_laylaos: unsupported channels (%d), format (%s) "
		        "or sample rate (%d)\n",
		            channels, af_fmt2str(format, buf, 128), rate);

		info.play.encoding = AUDIO_ENCODING_SLINEAR;
		info.play.precision = 16;
		info.play.channels = ao_data.channels = 2;
		info.play.sample_rate = ao_data.samplerate = 44100;
		ao_data.format = AF_FORMAT_S16_NE;
		ao_data.bps = ao_data.channels * ao_data.samplerate * 2;

		if (ioctl(audio_fd, AUDIO_SETINFO, &info) < 0) {
			mp_msg(MSGT_AO, MSGL_ERR,
		            "ao2_laylaos: failed to init audio device %s : %s\n",
		            audio_dev, strerror(errno));
			return 0;
		}

		mp_msg(MSGT_AO, MSGL_ERR,
		        "ao2_laylaos: using system default settings\n");
    }

	byte_per_sec = ao_data.bps;
	reset();

	return 1;
}

// close audio device
static void uninit(int immed)
{
    // throw away buffered data in the audio driver's STREAMS queue
    if (immed)
		ioctl(audio_fd, AUDIO_FLUSH, 0);
    else
		ioctl(audio_fd, AUDIO_DRAIN, 0);

    close(audio_fd);
    audio_fd = -1;

	//av_fifo_free(buffer);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void)
{
    audio_info_t info;
	ioctl(audio_fd, AUDIO_FLUSH, 0);

    AUDIO_INITINFO(&info);
    info.play.samples = 0;
    info.play.eof = 0;
    info.play.error = 0;
    ioctl(audio_fd, AUDIO_SETINFO, &info);

    queued_bursts = 0;
	//av_fifo_reset(buffer);
}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
    ioctl(audio_fd, AUDIO_STOP, 0);
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
    ioctl(audio_fd, AUDIO_START, 0);
}

// return: how many bytes can be played without blocking
static int get_space(void)
{
    audio_info_t info;

    ioctl(audio_fd, AUDIO_GETINFO, &info);
    if (queued_bursts - info.play.eof > 2) return 0;
    return ao_data.outburst;

	//return av_fifo_space(buffer);
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data, int len, int flags)
{
    if (!(flags & AOPLAY_FINAL_CHUNK)) {
		len /= ao_data.outburst;
		len *= ao_data.outburst;
    }

    if (len <= 0) return 0;

    len = write(audio_fd, data, len);

    if(len > 0) {
		if (write(audio_fd, data, 0) < 0)
		    perror("ao_laylaos: send EOF audio record");
		else
		    queued_bursts++;
    }

    return len;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(void)
{
    audio_info_t info;
    ioctl(audio_fd, AUDIO_GETINFO, &info);
	return (float)((queued_bursts - info.play.eof) * ao_data.outburst) / (float)byte_per_sec;
}

