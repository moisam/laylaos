/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: tty.h
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
 *  \file tty.h
 *
 *  Helper functions for working with terminal devices.
 */

#ifndef __KERNEL_TTY_H__
#define __KERNEL_TTY_H__

#include <stddef.h>
#include <stdint.h>
#include <termios.h>

#define LF                      10      /* \n */
#define VT                      11      /* \v */
#define FF                      12      /* \f */
#define CR                      13      /* \r */

#include <kernel/ttydefaults.h>

/**
 * \def TTY_BUF_SIZE
 *
 * Terminal buffer size
 */
#define TTY_BUF_SIZE            1024

/*
 * Flags for all tty devices
 */
#define TTY_FLAG_EXCLUSIVE            0x01    /**< exclusive opening 
                                                   (open fails with EBUSY) */
#define TTY_FLAG_REVERSE_VIDEO        0x02    /**< reverse video mode */
#define TTY_FLAG_AUTOWRAP             0x04    /**< autowrap mode */
#define TTY_FLAG_CURSOR_RELATIVE      0x08    /**< cursor addressing relative to
                                                   scroll region */
#define TTY_FLAG_LFNL                 0x10    /**< follow each LF/VT/FF with 
                                                   a CR */
#define TTY_FLAG_FRAMEBUFFER          0x80    /**< graphics managed by the
                                                   framebuffer device */
#define TTY_FLAG_NO_TEXT              0x100   /**< no text (used by the gui) */
#define TTY_FLAG_ACTIVE               0x200   /**< the active tty (there can be
                                                   only one - highlander) */
#define TTY_FLAG_APP_KEYPAD_MODE      0x400   /**< tty in application keypad mode */
#define TTY_FLAG_APP_CURSORKEYS_MODE  0x800   /**< tty in cursor keys mode */
#define TTY_FLAG_INSERT_MODE          0x1000  /**< tty in insert mode */
#define TTY_FLAG_STOPPED              0x2000  /**< tty is stopped */

/*
 * Flags for pseudo-ttys
 */
#define TTY_FLAG_LOCKED           0x20      /**< pty slave is locked */
#define TTY_FLAG_MASTER_CLOSED    0x40      /**< pty master is closed */

/*
 * Terminal attribs
 */
#define ATTRIB_BOLD             0x01        /**< output bold text */
#define ATTRIB_BRIGHT_FG        0x02        /**< output text with 
                                                   bright foreground */
#define ATTRIB_UNDERLINE        0x04        /**< output underlined text */
#define ATTRIB_BRIGHT_BG        0x08        /**< output text with 
                                                   bright background */

/*
 * Terminal buffer cell attribs
 */
#define CELL_FLAG_BOLD          0x01        /**< cell is bold */
#define CELL_FLAG_CHARSET_LATIN 0x02        /**< uses default Latin charset */
#define CELL_FLAG_CHARSET_VT100 0x04        /**< uses VT100 graphics charset */
#define CELL_FLAG_CHARSET_SUPPL 0x08        /**< uses DEC supplemental charset */

#define NTTYS                   7           /**< number of virtual terminals */

/*
 * Extended tty ioctl commands (all formatted as hex for "VTC"-number)
 */
#define VT_SWITCH_TTY           0x56544301
#define VT_RAW_INPUT            0x56544302
#define VT_GRAPHICS_MODE        0x56544303

#define INVERT_COLOR(c)                 \
    (((c) & 0x0f) << 4) | (((c) & 0xf0) >> 4)


#ifdef KERNEL

#include <kernel/kqueue.h>
#include <kernel/select.h>
#include <kernel/vfs.h>

// the delete key
#define DEL                 127

// maximum parameters for a CSI-sequence
#define NPAR                16

#define ACTIVE_BUF(tty)         tty->buf[tty->active_buf]
#define ACTIVE_CELLATTRIBS(tty) tty->cellattribs[tty->active_buf]

/**
 * @struct tty_t
 * @brief The tty_t structure.
 *
 * A structure to represent a terminal (tty) device within the kernel.
 */
