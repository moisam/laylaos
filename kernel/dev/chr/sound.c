/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: sound.c
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
 *  \file sound.c
 *
 *  Read, write, select and poll switch functions for sound devices
 *  (major = 14).
 */

#include <errno.h>
#include <sys/audioio.h>
#include <kernel/laylaos.h>
#include <kernel/hda.h>
#include <kernel/task.h>
#include <kernel/user.h>
#include <kernel/fcntl.h>
#include <kernel/dev.h>
#include <mm/kheap.h>

#include <kernel/asm.h>

#define MAX_QUEUED                      256
#define MAX_BYTES                       (BDL_ENTRIES * BDL_BUFSZ)

#if BYTE_ORDER == LITTLE_ENDIAN
# define AUDIO_ENCODING_PLATFORM        AUDIO_ENCODING_SLINEAR_LE
#elif BYTE_ORDER == BIG_ENDIAN
# define AUDIO_ENCODING_PLATFORM        AUDIO_ENCODING_SLINEAR_BE
#else
# error Byte order not specified!
#endif


static struct hda_dev_t *hda_for_devid(dev_t dev)
{
    struct hda_dev_t *hda = first_hda;

    while(hda)
    {
        //printk("dev 0x%x, hda->devid 0x%x\n", dev, hda->devid);

        if(hda->devid == dev)
        {
            return hda;
        }
        
        hda = hda->next;
    }
    
    return NULL;
}


#define COPY_RESULT_AND_RETURN(arg, st)                 \
    if(kernel) {                                        \
        A_memcpy(arg, &st, sizeof(st));                 \
        return 0;                                       \
    } else return copy_to_user(arg, &st, sizeof(st));


/*
 * General device control function.
 */
