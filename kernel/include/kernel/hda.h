/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: hda.h
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
 *  \file hda.h
 *
 *  Functions and macros for working with Intel High Definition Audio (HDA)
 *  capable devices.
 */

#ifndef __INTEL_HDAUDIO_H__
#define __INTEL_HDAUDIO_H__

#include <sys/types.h>
#include <kernel/mutex.h>


/*
 * Registers
 */
#define REG_GLOBCAP             0x00        // global capabilities
#define REG_GLOBCTL             0x08        // global control
#define REG_WAKEEN              0x0c        // wake enable
#define REG_STATESTS            0x0e        // state change status
#define REG_INTCTL              0x20        // interrupt control
#define REG_INTSTS              0x24        // interrupt status

#define REG_CORBLBASE           0x40        // CORB lower base address
#define REG_CORBUBASE           0x44        // CORB upper base address
#define REG_CORBWP              0x48        // CORB write pointer
#define REG_CORBRP              0x4a        // CORB read pointer
#define REG_CORBCTL             0x4c        // CORB control
#define REG_CORBSIZE            0x4e        // CORB size

#define REG_RIRBLBASE           0x50        // RIRB lower base address
#define REG_RIRBUBASE           0x54        // RIRB upper base address
#define REG_RIRBWP              0x58        // RIRB write pointer
#define REG_RINTCNT             0x5a        // RIRB response interrupt count
#define REG_RIRBCTL             0x5c        // RIRB control
#define REG_RIRBSTS             0x5d        // RIRB interrupt status
#define REG_RIRBSIZE            0x5e        // RIRB size

#define REG_DPLBASE             0x70        // DMA position lower base address
#define REG_DPUBASE             0x74        // DMA position upper base address

#define REG_ISS0_CTL            0x80

#define REG_OFFSET_OUT_CTLL     0x00
#define REG_OFFSET_OUT_CTLU     0x02
#define REG_OFFSET_OUT_STS      0x03
#define REG_OFFSET_OUT_LPIB     0x04
#define REG_OFFSET_OUT_CBL      0x08
#define REG_OFFSET_OUT_STLVI    0x0c
#define REG_OFFSET_OUT_FMT      0x12
#define REG_OFFSET_OUT_BDLPL    0x18
#define REG_OFFSET_OUT_BDLPU    0x1c


/*
 * Codec verbs
 */
#define VERB_GET_PARAMETER          0xf0000
#define VERB_SET_STREAM_CHANNEL     0x70600
#define VERB_SET_FORMAT             0x20000
#define VERB_GET_AMP_GAIN_MUTE      0xb0000
#define VERB_SET_AMP_GAIN_MUTE      0x30000
#define VERB_GET_CONFIG_DEFAULT     0xf1c00
#define VERB_GET_CONN_LIST          0xf0200
#define VERB_GET_CONN_SELECT        0xf0100
#define VERB_GET_PIN_CONTROL        0xf0700
#define VERB_SET_PIN_CONTROL        0x70700
#define VERB_GET_EAPD_BTL           0xf0c00
#define VERB_SET_EAPD_BTL           0x70c00
#define VERB_GET_POWER_STATE        0xf0500
#define VERB_SET_POWER_STATE        0x70500


/*
 * Codec parameters
 */
#define WIDGET_PARAM_VENDOR_ID              0x0
#define WIDGET_PARAM_REVISION_ID            0x2
#define WIDGET_PARAM_SUBNODE_COUNT          0x4
#define WIDGET_PARAM_FUNC_GROUP_TYPE        0x5
#define WIDGET_PARAM_WIDGET_CAPS            0x9
#define WIDGET_PARAM_OUT_AMP_CAPS           0x12


/*
 * Function group types
 */
#define FN_GROUP_AUDIO              0x01


/*
 * Widget types
 */
#define WIDGET_OUTPUT               0x0
#define WIDGET_INPUT                0x1
#define WIDGET_MIXER                0x2
#define WIDGET_SELECTOR             0x3
#define WIDGET_PIN                  0x4
#define WIDGET_POWER                0x5
#define WIDGET_VOLUME_KNOB          0x6
#define WIDGET_BEEP_GEN             0x7
#define WIDGET_VENDOR_DEFINED       0xf


/*
 * BDL memory size
 */
#define BDL_ENTRIES                 16
#define BDL_BUFSZ                   (PAGE_SIZE >> 1)


/*
 * Sample rate (TODO: revise page 207 of the spec)
 */