struct tty_t
{
	struct termios termios;     /**< tty info struct */
	pid_t pgid,                 /**< tty foreground process group id */
	      sid;                  /**< tty session id */
	void (*write)(struct tty_t *tty);   /**< function to write to device */
	struct kqueue_t read_q;     /**< tty read queue */
	struct kqueue_t write_q;    /**< tty write queue */
	struct kqueue_t secondary;  /**< tty secondary queue */
	struct winsize window;      /**< tty window size */
    //struct selinfo rsel;        /**< select channel for the read queue */
    //struct selinfo wsel;        /**< select channel for the write queue */
    //struct selinfo ssel;        /**< select channel for the secondary queue */

    void (*process_key)(struct tty_t *tty, int c);  /**< function to process 
                                                           input keys */
    void (*copy_to_buf)(struct tty_t *tty);         /**< function to copy 
                                                           input to secondary
                                                           buffer */

	volatile unsigned int flags;    /**< tty flags */

	unsigned int scroll_top,        /**< top row of scrolling window */
	             scroll_bottom;     /**< bottom row of scrolling window */
    //volatile struct task_t *waiting_task;   /**< task waiting on input */

    unsigned long npar,             /**< number of CSI-sequence parameters */
                  par[NPAR];        /**< parameters of a CSI-sequence */
    char palette_str[8];            /**< temp string used when setting palette */

    uint32_t vga_width,             /**< display width */
             vga_height;            /**< display height */

    uint32_t row,                   /**< current row */
             col;                   /**< current column */
    uint8_t color,                  /**< current color */
            default_color;          /**< default color */

    int active_buf;                 /**< 0 = normal buffer, 
                                         1 = alternate buffer */
    uint16_t *buf[2];               /**< terminal buffers */
    uint8_t *cellattribs[2];        /**< attributes for terminal buffer cells */

    // used by the framebuffer device
    uint32_t attribs;               /**< display attributes */
    int cursor_shown,               /**< is the cursor shown? */
        cursor_enabled;             /**< is the cursor enabled? */
    uint32_t fb_fgcolor,            /**< framebuffer foreground color */
             fb_bgcolor;            /**< framebuffer background color */
    uint32_t fb_palette[16];        /**< current color palette */
    uint8_t state;                  /**< terminal state */
    uint8_t *g[2], *gbold[2];       /**< pointers to G0 and G1 fonts, for both
                                         bold and normal states */
    uint8_t *gl, *glbold;           /**< GL is used for chars with the highest
                                         bit clear, G1 for those with the 
                                         highest bit set */
    uint8_t *gr, *grbold;           /**< GR is used for chars with the highest
                                         bit set */

    // saved state
    struct
    {
        uint32_t vga_width;         /**< saved display width */
        uint32_t vga_height;        /**< saved display height */
        uint32_t row;               /**< saved row */
        uint32_t col;               /**< saved column */
        uint8_t  color;             /**< saved color */
        uint32_t fb_fgcolor,        /**< saved framebuffer foreground color */
                 fb_bgcolor;        /**< saved framebuffer background color */
        uint32_t attribs;           /**< saved display attributes */
        int cursor_shown,           /**< saved cursor shown/hidden */
            cursor_enabled;         /**< saved cursor enabled/disabled */
        
        void *buf;                  /**< saved terminal buffer */
        size_t bufsz;               /**< saved buffer size */
    } saved_state;          /**< saved tty state */
};

//#endif      /* KERNEL */


/*
 * externs defined in ../../kernel/console.c
 */
extern void (*erase_display)(struct tty_t *, uint32_t, uint32_t, unsigned long);
extern void (*erase_line)(struct tty_t *, unsigned long);
extern void (*delete_chars)(struct tty_t *, unsigned long);
extern void (*insert_chars)(struct tty_t *, unsigned long);
extern void (*move_cur)(struct tty_t *);
extern void (*enable_cursor)(struct tty_t *, uint8_t, uint8_t);
extern void (*hide_cur)(struct tty_t *);
extern void (*tputchar)(struct tty_t *, char);
extern void (*scroll_up)(struct tty_t *, uint32_t, uint32_t, uint32_t);
extern void (*scroll_down)(struct tty_t *, uint32_t, uint32_t);
extern void (*set_attribs)(struct tty_t *, unsigned long, unsigned long *);
//extern void (*save_state)(void);
//extern void (*restore_state)(void);

