/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: hda2.c
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
 *  \file hda2.c
 *
 *  Intel High Definition Audio (HDA) device driver implementation.
 */

#include <errno.h>
#include <kernel/laylaos.h>
#include <kernel/pci.h>
#include <kernel/clock.h>
#include <kernel/timer.h>
#include <kernel/io.h>
#include <kernel/hda.h>
#include <kernel/pic.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <mm/kheap.h>
#include <mm/kstack.h>
#include <gui/vbe.h>
#include <kernel/asm.h>


uint8_t __inb(uintptr_t port) { return inb(port); }
uint16_t __inw(uintptr_t port) { return inw(port); }
uint32_t __inl(uintptr_t port) { return inl(port); }
void __outb(uintptr_t port, uint8_t command) { outb(port, command); }
void __outw(uintptr_t port, uint16_t command) { outw(port, command); }
void __outl(uintptr_t port, uint32_t command) { outl(port, command); }

#define hda_inb(p)          \
    ((hda->mmio) ? mmio_inb(hda->iobase + p) : __inb(hda->iobase + p))

#define hda_inw(p)          \
    ((hda->mmio) ? mmio_inw(hda->iobase + p) : __inw(hda->iobase + p))

#define hda_inl(p)          \
    ((hda->mmio) ? mmio_inl(hda->iobase + p) : __inl(hda->iobase + p))

#define hda_outb(p, c)      \
    ((hda->mmio) ? mmio_outb(hda->iobase + p, c) : __outb(hda->iobase + p, c))

#define hda_outw(p, c)      \
    ((hda->mmio) ? mmio_outw(hda->iobase + p, c) : __outw(hda->iobase + p, c))

#define hda_outl(p, c)      \
    ((hda->mmio) ? mmio_outl(hda->iobase + p, c) : __outl(hda->iobase + p, c))


static int last_unit = 0;
struct hda_dev_t *first_hda = NULL;

int hda_intr(struct regs *r, int unit);
void hda_task_func(void *arg);

// device for dummy output
static struct hda_dev_t dummy_hda = { 0, };
static struct hda_out_t dummy_out = { 0, };


static inline void hda_wait(int msecs)
{
    volatile unsigned long long last_ticks = ticks;

    while(msecs)
    {
        if(ticks != last_ticks)
        {
            msecs--;
            last_ticks = ticks;
        }

        __asm__ __volatile__("nop" ::: "memory");
    }
}


static int hda_send_verb(struct hda_dev_t *hda, uint32_t verb,
                                                uint64_t *response)
{
    uint16_t wp = hda_inw(REG_CORBWP);
    uint16_t rp1 = hda_inw(REG_RIRBWP);
    uint16_t rp2;
    int timeout;

    wp = (wp + 1) % hda->ncorb;

    hda->corb[wp] = verb;
    hda_outw(REG_CORBWP, wp);

    // wait 200 msecs (20 ticks at a rate of 100 ticks per sec)
    timeout = 20;
    rp2 = hda_inw(REG_RIRBWP);

    while((rp2 == rp1) && timeout--)
    {
        hda_wait(2);
        rp2 = hda_inw(REG_RIRBWP);
    }

    if(timeout <= 0)
    {
        printk("hda: timeout waiting for verb response\n");
        return 1;
    }

    *response = hda->rirb[(rp1 + 1) % hda->nrirb];

    return 0;
}


static inline uint32_t hda_make_verb(uint32_t codec, uint32_t node,
                                     uint32_t payload)
{
    return ((codec & 0xf) << 28) | 
           ((node & 0xff) << 20) | 
           (payload & 0xfffff);
}


static inline uint32_t hda_get_codec_param(struct hda_dev_t *hda,
                                           uint32_t codec, uint32_t node,
                                           uint32_t param)
{
    uint64_t response = 0;

    int res = hda_send_verb(hda, 
                            hda_make_verb(codec, node, 
                                          VERB_GET_PARAMETER | param), 
                            &response);

    if(res)
    {
        return 0xffffffff;
    }

    uint32_t dword = response >> 32;

    if((dword & 0xf) != codec)
    {
        return 0xffffffff;
    }

    return response & 0xffffffff;
}


static inline void hda_set_output_format(struct hda_dev_t *hda,
                                         struct hda_out_t *out)
{
    uint64_t qword;
    uint16_t word;

    word = out->sample_format | out->sample_rate | (out->nchan - 1);
    hda_send_verb(hda, hda_make_verb(out->codec, out->node,
                                     VERB_SET_FORMAT | word), &qword);
    hda_outw(out->base_port + REG_OFFSET_OUT_FMT, word);
}


