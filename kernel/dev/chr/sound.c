/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
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


/*
 * General device control function.
 */
int snddev_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel)
{
    struct hda_dev_t *hda;
    audio_info_t info;
    int res;

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

            if(kernel)
            {
                A_memcpy(arg, &info, sizeof(audio_info_t));
                return 0;
            }
            else
            {
                return copy_to_user(arg, &info, sizeof(audio_info_t));
            }

        case AUDIO_START:
            return hda_play_stop(hda, 1);

        case AUDIO_STOP:
            return hda_play_stop(hda, 0);

        case AUDIO_FLUSH:
            if(hda->flags & HDA_FLAG_DUMMY)
            {
                return 0;
            }

            if(hda->out)
            {
                struct hda_buf_t *buf;

                elevated_priority_lock(&hda->outq.lock);

                while(hda->outq.head)
                {
                    buf = hda->outq.head;
                    hda->outq.head = buf->next;
                    kfree(buf);
                }

                hda->outq.tail = NULL;
                hda->outq.queued = 0;

                elevated_priority_unlock(&hda->outq.lock);
            }

            /*
             * TODO: flush input (microphone/recording) buffers as well.
             */

            return 0;

        case AUDIO_DRAIN:
            if(hda->flags & HDA_FLAG_DUMMY)
            {
                return 0;
            }

            if(hda->out)
            {
                volatile int queued;

                elevated_priority_lock(&hda->outq.lock);
                queued = hda->outq.queued;

                while(queued)
                {
                    elevated_priority_unlock(&hda->outq.lock);
                    lock_scheduler();
                    scheduler();
                    unlock_scheduler();
                    elevated_priority_relock(&hda->outq.lock);
                    queued = hda->outq.queued;
                }

                elevated_priority_unlock(&hda->outq.lock);
            }

            /*
             * TODO: drain input (microphone/recording) buffers as well.
             */

            return 0;
    }
    
    return -EINVAL;
}


/*
 * Write to a sound device (major = 14).
 */
ssize_t snddev_write(struct file_t *f, off_t *pos,
                     unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
    UNUSED(kernel);

    struct hda_dev_t *hda;
    struct hda_buf_t *hdabuf;
    volatile int queued;
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
    
    // For simplicity, we output a maximum of BDL_BUFSZ bytes. It is the
    // caller's responsibility to check the result and issue another call
    // with the rest of the stream data
    if(count > BDL_BUFSZ)
    {
        count = BDL_BUFSZ;
    }

    if(hda->flags & HDA_FLAG_DUMMY)
    {
        return count;
    }

    if(!(hdabuf = kmalloc(sizeof(struct hda_buf_t) + count)))
    {
        return -ENOMEM;
    }
    
    A_memset(hdabuf, 0, sizeof(struct hda_buf_t));
    hdabuf->size = count;
    //hdabuf->buf = &hdabuf[1];
    
    if(copy_from_user(hdabuf->buf, buf, count) != 0)
    {
        kfree(hdabuf);
        return -EFAULT;
    }
    
    elevated_priority_lock(&hda->outq.lock);
    
    queued = hda->outq.queued;
    
    while(queued >= MAX_QUEUED)
    {
        elevated_priority_unlock(&hda->outq.lock);
        //block_task2(&hda->outq, PIT_FREQUENCY);
        lock_scheduler();
        scheduler();
        unlock_scheduler();
        elevated_priority_relock(&hda->outq.lock);
        queued = hda->outq.queued;
    }
    
    if(hda->outq.head == NULL)
    {
        hda->outq.head = hdabuf;
        hda->outq.tail = hdabuf;
    }
    else
    {
        hda->outq.tail->next = hdabuf;
        hda->outq.tail = hdabuf;
    }
    
    hda->outq.queued++;
    
    elevated_priority_unlock(&hda->outq.lock);
    unblock_kernel_task(hda->task);
    
    return count;
}


/*
 * Read from a sound device (major = 14).
 */
ssize_t snddev_read(struct file_t *f, off_t *pos,
                    unsigned char *buf, size_t count, int kernel)
{
    UNUSED(pos);
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
int snddev_select(struct file_t *f, int which)
{
    dev_t dev;

    if(!f || !f->node)
    {
        return 0;
    }
    
	if(!S_ISCHR(f->node->mode))
	{
	    return 0;
	}
	
	dev = f->node->blocks[0];

    if(!hda_for_devid(dev))
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

    	case FWRITE:
            /*
             * We buffer writing anyway, so return success always.
             */
            return 1;
    	    
    	case 0:
   			break;
	}

	return 0;
}

