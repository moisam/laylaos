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

/*
 * The code to parse and handle report descriptors and device reports is based
 * on Haiku's implmentation. See the files under:
 *
 *    https://github.com/haiku/haiku/tree/95142492b9805f1c9d6f83cd8f2f96a92dffb5e1/src/add-ons/kernel/drivers/input/hid_shared
 *    https://github.com/haiku/haiku/tree/95142492b9805f1c9d6f83cd8f2f96a92dffb5e1/src/add-ons/kernel/drivers/input/usb_hid
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


static int get_item_data(char *buf, struct hid_report_item_t *item)
{
    uint32_t data;

    if(!item)
    {
        return 0;
    }

    // each item data is max 4 bytes
    A_memcpy(&data, buf + item->byteoff, item->bytecnt);
    data >>= item->shift;
    data &= item->mask;

    /*
    printk("data 0x%x, mask 0x%x, off %d, cnt %d, shift %d, min %x (%d), max %x (%d)\n", 
            data, item->mask, item->byteoff, item->bytecnt, item->shift,
            item->min, item->min, item->max, item->max);
    */

    // check if item's data is signed
    if(item->min > item->max)
    {
        // and sign extend if needed
        if((data & ~(item->mask >> 1)) != 0)
        {
            data |= ~item->mask;
        }

        /*
        printk("signed: data %d, min %d, max %d\n", 
                (int32_t)data, (int32_t)item->min, (int32_t)item->max);
        */

        // check for validity
        if((int32_t)data >= (int32_t)item->min && 
           (int32_t)data <= (int32_t)item->max)
        {
            return (int)data;
        }
    }
    else
    {
        // check for validity
        if(data >= item->min && data <= item->max)
        {
            return (int)data;
        }
    }

    return 0;
}


/*
 * Handle report protocol mouse input.
 */
static void usb_handle_mouse_report(struct usb_hid_dev_t *hid)
{
    struct usage_val_t usage;
    mouse_buttons_t buttons;
    char *buf = (char *)hid->buf;
    volatile int i, j;
    volatile int dx, dy, hw, vw;
    int b = 0;

    /*
    printk("usb_handle_mouse_report: ");
    for(int i = 0; i < 4; i++) printk("%d ", buf[i]);
    printk("\n");
    */

    /*
     * TODO: Is this correct? Do report offsets include report id or not?
     */
    if(hid->flags & HID_FLAG_USES_REPID)
    {
        // skip the report id byte
        buf++;
    }

    // get dx and dy
    dx = get_item_data(buf, hid->report_item_x);
    dy = -get_item_data(buf, hid->report_item_y);

    // get button states
    for(i = 0; i < hid->report_button_count; i++)
    {
        usage.val.ext = hid->report_item_button[i]->usage;
        j = get_item_data(buf, hid->report_item_button[i]) & 0x01;
        b |= (j << (usage.val.idpage.id - 1));
    }

    buttons = b0[b];

    // get wheel state (if present)
    vw = get_item_data(buf, hid->report_item_wheel);
    hw = get_item_data(buf, hid->report_item_horizpan);

    if(vw < 0)
    {
        buttons |= MOUSE_VSCROLL_DOWN;
    }
    else if(vw > 0)
    {
        buttons |= MOUSE_VSCROLL_UP;
    }

    if(hw < 0)
    {
        buttons |= MOUSE_HSCROLL_RIGHT;
    }
    else if(hw > 0)
    {
        buttons |= MOUSE_HSCROLL_LEFT;
    }

    //printk("dx %d, dy %d, buttons 0x%x, vw %d, hw %d\n", dx, dy, buttons, vw, hw);

    cur_button_state = buttons;
    add_mouse_packet(dx, dy, buttons);

    // for wheel motion, add extra packets depending on the length of motion
    while(vw < -1)
    {
        add_mouse_packet(0, 0, buttons);
        vw++;
    }

    while(vw > 1)
    {
        add_mouse_packet(0, 0, buttons);
        vw--;
    }

    while(hw < -1)
    {
        add_mouse_packet(0, 0, buttons);
        hw++;
    }

    while(hw > 1)
    {
        add_mouse_packet(0, 0, buttons);
        hw--;
    }

    unblock_kernel_task(mouse_task);
}