static int hda_add_codec_output(struct hda_dev_t *hda, int codec, int node)
{
    struct hda_out_t *out;
    uintptr_t phys, virt;
    uint64_t qword;
    uint32_t dword;
    uint16_t word;
    uint8_t byte;
    size_t i;
    int timeout;

    if(!(out = kmalloc(sizeof(struct hda_out_t))))
    {
        return -ENOMEM;
    }

    A_memset(out, 0, sizeof(struct hda_out_t));
    out->codec = codec;
    out->node = node;
    out->sample_format = BITS_16;
    out->sample_rate = SR_48_KHZ;
    out->nchan = 2;
    //out->amp_gain_steps = (amp_cap >> 8) & 0x7f;


    out->base_port = REG_ISS0_CTL + (hda->nin * 0x20);

#define PAGE_FLAGS      (PTE_FLAGS_PW | I86_PTE_NOT_CACHEABLE)

    if(get_next_addr(&phys, &virt, PAGE_FLAGS, REGION_DMA) != 0)
    {
        kfree(out);
        return -ENOMEM;
    }

    out->bdl = (struct hda_bdl_entry_t *)virt;
    out->pbdl_base = phys;

    for(i = 0; i < BDL_ENTRIES; i += 2)
    {
        if(get_next_addr(&phys, &virt, PAGE_FLAGS, REGION_DMA) != 0)
        {
            kfree(out);
            return -ENOMEM;
        }

        out->bdl[i].len = BDL_BUFSZ;
        out->bdl[i].flags = 1;
        out->bdl[i].paddr = phys;
        out->vbdl[i] = virt;

        out->bdl[i + 1].len = BDL_BUFSZ;
        out->bdl[i + 1].flags = 1;
        out->bdl[i + 1].paddr = phys + (PAGE_SIZE >> 1);
        out->vbdl[i + 1] = virt + (PAGE_SIZE >> 1);
    }

#undef PAGE_FLAGS

    // set BDL address
    hda_outl(out->base_port + REG_OFFSET_OUT_BDLPL, 
                    (out->pbdl_base & 0xffffffff));
    hda_outl(out->base_port + REG_OFFSET_OUT_BDLPU, (out->pbdl_base >> 32));

    // exit stream reset
    word = hda_inw(out->base_port + REG_OFFSET_OUT_CTLL);
    hda_outw(out->base_port + REG_OFFSET_OUT_CTLL, word & ~0x1);

    // wait 400 msecs (40 ticks at a rate of 100 ticks per sec)
    timeout = 40;
    word = hda_inw(out->base_port + REG_OFFSET_OUT_CTLL);

    while(((word & 0x1) != 0) && timeout--)
    {
        hda_wait(1);
        word = hda_inw(out->base_port + REG_OFFSET_OUT_CTLL);
    }

    if(timeout <= 0)
    {
        printk("hda: stream reset timeout\n");
        kfree(out);
        return -ETIMEDOUT;
    }

    word = hda_inw(out->base_port + REG_OFFSET_OUT_CTLL);
    // clear reset, run, stripe mask, traffic priority, bidirectional dir, 
    // stream data tag
    word &= ~0x03;

    // enable IRQs
    word |= 0x1c;

    // set stream number (we are using the first output stream)
    byte = ((hda->nin & 0xf) << 4);
    hda_outb(out->base_port + REG_OFFSET_OUT_CTLU, byte);

    // clear status bits
    byte = hda_inb(out->base_port + REG_OFFSET_OUT_STS);
    hda_outb(out->base_port + REG_OFFSET_OUT_STS, byte | 0xe);

    hda_outl(out->base_port + REG_OFFSET_OUT_CBL, BDL_ENTRIES * BDL_BUFSZ);

    word = hda_inw(out->base_port + REG_OFFSET_OUT_STLVI);
    word &= ~0xff;
    word |= (BDL_ENTRIES - 1);
    hda_outw(out->base_port + REG_OFFSET_OUT_STLVI, word);

    // set output format
    hda_set_output_format(hda, out);

    // set stream channel (again, we are using the first output stream)
    hda_send_verb(hda, hda_make_verb(codec, node,
                                     VERB_SET_STREAM_CHANNEL |
                                        ((hda->nin & 0xf) << 4)), &qword);

    dword = hda_get_codec_param(hda, codec, node, WIDGET_PARAM_OUT_AMP_CAPS);
    out->amp_gain_steps = (dword >> 8) & 0x7f;
    
    // enable IRQs from this stream
    dword = hda_inl(REG_INTCTL);
    dword |= (1 << hda->nin);
    hda_outl(REG_INTCTL, dword);

    // add to device struct
    out->next = hda->out;
    hda->out = out;

    return 0;
}