//extern void (*save_screen)(struct tty_t *);
extern void (*restore_screen)(struct tty_t *);


// tty.c

extern int cur_tty;
extern int total_ttys;
extern struct tty_t ttytab[];


/**************************************
 * Functions defined in tty.c
 **************************************/

/**
 * @brief Allocate tty buffer.
 *
 * Allocate internal tty buffers.
 *
 * @param   tty         pointer to terminal device
 * @param   buf_index   0 for the normal buffer, 1 for the alternate buffer
 *
 * @return  zero on success, -(errno) on failure.
 */
int tty_alloc_buffer(struct tty_t *tty, int buf_index);

/**
 * @brief Dummy tty write.
 *
 * Dummy function for ttys that do not actually write to the screen.
 *
 * @param   tty     pointer to terminal device
 *
 * @return  nothing.
 */
void dummy_write(struct tty_t *tty);

/**
 * @brief Initialise ttys.
 *
 * Initialize terminal device queues and init system console.
 *
 * @return  nothing.
 */
void tty_init(void);

/**
 * @brief Set tty defaults.
 *
 * Set tty struct fields to builtin defaults. Called when creating new
 * terminal and pseudo-terminal devices.
 *
 * @param   tty     pointer to terminal device
 *
 * @return  nothing.
 */
void tty_set_defaults(struct tty_t *tty);

/**
 * @brief Perform a select operation on a tty device.
 *
 * After opening a terminal (tty) device, a select operation
 * can be performed on the device (accessed via the file struct \a f).
 * The operation (passed in \a which), can be FREAD, FWRITE, 0, or a 
 * combination of these.
 *
 * @param   f       open file struct
 * @param   which   the select operation to perform
 *
 * @return  1 if there are selectable events, 0 otherwise.
 */
long tty_select(struct file_t *f, int which);

/**
 * @brief Perform a poll operation on a tty device.
 *
 * After opening a terminal (tty) device, a poll operation
 * can be performed on the device (accessed via the file struct \a f).
 * The \a events field of \a pfd contains a combination of requested events
 * (POLLIN, POLLOUT, ...). The resultant events are returned in the \a revents
 * member of \a pfd.
 *
 * @param   f       open file struct
 * @param   pfd     poll events
 *
 * @return  1 if there are pollable events, 0 otherwise.
 */
long tty_poll(struct file_t *f, struct pollfd *pfd);

/**
 * @brief Send a tty signal.
 *
 * Send a signal to a terminal device's foreground/background process group.
 *
 * @param   pgid    the process group id
 * @param   signal  number of signal to send
 *
 * @return  nothing.
 */
void tty_send_signal(pid_t pgid, int signal);

/**
 * @brief Handler for syscall vhangup().
 *
 * Perform a "virtual" terminal hangup.
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see     https://docs.oracle.com/cd/E88353_01/html/E37841/vhangup-2.html
 */
long syscall_vhangup(void);

/**
 * @brief Read from tty device.
 *
 * Read data from a terminal (tty) device.
 *
 * @param   f       open file struct
 * @param   pos     offset in \a f to read from
 * @param   buf     input buffer where data is copied
 * @param   count   number of characters to read
 * @param   kernel  non-zero if the caller is a kernel function, zero if
 *                    it is a syscall from userland
 *
 * @return  number of characters read on success, -(errno) on failure.
 */
ssize_t ttyx_read(struct file_t *f, off_t *pos,
                  unsigned char *buf, size_t count, int kernel);

/**
 * @brief Write to tty device.
 *
 * Write data to a terminal (tty) device.
 *
 * @param   f       open file struct
 * @param   pos     offset in \a f to write to
 * @param   buf     output buffer where data is copied from
 * @param   count   number of characters to write
 * @param   kernel  non-zero if the caller is a kernel function, zero if
 *                    it is a syscall from userland
 *
 * @return  number of characters written on success, -(errno) on failure.
 */