long snddev_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel)
{
    struct hda_dev_t *hda;
    audio_info_t info;
    struct audio_swpar swpar;
    long res;

    if(!(hda = hda_for_devid(dev)))
    {
        return -ENOTTY;
    }
    
    switch(cmd)
    {
        case AUDIO_SETINFO:
            if(kernel)
            {
                A_memcpy(&info, arg, sizeof(audio_info_t));
            }
            else
            {
                COPY_FROM_USER(&info, arg, sizeof(audio_info_t));
            }

            if(info.mode & AUMODE_PLAY)
            {
                /*
                 * Currently we only support signed integer samples.
                 */
                if(info.play.encoding &&
                   info.play.encoding != AUDIO_ENCODING_SLINEAR &&
                   info.play.encoding != AUDIO_ENCODING_PLATFORM)
                {
                    return -EINVAL;
                }
                
                if((res = hda_set_bits_per_sample(hda,
                                                  info.play.precision)) != 0)
                {
                    return -EINVAL;
                }

                if((res = hda_set_channels(hda, info.play.channels)) != 0)
                {
                    return -EINVAL;
                }

                if((res = hda_set_sample_rate(hda,
                                              info.play.sample_rate)) != 0)
                {
                    return -EINVAL;
                }

                hda->eof = info.play.eof;
                
                if(!info.play.error)
                {
                    hda->flags &= ~HDA_FLAG_ERROR;
                }

                #if 0
                if(info.output_muted)
                {
                    hda_set_volume(hda, 0, 0);      // mute
                    /*
                    uint32_t codec = (hda->nodes[hda->speaker_node] >> 16) & 0xff;
                    uint32_t node = hda->nodes[hda->speaker_node] & 0xff;
                    hda->volume = 0;
                    hda_set_volume(hda, codec, node);
                    */
                }
                //else if(info.play.gain)
                else if(info.play.gain &&
                        info.play.gain <= 255 /* && hda->speaker_node != 0xffffffff */)
                {
                    hda_set_volume(hda, info.play.gain, 1);
                    /*
                    uint32_t codec = (hda->nodes[hda->speaker_node] >> 16) & 0xff;
                    uint32_t node = hda->nodes[hda->speaker_node] & 0xff;
                    hda->volume = info.play.gain;
                    hda_set_volume(hda, codec, node);
                    */
                }
                else if(hda->out)
                {
                    hda_set_volume(hda, hda->out->vol, 0);   // unmute
                }
                #endif

                //hda_config_out_widgets(hda);

                /*
                if((res = hda_play_stop(hda, 1)) != 0)
                {
                    return res;
                }
                */
            }

            if(info.mode & AUMODE_RECORD)
            {
                if((res = hda_play_stop(hda, 0)) != 0)
                {
                    return res;
                }
            }

            if((res = hda_set_blksz(hda, info.blocksize)) != 0)
            {
                return -EINVAL;
            }
            
            return 0;

        case AUDIO_GETINFO:
            A_memset(&info, 0, sizeof(audio_info_t));
            
            if(hda->out)
            {
                info.play.sample_rate = hda_get_sample_rate(hda);
                info.play.precision = hda_get_bits_per_sample(hda);
                info.play.channels = hda->out->nchan;
                info.play.gain = hda->out->vol;
                info.play.encoding = AUDIO_ENCODING_PLATFORM;
                info.play.buffer_size = BDL_BUFSZ;
                info.play.active = !!(hda->flags & HDA_FLAG_PLAYING);
                info.play.pause = !(hda->flags & HDA_FLAG_PLAYING);
                info.play.eof = hda->eof;
                info.play.error = !!(hda->flags & HDA_FLAG_ERROR);
                info.output_muted = !!(hda->flags & HDA_FLAG_MUTED);
                info.blocksize = BDL_BUFSZ;
                info.mode = AUMODE_PLAY;
            }

            COPY_RESULT_AND_RETURN(arg, info);

        case AUDIO_SETPAR:
            if(kernel)
            {
                A_memcpy(&swpar, arg, sizeof(swpar));
            }
            else
            {
                COPY_FROM_USER(&swpar, arg, sizeof(swpar));
            }

            // NOTE: We ignore the bps (bytes per sample) field and use
            //       the bits per sample field instead
            if(swpar.bits &&
               (res = hda_set_bits_per_sample(hda, swpar.bits)) != 0)
            {
                return -EINVAL;
            }

#if BYTE_ORDER == BIG_ENDIAN
            if(swpar.le)
            {
                return -EINVAL;
            }
#endif

            if(swpar.pchan && (res = hda_set_channels(hda, swpar.pchan)) != 0)
            {
                return -EINVAL;
            }

            /*
             * TODO: Use rchan to set recording channels.
             */

            if(swpar.rate && (res = hda_set_sample_rate(hda, swpar.rate)) != 0)
            {
                return -EINVAL;
            }

            return 0;

        case AUDIO_GETPAR:
            A_memset(&swpar, 0, sizeof(swpar));

            if(hda->out)
            {
#if BYTE_ORDER == LITTLE_ENDIAN
                swpar.le = 1;
#elif BYTE_ORDER == BIG_ENDIAN
                swpar.le = 0;
#else
# error Byte order not specified!
#endif

                swpar.sig = 1;
                swpar.bits = hda_get_bits_per_sample(hda);
                swpar.bps = (swpar.bits <= 16) ? 2 : 4;
                swpar.msb = 1;
                swpar.rate = hda_get_sample_rate(hda);
                swpar.pchan = hda->out->nchan;
                swpar.rchan = 2;    // TODO
                swpar.nblks = 2;
                swpar.round = BDL_BUFSZ / 2;
            }

            COPY_RESULT_AND_RETURN(arg, swpar);

        case AUDIO_GETPOS:
        {
            struct audio_pos pos;

            pos.play_pos = hda->bytes_played;  // total bytes played

            // TODO: Fill these fields
            pos.play_xrun = 0;      // bytes of silence inserted
            pos.rec_pos = 0;        // total bytes recorded
            pos.rec_xrun = 0;       // bytes dropped

            COPY_RESULT_AND_RETURN(arg, pos);
        }

        case AUDIO_START:
            hda->bytes_played = 0;
            return hda_play_stop(hda, 1);

        case AUDIO_STOP:
            hda->bytes_played = 0;
            return hda_play_stop(hda, 0);

        case AUDIO_FLUSH:
            if(hda->flags & HDA_FLAG_DUMMY)
            {
                return 0;
            }

#if 0

            if(hda->out)
            {
                volatile struct hda_buf_t *buf;

                elevated_priority_lock(&hda->outq.lock);

                while(hda->outq.head)
                {
                    buf = hda->outq.head;
                    hda->outq.head = buf->next;
                    kfree((void *)buf);
                }

                hda->outq.tail = NULL;
                hda->outq.queued = 0;
                hda->outq.bytes = 0;

                elevated_priority_unlock(&hda->outq.lock);
            }

#endif

            /*
             * TODO: flush input (microphone/recording) buffers as well.
             */

            hda->bytes_played = 0;
            return hda_play_stop(hda, 0);

        case AUDIO_DRAIN:
            if(hda->flags & HDA_FLAG_DUMMY)
            {
                return 0;
            }

#if 0

            if(hda->out)
            {
                volatile int queued;

                elevated_priority_lock(&hda->outq.lock);
                queued = hda->outq.queued;

                while(queued)
                {
                    elevated_priority_unlock(&hda->outq.lock);
                    scheduler();
                    elevated_priority_relock(&hda->outq.lock);
                    queued = hda->outq.queued;
                }

                elevated_priority_unlock(&hda->outq.lock);
            }

#endif

            /*
             * TODO: drain input (microphone/recording) buffers as well.
             */

            hda->bytes_played = 0;
            return hda_play_stop(hda, 0);

        case AUDIO_GETDEV:
        {
            struct audio_device adev;

            A_memset(&adev, 0, sizeof(adev));
            ksprintf(adev.name, MAX_AUDIO_DEV_LEN, "%s", "Intel HDA");
            COPY_RESULT_AND_RETURN(arg, adev);
        }
    }
    
    return -EINVAL;
}


