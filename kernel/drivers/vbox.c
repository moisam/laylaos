/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: vbox.c
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
 *  \file vbox.c
 *
 *  Oracle VM VirtualBox device driver implementation.
 */

#include <kernel/laylaos.h>
#include <kernel/io.h>
#include <kernel/pci.h>
#include <kernel/pic.h>
#include <kernel/vga.h>
#include <kernel/vbox.h>
#include <kernel/mouse.h>
#include <kernel/task.h>
#include <kernel/tty.h>
#include <gui/fb.h>
#include <gui/vbe.h>
#include <mm/kstack.h>

#define VBOX_VMMDEV_VERSION             0x00010003
#define VBOX_REQUEST_HEADER_VERSION     0x10001

#define VBOX_REQUEST_ACK_EVENTS         41 
#define VBOX_REQUEST_GUEST_INFO         50
#define VBOX_REQUEST_DISPLAY_CHANGE     51

#define VBOX_REQUEST_GET_MOUSE          1
#define VBOX_REQUEST_SET_MOUSE          2

#define VBOX_EVENT_MOUSE                (1 << 9)


#define VMM_ReportGuestCapabilities     55
#define VMMCAP_Graphics                 (1 << 2)



/* VBox Guest packet header */
struct vbox_header_t
{
    uint32_t size;          /* Size of the entire packet (including 
                               this header) */
    uint32_t version;       /* Version; always VBOX_REQUEST_HEADER_VERSION */
    uint32_t requestType;   /* Request code */
    int32_t  rc;            /* This will get filled with the return code 
                               from the requset */
    uint32_t reserved1;     /* These are unused */
    uint32_t reserved2;
} __attribute__((packed));

/* VBox Guest Info packet (legacy) */
struct vbox_guest_info_t
{
    struct vbox_header_t header;
    uint32_t version;
    uint32_t ostype;
} __attribute__((packed));

struct vbox_guest_caps_t
{
	struct vbox_header_t header;
	uint32_t caps;
} __attribute__((packed));

/* VBox Acknowledge Events packet */
struct vbox_ack_events_t
{
    struct vbox_header_t header;
    uint32_t events;
} __attribute__((packed));

struct vbox_display_change_t
{
	struct vbox_header_t header;
	uint32_t xres;
	uint32_t yres;
	uint32_t bpp;
	uint32_t eventack;
} __attribute__((packed));

/*
 * The Mouse packet is used both to advertise our guest capabilities and to
 * receive mouse movements.
 */
struct vbox_mouse_absolute_t
{
    struct vbox_header_t header;
    uint32_t features;
    int32_t x;
    int32_t y;
} __attribute__((packed));

int vbox_irq = 0;
unsigned int vbox_port = 0;
uintptr_t vbox_vmmdev_phys = 0;
volatile uint32_t *vbox_vmmdev_virt = 0;

uintptr_t guest_info_phys = 0;
uintptr_t guest_info_virt = 0;

struct vbox_mouse_absolute_t *vbox_mouse_virt = NULL;
uintptr_t vbox_mouse_phys = 0;

struct vbox_mouse_absolute_t *vbox_mouse_get_virt = NULL;
uintptr_t vbox_mouse_get_phys = 0;

struct vbox_ack_events_t *vbox_ack_virt = NULL;
uintptr_t vbox_ack_phys = 0;

/*
struct vbox_display_change_t *vbox_disp_virt = NULL;
uintptr_t vbox_disp_phys = 0;
*/

static int vbox_mouse_x = 0;
static int vbox_mouse_y = 0;

uint32_t vbox_xres = 0;
uint32_t vbox_yres = 0;

// defined in dev/chr/input_mouse.c
extern mouse_buttons_t cur_button_state;

int vbox_intr(struct regs *r, int unit);


/*
 * Initialise VirtualBox device.
 */