static void hda_enum_widgets(struct hda_dev_t *hda, int codec)
{
    uint32_t vendor, revision, subnodes;
    int first_node, node_count, i;

    if((vendor = hda_get_codec_param(hda, codec, 0, 
                                     WIDGET_PARAM_VENDOR_ID)) == 0xffffffff)
    {
        printk("hda: ignoring device with vendor 0x%x\n", vendor);
        screen_refresh(NULL);
        return;
    }

    revision = hda_get_codec_param(hda, codec, 0, WIDGET_PARAM_REVISION_ID);
    subnodes = hda_get_codec_param(hda, codec, 0, WIDGET_PARAM_SUBNODE_COUNT);

    first_node = (subnodes >> 16) & 0xff;
    node_count = (subnodes & 0xff);

    printk("hda: codec %d - vendorid 0x%x, revid 0x%x, %d func groups "
           "starting at %d\n",
            codec, vendor, revision, node_count, first_node);
    //screen_refresh(NULL);

    for(i = first_node; i < first_node + node_count; i++)
    {
        uint32_t f = hda_get_codec_param(hda, codec, i, 
                                         WIDGET_PARAM_FUNC_GROUP_TYPE);

        if((f & 0xff) != FN_GROUP_AUDIO)
        {
            // not an audio function group
            continue;
        }

        uint32_t cap = hda_get_codec_param(hda, codec, i, 
                                           WIDGET_PARAM_WIDGET_CAPS);
        uint8_t type = ((cap >> 20) & 0xf);

        printk("hda: found widget of type 0x%x (codec %d, node %d)\n", 
               type, codec, i);

        if(type == WIDGET_OUTPUT)
        {
            printk("hda: found audio output at codec %d, node %d\n", codec, i);
            screen_refresh(NULL);
            hda_add_codec_output(hda, codec, i);
        }
    }
}