#define SR_48_KHZ                   (0)
#define SR_44_KHZ                   (1 << 14)
#define SR_96_KHZ                   (SR_48_KHZ | (1 << 11))
#define SR_88_KHZ                   (SR_44_KHZ | (1 << 11))
#define SR_144_KHZ                  (SR_48_KHZ | (2 << 11))
#define SR_192_KHZ                  (SR_48_KHZ | (3 << 11))
#define SR_176_KHZ                  (SR_44_KHZ | (3 << 11))

#define SR_24_KHZ                   (SR_48_KHZ | (1 << 8))
#define SR_22_KHZ                   (SR_44_KHZ | (1 << 8))
#define SR_16_KHZ                   (SR_48_KHZ | (2 << 8))
#define SR_14_KHZ                   (SR_44_KHZ | (2 << 8))
#define SR_11_KHZ                   (SR_44_KHZ | (3 << 8))
#define SR_9_KHZ                    (SR_48_KHZ | (4 << 8))
#define SR_8_KHZ                    (SR_48_KHZ | (5 << 8))
#define SR_6_KHZ                    (SR_48_KHZ | (7 << 8))


/*
 * Bits per sample
 */
#define BITS_8                      (0 <<  4)
#define BITS_16                     (1 <<  4)
#define BITS_20                     (2 <<  4)
#define BITS_24                     (3 <<  4)
#define BITS_32                     (4 <<  4)


/**
 * @struct hda_buf_t
 * @brief The hda_buf_t structure.
 *
 * A structure to represent an HDA buffer.
 */
struct hda_buf_t
{
    size_t size;            /**< buffer size in bytes */
    struct hda_buf_t *next; /**< next buffer in device list */
    uint8_t *curptr;
    uint8_t buf[];          /**< buffer data */
};


/**
 * @struct hda_out_t
 * @brief The hda_out_t structure.
 *
 * A structure to represent an HDA output device.
 */
struct hda_out_t
{
    int sample_format;          /**< output sample format */
    uint8_t vol;                /**< output volume */
    int codec,                  /**< codec to which this output belongs */
        node;                   /**< node to which this output belongs */
    int amp_gain_steps,         /**< amplifier gain step */
        nchan;                  /**< output channel count */
    uint32_t sample_rate;       /**< output sample rate */
    uint16_t base_port;         /**< base port */

    uintptr_t pbdl_base;            /**< BDL physical memory address */
    struct hda_bdl_entry_t *bdl;    /**< BDL entries */
    uintptr_t vbdl[BDL_ENTRIES];    /**< BDL virtual memory address */

    int hasdata[BDL_ENTRIES];
    uint32_t curdesc;
    ssize_t bytes_playing;          /**< count of bytes currently playing */

    struct hda_out_t *next;         /**< next output for this device */
};


/**
 * @struct hda_queue_t
 * @brief The hda_queue_t structure.
 *
 * A structure to represent an HDA output queue.
 */
struct hda_queue_t
{
    int queued;                 /**< number of queued entries */
    size_t bytes;               /**< count of queued bytes in all entries */
    struct hda_buf_t *head,     /**< pointer to queue head */
                     *tail;     /**< pointer to queue tail */
    struct kernel_mutex_t lock; /**< queue lock */
};


/**
 * @struct hda_bdl_entry_t
 * @brief The hda_bdl_entry_t structure.
 *
 * A structure to represent a single BDL table entry.
 */
struct hda_bdl_entry_t
{
    uint64_t paddr;     /**< buffer physical address */
    uint32_t len;       /**< buffer length */
    uint32_t flags;     /**< entry flags */
} __attribute__((packed));


/**
 * @struct hda_dev_t
 * @brief The hda_dev_t structure.
 *
 * A structure to represent an HDA device.
 */
struct hda_dev_t
{

#define HDA_FLAG_PLAYING                0x01    /**< device is playing */
#define HDA_FLAG_MUTED                  0x02    /**< device is muted */
#define HDA_FLAG_DUMMY                  0x04    /**< device is a dummy */
#define HDA_FLAG_ERROR                  0x08    /**< device has error(s) */
    int flags;          /**< device flags */

    dev_t devid;        /**< device id */
    int mmio;           /**< device uses memory-mapped I/O (MMIO) */

    uintptr_t iobase,   /**< I/O space base address */
              iosize;   /**< I/O space size */

    size_t nin,         /**< in stream count (upto 15) */
           nout,        /**< out stream count (upto 15) */
           nbi;         /**< bidirectional stream count (upto 15) */

    size_t ncorb,       /**< number of CORB entries */
           nrirb;       /**< number of RIRB entries */