void vbox_init(struct pci_dev_t *pci)
{
    // BAR0 is the I/O port
    vbox_port = (pci->bar[0] & 0xFFFFFFF0);
    
    // BAR1 is the MMIO device area
    vbox_vmmdev_phys = (pci->bar[1] & 0xFFFFFFF0);
    vbox_vmmdev_virt = (uint32_t *)mmio_map(vbox_vmmdev_phys, 
                                            vbox_vmmdev_phys + PAGE_SIZE);
    
    // Get the IRQ number
    vbox_irq = pci->irq[0];
    
    printk("vbox: port 0x%x, phys " _XPTR_ ", virt " _XPTR_ ", IRQ 0x%x\n",
            vbox_port, vbox_vmmdev_phys, vbox_vmmdev_virt, vbox_irq);
    //screen_refresh(NULL);

    if(vbe_framebuffer.width && vbe_framebuffer.height)
    {
        vbox_mouse_x = vbe_framebuffer.width / 2;
        vbox_mouse_y = vbe_framebuffer.height / 2;
    }
    
    pci_enable_busmastering(pci);
    pci_enable_interrupts(pci);
    pci_enable_memoryspace(pci);
    pci_register_irq_handler(pci, vbox_intr, "vbox");

    /*
    if(framebuf_mem && VGA_WIDTH && VGA_HEIGHT)
    {
        vbox_mouse_x = VGA_WIDTH / 2;
        vbox_mouse_y = VGA_HEIGHT / 2;
    }
    */

#define PAGE_FLAGS      (PTE_FLAGS_PW | I86_PTE_NOT_CACHEABLE)
    
    // Allocate memory for our Guest Info packet
    if(get_next_addr(&guest_info_phys, &guest_info_virt, 
                     PAGE_FLAGS, REGION_DMA) != 0)
    {
        return;
    }

#undef PAGE_FLAGS

    // Populate the packet
    struct vbox_guest_info_t *guest_info = 
                            (struct vbox_guest_info_t *)guest_info_virt;
    guest_info->header.size = sizeof(struct vbox_guest_info_t);
    guest_info->header.version = VBOX_REQUEST_HEADER_VERSION;
    guest_info->header.requestType = VBOX_REQUEST_GUEST_INFO;
    guest_info->header.rc = 0;
    guest_info->header.reserved1 = 0;
    guest_info->header.reserved2 = 0;
    guest_info->version = VBOX_VMMDEV_VERSION;

#ifdef __x86_64__
    guest_info->ostype = 0x00100;   /* Unknown, x86-64 */
#else
    guest_info->ostype = 0;         /* Unknown, 32-bit */
#endif

    // And send it to the VM
    outl(vbox_port, guest_info_phys);
    
    
    /*
    struct vbox_guest_caps_t *caps = (struct vbox_guest_caps_t *)(guest_info_virt + 512);
	caps->header.size = sizeof(struct vbox_guest_caps_t);
	caps->header.version = VBOX_REQUEST_HEADER_VERSION;
	caps->header.requestType = VMM_ReportGuestCapabilities;
	caps->header.rc = 0;
	caps->header.reserved1 = 0;
	caps->header.reserved2 = 0;
	caps->caps = VMMCAP_Graphics;
	outl(vbox_port, guest_info_phys + 512);
	
    //printk("caps " _XPTR_ ", caps_phys " _XPTR_ "\n", guest_info_virt + 512, guest_info_phys + 512);
    //printk("rc = %d\n", caps->header.rc);
    */
    

    /*
    vbox_disp_virt = (struct vbox_display_change_t *)(guest_info_virt + 512);
    vbox_disp_phys = (guest_info_phys + 512);
	vbox_disp_virt->header.size = sizeof(struct vbox_display_change_t);
	vbox_disp_virt->header.version = VBOX_REQUEST_HEADER_VERSION;
	vbox_disp_virt->header.requestType = VBOX_REQUEST_DISPLAY_CHANGE;
	vbox_disp_virt->header.rc = 0;
	vbox_disp_virt->header.reserved1 = 0;
	vbox_disp_virt->header.reserved2 = 0;
	vbox_disp_virt->xres = 0;
	vbox_disp_virt->yres = 0;
	vbox_disp_virt->bpp = 0;
	vbox_disp_virt->eventack = 1;
	*/
	

    // We'll also set up the packets we'll use later for 
    // AcknowledgeEvents and GetDisplayChange
    vbox_ack_virt = (struct vbox_ack_events_t *)(guest_info_virt + 1024);
    vbox_ack_phys = (guest_info_phys + 1024);
    vbox_ack_virt->header.size = sizeof(struct vbox_ack_events_t);
    vbox_ack_virt->header.version = VBOX_REQUEST_HEADER_VERSION;
    vbox_ack_virt->header.requestType = VBOX_REQUEST_ACK_EVENTS;
    vbox_ack_virt->header.rc = 0;
    vbox_ack_virt->header.reserved1 = 0;
    vbox_ack_virt->header.reserved2 = 0;
    vbox_ack_virt->events = 0;
    
    vbox_mouse_virt = (struct vbox_mouse_absolute_t *)(guest_info_virt + 2048);
    vbox_mouse_phys = (guest_info_phys + 2048);
    vbox_mouse_virt->header.size = sizeof(struct vbox_mouse_absolute_t);
    vbox_mouse_virt->header.version = VBOX_REQUEST_HEADER_VERSION;
    vbox_mouse_virt->header.requestType = VBOX_REQUEST_SET_MOUSE;
    vbox_mouse_virt->header.rc = 0;
    vbox_mouse_virt->header.reserved1 = 0;
    vbox_mouse_virt->header.reserved2 = 0;
    /*
     * bit 0 says "guest supports (and wants) absolute mouse"; 
     * bit 4 says we'll query absolute positions on interrupts
     */
    vbox_mouse_virt->features = (1 << 0) | (1 << 4);
    vbox_mouse_virt->x = 0;
    vbox_mouse_virt->y = 0;
    outl(vbox_port, vbox_mouse_phys);

    /* For use with later receives */
	vbox_mouse_get_virt = (struct vbox_mouse_absolute_t *)(guest_info_virt +
	                                                       2048 + 1024);
	vbox_mouse_get_phys = (guest_info_phys + 2048 + 1024);
	vbox_mouse_get_virt->header.size = sizeof(struct vbox_mouse_absolute_t);
	vbox_mouse_get_virt->header.version = VBOX_REQUEST_HEADER_VERSION;
	vbox_mouse_get_virt->header.requestType = VBOX_REQUEST_GET_MOUSE;
	vbox_mouse_get_virt->header.rc = 0;
	vbox_mouse_get_virt->header.reserved1 = 0;
	vbox_mouse_get_virt->header.reserved2 = 0;

    /*
    outl(vbox_port, vbox_disp_phys);
    outl(vbox_port, vbox_disp_phys);
    vbox_xres = vbox_disp_virt->xres;
    vbox_yres = vbox_disp_virt->yres;
    printk("vbox_xres %d, vbox_yres %d\n", vbox_xres, vbox_yres);
    //screen_refresh(NULL);
    //empty_loop();
    */
	
	vbox_vmmdev_virt[3] = 0xFFFFFFFF; /* Enable all for now */
    
    /*
     * By trial and error, I found that VirtualBox sends only one mouse byte 
     * containing button data. It does not send the 2nd and 3rd bytes with
     * cursor position, as they are delivered via IRQs. Waiting in the mouse
     * IRQ handler for the normal 3 bytes results in missing button changes, 
     * as a packet is formed incorrectly using button events with no cursor 
     * position changes. Therefore, we restrict mouse packet size to 1 byte 
     * here.
     */
    byte_count = 1;
}