int hda_init(struct pci_dev_t *pci)
{
    struct hda_dev_t *hda;
    //uint16_t cap;
    uintptr_t bar0, phys, virt;
    int timeout;
    uint32_t dword;
    uint16_t word, i;
    uint8_t byte;
    int res = 0;
    char buf[8];
    
    if(!(hda = kmalloc(sizeof(struct hda_dev_t))))
    {
        printk("hda: insufficient memory to init device\n");
        return -ENOMEM;
    }
    
    A_memset(hda, 0, sizeof(struct hda_dev_t));
    
    printk("hda: found intel high definition audio (HDA) device\n");
    //printk("bar0 0x%x\n", pci->bar[0]);

#define BAR0_OFFSET             0x10

    // determine the size of the BAR
    bar0 = pci->bar[0];
    pci_config_write_long(pci->bus, pci->dev, pci->function, 
                                    BAR0_OFFSET, 0xffffffff);
    hda->iosize = pci_config_read_long(pci->bus, pci->dev, pci->function, 
                                    BAR0_OFFSET);
    hda->iosize &= ~0xf;    // mask the lower 4 bits
    hda->iosize = (~(hda->iosize) & 0xffffffff) + 1;     // invert and add 1
    pci_config_write_long(pci->bus, pci->dev, pci->function, 
                                    BAR0_OFFSET, bar0);
    bar0 = pci_config_read_long(pci->bus, pci->dev, pci->function, 
                                    BAR0_OFFSET);
    
    printk("hda: BAR0 " _XPTR_ ", iosize " _XPTR_ "\n", bar0, hda->iosize);

#undef BAR0_OFFSET

    // check whether I/O is memory-mapped or normal I/O
    if((pci->bar[0] & 0x1))
    {
        // I/O
        hda->iobase = pci->bar[0] & ~0x3;
    }
    else
    {
        // MMIO
        bar0 = pci->bar[0] & ~0xf;
        hda->iobase = mmio_map(bar0, bar0 + hda->iosize);
        hda->mmio = 1;
    }

    pci->unit = last_unit++;
    hda->devid = TO_DEVID(14, pci->unit);
    hda->pci = pci;

    if(!first_hda)
    {
        first_hda = hda;
    }
    else
    {
        struct hda_dev_t *tmp = first_hda;

        while(tmp->next)
        {
            tmp = tmp->next;
        }

        tmp->next = hda;
    }

    pci_enable_busmastering(pci);
    pci_enable_interrupts(pci);
    pci_enable_memoryspace(pci);

#define PAGE_FLAGS      (PTE_FLAGS_PW | I86_PTE_NOT_CACHEABLE)

    // alloc memory for RIRB & CORB buffers
    if(get_next_addr(&phys, &virt, PAGE_FLAGS, REGION_DMA) != 0)
    {
        res = -ENOMEM;
        goto err;
    }

    hda->pcorb = phys;
    hda->corb = (uint32_t *)virt;

    if(get_next_addr(&phys, &virt, PAGE_FLAGS, REGION_DMA) != 0)
    {
        res = -ENOMEM;
        goto err;
    }

    hda->prirb = phys;
    hda->rirb = (uint64_t *)virt;

#undef PAGE_FLAGS

    // register IRQ handler
    ksprintf(buf, 8, "hda%d", pci->unit);
    (void)start_kernel_task(buf, hda_task_func, hda, &hda->task,
                                 KERNEL_TASK_ELEVATED_PRIORITY);
    pci_register_irq_handler(pci, hda_intr, buf);

    // reset
    hda_outl(REG_GLOBCTL, 1);

    // wait 200 msecs (20 ticks at a rate of 100 ticks per sec)
    timeout = 20;

    while((hda_inl(REG_GLOBCTL) & 0x1) == 0 && timeout--)
    {
        hda_wait(1);
    }

    if(timeout <= 0)
    {
        printk("hda: device reset timeout\n");
        res = -ETIMEDOUT;
        goto err;
    }

    // disable ints
    hda_outl(REG_INTCTL, 0);

    // stop CORB/RIRB
    hda_outb(REG_CORBCTL, 0);
    hda_outb(REG_RIRBCTL, 0);

    // wait 200 msecs (20 ticks at a rate of 100 ticks per sec)
    timeout = 20;

    while(((hda_inl(REG_CORBCTL) & 0x2) ||
          (hda_inl(REG_RIRBCTL) & 0x2)) && timeout--)
    {
        hda_wait(1);
    }

    if(timeout <= 0)
    {
        printk("hda: device reset timeout\n");
        res = -ETIMEDOUT;
        goto err;
    }

    // get number of I/O ports
    word = hda_inw(REG_GLOBCAP);
    hda->nout = (word >> 12) & 0x0f;
    hda->nin = (word >> 8) & 0x0f;
    hda->nbi = (word >> 3) & 0x0f;

    printk("hda: iobase " _XPTR_ " (%s), sz 0x%x, cap 0x%x, "
           "nin 0x%x, nout 0x%x, nbi 0x%x\n",
            hda->iobase, hda->mmio ? "MMIO" : "IO", hda->iosize,
            word, hda->nin, hda->nout, hda->nbi);

    // turn DMA off
    hda_outl(REG_DPLBASE, 0);

    // setup CORB
    byte = hda_inb(REG_CORBSIZE);

    // if more than one size is supported, make sure we choose the
    // largest one and set it
    if(byte & (1 << 6))
    {
        byte = (byte & ~0x3) | 0x2;
        hda->ncorb = 256;
    }
    else if(byte & (1 << 5))
    {
        byte = (byte & ~0x3) | 0x1;
        hda->ncorb = 16;
    }
    else if(byte & (1 << 4))
    {
        hda->ncorb = 2;
    }
    else
    {
        printk("hda: unknown CORB size!\n");
        res = -EINVAL;
        goto err;
    }

    hda_outb(REG_CORBSIZE, byte);

    // setup RIRB
    byte = hda_inb(REG_RIRBSIZE);

    // if more than one size is supported, make sure we choose the
    // largest one and set it
    if(byte & (1 << 6))
    {
        byte = (byte & ~0x3) | 0x2;
        hda->nrirb = 256;
    }
    else if(byte & (1 << 5))
    {
        byte = (byte & ~0x3) | 0x1;
        hda->nrirb = 16;
    }
    else if(byte & (1 << 4))
    {
        hda->nrirb = 2;
    }
    else
    {
        printk("hda: unknown RIRB size!\n");
        res = -EINVAL;
        goto err;
    }

    hda_outb(REG_RIRBSIZE, byte);

    // setup CORB/RIRB base addresses
    hda_outl(REG_CORBLBASE, hda->pcorb & 0xffffffff);
    hda_outl(REG_CORBUBASE, hda->pcorb >> 32);
    hda_outl(REG_RIRBLBASE, hda->prirb & 0xffffffff);
    hda_outl(REG_RIRBUBASE, hda->prirb >> 32);

    // reset CORB/RIRB pointers
    hda_outw(REG_CORBWP, 0);
    hda_outw(REG_CORBRP, 0x8000);
    hda_outw(REG_RIRBWP, 0x8000);
    
    printk("hda: CORB sz %d, RIRB sz %d\n", hda->ncorb, hda->nrirb);

    /*
     * The standard says about bit 15 in the CORBRP register:
     *     Software writes a 1 to this bit to reset the CORB Read Pointer to 0
     *     and clear any residual pre-fetched commands in the CORB hardware 
     *     buffer within the controller. The hardware will physically update
     *     this bit to 1 when the CORB pointer reset is complete. Software must
     *     read a 1 to verify that the reset completed correctly. Software must
     *     clear this bit back to 0, by writing a 0, and then read back the 0 
     *     to verify that the clear completed correctly. The CORB DMA engine 
     *     must be stopped prior to resetting the Read Pointer or else DMA
     *     transfer may be corrupted.
     */
    timeout = 20;

    // read until bit 15 is set
    while(!(hda_inw(REG_CORBRP) & 0x8000) && timeout--)
    {
        hda_wait(1);
    }

    if(timeout <= 0)
    {
        printk("hda: CORBRP reset timeout (1)\n");
        res = -ETIMEDOUT;
        goto err;
    }

    // reset bit 15
    hda_outw(REG_CORBRP, 0);
    timeout = 20;

    // ensure we read back 0 in bit 15
    while((hda_inw(REG_CORBRP) & 0x8000) && timeout--)
    {
        hda_wait(1);
    }

    if(timeout <= 0)
    {
        printk("hda: CORBRP reset timeout (2)\n");
        res = -ETIMEDOUT;
        goto err;
    }

    if(timeout <= 0)
    {
        printk("hda: CORB/RIRB reset timeout (0x%x, 0x%x)\n", 
               hda_inw(REG_CORBRP), hda_inw(REG_RIRBWP));
        res = -ETIMEDOUT;
        goto err;
    }

    // disable wake IRQs
    word = hda_inw(REG_WAKEEN);
    hda_outw(REG_WAKEEN, word & ~0x7fU);
    //hda_outw(REG_WAKEEN, 0xffff);

    // enable IRQs and unsolicited responses
    dword = hda_inl(REG_GLOBCTL);
    hda_outl(REG_GLOBCTL, dword | 0x100);
    hda_outl(REG_INTCTL, 0xC0000000);
    hda_outw(REG_RINTCNT, 1);

    // start RIRB/CORB
    byte = hda_inb(REG_CORBCTL);
    hda_outb(REG_CORBCTL, byte | 0x03);
    byte = hda_inb(REG_RIRBCTL);
    hda_outb(REG_RIRBCTL, byte | 0x03);

    // wait at least 521 usecs to ensure codecs reset and registered
    hda_wait(10);

    word = hda_inw(REG_STATESTS);

    //printk("hda: device status 0x%x\n", word);
    //screen_refresh(NULL);

    for(i = 0; i < 16; i++)
    {
        //printk("i %d,", i);

        if((word & (1 << i)))
        {
            printk("hda: found device at index %d\n", i);
            //screen_refresh(NULL);

            hda_enum_widgets(hda, i);
        }
    }

    hda_set_volume(hda, 255, 1);

    printk("hda: done\n");
    screen_refresh(NULL);

    return 0;


err:
    if(hda->mmio)
    {
        vmmngr_free_pages(hda->iobase, hda->iobase + hda->iosize);
        hda->iobase = 0;
    }

    if(hda->corb)
    {
        vmmngr_free_page(get_page_entry((void *)hda->corb));
        hda->corb = 0;
    }

    if(hda->rirb)
    {
        vmmngr_free_page(get_page_entry((void *)hda->rirb));
        hda->rirb = 0;
    }

    //kfree(hda);
    return res;
}