    uint32_t *corb;             /**< CORB virtual address */
    volatile uint64_t *rirb;    /**< RIRB virtual address */
    struct hda_bdl_entry_t *bdl;    /**< BDL entries */
    struct hda_out_t *out;      /**< output list */
    struct pci_dev_t *pci;      /**< back pointer to PCI device */
    struct task_t *task;        /**< IRQ handler kernel task */
    struct hda_queue_t outq;    /**< output queue */
    struct hda_dev_t *next;     /**< next HDA device */

    uintptr_t pcorb,    /**< CORB physical address */
              prirb;    /**< RIRB physical address */

    int eof;            /**< EOF (zero-sized writes) counter */
};


/**
 * @var first_hda
 * @brief Head of the Intel HDA device list.
 *
 * This variable points to the first device on the Intel HDA device list.
 */
extern struct hda_dev_t *first_hda;


/************************
 * Functions
 ************************/

/**
 * @brief Initialise an HDA device.
 *
 * Initialise the given PCI device, which should be an Intel High Definition
 * Audio (HDA) capable device.
 *
 * @param   pci     PCI structure of the ATA device to initialize
 *
 * @return  zero on success, -(errno) on failure.
 */
int hda_init(struct pci_dev_t *pci);

/**
 * @brief Set HDA device output volume.
 *
 * The given volume should be in the range 0 to 255, inclusive.
 *
 * @param   hda         Intel HDA device
 * @param   vol         volume
 * @param   overwrite   if zero, output volume is set but volume value is
 *                        not saved in the \a hda structure (useful when
 *                        muting output)
 *
 * @return  nothing.
 */
void hda_set_volume(struct hda_dev_t *hda, uint8_t vol, int overwrite);

/**
 * @brief Set HDA device output channels.
 *
 * Channels should be in the range 1 to 16, inclusive.
 *
 * @param   hda         Intel HDA device
 * @param   nchan       number of channels (1 to 16)
 *
 * @return  zero on success, -(errno) on failure.
 */
int hda_set_channels(struct hda_dev_t *hda, int nchan);

/**
 * @brief Set HDA device output sample rate.
 *
 * Valid sample rates are:
 * - 48000 (48 kHz)
 * - 44100 (44.1 kHz)
 * - 96000 (96 kHz)
 * - 88200 (88.2 kHz)
 * - 144000 (144 kHz)
 * - 192000 (192 kHz)
 * - 176400 (176.4 kHz)
 * - 24000 (24 kHz)
 * - 22050 (22.05 kHz)
 * - 16000 (16 kHz)
 * - 14000 (14 kHz)
 * - 11025 (11.025 kHz)
 * - 9000 (9 kHz)
 * - 8000 (8 kHz)
 * - 6000 (6 kHz).
 *
 * @param   hda         Intel HDA device
 * @param   sample_rate sample rate
 *
 * @return  zero on success, -(errno) on failure.
 */
int hda_set_sample_rate(struct hda_dev_t *hda, unsigned int sample_rate);

/**
 * @brief Set HDA device output bits per sample.
 *
 * Valid values are:
 * - 8
 * - 16
 * - 20
 * - 24
 * - 32.
 *
 * @param   hda         Intel HDA device
 * @param   bits        bits per sample
 *
 * @return  zero on success, -(errno) on failure.
 */
int hda_set_bits_per_sample(struct hda_dev_t *hda, int bits);

/**
 * @brief Set HDA device block size.
 *
 * This function is a no-op currently.
 *
 * @param   hda         Intel HDA device
 * @param   blksz       block size in bytes
 *
 * @return  zero on success, -(errno) on failure.
 */
int hda_set_blksz(struct hda_dev_t *hda, unsigned int blksz);

/**
 * @brief Get HDA device output sample rate.
 *
 * See the description of hda_set_sample_rate() for valid values.
 *
 * @param   hda         Intel HDA device
 *
 * @return  sample rate on success, 0 on failure.
 */
unsigned int hda_get_sample_rate(struct hda_dev_t *hda);

/**
 * @brief Get HDA device output bits per sample.
 *
 * See the description of hda_set_bits_per_sample() for valid values.
 *
 * @param   hda         Intel HDA device
 *
 * @return  bits per sample on success, 0 on failure.
 */
int hda_get_bits_per_sample(struct hda_dev_t *hda);

/**
 * @brief Start/stop HDA device output.
 *
 * If cmd is non-zero, output is started, otherwise it is stopped.
 *
 * @param   hda         Intel HDA device
 * @param   cmd         start or stop sound output
 *
 * @return  always zero.
 */
int hda_play_stop(struct hda_dev_t *hda, int cmd);

/**
 * @brief Create dummy HDA device.
 *
 * Create an HDA device for dummy output.
 *
 * @return  dummy device id.
 */
dev_t create_dummy_hda(void);

#endif      /* __INTEL_HDAUDIO_H__ */
