/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025, 2026 (c)
 * 
 *    file: usb_hid.c
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
 *  \file usb_hid.c
 *
 *  The Universal Serial Bus (USB) driver code is split into several files:
 *    - usb.c       => main entry point and general functions
 *    - usb_msd.c   => functions to handle Mass Storage Devices (MSD)
 *    - usb_hid.c   => functions to handle Human Interaction Devices (HID)
 *    - usb_hub.c   => functions to handle USB hubs
 *    - usb_ioctl.c => functions to handle ioctl() calls
 *    - usb_ohci.c  => OHCI layer
 *    - usb_uhci.c  => UHCI layer
 *    - usb_ehci.c  => EHCI layer
 */

//#define __DEBUG

#define KQUEUE_DEFINE_INLINES   1
#define KQUEUE_SIZE             KEY_BUF_SIZE

#include <errno.h>
#include <kernel/pci.h>
#include <kernel/asm.h>
#include <kernel/usb.h>
#include <kernel/usb_hid.h>
#include <kernel/mouse.h>
#include <kernel/kbd.h>
#include <kernel/kqueue.h>
#include <kernel/keycodes.h>
#include <mm/kheap.h>

//volatile struct task_t *hid_task;
struct usb_hid_dev_t hid_list;
struct kernel_mutex_t usb_hid_tablock;

// defined in drivers/mouse.c
extern mouse_buttons_t cur_button_state;

// defined in usb_keytable.c
extern char usb_keycodes[];


int usb_hid_set_protocol(struct usb_dev_t *usb, uint8_t protocol)
{
    struct usb_transfer_t transfer;

    usb_setup_transfer(usb, usb->endpoints, &transfer, USB_TRANSFER_CTRL);
    usb_setup_transaction(&transfer, 0x21, 0x0B, 0, protocol, 0, 0);
    //usb_in_transaction(&transfer, 1, 0, 0);
    usb_schedule_transfer(&transfer);
    usb_wait_transfer(&transfer);
    usb_delete_transfer(&transfer);

    return transfer.success;
}


static void usb_handle_mouse_input(void *__hid)
{
    static mouse_buttons_t b0[] =
    {
        0,
        MOUSE_LBUTTON_DOWN,
        MOUSE_RBUTTON_DOWN,
        MOUSE_LBUTTON_DOWN | MOUSE_RBUTTON_DOWN,
        MOUSE_MBUTTON_DOWN,
        MOUSE_MBUTTON_DOWN | MOUSE_LBUTTON_DOWN,
        MOUSE_MBUTTON_DOWN | MOUSE_RBUTTON_DOWN,
        MOUSE_MBUTTON_DOWN | MOUSE_RBUTTON_DOWN | MOUSE_LBUTTON_DOWN,
    };

    /*
     * handle mouse movement
     * See: https://wiki.osdev.org/USB_Human_Interface_Devices
     *
     * TODO: handle scroll movement
     * TODO: we currently use the boot protocol for its simplicity --
     *       aim to support the report protocol
     */

    struct usb_hid_dev_t *hid = __hid;
    char *buf = (char *)hid->buf;
    int dx = buf[1];
    int dy = -(buf[2]);
    mouse_buttons_t buttons = b0[buf[0] & 0x07];

    /*
    printk("usb_handle_mouse_input: ");
    for(int i = 0; i < 3; i++) printk("%d ", buf[i]);
    printk("\n");
    */

    cur_button_state = buttons;
    add_mouse_packet(dx, dy, buttons);
    unblock_kernel_task(mouse_task);
}


static void toggle_led(struct usb_hid_dev_t *hid, uint8_t bit)
{
    hid->leds ^= (1 << bit);
    usb_ctrl_out(hid->iface->usb, &hid->leds, 0x21, 9, 2, 0, hid->iface->desc.interfacenum, 1);
}


static inline int key_in_buf(uint8_t key, uint8_t *buf)
{
    return (key == buf[2] || key == buf[3] || key == buf[4] ||
            key == buf[5] || key == buf[6] || key == buf[7]);
}