/*
 * Intel HDA IRQ callback function.
 */
int hda_intr(struct regs *r, int unit)
{
    UNUSED(r);

    //printk("hda_intr:\n");
    //screen_refresh(NULL);
    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    struct hda_dev_t *hda = first_hda;
    struct hda_out_t *out;
    uint32_t isr;
    uint8_t sts;
    int unblock = 0;

    while(hda)
    {
        if(hda->pci->unit == unit)
        {
            break;
        }

        hda = hda->next;
    }

    if(!hda)
    {
        return 0;
    }

    isr = hda_inl(REG_INTSTS);

    if((isr & (1 << 31)) == 0)
    {
        return 0;
    }


    sts = hda_inb(REG_RIRBSTS);

    if(sts & 0x4)
    {
        printk("hda: RIRB overrun\n");
    }

    // clear the overrun and IRQ flags
    sts |= 0x5;
    hda_outb(REG_RIRBSTS, sts);

    out = hda->out;

    while(out)
    {
        uint8_t outsts;
        
        outsts = hda_inb(out->base_port + REG_OFFSET_OUT_STS);

        if(outsts == 0)
        {
            out = out->next;
            continue;
        }
        
        // buffer completed?
        if(outsts & 0x4)
        {
            unblock = 1;

            out->bytes_playing -= BDL_BUFSZ;

            if(out->bytes_playing < 0)
            {
                out->bytes_playing = 0;
            }

            /*
            out->hasdata[out->curdesc++] = 0;

            if(out->curdesc == BDL_ENTRIES)
            {
                out->curdesc = 0;
            }
            */
        }
        
        hda_outb(out->base_port + REG_OFFSET_OUT_STS, outsts);
        out = out->next;
    }
    
    hda_outl(REG_INTSTS, isr);
    pic_send_eoi(hda->pci->irq[0]);

    if(unblock && hda->task)
    {
        unblock_kernel_task(hda->task);
    }

    return 1;
}