/*
 * Handle boot protocol mouse input.
 */
static void usb_handle_mouse_input(void *__hid)
{
    /*
     * handle mouse movement
     * See: https://wiki.osdev.org/USB_Human_Interface_Devices
     *
     * TODO: handle scroll movement
     */

    struct usb_hid_dev_t *hid = __hid;

    if(hid->flags & HID_FLAG_USES_REPPROTO)
    {
        usb_handle_mouse_report(hid);
        return;
    }

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


/*
 * Handle boot protocol keyboard input.
 */
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
static char *item_type_str[] = { "main", "global", "local", "long" };
*/


static struct hid_report_item_t *report_item_of_type(struct hid_report_t *rep,
                                                     uint16_t page, uint16_t id)
{
    struct hid_report_item_t *item;
    struct usage_val_t usage;

    for(item = rep->first_item; item != NULL; item = item->next)
    {
        usage.val.ext = item->usage;

        /*
        printk("repitem page 0x%x (0x%x), id 0x%x (0x%x)\n",
                usage.val.idpage.page, page,
                usage.val.idpage.id, id);
        */

        if(usage.val.idpage.page == page && usage.val.idpage.id == id)
        {
            return item;
        }
    }

    return NULL;
}


static int count_reports_of_type(struct usb_hid_dev_t *hid, uint8_t reptype)
{
    struct hid_report_t *rep;
    int count = 0;

    // search for an existing report
    for(rep = hid->reports; rep != NULL; rep = rep->next)
    {
        if((rep->type & reptype) != 0)
        {
            count++;
        }
    }

    return count;
}


static void get_reports_of_type(struct usb_hid_dev_t *hid, 
                                struct hid_report_t **reps, uint8_t reptype)
{
    struct hid_report_t *rep;
    int count = 0;

    // search for an existing report
    for(rep = hid->reports; rep != NULL; rep = rep->next)
    {
        if((rep->type & reptype) != 0)
        {
            reps[count++] = rep;
        }
    }
}


static void __parse_collection(struct usb_hid_dev_t *hid, struct hid_collection_t *coll)
{
    struct hid_report_item_t *item;
    struct usage_val_t usage;
    int i, maxcount;

    usage.val.ext = coll->usage;

    if(usage.val.idpage.page == HID_USAGE_PAGE_GENDESK &&
       (usage.val.idpage.id == HID_UID_GENDESK_POINTER ||
        usage.val.idpage.id == HID_UID_GENDESK_MOUSE))
    {
        // found a mouse
        maxcount = count_reports_of_type(hid, HID_REPORT_INPUT);

        if(!maxcount)
        {
            return;
        }

        // try to find absolute X and Y axis report items
        struct hid_report_t *reps[maxcount];

        get_reports_of_type(hid, reps, HID_REPORT_INPUT);

        for(i = 0; i < maxcount; i++)
        {
            // find X axis
            item = report_item_of_type(reps[i], HID_USAGE_PAGE_GENDESK, HID_UID_GENDESK_X);
            //printk("x ? %s, flags 0x%x\n", item ? "yes" : "no", item ? item->flags : 0);

            if(!item || !(item->flags & HID_REPORT_ITEM_IS_RELATIVE))
            {
                continue;
            }

            hid->report_item_x = item;

            // find Y axis
            item = report_item_of_type(reps[i], HID_USAGE_PAGE_GENDESK, HID_UID_GENDESK_Y);
            //printk("y ? %s, flags 0x%x\n", item ? "yes" : "no", item ? item->flags : 0);

            if(!item || !(item->flags & HID_REPORT_ITEM_IS_RELATIVE))
            {
                hid->report_item_x = NULL;
                continue;
            }

            hid->report_item_y = item;

            hid->input_report = reps[i];
            hid->flags |= HID_FLAG_USES_REPPROTO;

            // find wheel (optional)
            hid->report_item_wheel = 
                report_item_of_type(reps[i], HID_USAGE_PAGE_GENDESK, HID_UID_GENDESK_WHEEL);

            // find horizontal pan (optional)
            hid->report_item_horizpan = 
                report_item_of_type(reps[i], HID_USAGE_PAGE_CONSUMER, HID_UID_GENDESK_HORIZPAN);

            // find mouse buttons
            for(i = 0, item = reps[i]->first_item;
                item != NULL && i < HID_MAX_MOUSE_BUTTONS;
                item = item->next)
            {
                usage.val.ext = item->usage;

                /*
                printk("page 0x%x (0x%x), id 0x%x\n", 
                        usage.val.idpage.page, HID_USAGE_PAGE_BUTTON, usage.val.idpage.id);
                */

                if(usage.val.idpage.page == HID_USAGE_PAGE_BUTTON &&
                   usage.val.idpage.id - 1 < HID_MAX_MOUSE_BUTTONS)
                {
                    hid->report_item_button[hid->report_button_count++] = item;
                }
            }
        }

        printk("usb-hid: mouse with %d button(s)%sand%swheel\n",
                hid->report_button_count,
                hid->report_item_horizpan ? ", horizontal pan " : " ",
                hid->report_item_wheel ? " " : " no ");
    }
}


static void parse_collection(struct usb_hid_dev_t *hid, struct hid_collection_t *parent)
{
    struct hid_collection_t *coll;

    // look for application collections
    if(parent->type == COLL_TYPE_APP)
    {
        __parse_collection(hid, parent);
    }

    for(coll = parent->first_child; coll != NULL; coll = coll->next)
    {
        parse_collection(hid, coll);
    }
}


static 
struct hid_collection_t *create_hid_collection(struct hid_collection_t *parent,
                                               struct local_state_t *locst,
                                               uint8_t type)
{
    struct hid_collection_t *coll;
    struct usage_val_t usage;

    usage.val.ext = 0;

    if(locst->usage_stack && locst->usage_stack_cnt)
    {
        usage.val.ext = locst->usage_stack->val.ext;
    }
    else if(locst->flags & FLAG_USAGE_MIN_SET)
    {
        usage.val.ext = locst->usage_min.val.ext;
    }
    else if(locst->flags & FLAG_USAGE_MAX_SET)
    {
        usage.val.ext = locst->usage_max.val.ext;
    }
    else if(type != COLL_TYPE_LOG)
    {
        printk("usb-hid: collection with invalid usage\n");
    }

    if(!(coll = kmalloc(sizeof(struct hid_collection_t))))
    {
        printk("usb-hid: insufficient memory for new collection\n");
        return NULL;
    }

    A_memset(coll, 0, sizeof(struct hid_collection_t));

    coll->parent = parent;
    coll->type = type;
    coll->strid = locst->str_index;
    coll->physid = locst->designator_index;
    coll->usage = usage.val.ext;

    return coll;
}


/*
static void free_collection(volatile struct hid_collection_t *coll)
{
    volatile struct hid_collection_t *child, *nextcoll;

    child = coll->first_child;

    while(child)
    {
        nextcoll = child->next;
        child->next = NULL;
        free_collection(child);
        child = nextcoll;
    }

    kfree((void *)coll);
}
*/


static void collection_add_child(struct hid_collection_t *parent, 
                                 struct hid_collection_t *child)
{
    if(parent->first_child == NULL)
    {
        parent->first_child = child;
        parent->last_child = child;
    }
    else
    {
        parent->last_child->next = child;
        parent->last_child = child;
    }
}


static void collection_add_item(struct hid_collection_t *coll, 
                                struct hid_report_item_t *item)
{
    /*
    struct usage_val_t usage;

    usage.val.ext = item->usage;

    printk("collitem page 0x%x, id 0x%x\n",
                usage.val.idpage.page, usage.val.idpage.id);
    */

    if(coll->first_item == NULL)
    {
        coll->first_item = item;
        coll->last_item = item;
    }
    else
    {
        coll->last_item->next = item;
        coll->last_item = item;
    }
}


static void report_add_item(struct hid_report_t *rep, struct hid_report_item_t *item)
{
    if(rep->first_item == NULL)
    {
        rep->first_item = item;
        rep->last_item = item;
    }
    else
    {
        rep->last_item->next = item;
        rep->last_item = item;
    }
}


static struct hid_report_t *may_create_report(struct usb_hid_dev_t *hid,
                                              uint8_t reptype, uint8_t repid)
{
    struct hid_report_t *rep;

    // search for an existing report
    for(rep = hid->reports; rep != NULL; rep = rep->next)
    {
        if((rep->type & reptype) != 0 && rep->id == repid)
        {
            return rep;
        }
    }

    // not found, create a new report
    if(!(rep = kmalloc(sizeof(struct hid_report_t))))
    {
        printk("usb-hid: insufficient memory for new report\n");
        return NULL;
    }

    A_memset(rep, 0, sizeof(struct hid_report_t));

    rep->type = reptype;
    rep->id = repid;

    if(hid->reports == NULL)
    {
        hid->reports = rep;
    }
    else
    {
        volatile struct hid_report_t *tmp = hid->reports;

        while(tmp->next)
        {
            tmp = tmp->next;
        }

        tmp->next = rep;
    }

    return rep;
}


static struct hid_report_item_t *create_report_item(struct main_data_t *maindata,
                                                    uint32_t bitoff, uint32_t bitlen,
                                                    uint32_t min, uint32_t max, uint32_t usage)
{
    struct hid_report_item_t *item;

    if(!(item = kmalloc(sizeof(struct hid_report_item_t))))
    {
        printk("usb-hid: insufficient memory for new report item\n");
        return NULL;
    }

    A_memset(item, 0, sizeof(struct hid_report_item_t));

    item->byteoff = bitoff / 8;
    item->shift = bitoff % 8;
    item->mask = ~(0xffffffff << bitlen);
    item->bitcnt = bitlen;
    item->bytecnt = (item->shift + item->bitcnt + 7) / 8;
    item->min = min;
    item->max = max;
    item->usage = usage;

    if(maindata->val.bits.constant == 0)
    {
        item->flags |= HID_REPORT_ITEM_HAS_DATA;
    }

    if(maindata->val.bits.isvar == 0)
    {
        item->flags |= HID_REPORT_ITEM_IS_ARRAY;
    }

    if(maindata->val.bits.relative)
    {
        item->flags |= HID_REPORT_ITEM_IS_RELATIVE;
    }

    return item;
}


static struct hid_report_item_t *dup_report_item(struct hid_report_item_t *olditem)
{
    struct hid_report_item_t *item;

    if(!(item = kmalloc(sizeof(struct hid_report_item_t))))
    {
        printk("usb-hid: insufficient memory for new report item\n");
        return NULL;
    }

    A_memcpy(item, olditem, sizeof(struct hid_report_item_t));

    return item;
}


static void __report_sign_extend(uint32_t *min, uint32_t *max)
{
    uint32_t mask = 0x80000000;
    int i;

    for(i = 0; i < 4; i++)
    {
        if(*min & mask)
        {
            *min |= mask;

            if(*max & mask)
            {
                *max |= mask;
            }

            return;
        }

        mask >>= 8;
        mask |= 0xff000000;
    }
}


static void report_add_main_item(struct hid_report_t *rep,
                                 struct global_state_t *globst,
                                 struct local_state_t *locst,
                                 struct hid_collection_t *coll,
                                 uint32_t data)
{
    struct hid_report_item_t *item;
    struct main_data_t maindata;
    uint32_t usage;
    uint32_t logmin, logmax, physmin, physmax;
    unsigned int i;

    logmin = globst->logmin;
    logmax = globst->logmax;
    physmin = globst->physmin;
    physmax = globst->physmax;
    maindata.val.raw = data;

    if(logmin > logmax)
    {
        __report_sign_extend(&logmin, &logmax);
    }

    if(physmin > physmax)
    {
        __report_sign_extend(&physmin, &physmax);
    }

    for(i = 0; i < locst->usage_stack_cnt; i++)
    {
        if(i >= HID_MAX_REPORT_USAGES)
        {
            printk("usb-hid: report with too many usages\n");
            break;
        }

        rep->usages[rep->usage_cnt] = locst->usage_stack[i].val.ext;
        rep->usage_cnt++;
    }

    if(locst->usage_stack_cnt)
    {
        struct usage_val_t page;

        page = locst->usage_stack[0];
        page.val.idpage.id = 0;
        usage = page.val.ext;
    }
    else
    {
        usage = 0;
    }

    for(i = 0; i < globst->repcnt; i++)
    {
        if(maindata.val.bits.isvar)
        {
            if(i < locst->usage_stack_cnt)
            {
                usage = locst->usage_stack[i].val.ext;
            }
        }

        if(!(item = create_report_item(&maindata, rep->sz, globst->repsz,
                                       logmin, logmax, usage)))
        {
            break;
        }

        report_add_item(rep, dup_report_item(item));

        if(coll)
        {
            collection_add_item(coll, item);
        }
        else
        {
            printk("usb-hid: item not part of a collection\n");
            kfree(item);
        }

        rep->sz += globst->repsz;
    }
}


static int parse_report_descriptor(struct usb_hid_dev_t *hid, uint8_t *desc, size_t desclen)
{
    struct global_state_t globst;
    struct local_state_t locst;
    struct usage_val_t usage_stack[HID_MAX_REPORT_USAGES];
    struct hid_collection_t *root, *curcoll;
    int usage_stack_cnt = 0;
    uint8_t *p, *p2, tag;
    uint32_t data;
    size_t itemsz;
    int i;

    A_memset(&globst, 0, sizeof(struct global_state_t));
    A_memset(&locst, 0, sizeof(struct local_state_t));

    if(!(root = create_hid_collection(NULL, &locst, COLL_TYPE_LOG)))
    {
        return -ENOMEM;
    }

    curcoll = root;
    p = desc;
    p2 = desc + desclen;

    while(p < p2)
    {
        itemsz = HID_ITEM_SIZE(*p);
        tag = HID_ITEM_TAG(*p);
        data = 0;

        // special case as per the HID spec
        if(itemsz == 3)
        {
            itemsz = 4;
        }

        if(HID_ITEM_TYPE(*p) == HID_ITEM_TYPE_LONG)
        {
            // skip long items, they are not defined in the HID spec
            itemsz += p[1];
        }
        else
        {
            struct short_item_t *shitem = (struct short_item_t *)p;

            if(itemsz == 1)
            {
                data = shitem->data.u8[0];
            }
            else if(itemsz == 2)
            {
                data = shitem->data.u16[0];
            }
            else if(itemsz == 4)
            {
                data = shitem->data.u32;
            }
        }

        /*
        printk("item: type %s, sz %lu, tag %u, data %u\n",
                item_type_str[HID_ITEM_TYPE(*p)], itemsz, tag, data);
        */

        switch(HID_ITEM_TYPE(*p))
        {
            case HID_ITEM_TYPE_MAIN:
                // process the local state if not end of collection
                if(tag != HID_ITEM_TAG_MAIN_END_COLL)
                {
                    for(i = 0; i < usage_stack_cnt; i++)
                    {
                        if(!(usage_stack[i].is_ext))
                        {
                            usage_stack[i].val.idpage.page = globst.page;
                            usage_stack[i].is_ext = 1;
                        }
                    }

                    locst.usage_stack = usage_stack;
                    locst.usage_stack_cnt = usage_stack_cnt;
                }

                if(tag == HID_ITEM_TAG_MAIN_COLL)
                {
                    // start a new collection
                    struct hid_collection_t *newcoll;

                    if(!(newcoll = create_hid_collection(curcoll, &locst, data)))
                    {
                        break;
                    }

                    collection_add_child(curcoll, newcoll);
                    curcoll = newcoll;
                }
                else if(tag == HID_ITEM_TAG_MAIN_END_COLL)
                {
                    if(curcoll == root)
                    {
                        printk("usb-hid: end collection tag on root\n");
                        break;
                    }

                    curcoll = curcoll->parent;
                }
                else
                {
                    struct hid_report_t *rep;
                    uint8_t reptype;

                    if(tag == HID_ITEM_TAG_MAIN_INPUT)
                    {
                        reptype = HID_REPORT_INPUT;
                    }
                    else if(tag == HID_ITEM_TAG_MAIN_OUTPUT)
                    {
                        reptype = HID_REPORT_OUTPUT;
                    }
                    else if(tag == HID_ITEM_TAG_MAIN_FEAT)
                    {
                        reptype = HID_REPORT_FEAT;
                    }
                    else
                    {
                        printk("usb-hid: skipping unknown main item tag: 0x%x\n", tag);
                        break;
                    }

                    if(!(rep = may_create_report(hid, reptype, globst.repid)))
                    {
                        break;
                    }

                    // use default if index is not set
                    if(!(locst.flags & FLAG_DISGNATOR_INDEX_SET))
                    {
                        locst.designator_index = locst.designator_min;
                    }

                    if(!(locst.flags & FLAG_STRING_INDEX_SET))
                    {
                        locst.str_index = locst.str_min;
                    }

                    report_add_main_item(rep, &globst, &locst, curcoll, data);
                }

                // reset local items for the next main item
                A_memset(&locst, 0, sizeof(struct local_state_t));
                usage_stack_cnt = 0;
                break;

            case HID_ITEM_TYPE_GLOBAL:
                if(tag == HID_ITEM_TAG_GLOBAL_USEPG)
                {
                    globst.page = data;
                }
                else if(tag == HID_ITEM_TAG_GLOBAL_LOGMIN)
                {
                    globst.logmin = data;
                }
                else if(tag == HID_ITEM_TAG_GLOBAL_LOGMAX)
                {
                    globst.logmax = data;
                }
                else if(tag == HID_ITEM_TAG_GLOBAL_PHYSMIN)
                {
                    globst.physmin = data;
                }
                else if(tag == HID_ITEM_TAG_GLOBAL_PHYSMAX)
                {
                    globst.physmax = data;
                }
                else if(tag == HID_ITEM_TAG_GLOBAL_UNITEXP)
                {
                    globst.unitexp = data;
                }
                else if(tag == HID_ITEM_TAG_GLOBAL_UNIT)
                {
                    globst.unit = data;
                }
                else if(tag == HID_ITEM_TAG_GLOBAL_REPSZ)
                {
                    globst.repsz = data;
                }
                else if(tag == HID_ITEM_TAG_GLOBAL_REPID)
                {
                    globst.repid = data;
                    hid->flags |= HID_FLAG_USES_REPID;
                }
                else if(tag == HID_ITEM_TAG_GLOBAL_REPCOUNT)
                {
                    globst.repcnt = data;
                }
                else if(tag == HID_ITEM_TAG_GLOBAL_PUSH)
                {
                    struct global_state_t *copy;

                    if(!(copy = kmalloc(sizeof(struct global_state_t))))
                    {
                        printk("usb-hid: insufficient memory for global push\n");
                        break;
                    }

                    A_memcpy(copy, &globst, sizeof(struct global_state_t));
                    globst.next = copy;
                }
                else if(tag == HID_ITEM_TAG_GLOBAL_POP)
                {
                    struct global_state_t *copy = globst.next;

                    if(copy == NULL)
                    {
                        printk("usb-hid: global pop on an empty stack\n");
                        break;
                    }

                    A_memcpy(&globst, copy, sizeof(struct global_state_t));
                    kfree(copy);
                }
                else
                {
                    printk("usb-hid: skipping unknown global item tag: 0x%x\n", tag);
                }

                break;

            case HID_ITEM_TYPE_LOCAL:
                if(tag == HID_ITEM_TAG_LOCAL_USAGE)
                {
                    struct usage_val_t use;

                    use.is_ext = (itemsz == sizeof(uint32_t));
                    use.val.ext = data;

                    if(usage_stack_cnt >= HID_MAX_REPORT_USAGES)
                    {
                        printk("usb-hid: too many usages\n");
                        break;
                    }

                    usage_stack[usage_stack_cnt++] = use;
                }
                else if(tag == HID_ITEM_TAG_LOCAL_USAGEMIN)
                {
                    locst.usage_min.is_ext = (itemsz == sizeof(uint32_t));
                    locst.usage_min.val.ext = data;
                    locst.flags |= FLAG_USAGE_MIN_SET;
                }
                else if(tag == HID_ITEM_TAG_LOCAL_USAGEMAX)
                {
                    locst.usage_max.is_ext = (itemsz == sizeof(uint32_t));
                    locst.usage_max.val.ext = data;
                    //locst.flags |= FLAG_USAGE_MAX_SET;

                    if(locst.usage_min.val.ext <=
                       locst.usage_max.val.ext)
                    {
                        struct usage_val_t use = locst.usage_min;
                        uint32_t j, cnt = locst.usage_max.val.ext -
                                          locst.usage_min.val.ext + 1;

                        for(j = 0; j < cnt; j++)
                        {
                            if(usage_stack_cnt >= HID_MAX_REPORT_USAGES)
                            {
                                printk("usb-hid: too many usages\n");
                                break;
                            }

                            usage_stack[usage_stack_cnt++] = use;
                            use.val.ext++;
                        }
                    }

                    locst.flags &= ~(FLAG_USAGE_MIN_SET|FLAG_USAGE_MAX_SET);
                }
                else if(tag == HID_ITEM_TAG_LOCAL_DESIGIDX)
                {
                    locst.designator_index = data;
                    locst.flags |= FLAG_DISGNATOR_INDEX_SET;
                }
                else if(tag == HID_ITEM_TAG_LOCAL_DESIGMIN)
                {
                    locst.designator_min = data;
                }
                else if(tag == HID_ITEM_TAG_LOCAL_DESIGMAX)
                {
                    locst.designator_max = data;
                }
                else if(tag == HID_ITEM_TAG_LOCAL_STRIDX)
                {
                    locst.str_index = data;
                    locst.flags |= FLAG_STRING_INDEX_SET;
                }
                else if(tag == HID_ITEM_TAG_LOCAL_STRMIN)
                {
                    locst.str_min = data;
                }
                else if(tag == HID_ITEM_TAG_LOCAL_STRMAX)
                {
                    locst.str_max = data;
                }
                else
                {
                    printk("usb-hid: skipping unknown local item tag: 0x%x\n", tag);
                }

                break;

            case HID_ITEM_TYPE_LONG:
                printk("usb-hid: skipping long item\n");
                break;
        }

        p += itemsz + 1;
    }

    printk("usb-hid: finished parsing report\n");

    struct hid_report_t *rep;

    for(i = 0, rep = hid->reports; rep != NULL; rep = rep->next, i++)
    {
        printk("  Report %d: type 0x%x, id 0x%x, sz 0x%x, usages 0x%x\n", 
                i, rep->type, rep->id, rep->sz, rep->usage_cnt);
    }

    // free used memory
    volatile struct global_state_t *st = globst.next, *next;

    while(st)
    {
        next = st->next;
        kfree((void *)st);
        st = next;
    }

    //free_collection(root);
    hid->rootcoll = root;

    return 0;
}


static size_t get_report_size(struct usb_hid_dev_t *hid)
{
    size_t sz;

    if(hid->input_report)
    {
        sz = (hid->input_report->sz + 7) / 8;

        if(hid->flags & HID_FLAG_USES_REPID)
        {
            sz++;
        }

        return sz;
    }

    return (hid->iface->desc.protocol == 1 ? 8 : 3);
}


static int hid_get_report_descriptor(struct usb_hid_dev_t *hid)
{
    struct usb_interface_t *iface = hid->iface;
    struct usb_dev_t *usb = iface->usb;
    struct usb_hid_descriptor_t *hiddesc = usb->hid_desc;
    int i;
    uint16_t desclen;
    uint8_t *descriptors, *buf;

    if(!hiddesc || !hiddesc->len)
    {
        printk("usb-hid: no HID descriptor found\n");
        return -EINVAL;
    }

    // find report descriptor number and size
    descriptors = (uint8_t *)hiddesc->descriptors;

    for(i = 0; i < hiddesc->descriptor_count; i++)
    {
        desclen = descriptors[1] | (descriptors[2] << 8);

        if(descriptors[0] == 0x22 && desclen != 0)
        {
            break;
        }

        descriptors += 3;
    }

    if(i == hiddesc->descriptor_count)
    {
        printk("usb-hid: no report descriptor found\n");
        return -EINVAL;
    }

    if(!(buf = kmalloc(desclen)))
    {
        printk("usb-hid: failed to alloc report descriptor buffer\n");
        return ENOMEM;
    }

    if(!usb_ctrl_in(usb, buf, 0x81, 6, 0x22, 0, iface->desc.interfacenum, desclen))
    {
        printk("usb-hid: failed to read report descriptor\n");
        kfree(buf);
        return EIO;
    }

    printk("usb-hid: found report descriptor len %d\n", desclen);

    i = parse_report_descriptor(hid, buf, desclen);

    kfree(buf);

    return i;
}


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
        printk("usb-hid: insufficient memory to init HID device\n");
        return -ENOMEM;
    }
    
    A_memset(hid, 0, sizeof(struct usb_hid_dev_t));
    hid->iface = iface;

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
        printk("usb-hid: HID has invalid IN endpoint\n");
        kfree(hid);
        return -EINVAL;
    }

    //printk("proto %d\n", iface->desc.protocol);

    if(iface->desc.protocol == 2)       // mouse
    {
        if(hid_get_report_descriptor(hid) < 0)
        {
            printk("usb-hid: failed to read report descriptor\n");
        }
        else
        {
            parse_collection(hid, hid->rootcoll);
        }
    }

    // set protocol (0=boot; 1=report)
    if(!usb_hid_set_protocol(iface->usb, !!(hid->flags & HID_FLAG_USES_REPPROTO)))
    {
        printk("usb-hid: failed to set HID protocol %d\n",
                        !!(hid->flags & HID_FLAG_USES_REPPROTO));

        // try the boot protocol
        if(!usb_hid_set_protocol(iface->usb, 0))
        {
            printk("usb-hid: failed to set HID boot protocol\n");
            kfree(hid);
            return -EINVAL;
        }

        hid->flags &= ~HID_FLAG_USES_REPPROTO;
    }

    // allocate buffer using the protocol's size, or if using the boot
    // protocol, default to 8 bytes for keyboards and 3 bytes for mice
    hid->bufbytes = (hid->flags & HID_FLAG_USES_REPPROTO) ?
                            get_report_size(hid) : 
                            (iface->desc.protocol == 1 ? 8 : 3);

    hid->buf = kmalloc(hid->bufbytes);
    printk("usb-hid: allocated buffer sz %u\n", hid->bufbytes);

    elevated_priority_lock(&usb_hid_tablock);
    hid->next = hid_list.next;
    hid_list.next = hid;
    elevated_priority_unlock(&usb_hid_tablock);

    if(iface->desc.protocol == 1)       // keyboard
    {
        printk("usb-hid: scheduling interrupt transfer for USB %s\n", "keyboard");
        usb_schedule_inttransfer(iface->usb, 
                                 (struct usb_endpoint_t *)endpoint, 
                                 &hid->transfer, hid->buf, hid->bufbytes,
                                 usb_handle_kbd_input, hid,
                                 endpoint->interval ? endpoint->interval : 10);
    }
    else if(iface->desc.protocol == 2)  // mouse
    {
        printk("usb-hid: scheduling interrupt transfer for USB %s\n", "mouse");
        usb_schedule_inttransfer(iface->usb, 
                                 (struct usb_endpoint_t *)endpoint, 
                                 &hid->transfer, hid->buf, hid->bufbytes,
                                 usb_handle_mouse_input, hid,
                                 endpoint->interval ? endpoint->interval : 10);
    }
    else
    {
        printk("usb-hid: unkown HID protocol: %d\n", iface->desc.protocol);
    }

    printk("usb-hid: finished intializing HID device\n");

    return 0;
}