static void usb_handle_kbd_input(void *__hid)
{
    // handle keyboard input
    // See: https://wiki.osdev.org/USB_Human_Interface_Devices

    struct usb_hid_dev_t *hid = __hid;
    volatile int unblock = 0;
    volatile int i;
    uint8_t key;

    // check for packets with errors
    for(i = 2; i < 8; i++)
    {
        if(hid->buf[i] == 1 || hid->buf[i] == 2 || hid->buf[i] == 3)
        {
            return;
        }
    }

    /*
    printk("usb_handle_kbd_input: ");
    for(i = 0; i < 8; i++) printk("%d ", hid->buf[i]);
    printk("\n");
    */

#define BRK                     (KEYCODE_BREAK_MASK << 8)

#define BUFBIT(buf, bit)        (buf[0] & (1 << bit))

#define PROCESS_MODIFIER(bit, code)                             \
    if(BUFBIT(hid->buf, bit) != BUFBIT(hid->last_packet, bit)) {\
        kbdbuf_enqueue(&kbd_queue, code | (BUFBIT(hid->buf, bit) ? 0 : BRK));\
        unblock = 1;                                            \
    }

    // process CTRL, ALT, SHIFT
    // TODO: process the GUI/Windows keys
    PROCESS_MODIFIER(0, KEYCODE_LCTRL);
    PROCESS_MODIFIER(1, KEYCODE_LSHIFT);
    PROCESS_MODIFIER(2, KEYCODE_LALT);
    PROCESS_MODIFIER(4, KEYCODE_RCTRL);
    PROCESS_MODIFIER(5, KEYCODE_RSHIFT);
    PROCESS_MODIFIER(6, KEYCODE_RALT);

#undef PROCESS_MODIFIER
#undef BUFBIT

#define DONE()                  \
    unblock = 1;                \
    hid->last_key_pressed = 0;  \
    hid->last_key_counter = 0;

    // next, process key presses
    for(i = 2; i < 8; i++)
    {
        key = hid->buf[i];

        // scancodes < 3 are errors
        // See: https://aeb.win.tue.nl/linux/kbd/scancodes-14.html
        if(key > 3)
        {
            // check if the key was newly pressed
            if(!key_in_buf(key, hid->last_packet))
            {
                switch(usb_keycodes[key])
                {
                    // switch LEDs if needed
                    case KEYCODE_NUM:
                        toggle_led(hid, 0);
                        kbdbuf_enqueue(&kbd_queue, KEYCODE_NUM);
                        DONE();
                        break;

                    case KEYCODE_CAPS:
                        toggle_led(hid, 1);
                        kbdbuf_enqueue(&kbd_queue, KEYCODE_CAPS);
                        DONE();
                        break;

                    case KEYCODE_SCROLL:
                        toggle_led(hid, 2);
                        kbdbuf_enqueue(&kbd_queue, KEYCODE_SCROLL);
                        DONE();
                        break;

                    default:
                        if(usb_keycodes[key])
                        {
                            kbdbuf_enqueue(&kbd_queue, usb_keycodes[key]);
                            DONE();
                        }
                        break;
                }
            }
            else
            {
                hid->last_key_pressed = key;
                hid->last_key_counter++;
            }
        }
    }

#undef DONE

    for(i = 2; i < 8; i++)
    {
        key = hid->last_packet[i];

        // check for key releases
        if(key && !key_in_buf(key, hid->buf))
        {
            if(key == hid->last_key_pressed)
            {
                hid->last_key_pressed = 0;
                hid->last_key_counter = 0;
            }

            switch(usb_keycodes[key])
            {
                case KEYCODE_NUM:
                    kbdbuf_enqueue(&kbd_queue, KEYCODE_NUM | BRK);
                    unblock = 1;
                    break;

                case KEYCODE_CAPS:
                    kbdbuf_enqueue(&kbd_queue, KEYCODE_CAPS | BRK);
                    unblock = 1;
                    break;

                case KEYCODE_SCROLL:
                    kbdbuf_enqueue(&kbd_queue, KEYCODE_SCROLL | BRK);
                    unblock = 1;
                    break;

                default:
                    if(usb_keycodes[key])
                    {
                        unblock = 1;
                        kbdbuf_enqueue(&kbd_queue, usb_keycodes[key] | BRK);
                    }
                    break;
            }
        }
    }

#undef BRK

    for(i = 0; i < 8; i++)
    {
        hid->last_packet[i] = hid->buf[i];
    }

    // delay for a bit before sending repeat key presses
    if(hid->last_key_counter > 5 && usb_keycodes[hid->last_key_pressed])
    {
        unblock = 1;
        kbdbuf_enqueue(&kbd_queue, usb_keycodes[hid->last_key_pressed]);
    }

    if(unblock)
    {
        unblock_kernel_task(kbd_task);
    }
}