void hda_task_func(void *arg)
{
    struct hda_dev_t *hda = (struct hda_dev_t *)arg;
    uint32_t /* lpib, */ curbuf, stopatbuf;
    struct hda_buf_t *buf;
    uintptr_t ptr;
    size_t left;
    
    if(!hda)
    {
        kpanic("hda: hda_task_func() called with NULL arg\n");
    }
    
    for(;;)
    {
        // task started too eary during boot
        if(!hda->out)
        {
            block_task2(hda, PIT_FREQUENCY);
            continue;
        }

        // check nothing is currently playing
        if(hda->out->bytes_playing)
        {
            block_task2(hda, PIT_FREQUENCY);
            continue;
        }

        /*
        for(curbuf = 0; curbuf < BDL_ENTRIES; curbuf++)
        {
            if(hda->out->hasdata[curbuf])
            {
                if(!(hda->flags & HDA_FLAG_PLAYING))
                {
                    hda_play_stop(hda, 1);
                }

                block_task2(hda, PIT_FREQUENCY);
                continue;
            }
        }

        A_memset(hda->out->hasdata, 0, sizeof(hda->out->hasdata));
        hda->out->curdesc = 0;
        */

        kernel_mutex_lock(&hda->outq.lock);

        if((hda->flags & HDA_FLAG_PLAYING))
        {
            hda_play_stop(hda, 0);
        }

        if(hda->outq.head == NULL)
        {
            kernel_mutex_unlock(&hda->outq.lock);
            block_task2(hda, PIT_FREQUENCY);
            continue;
        }

        // get link position in buffer
        /*
        lpib = hda_inl(hda->out->base_port + REG_OFFSET_OUT_LPIB);
        curbuf = ((lpib + (BDL_BUFSZ - 1)) / BDL_BUFSZ);

        if(curbuf == BDL_ENTRIES)
        {
            curbuf = 0;
        }
        */

        for(curbuf = 0; curbuf < BDL_ENTRIES; curbuf += 2)
        {
            A_memset((void *)hda->out->vbdl[curbuf], 0, PAGE_SIZE);
        }

        //stopatbuf = (curbuf == 0 ? BDL_ENTRIES : curbuf) - 1;
        curbuf = 0;
        stopatbuf = BDL_ENTRIES;
        hda->out->bytes_playing = 0;

        while(hda->outq.head)
        {
            if(curbuf == stopatbuf)
            {
                break;
            }

            ptr = hda->out->vbdl[curbuf];
            left = BDL_BUFSZ;

            while(hda->outq.head)
            {
                buf = hda->outq.head;

                // Buffer bigger than space left. Copy whatever we can and
                // leave the rest for the next HDA buffer.
                if(buf->size > left)
                {
                    A_memcpy((void *)ptr, buf->curptr, left);
                    buf->curptr += left;
                    buf->size -= left;
                    ptr += left;
                    hda->outq.bytes -= left;
                    //hda->out->hasdata[curbuf] = 1;
                    hda->out->bytes_playing += left;
                    left = 0;
                    break;
                }

                hda->outq.head = hda->outq.head->next;

                if(hda->outq.head == NULL)
                {
                    hda->outq.tail = NULL;
                }

                hda->outq.queued--;
                hda->outq.bytes -= buf->size;
                //hda->out->hasdata[curbuf] = 1;
                hda->out->bytes_playing += buf->size;

                kernel_mutex_unlock(&hda->outq.lock);

                A_memcpy((void *)ptr, buf->curptr, buf->size);
                ptr += buf->size;
                left -= buf->size;
                kfree(buf);

                kernel_mutex_lock(&hda->outq.lock);
            }

            /*
            if(left)
            {
                A_memset((void *)ptr, 0, left);
            }
            */

            curbuf++;
        }

        kernel_mutex_unlock(&hda->outq.lock);

        //if(!(hda->flags & HDA_FLAG_PLAYING))
        {
            hda_play_stop(hda, 1);
        }

        block_task2(hda, PIT_FREQUENCY);

        /*
        kernel_mutex_lock(&hda->outq.lock);
        
        if(hda->outq.head == NULL)
        {
            kernel_mutex_unlock(&hda->outq.lock);
            
            if((hda->flags & HDA_FLAG_PLAYING))
            //if(hda->playing)
            {
                hda_play_stop(hda, 0);
            }
            
            block_task(hda, 0);
            continue;
        }
        
        buf = hda->outq.head;
        hda->outq.head = hda->outq.head->next;
        
        if(hda->outq.head == NULL)
        {
            hda->outq.tail = NULL;
        }

        hda->outq.queued--;
        
        kernel_mutex_unlock(&hda->outq.lock);

        // get link position in buffer
        lpib = hda_inl(hda->out->base_port + REG_OFFSET_OUT_LPIB);
        curbuf = (lpib / BDL_BUFSZ);
        
        if(curbuf == BDL_ENTRIES)
        {
            curbuf = 0;
        }
        
        ptr = hda->out->vbdl[curbuf];
        A_memcpy((void *)ptr, buf->buf, buf->size);
        
        if(buf->size < BDL_BUFSZ)
        {
            A_memset((void *)(ptr + buf->size), 0, BDL_BUFSZ - buf->size);
        }

        kfree(buf);
        
        //printk("freeing buf " _XPTR_ "\n", buf);
        
        if(!(hda->flags & HDA_FLAG_PLAYING))
        //if(!hda->playing)
        {
            hda_play_stop(hda, 1);
        }

        //unblock_tasks(&hda->outq);
        block_task(hda, 0);
        */
    }
}