/*
static inline void free_bufs(struct hda_buf_t *head)
{
    struct hda_buf_t *hdabuf, *next;

    for(hdabuf = head; hdabuf; )
    {
        next = hdabuf->next;
        kfree(hdabuf);
        hdabuf = next;
    }
}
*/


/*
 * Write to a sound device (major = 14).
 */
ssize_t snddev_write(struct file_t *f, off_t *pos,
                     unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(kernel);

    struct hda_dev_t *hda;
    //struct hda_buf_t *hdabuf /* , *head, *tail */;
    //volatile size_t pending;
    ////volatile int queued;
    ////int bufcount;
    ////size_t left, len;
    dev_t dev = f->node->blocks[0];

    if(!(hda = hda_for_devid(dev)))
    {
        return -ENOTTY;
    }
    
    if(/* !buf || */ !count)
    {
        // record EOF (zero-sized writes)
        if(hda->out)
        {
            hda->eof++;
            return 0;
        }

        return -EINVAL;
    }

    if(hda->flags & HDA_FLAG_DUMMY)
    {
        hda->bytes_played += count;
        return count;
    }


    return hda_write_buf(hda, (char *)buf, count);


#if 0

    elevated_priority_lock(&hda->outq.lock);

#if 0

    if(hda->outq.bytes >= MAX_BYTES)
    {
        elevated_priority_unlock(&hda->outq.lock);
        unblock_kernel_task(hda->task);
        return -EAGAIN;
    }

#endif

    pending = hda->outq.bytes;
    
    while(pending + count /* + bufcount */ >= MAX_BYTES /* MAX_QUEUED */)
    {
        elevated_priority_unlock(&hda->outq.lock);
        //block_task2(&hda->outq, PIT_FREQUENCY);
        scheduler();
        elevated_priority_relock(&hda->outq.lock);
        pending = hda->outq.bytes;
    }

    elevated_priority_unlock(&hda->outq.lock);

    // The HDA driver expects buffers <= BDL_BUFSZ in length. If the data
    // is larger, break it down into multiple buffers and then them to the
    // device's queue.

    if(!(hdabuf = kmalloc(sizeof(struct hda_buf_t) + count)))
    {
        return -ENOMEM;
    }

    A_memset(hdabuf, 0, sizeof(struct hda_buf_t));

    hdabuf->size = count;
    hdabuf->curptr = hdabuf->buf;
    /*
    __lock_xchg_ptr(&hdabuf->size, count);
    __lock_xchg_ptr(&hdabuf->curptr, (uintptr_t)hdabuf->buf);
    */

    if(copy_from_user(hdabuf->buf, buf, count) != 0)
    {
        return -EFAULT;
    }

    /*
    left = count;
    bufcount = 0;
    head = NULL;
    tail = NULL;

    while(left != 0)
    {
        len = (left > BDL_BUFSZ) ? BDL_BUFSZ : left;

        if(!(hdabuf = kmalloc(sizeof(struct hda_buf_t) + len)))
        {
            free_bufs(head);
            return -ENOMEM;
        }

        A_memset(hdabuf, 0, sizeof(struct hda_buf_t));
        hdabuf->size = len;
        hdabuf->curptr = hdabuf->buf;

        if(copy_from_user(hdabuf->buf, buf, len) != 0)
        {
            free_bufs(head);
            return -EFAULT;
        }

        if(head == NULL)
        {
            head = hdabuf;
            tail = hdabuf;
        }
        else
        {
            tail->next = hdabuf;
        }

        buf += len;
        left -= len;
        bufcount++;
    }
    */
    
    elevated_priority_relock(&hda->outq.lock);
    
    if(hda->outq.head == NULL)
    {
        hda->outq.head = hdabuf /* head */;
        hda->outq.tail = hdabuf /* tail */;
    }
    else
    {
        hda->outq.tail->next = hdabuf /* head */;
        hda->outq.tail = hdabuf /* tail */;
    }

    hda->outq.queued++;
    //hda->outq.queued += bufcount;
    hda->outq.bytes += count;
    hda->bytes_played += count;
    /*
    __sync_fetch_and_add(&hda->outq.queued, 1);
    __sync_fetch_and_add(&hda->outq.bytes, count);
    __sync_fetch_and_add(&hda->bytes_played, count);
    */

    elevated_priority_unlock(&hda->outq.lock);
    unblock_kernel_task(hda->task);
    
    return count;

#endif

}