/*
static int hid_get_descriptor(struct usb_dev_t *usb,
                              uint8_t type, uint8_t index, uint16_t interface)
{
    struct usb_hid_descriptor_t desc;

    return usb_ctrl_in(usb, &desc, 0x81, 6, type, index, 
                        interface, sizeof(struct usb_hid_descriptor_t));
}
*/


void usb_hid_remove(struct usb_interface_t *iface)
{
    volatile struct usb_hid_dev_t *hid, *next, *prev = &hid_list;

    if(!iface)
    {
        return;
    }

    elevated_priority_lock(&usb_hid_tablock);

    for(hid = hid_list.next; hid != NULL; )
    {
        if(hid->iface == iface)
        {
            remove_interrupt_transfer((struct usb_transfer_t *)&hid->transfer);
            next = hid->next;
            prev->next = (struct usb_hid_dev_t *)next;
            kfree((void *)hid);
            hid = next;
        }
        else
        {
            prev = hid;
            hid = hid->next;
        }
    }

    elevated_priority_unlock(&usb_hid_tablock);
}


int init_hid(struct usb_interface_t *iface)
{
    struct usb_hid_dev_t *hid;
    volatile struct usb_endpoint_t *endpoint;

    if(!iface->usb || !iface->usb->endpoints)
    {
        return -EINVAL;
    }

    if(!(hid = kmalloc(sizeof(struct usb_hid_dev_t))))
    {
        printk("usb: insufficient memory to init HID device\n");
        return -ENOMEM;
    }
    
    A_memset(hid, 0, sizeof(struct usb_hid_dev_t));

    // find the interrupt endpoint
    for(endpoint = iface->usb->endpoints; endpoint != NULL; endpoint = endpoint->next)
    {
        if(endpoint->type == USB_ENDPOINT_INTERRUPT &&
           endpoint->direction == USB_ENDPOINT_IN)
        {
            break;
        }
    }

    if(!endpoint)
    {
        printk("usb: HID has invalid IN endpoints\n");
        kfree(hid);
        return -EINVAL;
    }

    /*
    if(!hid_get_descriptor(iface->usb, 0x21, 0, iface->desc.interfacenum))
    {
        printk("usb: failed to read HID descriptor\n");
        kfree(hid);
        return -EINVAL;
    }
    */

    // set protocol (0=boot; 1=report)
    if(!usb_hid_set_protocol(iface->usb, 0))
    {
        printk("usb: failed to set HID protocol\n");
        return -EIO;
    }

    elevated_priority_lock(&usb_hid_tablock);
    hid->iface = iface;
    hid->next = hid_list.next;
    hid_list.next = hid;
    elevated_priority_unlock(&usb_hid_tablock);

    if(iface->desc.protocol == 1)       // keyboard
    {
        printk("usb: scheduling interrupt transfer for USB %s\n", "keyboard");
        usb_schedule_inttransfer(iface->usb, 
                                 (struct usb_endpoint_t *)endpoint, 
                                 &hid->transfer, hid->buf, 8,
                                 usb_handle_kbd_input, hid,
                                 endpoint->interval ? endpoint->interval : 10);
    }
    else if(iface->desc.protocol == 2)  // mouse
    {
        printk("usb: scheduling interrupt transfer for USB %s\n", "mouse");
        usb_schedule_inttransfer(iface->usb, 
                                 (struct usb_endpoint_t *)endpoint, 
                                 &hid->transfer, hid->buf, 3,
                                 usb_handle_mouse_input, hid,
                                 endpoint->interval ? endpoint->interval : 10);
    }
    else
    {
        printk("usb: unkown HID protocol: %d\n", iface->desc.protocol);
    }

    printk("usb: finished intializing HID device\n");

    return 0;
}