/*
 * Set HDA device volume.
 */
void hda_set_volume(struct hda_dev_t *hda, uint8_t vol, int overwrite)
{
    struct hda_out_t *out = hda->out;
    int meta = 0xb000;      // output amp, left & right
    uint8_t rvol;
    uint64_t response;

    if(hda->flags & HDA_FLAG_DUMMY)
    {
        return;
    }

    while(out)
    {
        if(!vol)
        {
            rvol = 0x80;         // set the mute bit
            hda->flags |= HDA_FLAG_MUTED;
        }
        else
        {
            // scale to the gain steps
            rvol = vol * hda->out->amp_gain_steps / 255;
            hda->flags &= ~HDA_FLAG_MUTED;
        }

        if(overwrite)
        {
            out->vol = vol;
        }

        hda_send_verb(hda, hda_make_verb(out->codec, out->node,
                                         VERB_SET_AMP_GAIN_MUTE | meta | 127 /* rvol */),
                      &response);

        out = out->next;
    }
}


/*
 * Set HDA device output channels.
 */
int hda_set_channels(struct hda_dev_t *hda, int nchan)
{
    struct hda_out_t *out = hda->out;

    if(hda->flags & HDA_FLAG_DUMMY)
    {
        return 0;
    }

    // nothing to do
    if(nchan == 0)
    {
        return 0;
    }

    // channels can be 1-16
    if(nchan > 16)
    {
        return -EINVAL;
    }

    while(out)
    {
        // only change settings if needed
        if(out->nchan != nchan)
        {
            out->nchan = nchan;
            hda_set_output_format(hda, out);
        }
        
        out = out->next;
    }

    return 0;
}


/*
 * Set HDA device output sample rate.
 */
int hda_set_sample_rate(struct hda_dev_t *hda, unsigned int sample_rate)
{
    struct hda_out_t *out = hda->out;

    if(hda->flags & HDA_FLAG_DUMMY)
    {
        return 0;
    }

    switch(sample_rate)
    {
        case 0:
            return 0;

        case 48000:     // 48 kHz
            sample_rate = SR_48_KHZ;
            break;

        case 44100:     // 44.1 kHz
            sample_rate = SR_44_KHZ;
            break;

        case 96000:     // 96 kHz
            sample_rate = SR_96_KHZ;
            break;

        case 88200:     // 88.2 kHz
            sample_rate = SR_88_KHZ;
            break;

        case 144000:    // 144 kHz
            sample_rate = SR_144_KHZ;
            break;

        case 192000:    // 192 kHz
            sample_rate = SR_192_KHZ;
            break;

        case 176400:    // 176.4 kHz
            sample_rate = SR_176_KHZ;
            break;

        case 24000:     // 24 kHz
            sample_rate = SR_24_KHZ;
            break;

        case 22050:     // 22.05 kHz
            sample_rate = SR_22_KHZ;
            break;

        case 16000:     // 16 kHz
            sample_rate = SR_16_KHZ;
            break;

        case 14000:     // 14 kHz
            sample_rate = SR_14_KHZ;
            break;

        case 11025:     // 11.025 kHz
            sample_rate = SR_11_KHZ;
            break;

        case 9000:      // 9 kHz
            sample_rate = SR_9_KHZ;
            break;

        case 8000:      // 8 kHz
            sample_rate = SR_8_KHZ;
            break;

        case 6000:      // 6 kHz
            sample_rate = SR_6_KHZ;
            break;

        default:
            return -EINVAL;
    }

    while(out)
    {
        // only change settings if needed
        if(out->sample_rate != sample_rate)
        {
            out->sample_rate = sample_rate;
            hda_set_output_format(hda, out);
        }
        
        out = out->next;
    }

    return 0;
}


/*
 * Set HDA device output bits per sample.
 */
