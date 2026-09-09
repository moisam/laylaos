/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2025 (c)
 * 
 *    file: usb_hid.h
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
 *  \file usb_hid.h
 *
 *  The Universal Serial Bus (USB) driver definitions.
 */

#ifndef KERNEL_USB_HID_H
#define KERNEL_USB_HID_H

// fields in the item prefix
#define HID_ITEM_SIZE(i)                    ((i) & 0x3)
#define HID_ITEM_TYPE(i)                    (((i) >> 2) & 0x3)
#define HID_ITEM_TAG(i)                     (((i) >> 4) & 0xf)

// item types
#define HID_ITEM_TYPE_MAIN                  0x00
#define HID_ITEM_TYPE_GLOBAL                0x01
#define HID_ITEM_TYPE_LOCAL                 0x02
#define HID_ITEM_TYPE_LONG                  0x03

// item tags
#define HID_ITEM_TAG_MAIN_INPUT             0x08
#define HID_ITEM_TAG_MAIN_OUTPUT            0x09
#define HID_ITEM_TAG_MAIN_COLL              0x0a
#define HID_ITEM_TAG_MAIN_FEAT              0x0b
#define HID_ITEM_TAG_MAIN_END_COLL          0x0c
#define HID_ITEM_TAG_LONG                   0x0f

#define HID_ITEM_TAG_GLOBAL_USEPG           0x00
#define HID_ITEM_TAG_GLOBAL_LOGMIN          0x01
#define HID_ITEM_TAG_GLOBAL_LOGMAX          0x02
#define HID_ITEM_TAG_GLOBAL_PHYSMIN         0x03
#define HID_ITEM_TAG_GLOBAL_PHYSMAX         0x04
#define HID_ITEM_TAG_GLOBAL_UNITEXP         0x05
#define HID_ITEM_TAG_GLOBAL_UNIT            0x06
#define HID_ITEM_TAG_GLOBAL_REPSZ           0x07
#define HID_ITEM_TAG_GLOBAL_REPID           0x08
#define HID_ITEM_TAG_GLOBAL_REPCOUNT        0x09
#define HID_ITEM_TAG_GLOBAL_PUSH            0x0a
#define HID_ITEM_TAG_GLOBAL_POP             0x0b

#define HID_ITEM_TAG_LOCAL_USAGE            0x00
#define HID_ITEM_TAG_LOCAL_USAGEMIN         0x01
#define HID_ITEM_TAG_LOCAL_USAGEMAX         0x02
#define HID_ITEM_TAG_LOCAL_DESIGIDX         0x03
#define HID_ITEM_TAG_LOCAL_DESIGMIN         0x04
#define HID_ITEM_TAG_LOCAL_DESIGMAX         0x05
#define HID_ITEM_TAG_LOCAL_STRIDX           0x07
#define HID_ITEM_TAG_LOCAL_STRMIN           0x08
#define HID_ITEM_TAG_LOCAL_STRMAX           0x09
#define HID_ITEM_TAG_LOCAL_DELIM            0x0a

// collection types
#define COLL_TYPE_PHYS                      0x00
#define COLL_TYPE_APP                       0x01
#define COLL_TYPE_LOG                       0x02
#define COLL_TYPE_REPORT                    0x03
#define COLL_TYPE_ARR                       0x04
#define COLL_TYPE_USE_SWITCH                0x05
#define COLL_TYPE_USE_MOD                   0x06
#define COLL_TYPE_ALL                       0xff

// usage pages
#define HID_USAGE_PAGE_GENDESK              0x01
#define HID_USAGE_PAGE_BUTTON               0x09
#define HID_USAGE_PAGE_CONSUMER             0x0c

// usage ids for usage page 0x01 (generic desktop)
#define HID_UID_GENDESK_POINTER             0x01
#define HID_UID_GENDESK_MOUSE               0x02
#define HID_UID_GENDESK_KEYBOARD            0x06
#define HID_UID_GENDESK_X                   0x30
#define HID_UID_GENDESK_Y                   0x31
#define HID_UID_GENDESK_WHEEL               0x38
#define HID_UID_GENDESK_HORIZPAN            0x238

// report types
#define HID_REPORT_INPUT                    1
#define HID_REPORT_OUTPUT                   2
#define HID_REPORT_FEAT                     4

// max report usage count we can handle
#define HID_MAX_REPORT_USAGES               64

// max mouse buttons we can handle
#define HID_MAX_MOUSE_BUTTONS               5


struct short_item_t
{
    uint8_t prefix;

    union
    {
        uint8_t  u8[4];
        int8_t   s8[4];
        uint16_t u16[2];
        int16_t  s16[2];
        uint32_t u32;
        int32_t  s32;
    } data;
} __attribute__((packed));

struct long_item_t
{
    uint8_t prefix;
    uint8_t datasz;
    uint8_t longtag;
    uint8_t data[];
} __attribute__((packed));

/*
 * Data bits as defined in HID spec section 6.2.2.5 Input, Output, and Feature Items
 */