ssize_t ttyx_write(struct file_t *f, off_t *pos,
                   unsigned char *buf, size_t count, int kernel);


/**************************************
 * Functions defined in tty_input.c
 **************************************/

/**
 * @brief Process tty input.
 *
 * Process an input key and emit the proper sequence codes, which are then
 * added to the tty's read queue.
 *
 * @param   tty     pointer to terminal device
 * @param   c       input keycode
 *
 * @return  nothing.
 */
void process_key(struct tty_t *tty, int c);

/**
 * @brief Process tty input.
 *
 * Copy the input keycode to the tty's secondary queue. This function is used
 * by tty[2], which is the tty we reserve for the GUI.
 *
 * @param   tty     pointer to terminal device
 * @param   code    input keycode
 *
 * @return  nothing.
 */
void raw_process_key(struct tty_t *tty, int code);

/**
 * @brief Copy tty input to secondary queue.
 *
 * Copy input from the terminal device's read queue to the secondary queue
 * from which reading tasks can fetch input. If the terminal device is in
 * canonical mode, it also does some input processing on the input.
 *
 * @param   tty     pointer to terminal device
 *
 * @return  nothing.
 *
 * @see     https://man7.org/linux/man-pages/man3/termios.3.html
 */
void copy_to_buf(struct tty_t *tty);

/**
 * @brief Copy tty input to secondary queue.
 *
 * Copy input from the terminal device's read queue to the secondary queue
 * from which reading tasks can fetch input. This function is used
 * by tty[2], which is the tty we reserve for the GUI.
 *
 * @param   tty     pointer to terminal device
 *
 * @return  nothing.
 *
 * @see     https://man7.org/linux/man-pages/man3/termios.3.html
 */
void raw_copy_to_buf(struct tty_t *tty);


/**************************************
 * Functions defined in tty_ioctl.c
 **************************************/

/**
 * @brief Flush tty queue.
 *
 * Flush the given terminal read/write queue.
 *
 * @param   q   the queue to flush
 *
 * @return  nothing.
 */
void flush_queue(struct kqueue_t *q);

/**
 * @brief Set controlling tty.
 *
 * Set the controlling terminal of the calling process.
 *
 * @param   dev     the terminal's device id
 * @param   tty     pointer to the terminal's struct
 * @param   arg     controls what is done to the terminal 
 *                    (details in link below)
 *
 * @return  zero.
 *
 * @see     https://linux.die.net/man/4/tty_ioctl
 */
long set_controlling_tty(dev_t dev, struct tty_t *tty, int arg);

/**
 * @brief tty ioctl.
 *
 * Terminal ioctl function.
 *
 * @param   dev     device id of the terminal
 * @param   cmd     command to perform on the terminal (details in the
 *                    link below)
 * @param   arg     optional argument, depends on what cmd is
 * @param   kernel  non-zero if the caller is a kernel function, zero if
 *                    it is a syscall from userland
 *
 * @return  zero on success, -(errno) on failure.
 *
 * @see     https://linux.die.net/man/4/tty_ioctl
 */
long tty_ioctl(dev_t dev, unsigned int cmd, char *arg, int kernel);


/**************************************
 * Functions defined in tty_state.c
 **************************************/

/**
 * @brief switch tty.
 *
 * Called to switch to another tty, e.g. when starting the GUI desktop
 * or when we implement virtual consoles.
 *
 * @param   which   tty number (0 to total_ttys - 1)
 *
 * @return  zero on success, -(errno) on failure.
 */
int switch_tty(int which);

void save_tty_cursor_state(struct tty_t *tty);
void restore_tty_cursor_state(struct tty_t *tty);
void save_tty_state(struct tty_t *tty);
void restore_tty_state(struct tty_t *tty);


/**************************************
 * Functions defined in console.c
 **************************************/

/**
 * @brief Initialise system console.
 *
 * Initialise the console and clear the screen.
 *
 * @return  nothing.
 */
void console_init(void);

/**
 * @brief Remove last character.
 *
 * Remove the last character from the console screen.
 *
 * @param   tty     pointer to the terminal's struct
 *
 * @return  nothing.
 */