int hda_set_bits_per_sample(struct hda_dev_t *hda, int bits)
{
    struct hda_out_t *out = hda->out;

    if(hda->flags & HDA_FLAG_DUMMY)
    {
        return 0;
    }

    switch(bits)
    {
        case 0:
            return 0;

        case 8:
            bits = BITS_8;
            break;

        case 16:
            bits = BITS_16;
            break;

        case 20:
            bits = BITS_20;
            break;

        case 24:
            bits = BITS_24;
            break;

        case 32:
            bits = BITS_32;
            break;

        default:
            return -EINVAL;
    }

    while(out)
    {
        // only change settings if needed
        if(out->sample_format != bits)
        {
            out->sample_format = bits;
            hda_set_output_format(hda, out);
        }
        
        out = out->next;
    }

    return 0;
}


/*
 * Set HDA device block size.
 */
int hda_set_blksz(struct hda_dev_t *hda, unsigned int blksz)
{
    UNUSED(hda);

    if(hda->flags & HDA_FLAG_DUMMY)
    {
        return 0;
    }

    // nothing to do
    if(blksz == 0)
    {
        return 0;
    }

    /*
     * TODO:
     */

    return -ENOSYS;
}


/*
 * Get HDA device output sample rate.
 */
unsigned int hda_get_sample_rate(struct hda_dev_t *hda)
{
    struct hda_out_t *out = hda->out;

    if(hda->flags & HDA_FLAG_DUMMY)
    {
        return 48000;
    }

    if(!out)
    {
        return 0;
    }

    switch(out->sample_rate)
    {
        case SR_48_KHZ:     // 48 kHz
            return 48000;

        case SR_44_KHZ:     // 44.1 kHz
            return 44100;

        case SR_96_KHZ:     // 96 kHz
            return 96000;

        case SR_88_KHZ:     // 88.2 kHz
            return 88200;

        case SR_144_KHZ:    // 144 kHz
            return 144000;

        case SR_192_KHZ:    // 192 kHz
            return 192000;

        case SR_176_KHZ:    // 176.4 kHz
            return 176400;

        case SR_24_KHZ:     // 24 kHz
            return 24000;

        case SR_22_KHZ:     // 22.05 kHz
            return 22050;

        case SR_16_KHZ:     // 16 kHz
            return 16000;

        case SR_14_KHZ:     // 14 kHz
            return 14000;

        case SR_11_KHZ:     // 11.025 kHz
            return 11025;

        case SR_9_KHZ:      // 9 kHz
            return 9000;

        case SR_8_KHZ:      // 8 kHz
            return 8000;

        case SR_6_KHZ:      // 6 kHz
            return 6000;

        default:
            return 0;
    }
}


/*
 * Get HDA device output bits per sample.
 */
int hda_get_bits_per_sample(struct hda_dev_t *hda)
{
    struct hda_out_t *out = hda->out;

    if(hda->flags & HDA_FLAG_DUMMY)
    {
        return 8;
    }

    if(!out)
    {
        return 0;
    }

    switch(out->sample_format)
    {
        case BITS_8:
            return 8;

        case BITS_16:
            return 16;

        case BITS_20:
            return 20;

        case BITS_24:
            return 24;

        case BITS_32:
            return 32;

        default:
            return 0;
    }
}


/*
 * Start/stop HDA device output.
 */
int hda_play_stop(struct hda_dev_t *hda, int cmd)
{
    struct hda_out_t *out = hda->out;
    uint16_t ctl = 0;

    if(hda->flags & HDA_FLAG_DUMMY)
    {
        if(cmd)
        {
            hda->flags |= HDA_FLAG_PLAYING;
        }
        else
        {
            hda->flags &= ~HDA_FLAG_PLAYING;
        }

        return 0;
    }

    // play?
    if(cmd)
    {
        ctl = 0x1e;
    }
    else
    {
        ctl = 0x1c;
    }

    while(out)
    {
        hda_outw(out->base_port + REG_OFFSET_OUT_CTLL, ctl);
        out = out->next;
    }

    if(cmd)
    {
        hda->flags |= HDA_FLAG_PLAYING;
    }
    else
    {
        hda->flags &= ~HDA_FLAG_PLAYING;
    }

    return 0;
}


dev_t create_dummy_hda(void)
{
    struct hda_dev_t *hda;
    dev_t devid = TO_DEVID(14, 0);

    if(!first_hda)
    {
        first_hda = &dummy_hda;
    }
    else
    {
        hda = first_hda;

        while(hda->next)
        {
            if(hda->devid > devid)
            {
                devid = TO_DEVID(14, MINOR(hda->devid) + 1);
            }

            hda = hda->next;
        }

        hda->next = &dummy_hda;
    }

    hda = &dummy_hda;
    hda->devid = devid;
    hda->flags = HDA_FLAG_DUMMY;
    hda->next = NULL;

    hda->out = &dummy_out;
    hda->out->nchan = 2;
    hda->out->vol = 255;
    hda->out->next = NULL;

    return devid;
}