struct main_data_t
{
    union
    {
        struct
        {
            uint32_t constant  : 1;
            uint32_t isvar     : 1;
            uint32_t relative  : 1;
            uint32_t wrap      : 1;
            uint32_t nonlinear : 1;
            uint32_t nopref    : 1;
            uint32_t nullstate : 1;
            uint32_t isvolatile: 1;
            uint32_t bufbytes  : 1;
            uint32_t res       : 1;
        } bits;

        uint32_t raw;
    } val;
} __attribute__((packed));

struct usage_val_t
{
    union
    {
        struct
        {
            uint16_t id;
            uint16_t page;
        } __attribute__((packed)) idpage;

        uint32_t ext;
    } val;

    int is_ext;
};

struct global_state_t
{
    uint16_t page;                  /**< usage page */
    uint32_t logmin, logmax;        /**< logical min and max */
    uint32_t physmin, physmax;      /**< physical min and max */
    uint8_t unit, unitexp;          /**< unit and unit exponent */
    uint32_t repsz, repcnt;         /**< report size and count */
    uint8_t repid;                  /**< report id */
    struct global_state_t *next;    /**< next global state */
};

struct local_state_t
{
    struct usage_val_t *usage_stack;    /**< usage stack */
    unsigned int usage_stack_cnt;       /**< usage stack count */
    struct usage_val_t usage_min;       /**< usage min value */
    struct usage_val_t usage_max;       /**< usage max value */
    uint32_t designator_index;          /**< designator index */
    uint32_t designator_min;            /**< designator min value */
    uint32_t designator_max;            /**< designator max value */
    uint8_t str_index;                  /**< string index */
    uint8_t str_min;                    /**< string min value */
    uint8_t str_max;                    /**< string max value */

#define FLAG_USAGE_MIN_SET              0x01
#define FLAG_USAGE_MAX_SET              0x02
#define FLAG_DISGNATOR_INDEX_SET        0x04
#define FLAG_STRING_INDEX_SET           0x08
    unsigned int flags;                 /**< flags */
};

struct hid_report_item_t
{
    uint32_t byteoff;
    uint32_t mask;
    uint32_t min, max;
    uint32_t usage;
    uint8_t shift;
    uint8_t bitcnt;
    uint8_t bytecnt;

#define HID_REPORT_ITEM_HAS_DATA        0x01
#define HID_REPORT_ITEM_IS_ARRAY        0x02
#define HID_REPORT_ITEM_IS_RELATIVE     0x04
    uint32_t flags;

    struct hid_report_item_t *next;
};

struct hid_report_t
{
    uint8_t type;
    uint8_t id;
    uint32_t sz;
    uint32_t usages[HID_MAX_REPORT_USAGES];
    unsigned int usage_cnt;
    struct hid_report_t *next;
    struct hid_report_item_t *first_item, *last_item;
};

struct hid_collection_t
{
    uint8_t type;
    uint8_t strid;
    uint8_t physid;
    uint32_t usage;
    struct hid_collection_t *parent;
    struct hid_collection_t *next;
    struct hid_collection_t *first_child, *last_child;
    struct hid_report_item_t *first_item, *last_item;
};

struct usb_hid_descriptor_t
{
    uint8_t  len;
    uint8_t  type;
    uint16_t hid_bcd;
    uint8_t  country_code;
    uint8_t  descriptor_count;

    struct
    {
        uint8_t  type;
        uint16_t len;
    } descriptors[1];
} __attribute__((packed));

struct usb_hid_dev_t
{
    struct usb_transfer_t transfer;
    struct usb_interface_t *iface;  /**< pointer to the USB device struct */
    size_t bufbytes;
    uint8_t *buf;

    struct hid_report_t *reports;
    struct hid_collection_t *rootcoll;
    struct usb_hid_dev_t *next; /**< pointer to next HID device */

#define HID_FLAG_USES_REPID             0x01    /**< device uses report ids */
#define HID_FLAG_USES_REPPROTO          0x02    /**< device uses report protocol */
#define HID_FLAG_IS_MOUSE               0x04    /**< device is a mouse */
#define HID_FLAG_IS_KBD                 0x08    /**< device is a keyboard */
    int flags;

    /* fields for use by keyboard HID devices */
    uint8_t keystate[256];      /**< USB keyboard key state */
    uint8_t last_packet[8];     /**< last packet sent by USB keyboard */
    uint8_t leds;
    uint8_t last_key_pressed;
    int last_key_counter;

    /* fields for use by mouse HID devices */
    int report_button_count;
    struct hid_report_t *input_report;
    struct hid_report_item_t *report_item_x;
    struct hid_report_item_t *report_item_y;
    struct hid_report_item_t *report_item_wheel;
    struct hid_report_item_t *report_item_horizpan;
    struct hid_report_item_t *report_item_button[HID_MAX_MOUSE_BUTTONS];
};


int init_hid(struct usb_interface_t *iface);
void usb_hid_remove(struct usb_interface_t *iface);

#endif      /* KERNEL_USB_HID_H */