void tremove_last_char(struct tty_t *tty);

/**
 * @brief Write output to the system console.
 *
 * Read characters from the given tyy's output buffer and write them to
 * screen, while also processing control sequences, updating cursor
 * position, and scrolling the screen as appropriate.
 *
 * @param   tty     pointer to the terminal's struct
 *
 * @return  nothing.
 *
 * @see     https://man7.org/linux/man-pages/man4/console_codes.4.html
 */
void console_write(struct tty_t *tty);

/**
 * @brief Scroll screen up.
 *
 * Scroll the screen up by copying each line to the line before it, starting 
 * at the given \a row (if \a row == 0, the whole screen is scrolled up).
 *
 * @param   tty         pointer to the terminal's struct
 * @param   width       scroll area width
 * @param   screenh     scroll area height
 * @param   row         first row to scroll
 *
 * @return  nothing.
 */
void ega_scroll_up(struct tty_t *tty, uint32_t width, 
                                      uint32_t screenh, uint32_t row);

/**
 * @brief Scroll screen down.
 *
 * Scroll the screen down by copying each line to the line below it, ending at
 * the the current row (if row == 0, the whole screen is scrolled down).
 *
 * @param   tty         pointer to the terminal's struct
 * @param   width       scroll area width
 * @param   height      scroll area height
 *
 * @return  nothing.
 */
void ega_scroll_down(struct tty_t *tty, uint32_t width, uint32_t height);

/**
 * @brief Erase the screen.
 *
 * Erase display, the start and end of erased area depends on \a cmd:
 *    0 - erase from cursor to end of display
 *    1 - erase from start to cursor
 *    2 - erase whole display
 *    3 - erase whole display, including scroll-back buffer (not implemented)
 *
 * @param   tty         pointer to the terminal's struct
 * @param   width       screen width
 * @param   height      screen height
 * @param   cmd         how to erase (see description above)
 *
 * @return  nothing.
 */
void ega_erase_display(struct tty_t *tty, uint32_t width,
                                          uint32_t height, unsigned long cmd);

/**
 * @brief Erase a line.
 *
 * Erase line, the start and end of erased area depends on \a cmd:
 *    0 - erase from cursor to end of line
 *    1 - erase from start of line to cursor
 *    2 - erase whole line
 *
 * @param   tty         pointer to the terminal's struct
 * @param   cmd         how to erase (see description above)
 *
 * @return  nothing.
 */
void ega_erase_line(struct tty_t *tty, unsigned long cmd);

/**
 * @brief Delete characters.
 *
 * Delete \a count chars from the cursor's position.
 *
 * @param   tty         pointer to the terminal's struct
 * @param   count       how many characters to delete
 *
 * @return  nothing.
 */
void ega_delete_chars(struct tty_t *tty, unsigned long count);

/**
 * @brief Insert characters.
 *
 * Insert \a count blank chars at the cursor's position.
 *
 * @param   tty         pointer to the terminal's struct
 * @param   count       how many characters to insert
 *
 * @return  nothing.
 */
void ega_insert_chars(struct tty_t *tty, unsigned long count);

/**
 * @brief Set tty attribs.
 *
 * Set the terminal's graphics attributes.
 *
 * @param   tty         pointer to the terminal's struct
 * @param   npar        parameter count
 * @param   par         parameters
 *
 * @return  nothing.
 *
 * @see     https://man7.org/linux/man-pages/man4/console_codes.4.html
 */
void ega_set_attribs(struct tty_t *tty, unsigned long npar, 
                                        unsigned long *par);

/**
 * @brief Save cursor position.
 *
 * Save current cursor position.
 *
 * @return  nothing.
 */
void save_cursor(void);

/**
 * @brief Restore cursor position.
 *
 * Restore cursor position to the value saved by save_cursor().
 *
 * @return  nothing.
 */
void restore_cursor(void);

void ega_save_screen(struct tty_t *tty);
void ega_restore_screen(struct tty_t *tty);

#endif      /* KERNEL */

#endif      /* __KERNEL_TTY_H__ */