int vbox_intr(struct regs *r, int unit)
{
    UNUSED(r);
    UNUSED(unit);
    
    uint32_t ev;
    int unblock_mouse_task = 0;
    
    //printk("vbox_intr:\n");
    
    if(!vbox_vmmdev_virt || !vbox_vmmdev_virt[2])
    {
        return 0;
    }
    
    ev = vbox_vmmdev_virt[2];
    vbox_ack_virt->events = ev;
    outl(vbox_port, vbox_ack_phys);

    //printk("vbox_intr: ev 0x%x, ", ev);

    if(ev & VBOX_EVENT_MOUSE)
    {
        int x, y;
        
        outl(vbox_port, vbox_mouse_get_phys);
        
        /*
        if(vbox_xres && vbox_yres &&
           vbox_mouse_get_virt->x && vbox_mouse_get_virt->y)
        {
            x = ((unsigned int)vbox_mouse_get_virt->x * vbox_xres) / 0xFFFF;
            y = ((unsigned int)vbox_mouse_get_virt->y * vbox_yres) / 0xFFFF;
        }
        */
        if(vbe_framebuffer.width && vbe_framebuffer.height &&
           vbox_mouse_get_virt->x && vbox_mouse_get_virt->y)
        {
            x = ((unsigned int)vbox_mouse_get_virt->x * 
                                    vbe_framebuffer.width) / 0xFFFF;
            y = ((unsigned int)vbox_mouse_get_virt->y * 
                                    vbe_framebuffer.height) / 0xFFFF;
        }
        else
        {
            //x = vbox_mouse_get_virt->x;
            //y = vbox_mouse_get_virt->y;
            pic_send_eoi(vbox_irq);
            return 1;
        }

        //printk("x %d, y %d .. ", x, y);
        //screen_refresh(NULL);

        if(_mouse_id >= 0)
        {
            add_mouse_packet(x - vbox_mouse_x, -(y - vbox_mouse_y), 
                             cur_button_state);
            vbox_mouse_x = x;
            vbox_mouse_y = y;
            unblock_mouse_task = 1;
        }
    }

    pic_send_eoi(vbox_irq);
    
    if(unblock_mouse_task)
    {
        unblock_kernel_task(mouse_task);
    }
    
    return 1;
}