/*
 * Read from a sound device (major = 14).
 */
ssize_t snddev_read(struct file_t *f, off_t *pos,
                    unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(buf);
    UNUSED(kernel);

    struct hda_dev_t *hda;
    dev_t dev = f->node->blocks[0];

    if(!(hda = hda_for_devid(dev)))
    {
        return -ENOTTY;
    }
    
    if(/* !buf || */ !count)
    {
        return -EINVAL;
    }
    
    /*
     * No voice recording for now.
     * TODO: this!
     */
    return -ENOSYS;
}


/*
 * Perform a select operation on a sound device (major = 14).
 */
long snddev_select(struct file_t *f, int which)
{
    dev_t dev;
    //int res;
    struct hda_dev_t *hda;

    if(!f || !f->node)
    {
        return 0;
    }
    
	if(!S_ISCHR(f->node->mode))
	{
	    return 0;
	}
	
	dev = f->node->blocks[0];

    if(!(hda = hda_for_devid(dev)))
    {
        return 0;
    }

	switch(which)
	{
    	case FREAD:
            /*
             * No voice recording for now.
             * TODO: this!
             */
            return 0;

    	case FWRITE: ;
            /*
            elevated_priority_lock(&hda->outq.lock);
            //res = !(hda->outq.queued >= MAX_QUEUED);
            res = !(hda->outq.bytes >= MAX_BYTES);
            elevated_priority_unlock(&hda->outq.lock);
            return res;
            */
            return 1;
    	    
    	case 0:
   			break;
	}

	return 0;
}


/*
 * Perform a poll operation on a sound device (major = 14).
 */
long snddev_poll(struct file_t *f, struct pollfd *pfd)
{
    long res = 0;
    dev_t dev;
    struct hda_dev_t *hda;

    if(!f || !f->node)
    {
        return 0;
    }
    
	if(!S_ISCHR(f->node->mode))
	{
	    return 0;
	}
	
	dev = f->node->blocks[0];

    if(!(hda = hda_for_devid(dev)))
    {
        return 0;
    }
    
    if(pfd->events & POLLOUT)
    {
        pfd->revents |= POLLOUT;
        res = 1;
    }

    // TODO: we don't support reading for now (that is, no POLLIN events)
    
    return res;
}

