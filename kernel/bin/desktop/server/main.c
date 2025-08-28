/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024, 2025 (c)
 * 
 *    file: main.c
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
 *  \file main.c
 *
 *  The GUI server core. Here we initialise the server, fork the desktop
 *  task, and then listen to and serve client requests. We also process 
 *  mouse events and update the screen periodically if there are any
 *  "dirty" regions that need to be painted.
 */

#define GUI_SERVER
#define _GNU_SOURCE
#define _POSIX_THREADS
#define _POSIX_TIMERS
#define _POSIX_MONOTONIC_CLOCK
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <gui/vbe.h>
#include <gui/fb.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sched.h>
#include <kernel/laylaos.h>
#include <kernel/mouse.h>
#include <kernel/vfs.h>
#include <kernel/tty.h>
#include "../include/server/cursor.h"
#include "../include/server/event.h"
#include "../include/server/server.h"
#include "../include/gui.h"
#include "../include/keys.h"
#include "../include/resources.h"
#include "../include/clipboard.h"
#include "../include/directrw.h"

mutex_t update_lock = MUTEX_INITIALIZER;
mutex_t input_lock = MUTEX_INITIALIZER;

Rect rtmp[64];
int count = 0;

#define SCREEN_RECTS_NOT_EXTERNS
#include "inlines.c"

#define GLOB        __global_gui_data

// defined in server-window-mouse.c
extern int root_mouse_x, root_mouse_y;
extern struct mouse_state_t root_button_state;

int maxopenfd;
fd_set openfds;
struct clientfd_t clientfds[NR_OPEN];

struct framebuffer_t vbe_framebuffer = { 0, };
struct gc_t *gc;

struct server_window_t *root_window = NULL;
struct server_window_t *grabbed_mouse_window = NULL;
struct server_window_t *grabbed_keyboard_window = NULL;

Rect mouse_bounds = { 0, };
Rect desktop_bounds = { 0, };

int mouse_is_confined = 0;
int received_sigwinch = 0;

void tty_reset(void);


void sig_handler(int signum __attribute__((unused)))
{
}


void sigint_handler(int signum __attribute__((unused)))
{

}


void sighup_handler(int signum __attribute__((unused)))
{

}


void sigsegv_handler(int signum, siginfo_t *info, 
                     void *p __attribute__((unused)))
{
    tty_reset();
    write(0, "\033[?25l", 6);
    __asm__ __volatile__("xchg %%bx, %%bx"::);
    printf("guiserver: received SIGSEGV for address %p\n", info->si_addr);
    exit(EXIT_FAILURE);
}


void sigchld_handler(int signum __attribute__((unused)))
{
    int        pid, st;
    int        saved_errno = errno;

    while((pid = waitpid(-1, &st, WNOHANG)) != 0)
    {
        if(errno == ECHILD)
        {
            break;
        }
        
        printf("server: unknown child exited (pid %d)\n", pid);
    }

    errno = saved_errno;
}


void sigwinch_handler(int signum __attribute__((unused)))
{
    //__asm__ __volatile__("xchg %%bx, %%bx"::);
    received_sigwinch = 1;
}


void draw_mouse_cursor(int invalidate)
{
    if(cur_cursor == 0)
    {
        // Someone may have hidden the mouse or died and left us with no 
        // cursor. If the cursor is not hidden intentionally, ensure we have
        // a visible cursor by trying to find out which window is under the
        // cursor at the moment. If this still fails, we bail out.
        if(root_window)
        {
            struct server_window_t *child;
            ListNode *current_node;

            //We go front-to-back in terms of the window stack for free occlusion
            current_node = root_window->children->last_node;

            for( ; current_node != NULL; current_node = current_node->prev)
            {
                child = (struct server_window_t *)current_node->payload;

                //Don't check hidden windows
                if((child->flags & WINDOW_HIDDEN))
                {
                    continue;
                }

                //Check mouse is within window bounds
                if(!(root_mouse_x >= child->x && root_mouse_x <= child->xw1 &&
                     root_mouse_y >= child->y && root_mouse_y <= child->yh1))
                {
                    continue;
                }

                cur_cursor = child->cursor_id;
                break;
            }
        }

        // still no luck
        if(cur_cursor == 0)
        {
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            return;
        }
    }

    if(!cursor[cur_cursor].data)
    {
        __asm__ __volatile__("xchg %%bx, %%bx"::);
        __asm__ __volatile__("xchg %%bx, %%bx"::);
        return;
    }

    int _mouse_x = root_mouse_x - cursor[cur_cursor].hotx;
    int _mouse_y = root_mouse_y - cursor[cur_cursor].hoty;
    int index = 0;
    int mouse_w = cursor[cur_cursor].w;
    int mouse_h = cursor[cur_cursor].h;
    uint32_t y2 = _mouse_y * gc->pitch;
    uint32_t x2 = _mouse_x * gc->pixel_width;
    uint32_t *mouse_img;
    int ymax = _mouse_y;
    int xmax2 = _mouse_x + mouse_w;
    int ymax2 = _mouse_y + mouse_h;
        
    if(xmax2 > gc->w)
    {
        xmax2 = gc->w;
    }
    
    if(ymax2 > gc->h)
    {
        ymax2 = gc->h;
    }

    // Copy the pixels from our mouse image into the framebuffer
    if(gc->pixel_width == 1)
    {
        for( ; ymax < ymax2; y2 += gc->pitch, index += mouse_w, ymax++)
        {
            uint8_t *backbuf = gc->buffer + x2 + y2;
            int xmax = _mouse_x;

            mouse_img = cursor[cur_cursor].data + index;

            for( ; xmax < xmax2; mouse_img++, backbuf++, xmax++)
            {
                // Don't place a pixel if it's transparent
                if(*mouse_img != transparent_color)
                {
                    *backbuf = *mouse_img;
                }
            }
        }
    }
    else if(gc->pixel_width == 2)
    {
        for( ; ymax < ymax2; y2 += gc->pitch, index += mouse_w, ymax++)
        {
            uint8_t *backbuf = gc->buffer + x2 + y2;
            int xmax = _mouse_x;

            mouse_img = cursor[cur_cursor].data + index;

            for( ; xmax < xmax2; mouse_img++, backbuf += 2, xmax++)
            {
                // Don't place a pixel if it's transparent
                if(*mouse_img != transparent_color)
                {
                    *(uint16_t *)backbuf = *mouse_img;
                }
            }
        }
    }
    else if(gc->pixel_width == 3)
    {
        for( ; ymax < ymax2; y2 += gc->pitch, index += mouse_w, ymax++)
        {
            uint8_t *backbuf = gc->buffer + x2 + y2;
            int xmax = _mouse_x;

            mouse_img = cursor[cur_cursor].data + index;

            for( ; xmax < xmax2; mouse_img++, backbuf += 3, xmax++)
            {
                // Don't place a pixel if it's transparent
                if(*mouse_img != transparent_color)
                {
                    backbuf[0] = (*mouse_img) & 0xff;
                    backbuf[1] = (*mouse_img >> 8) & 0xff;
                    backbuf[2] = (*mouse_img >> 16) & 0xff;
                }
            }
        }
    }
    else
    {
        for( ; ymax < ymax2; y2 += gc->pitch, index += mouse_w, ymax++)
        {
            uint8_t *backbuf = gc->buffer + x2 + y2;
            int xmax = _mouse_x;

            mouse_img = cursor[cur_cursor].data + index;

            for( ; xmax < xmax2; mouse_img++, backbuf += 4, xmax++)
            {
                // Don't place a pixel if it's transparent
                if(*mouse_img != transparent_color)
                {
                    *(uint32_t *)backbuf = *mouse_img;
                }
            }
        }
    }
    
    if(invalidate)
    {
        invalidate_screen_rect(_mouse_y, _mouse_x, 
                               _mouse_y + mouse_h - 1, 
                               _mouse_x + mouse_w - 1);
    }
}


void force_redraw_cursor(int _mouse_x, int _mouse_y)
{
    volatile int mx, my;

    my = root_mouse_y - cursor[old_cursor].hoty;
    mx = root_mouse_x - cursor[old_cursor].hotx;

    RectList dirty_list;
    Rect mouse_rect;

    dirty_list.root = &mouse_rect;
    dirty_list.last = &mouse_rect;

    mouse_rect.top = my;
    mouse_rect.left = mx;
    mouse_rect.bottom = my + cursor[old_cursor].h - 1;
    mouse_rect.right = mx + cursor[old_cursor].w - 1;
    mouse_rect.next = NULL;

    // Do a dirty update for the desktop, which will, in turn, do a 
    // dirty update for all affected child windows
    server_window_paint(gc, root_window, &dirty_list, 
                        FLAG_PAINT_CHILDREN | FLAG_PAINT_BORDER);

    // Update mouse position
    root_mouse_x = _mouse_x;
    root_mouse_y = _mouse_y;
    
    draw_mouse_cursor(0);

    int new_mouse_x = root_mouse_x - cursor[cur_cursor].hotx;
    int new_mouse_y = root_mouse_y - cursor[cur_cursor].hoty;
    int new_mouse_b = new_mouse_y + cursor[cur_cursor].h - 1;
    int new_mouse_r = new_mouse_x + cursor[cur_cursor].w - 1;
    
    if(new_mouse_x < mouse_rect.left)
    {
        mouse_rect.left = new_mouse_x;
    }

    if(new_mouse_y < mouse_rect.top)
    {
        mouse_rect.top = new_mouse_y;
    }
    
    if(new_mouse_b > mouse_rect.bottom)
    {
        mouse_rect.bottom = new_mouse_b;
    }

    if(new_mouse_r > mouse_rect.right)
    {
        mouse_rect.right = new_mouse_r;
    }

    invalidate_screen_rect(mouse_rect.top, mouse_rect.left, 
                           mouse_rect.bottom, mouse_rect.right);
}


mutex_t mouse_lock = MUTEX_INITIALIZER;

void process_mouse(struct mouse_packet_t *packet)
{
    int _mouse_x = root_mouse_x;
    int _mouse_y = root_mouse_y;

    if(!root_window)
    {
        return;
    }
      
    _mouse_x += packet->dx;
    _mouse_y -= packet->dy;
    
    // make sure mouse is within bounds
    if(_mouse_x < mouse_bounds.left)
    {
        _mouse_x = mouse_bounds.left;
    }

    if(_mouse_x > mouse_bounds.right)
    {
        _mouse_x = mouse_bounds.right;
    }

    if(_mouse_y < mouse_bounds.top)
    {
        _mouse_y = mouse_bounds.top;
    }

    if(_mouse_y > mouse_bounds.bottom)
    {
        _mouse_y = mouse_bounds.bottom;
    }


    struct mouse_state_t mstate;
    int lbutton_down = packet->buttons & MOUSE_LBUTTON_DOWN;
    int last_lbutton_down = root_button_state.buttons & MOUSE_LBUTTON_DOWN;
    struct server_window_t *old_mouseover_child = root_window->mouseover_child;
    struct server_window_t *old_active_child = root_window->active_child;

    mstate.buttons = packet->buttons;
    root_button_state.buttons = packet->buttons;

    mstate.left_pressed = (lbutton_down && !last_lbutton_down);
    mstate.left_released = (!lbutton_down && last_lbutton_down);

    if(grabbed_mouse_window)
    {
        mstate.x = _mouse_x - grabbed_mouse_window->x;
        mstate.y = _mouse_y - grabbed_mouse_window->y;
        server_window_mouseover(gc, grabbed_mouse_window, &mstate);
    }
    else if(root_window->tracked_child)
    {
        mstate.x = _mouse_x - root_window->tracked_child->x;
        mstate.y = _mouse_y - root_window->tracked_child->y;
        server_window_mouseover(gc, root_window->tracked_child, &mstate);

        if(mstate.left_released)
        {
            root_window->tracked_child = NULL;
        }
    }
    else
    {
        mstate.x = _mouse_x;
        mstate.y = _mouse_y;
        server_window_process_mouse(gc, root_window, &mstate);
    }

#if 0
    // if no window was clicked, the click must have occurred in the desktop
    // background itself
    if(mstate.left_pressed && !grabbed_mouse_window &&
                              !root_window->drag_child &&
                              !root_window->tracked_child)
    {
        //__asm__ __volatile__("xchg %%bx, %%bx"::);
        if(root_window->active_child)
        {
            // hide any open menus
            server_window_hide_menu(root_window->active_child);
        }
    }
#endif

    if(old_mouseover_child && 
       old_mouseover_child != root_window->mouseover_child)
    {
        mouse_exit(gc, old_mouseover_child, 
                       mstate.x, mstate.y, mstate.buttons);
    }

    if(old_active_child && old_active_child != root_window->active_child)
    {
        mouse_exit(gc, old_active_child, mstate.x, mstate.y, mstate.buttons);
    }

    force_redraw_cursor(_mouse_x, _mouse_y);
}


#define KEY_PREFIX          0x8000

uint8_t *create_canvas(uint32_t canvas_size, int *__shmid)
{
    static int next_id = 1;
    int shmid;
    //key_t key = ftok("/sbin/desktop", pid);
    key_t key = KEY_PREFIX + next_id;
    void *p;
    
    if((shmid = shmget(key, canvas_size, IPC_CREAT | IPC_EXCL | 0666)) == -1)
    {
        return NULL;
    }

    if((p = shmat(shmid, NULL, 0)) == (void *)-1)
    {
        return NULL;
    }
    
    *__shmid = shmid;

    next_id++;
    
    return p;
}


struct server_window_t *server_window_by_winid(winid_t winid)
{
    struct server_window_t *window;
    ListNode *current_node;

    if(!root_window)
    {
        return NULL;
    }
    
    if(winid == root_window->winid)
    {
        return root_window;
    }

    if(!root_window->children)
    {
        return NULL;
    }

    // Find the child in the list
    for(current_node = root_window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        window = (struct server_window_t *)current_node->payload;

        if(winid == window->winid)
        {
            return (struct server_window_t *)window;
        }
    }
    
    return NULL;
}


void server_window_add(struct server_window_t *window)
{
    if(root_window == NULL)
    {
        root_window = window;
        root_window->children = List_new();

        root_mouse_x = root_window->w / 2;
        root_mouse_y = root_window->h / 2;
        A_memset(&root_button_state, 0, sizeof(root_button_state));
        root_window->cursor_id = CURSOR_NORMAL;
    }
    else
    {
        server_window_insert_child(root_window, window);
        
        window->cursor_id = window->parent->cursor_id;
    }
}


struct server_window_t *server_window_create(int16_t x, int16_t y, 
                                             uint16_t w, uint16_t h,
                                             int gravity, uint32_t flags, 
                                             winid_t winid)
{
    struct server_window_t *win;
    
    if(!(win = malloc(sizeof(struct server_window_t))))
    {
        return NULL;
    }

    A_memset(win, 0, sizeof(struct server_window_t));

    if(!(win->clipping.clip_rects = RectList_new()))
    {
        free(win);
        return NULL;
    }

    win->clipping.clipping_on = 0;
    
    if((flags & WINDOW_NODECORATION))
    {
        flags |= WINDOW_NOCONTROLBOX;
    }
    
    /*
    if((flags & WINDOW_NORESIZE))
    {
        win->controlbox_state |= MAXIMIZEBUTTON_DISABLED;
    }

    if((flags & WINDOW_NOMINIMIZE))
    {
        win->controlbox_state |= MAXIMIZEBUTTON_DISABLED;
    }
    */

    win->type = WINDOW_TYPE_WINDOW;
    win->flags = flags;
    win->winid = winid;
    win->state = WINDOW_STATE_NORMAL;

    // apply gravity constraints (if any)
    if((gravity & WINDOW_ALIGN_TOP))
    {
        y = desktop_bounds.top;
    }

    if((gravity & WINDOW_ALIGN_BOTTOM))
    {
        y = desktop_bounds.bottom - h;

        if(!(flags & WINDOW_NODECORATION))
        {
            y -= WINDOW_TITLEHEIGHT + WINDOW_BORDERWIDTH;
        }
    }

    if((gravity & WINDOW_ALIGN_CENTERV))
    {
        uint16_t h2 = h;

        y = ((desktop_bounds.bottom - desktop_bounds.top) / 2);

        if(!(flags & WINDOW_NODECORATION))
        {
            h2 += WINDOW_TITLEHEIGHT + WINDOW_BORDERWIDTH;
        }

        y -= h2 / 2;

        // make sure it doesn't fall off screen
        if(y < desktop_bounds.top)
        {
            y = desktop_bounds.top;
        }
    }

    if((gravity & WINDOW_ALIGN_LEFT))
    {
        x = desktop_bounds.left;
    }

    if((gravity & WINDOW_ALIGN_RIGHT))
    {
        x = desktop_bounds.right - w;

        if(!(flags & WINDOW_NODECORATION))
        {
            x -= (2 * WINDOW_BORDERWIDTH);
        }
    }

    if((gravity & WINDOW_ALIGN_CENTERH))
    {
        x = ((desktop_bounds.right - desktop_bounds.left) / 2);

        if(!(flags & WINDOW_NODECORATION))
        {
            x -= (w + (2 * WINDOW_BORDERWIDTH)) / 2;
        }
        else
        {
            x -= w / 2;
        }

        // make sure it doesn't fall off screen
        if(x < 0)
        {
            x = 0;
        }
    }

    // now set the window's size
    server_window_set_size(win, x, y, w, h);
    win->minw = WINDOW_MIN_WIDTH;
    win->minh = WINDOW_MIN_HEIGHT;
    
    // and get our canvas
    win->canvas_size = w * h * vbe_framebuffer.pixel_width;
    
    if(!(win->canvas = create_canvas(win->canvas_size, &win->shmid)))
    {
        free(win);
        return NULL;
    }
    
    win->canvas_alloced_size = win->canvas_size;
    win->canvas_pitch = w * vbe_framebuffer.pixel_width;

    win->icon = server_resource_load(DEFAULT_EXE_ICON_PATH);

    mutex_init(&win->lock);
    
    // add to our list
    server_window_add(win);

    return win;
}


/*
 * The very last step in destroying an application window. Frees shared 
 * memory, resources (bitmaps, icons, ...), menu frames, and the window 
 * itself.
 * In case the application didn't exit properly (e.g. killed by a signal, or
 * just some asshole who didn't exit properly), we free kernel resources as 
 * well.
 */
void server_window_destroy(struct server_window_t *window)
{
    if(!window)
    {
        return;
    }

    // ensure we hide any open menus before we destroy them!
    draw_mouse_cursor(1);
    
    struct server_window_t *tmp;
    ListNode *current_node;

    if(root_window && window->winid != root_window->winid && 
       root_window->children)
    {
        // Find the child in the list
        for(current_node = root_window->children->root_node;
            current_node != NULL;
            current_node = current_node->next)
        {
            tmp = (struct server_window_t *)current_node->payload;

            if((tmp->type == WINDOW_TYPE_MENU_FRAME ||
                tmp->type == WINDOW_TYPE_DIALOG) &&
               tmp->owner_winid == window->winid)
            {
                server_window_may_hide(tmp);
                server_window_destroy(tmp);

                // restart from the top as root_window child pointers are
                // messed up now
                current_node = root_window->children->root_node;
            }
        }
    }
    
    // the call to server_window_remove_child() will call cancel_active_child()

    server_resource_free(window->icon);
    window->icon = NULL;

    shmctl(window->shmid, IPC_RMID, NULL);
    shmdt(window->canvas);
    window->shmid = 0;
    window->canvas = NULL;

    server_window_remove_child(root_window, window);
    notify_parent_win_destroyed(window);
    
    if(window->owner_winid && 
       (tmp = server_window_by_winid(window->owner_winid)))
    {
        if(tmp->displayed_dialog == window)
        {
            tmp->displayed_dialog = NULL;
        }
    }

    if(window->clientfd->fd >= 0)
    {
        window->clientfd->clients--;
        /*
        if(--window->clientfd->clients < 0)
        {
            FD_CLR(window->clientfd->fd, &openfds);
            close(window->clientfd->fd);
            window->clientfd->fd = -1;
        }
        */
    }

    free(window);
}


/*
 * We detected an unresponsive window which means the application is very
 * likely dead. This function frees the memory allocated to this window
 * and removes it from screen.
 *
 * TODO: show the user a dialog box saying "unresponsive window" and ask if
 *       they want to kill the app (i.e. handle this properly).
 */
void server_window_dead(struct server_window_t *window)
{
    if(window == root_window)
    {
        __asm__ __volatile__("xchg %%bx, %%bx"::);
    }

    //server_window_destroy(window);
}


void cancel_active_child(struct server_window_t *parent, 
                         struct server_window_t *win)
{
    if(parent->active_child == win)
    {
        parent->active_child = NULL;
    }

    if(parent->focused_child == win)
    {
        parent->focused_child = parent->active_child;
    }

    if(parent->drag_child == win)
    {
        parent->drag_child = NULL;
    }

    if(parent->tracked_child == win)
    {
        parent->tracked_child = NULL;
    }

    if(grabbed_keyboard_window == win)
    {
        grabbed_keyboard_window = NULL;
    }


    if(grabbed_mouse_window == win)
    {
        ungrab_mouse();
    }
}


void may_draw_mouse_cursor(struct server_window_t *win)
{
    int top = win->y;
    int left = win->x;
    int bottom = win->yh1;
    int right = win->xw1;

    if(root_mouse_x <= right &&
       (root_mouse_x + cursor[cur_cursor].w) >= left &&
       root_mouse_y <= bottom &&
       (root_mouse_y + cursor[cur_cursor].h) >= top)
    {

        draw_mouse_cursor(0);
    }
}


void may_change_mouse_cursor(struct server_window_t *win)
{
    int top = win->y;
    int left = win->x;
    int bottom = win->yh1;
    int right = win->xw1;

    if(root_mouse_x <= right &&
       (root_mouse_x + cursor[cur_cursor].w) >= left &&
       root_mouse_y <= bottom &&
       (root_mouse_y + cursor[cur_cursor].h) >= top &&
       win->cursor_id != cur_cursor)
    {
        change_cursor(win->cursor_id);
        force_redraw_cursor(root_mouse_x, root_mouse_y);
    }
}


void server_window_may_hide(struct server_window_t *win)
{
    int redraw = 0;
    
    if(win->state != WINDOW_STATE_MINIMIZED)
    {
        server_window_toggle_minimize(gc, win);
        redraw = 1;

        /*
        if(grabbed_mouse_window == win)
        {
            ungrab_mouse();
        }

        if(grabbed_keyboard_window == win)
        {
            grabbed_keyboard_window = NULL;
        }
        */
    }

    if(redraw)
    {
        int top = win->y;
        int left = win->x;
        int bottom = win->yh1;
        int right = win->xw1;

        if(root_mouse_x <= right &&
           (root_mouse_x + cursor[cur_cursor].w) >= left &&
           root_mouse_y <= bottom &&
           (root_mouse_y + cursor[cur_cursor].h) >= top)
        {
            /*
             * Force showing the new cursor. We call process_mouse() so it
             * can determine the appropriate mouse cursor according to who
             * is under focus now.
             */
            struct mouse_packet_t mouse_packet;
            mouse_packet.dx = 0;
            mouse_packet.dy = 0;
            mouse_packet.buttons = root_button_state.buttons;
            process_mouse(&mouse_packet);
        }
    }
}


void *screen_updater(void *unused)
{
    (void)unused;
    
    while(1)
    {
        uint64_t start = time_in_millis();

        mutex_lock(&update_lock);
        do_screen_update();
        mutex_unlock(&update_lock);

        uint64_t end = time_in_millis();

        static uint64_t needed = 1000 / 100;
        uint64_t elapsed = (end - start);
        
        if(elapsed < needed)
        {
            //__asm__ __volatile__("xchg %%bx, %%bx"::);
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = (needed - elapsed) * 1000;
            select(0, NULL, NULL, NULL, &tv);
        }
    }

    return NULL;
}


// Get window or send error response
#define GET_WINDOW(win, id, type)                       \
    if(!(win = server_window_by_winid(id)))             \
    {                                                   \
        send_err_event(clientfd->fd, ev->src, type, ENOENT, ev->seqid); \
        break;                                          \
    }

// Get window or break (no error response sent)
#define GET_WINDOW_SILENT(win, id)                      \
    if(!(win = server_window_by_winid(id)))             \
    {                                                   \
        break;                                          \
    }


#define MAX_CLIENT_TICKS            5


void process_win_create_request(struct clientfd_t *clientfd, 
                                struct event_t *ev)
{
    struct server_window_t *win, *owner = NULL;
    struct event_t ev2;
    uint32_t evtype;
    int wintype;

    if(ev->type == REQUEST_MENU_FRAME_CREATE)
    {
        evtype = EVENT_MENU_FRAME_CREATED;
        wintype = WINDOW_TYPE_MENU_FRAME;
    }
    else if(ev->type == REQUEST_DIALOG_CREATE)
    {
        evtype = EVENT_DIALOG_CREATED;
        wintype = WINDOW_TYPE_DIALOG;
    }
    else
    {
        evtype = EVENT_WINDOW_CREATED;
        wintype = WINDOW_TYPE_WINDOW;
    }

    // make sure no window exists with this id
    if((win = server_window_by_winid(ev->src)))
    {
        send_err_event(clientfd->fd, ev->src, evtype, EEXIST, ev->seqid);
        return;
    }

    if(ev->type == REQUEST_MENU_FRAME_CREATE ||
       ev->type == REQUEST_DIALOG_CREATE)
    {
        // get the owner window
        if(!(owner = server_window_by_winid(ev->win.owner)))
        {
            send_err_event(clientfd->fd, ev->src, evtype, ENOENT, ev->seqid);
            return;
        }
    }

    if(ev->type == REQUEST_MENU_FRAME_CREATE)
    {
        // set the proper flags for a menu frame
        ev->win.flags |= (WINDOW_NODECORATION | 
                          WINDOW_NOCONTROLBOX |
                          WINDOW_NOICON       | 
                          WINDOW_NORESIZE     | 
                          WINDOW_SKIPTASKBAR);
        ev->win.flags &= ~WINDOW_NOFOCUS;

        // the passed x & y coordinates are relative to the
        // owner window -- fix them now
        ev->win.x += server_window_screen_x(owner);
        ev->win.y += server_window_screen_y(owner);

        if(!(owner->flags & WINDOW_NODECORATION))
        {
            ev->win.x += WINDOW_BORDERWIDTH;
            ev->win.y += WINDOW_TITLEHEIGHT;
        }
    }

    // now create the menu frame
    if((win = server_window_create(ev->win.x, ev->win.y,
                                              ev->win.w, ev->win.h,
                                              ev->win.gravity,
                                              ev->win.flags | WINDOW_HIDDEN,
                                              ev->src)))
    {
        win->type = wintype;
        win->state = WINDOW_STATE_MINIMIZED;
        win->saved.state = WINDOW_STATE_NORMAL;
        win->clientfd = clientfd;
        clientfd->clients++;

        if(owner)
        {
            win->owner_winid = owner->winid;
        }

        ev2.type = evtype;
        ev2.seqid = ev->seqid;
        ev2.win.x = win->x;
        ev2.win.y = win->y;
        ev2.win.w = win->client_w;
        ev2.win.h = win->client_h;
        ev2.win.flags = win->flags;
        ev2.win.shmid = win->shmid;
        ev2.win.canvas_size = win->canvas_size;
        ev2.win.canvas_pitch = win->canvas_pitch;
        ev2.src = TO_WINID(GLOB.mypid, 0);
        ev2.dest = ev->src;
        ev2.valid_reply = 1;
        direct_write(clientfd->fd, (void *)&ev2, sizeof(struct event_t));

        if(ev->type == REQUEST_WINDOW_CREATE)
        {
            // notify parent
            notify_parent_win_created(win);
        }
    }
    else
    {
        send_err_event(clientfd->fd, ev->src, evtype, ENOMEM, ev->seqid);
    }
}


void process_win_icon_request(struct server_window_t *win, struct event_t *ev)
{
    struct resource_t *res;

    if(ev->type == REQUEST_WINDOW_SET_ICON)
    {
        struct event_buf_t *evbuf = (struct event_buf_t *)ev;
        
        // remove the window's icon if buf[0] == '\0'
        if(evbuf->buf[0] == '\0')
        {
            if(win->icon)
            {
                server_resource_free(win->icon);
                win->icon = NULL;
            }
            
            return;
        }

        // load the icon from file
        if(!(res = server_resource_load(evbuf->buf)))
        {
            return;
        }
    }
    else
    {
        struct event_res_t *evres = (struct event_res_t *)ev;
        
        // remove the window's icon if anything is NULL or zero
        if(/* evres->data[0] == '\0' || */ evres->img.w == 0 ||
           evres->img.h == 0 || evres->datasz == 0)
        {
            if(win->icon)
            {
                server_resource_free(win->icon);
                win->icon = NULL;
            }
            
            return;
        }

        // load the icon from memory
        if(!(res = server_load_image_from_memory(evres->img.w, evres->img.h,
                                                 (uint32_t *)evres->data,
                                                 evres->datasz)))
        {
            return;
        }
    }

    if(win->icon)
    {
        server_resource_free(win->icon);
    }

    win->icon = (struct resource_t *)res;

    // make sure the change is reflected on-screen
    if(!(win->flags & (WINDOW_HIDDEN | WINDOW_NODECORATION)))
    {
        server_window_update_title(gc, win);
    }

    if(!win->parent)
    {
        return;
    }

    struct event_res_t evres;

    evres.type = EVENT_CHILD_WINDOW_ICON_SET;
    evres.seqid = 0;
    evres.src = win->winid;
    evres.dest = win->parent->winid;
    evres.valid_reply = 1;
    evres.restype = RESOURCE_TYPE_IMAGE;
    evres.resid = win->icon->resid;

    direct_write(win->parent->clientfd->fd, (void *)&evres, 
                        sizeof(struct event_res_t));
}


void client_disconnected(struct clientfd_t *clientfd)
{
    int res;
    
    res = clientfd->fd;
    FD_CLR(res, &openfds);
    clientfd->fd = -1;
    //clientfd->flags = 0;
    __atomic_store_n(&clientfd->flags, 0, __ATOMIC_SEQ_CST);
    close(res);

    /*
     * TODO: Check if there are windows linked to this fd
     *       and kill them if necessary.
     */
    struct server_window_t *tmp;
    ListNode *current_node;

    if(!root_window || !root_window->children || 
       !root_window->children->root_node)
    {
        return;
    }
    
    for(current_node = root_window->children->root_node;
        current_node != NULL;
        current_node = current_node->next)
    {
        tmp = (struct server_window_t *)current_node->payload;
        
        if(tmp->clientfd != clientfd)
        {
            continue;
        }
        
        server_window_may_hide(tmp);
        server_window_destroy(tmp);
        current_node = root_window->children->root_node;
    }
}


void service_client(struct clientfd_t *clientfd)
{
    ssize_t sz;
    size_t evbufsz = GLOB.evbufsz;
    struct event_t *ev = (struct event_t *)GLOB.evbuf_internal;

    struct mouse_packet_t mouse_packet;
    struct server_window_t *win, *owner;
    struct event_t ev2;
    struct resource_t *res;

try:

        if((sz = direct_read(clientfd->fd, ev, evbufsz)) < 0)
        {
            if(errno == ENOTCONN || errno == ECONNREFUSED || errno == EINVAL)
            {
                // client disconnected
                __atomic_store_n(&clientfd->flags, 1, __ATOMIC_SEQ_CST);
            }
            else if(errno == EMSGSIZE /* ENOBUFS */)
            {
                // check for 'small buffer' errors
                evbufsz = GLOB.evbufsz * 2;

                if(!(ev = realloc((void *)GLOB.evbuf_internal, evbufsz)))
                {
                    __set_errno(ENOMEM);
                    return;
                }

                GLOB.evbuf_internal = ev;
                GLOB.evbufsz = evbufsz;
                goto try;
            }
            
            return;
        }

        if(!ev->type)
        {
            return;
        }
        
        switch(ev->type)
        {
            case REQUEST_MENU_FRAME_CREATE:
                process_win_create_request(clientfd, ev);
                break;
            
            case REQUEST_MENU_FRAME_SHOW:
                GET_WINDOW_SILENT(win, ev->src);
                GET_WINDOW_SILENT(owner, win->owner_winid);

                if(win->state != WINDOW_STATE_MINIMIZED)
                {
                    break;
                }

                if(owner->displayed_dialog &&
                   owner->displayed_dialog->type == WINDOW_TYPE_DIALOG)
                {
                    break;
                }

                server_window_toggle_minimize(gc, win);
                may_draw_mouse_cursor(win);
                break;
            
            case REQUEST_MENU_FRAME_HIDE:
                GET_WINDOW_SILENT(win, ev->src);
                server_window_may_hide(win);
                break;

            case REQUEST_DIALOG_CREATE:
                process_win_create_request(clientfd, ev);
                break;

            case REQUEST_DIALOG_SHOW:
                GET_WINDOW_SILENT(win, ev->src);
                GET_WINDOW_SILENT(owner, win->owner_winid);

                if(win->state != WINDOW_STATE_MINIMIZED)
                {
                    break;
                }
                
                if(owner->displayed_dialog)
                {
                    break;
                }

                server_window_toggle_minimize(gc, win);
                may_draw_mouse_cursor(win);
                owner->displayed_dialog = win;
                    
                break;

            case REQUEST_DIALOG_HIDE:
                GET_WINDOW_SILENT(win, ev->src);
                GET_WINDOW_SILENT(owner, win->owner_winid);
                
                server_window_may_hide(win);
                owner->displayed_dialog = NULL;
                break;

            case REQUEST_WINDOW_CREATE:
                process_win_create_request(clientfd, ev);
                break;

            case REQUEST_WINDOW_DESTROY:
                GET_WINDOW_SILENT(win, ev->src);
                server_window_may_hide(win);
                server_window_destroy(win);
                break;
            
            case REQUEST_WINDOW_SET_TITLE:
                GET_WINDOW_SILENT(win, ev->src);

                server_window_set_title(gc, win,
                                  (char *)((struct event_buf_t *)ev)->buf,
                                  ((struct event_buf_t *)ev)->bufsz);

                // notify parent
                if(!win->parent)
                {
                    break;
                }

                notify_win_title_event(win->parent->clientfd->fd, win->title,
                                       win->parent->winid, win->winid);

                break;
            
            case REQUEST_WINDOW_SET_ICON:
                GET_WINDOW_SILENT(win, ev->src);
                process_win_icon_request(win, ev);
                break;

            case REQUEST_WINDOW_LOAD_ICON:
                GET_WINDOW_SILENT(win, ev->src);
                process_win_icon_request(win, ev);
                break;
            
            case REQUEST_WINDOW_GET_ICON:
                GET_WINDOW(win, ev->src, EVENT_RESOURCE_LOADED);

                {
                    struct event_res_t *evres = (struct event_res_t *)ev;

                    if(!win->icon)
                    {
                        send_err_event(clientfd->fd, ev->src,
                                       EVENT_RESOURCE_LOADED, 
                                       ENOENT, ev->seqid);
                        break;
                    }

                    send_res_load_event(clientfd->fd, evres, win->icon);
                }

                break;
            
            case REQUEST_WINDOW_SHOW:
                GET_WINDOW_SILENT(win, ev->src);

                if(win->state == WINDOW_STATE_MINIMIZED)
                {
                    server_window_toggle_minimize(gc, win);
                    may_draw_mouse_cursor(win);
                    break;
                }

                break;

            case REQUEST_WINDOW_HIDE:
                GET_WINDOW_SILENT(win, ev->src);
                server_window_may_hide(win);
                break;

            case REQUEST_WINDOW_RAISE:
                GET_WINDOW_SILENT(win, ev->src);

                if(win->state == WINDOW_STATE_MINIMIZED)
                {
                    server_window_toggle_minimize(gc, win);
                }
                else
                {
                    server_window_raise(gc, win, 1);
                }

                break;

            case REQUEST_WINDOW_SET_POS:
                GET_WINDOW_SILENT(win, ev->src);
                
                if(win->type == WINDOW_TYPE_MENU_FRAME)
                {
                    // the passed x & y coordinates are relative to the
                    // owner window -- fix them now
                    GET_WINDOW_SILENT(owner, win->owner_winid);
                    ev->win.x += server_window_screen_x(owner);
                    ev->win.y += server_window_screen_y(owner);

                    if(!(owner->flags & WINDOW_NODECORATION))
                    {
                        ev->win.x += WINDOW_BORDERWIDTH;
                        ev->win.y += WINDOW_TITLEHEIGHT;
                    }
                }

                // if the window is hidden, just set the new position
                if((win->flags & WINDOW_HIDDEN))
                {
                    if(ev->win.y < desktop_bounds.top || ev->win.x < 0)
                    {
                        break;
                    }

                    server_window_set_size(win, ev->win.x, ev->win.y,
                                                win->client_w, win->client_h);
                }
                // otherwise, reposition and draw
                else
                {
                    server_window_move(gc, win, ev->win.x, ev->win.y);
                }

                break;

            case REQUEST_WINDOW_SET_MIN_SIZE:
                GET_WINDOW_SILENT(win, ev->src);

                if(ev->win.w > WINDOW_MIN_WIDTH)
                {
                    win->minw = ev->win.w;
                }

                if(ev->win.h > WINDOW_MIN_HEIGHT)
                {
                    win->minh = ev->win.h;
                }

                break;

            /*
             * Window resize happens in steps:
             * - The client sends a resize request, or the user drags
             *   the window's border to resize it.
             * - The server sends a resize offer event with the new 
             *   window size.
             * - The client sends a resize offer acceptance request.
             * - We create the new canvas and send a resize confirmation 
             *   event.
             * - The client sends a resize finalize request.
             * - The window is now resized.
             */

            case REQUEST_WINDOW_RESIZE:
                GET_WINDOW(win, ev->src, EVENT_WINDOW_RESIZE_OFFER);

                if(win->type == WINDOW_TYPE_MENU_FRAME)
                {
                    // the passed x & y coordinates are relative to the
                    // owner window -- fix them now
                    GET_WINDOW_SILENT(owner, win->owner_winid);
                    ev->win.x += server_window_screen_x(owner);
                    ev->win.y += server_window_screen_y(owner);

                    if(!(owner->flags & WINDOW_NODECORATION))
                    {
                        ev->win.x += WINDOW_BORDERWIDTH;
                        ev->win.y += WINDOW_TITLEHEIGHT;
                    }
                }

                server_window_resize_absolute(gc, win,
                                              ev->win.x, ev->win.y,
                                              ev->win.w, ev->win.h, ev->seqid);
                break;

            case REQUEST_WINDOW_RESIZE_ACCEPT:
                GET_WINDOW(win, ev->src, EVENT_WINDOW_RESIZE_CONFIRM);

                server_window_resize_accept(gc, win, ev->win.x, ev->win.y,
                                                     ev->win.w, ev->win.h, 
                                                     ev->seqid);
                break;
            
            case REQUEST_WINDOW_RESIZE_FINALIZE:
                GET_WINDOW_SILENT(win, ev->src);

                server_window_resize_finalize(gc, win);
                
                if(!(win->flags & WINDOW_HIDDEN))
                {
                    server_window_paint(gc, win, NULL,
                                        FLAG_PAINT_CHILDREN | 
                                        FLAG_PAINT_BORDER);
                    invalidate_window(win);
                    
                    draw_mouse_cursor(1);
                    
                    win->pending_resize = 0;
                        
                    if(win->pending_w || win->pending_h)
                    {
                        server_window_resize_absolute(gc, win,
                                                      win->pending_x,
                                                      win->pending_y,
                                                      win->pending_w,
                                                      win->pending_h, 0);
                    }
                }

                break;
            
            case REQUEST_WINDOW_INVALIDATE:
                if((win = server_window_by_winid(ev->src)))
                {
                    if(!(win->flags & WINDOW_HIDDEN))
                    {
                        int top = win->client_y + ev->rect.top;
                        int left = win->client_x + ev->rect.left;
                        int bottom = win->client_y + ev->rect.bottom;
                        int right = win->client_x + ev->rect.right;
                        
                        server_window_invalidate(gc, win,
                                            ev->rect.top, ev->rect.left,
                                            ev->rect.bottom, ev->rect.right);

                        may_draw_mouse_cursor(win);

                        invalidate_screen_rect(top, left, bottom, right);
                    }
                }

                break;

            case REQUEST_WINDOW_TOGGLE_STATE:
                GET_WINDOW_SILENT(win, ev->src);

                if(win->parent->active_child == win)
                {
                    server_window_toggle_minimize(gc, win);
                }
                else if(win->state == WINDOW_STATE_MINIMIZED)
                {
                    server_window_toggle_minimize(gc, win);
                }
                else
                {
                    server_window_raise(gc, win, 1);
                }

                break;

            case REQUEST_WINDOW_MAXIMIZE:
                GET_WINDOW(win, ev->src, EVENT_WINDOW_RESIZE_OFFER);

                // if window is already maximized, do nothing
                if(win->state == WINDOW_STATE_MAXIMIZED)
                {
                    send_resize_offer(win, win->x, win->y,
                                      win->client_w, win->client_h, 
                                      ev->seqid);
                    break;
                }

                // if window is hidden, show it first
                if(win->state == WINDOW_STATE_MINIMIZED)
                {
                    server_window_toggle_minimize(gc, win);
                }
                
                // lastly, maximize
                server_window_toggle_maximize(gc, win);

                break;

            case REQUEST_WINDOW_MINIMIZE:
                GET_WINDOW_SILENT(win, ev->src);

                // if window is already minimized, do nothing
                if(win->state == WINDOW_STATE_MINIMIZED)
                {
                    break;
                }

                // otherwise, minimize it
                server_window_toggle_minimize(gc, win);

                break;

            case REQUEST_WINDOW_RESTORE:
                GET_WINDOW_SILENT(win, ev->src);

                // if window is not minimized, do nothing
                if(win->state != WINDOW_STATE_MINIMIZED)
                {
                    break;
                }

                // otherwise, restore it
                server_window_toggle_minimize(gc, win);

                break;

            case REQUEST_WINDOW_ENTER_FULLSCREEN:
                GET_WINDOW_SILENT(win, ev->src);

                // if window is already fullscreen, do nothing
                if(win->state == WINDOW_STATE_FULLSCREEN)
                {
                    break;
                }
                
                // make sure it has the right flags
                if(win->flags & (WINDOW_NOFOCUS | 
                                 WINDOW_NORAISE | 
                                 WINDOW_NORESIZE))
                {
                    break;
                }

                // if window is hidden, show it first
                if(win->state == WINDOW_STATE_MINIMIZED)
                {
                    server_window_toggle_minimize(gc, win);
                }

                // lastly, make it fullscreen
                server_window_toggle_fullscreen(gc, win);

                //grab_mouse(win);
                break;

            case REQUEST_WINDOW_EXIT_FULLSCREEN:
                GET_WINDOW_SILENT(win, ev->src);

                // if window is not fullscreen, do nothing
                if(win->state != WINDOW_STATE_FULLSCREEN)
                {
                    break;
                }

                // if window is hidden, show it first
                if(win->state == WINDOW_STATE_MINIMIZED)
                {
                    server_window_toggle_minimize(gc, win);
                }

                // lastly, exit fullscreen
                server_window_toggle_fullscreen(gc, win);

                //ungrab_mouse();
                break;

            case REQUEST_WINDOW_SET_ATTRIBS:
                GET_WINDOW_SILENT(win, ev->winattr.winid);
                
                int flags = win->flags;
                
                // for now, only accept flags changes
                if((ev2.winattr.flags & WINDOW_NODECORATION) ||
                   (ev2.winattr.flags & WINDOW_NOCONTROLBOX))
                {
                    flags |= (WINDOW_NODECORATION | WINDOW_NOCONTROLBOX);
                }
                else
                {
                    flags &= ~(WINDOW_NODECORATION | WINDOW_NOCONTROLBOX);
                }

                if((ev2.winattr.flags & WINDOW_NORESIZE))
                {
                    flags |= (WINDOW_NORESIZE);
                }
                else
                {
                    flags &= ~(WINDOW_NORESIZE);
                }
                
                if(flags != win->flags)
                {
                    server_window_set_size(win, win->x, win->y,
                                                win->client_w, win->client_h);

                    server_window_paint(gc, win, (RectList *)0,
                                        FLAG_PAINT_CHILDREN | 
                                        FLAG_PAINT_BORDER);
                }

                break;
            
            case REQUEST_WINDOW_GET_ATTRIBS:
                GET_WINDOW(win, ev->winattr.winid, EVENT_WINDOW_ATTRIBS);
                ev2.type = EVENT_WINDOW_ATTRIBS;
                ev2.seqid = ev->seqid;
                ev2.winattr.x = win->x;
                ev2.winattr.y = win->y;
                ev2.winattr.w = win->client_w;
                ev2.winattr.h = win->client_h;
                ev2.winattr.flags = win->flags;
                ev2.src = TO_WINID(GLOB.mypid, 0);
                ev2.dest = ev->src;
                ev2.valid_reply = 1;
                direct_write(clientfd->fd, (void *)&ev2, sizeof(struct event_t));
                break;

            case REQUEST_WINDOW_GET_STATE:
                GET_WINDOW(win, ev->src, EVENT_WINDOW_STATE);
                ev2.type = EVENT_WINDOW_STATE;
                ev2.seqid = ev->seqid;
                ev2.src = TO_WINID(GLOB.mypid, 0);
                ev2.dest = ev->src;
                ev2.winst.state = win->state;
                ev2.valid_reply = 1;
                direct_write(clientfd->fd, (void *)&ev2, sizeof(struct event_t));
                break;

            case REQUEST_WINDOW_DESTROY_CANVAS:
                GET_WINDOW_SILENT(win, ev->src);

                shmctl(win->shmid, IPC_RMID, NULL);
                shmdt(win->canvas);
                win->shmid = 0;
                win->canvas = NULL;
                break;

            case REQUEST_WINDOW_NEW_CANVAS:
                GET_WINDOW(win, ev->src, EVENT_WINDOW_NEW_CANVAS);
                
                server_window_create_canvas(gc, win);
                
                if(win->shmid)
                {
                    send_canvas_event(win, ev->seqid);
                }
                else
                {
                    send_err_event(clientfd->fd, ev->src,
                                   EVENT_WINDOW_NEW_CANVAS, ENOMEM, ev->seqid);
                }
                
                break;

            case REQUEST_GET_ROOT_WINID:
                if(!root_window)
                {
                    send_err_event(clientfd->fd, ev->src,
                                   EVENT_ROOT_WINID, EINVAL, ev->seqid);
                    break;
                }

                ev2.type = EVENT_ROOT_WINID;
                ev2.seqid = ev->seqid;
                ev2.winattr.winid = root_window->winid;
                ev2.src = TO_WINID(GLOB.mypid, 0);
                ev2.dest = ev->src;
                ev2.valid_reply = 1;
                direct_write(clientfd->fd, (void *)&ev2, sizeof(struct event_t));
                break;

            case REQUEST_GRAB_MOUSE:
            case REQUEST_GRAB_AND_CONFINE_MOUSE:
                GET_WINDOW(win, ev->src, EVENT_MOUSE_GRABBED);

                if((win->flags & WINDOW_HIDDEN))
                {
                    send_err_event(clientfd->fd, ev->src,
                                   EVENT_MOUSE_GRABBED, EINVAL, ev->seqid);
                    break;
                }

                grab_mouse(win, (ev->type == REQUEST_GRAB_AND_CONFINE_MOUSE));
                notify_mouse_grab(win, 1, ev->seqid);
                break;

            case REQUEST_UNGRAB_MOUSE:
                GET_WINDOW_SILENT(win, ev->src);
                
                if(grabbed_mouse_window == win)
                {
                    ungrab_mouse();
                }

                break;

            case REQUEST_CURSOR_LOAD:
                {
                    curid_t curid;
                    struct event_cur_t *evcur = (struct event_cur_t *)ev;
                    
                    curid = server_cursor_load(gc, evcur->w, evcur->h,
                                                   evcur->hotx, evcur->hoty,
                                                   evcur->data);

                    ev2.type = EVENT_CURSOR_LOADED;
                    ev2.seqid = ev->seqid;
                    ev2.cur.curid = curid;
                    ev2.src = TO_WINID(GLOB.mypid, 0);
                    ev2.dest = ev->src;
                    ev2.valid_reply = 1;
                    direct_write(clientfd->fd, (void *)&ev2, sizeof(struct event_t));
                }

                break;

            case REQUEST_CURSOR_FREE:
                server_cursor_free(ev->cur.curid);
                break;

            case REQUEST_CURSOR_SHOW:
                GET_WINDOW_SILENT(win, ev->src);

                if(ev->cur.curid == 0)
                {
                    ev->cur.curid = 1;
                }

                // check the cursor id is valid
                if(ev->cur.curid >= CURSOR_COUNT || 
                   cursor[ev->cur.curid].data == NULL)
                {
                    break;
                }

                win->cursor_id = ev->cur.curid;
                may_change_mouse_cursor(win);
                break;

            case REQUEST_CURSOR_HIDE:
                GET_WINDOW_SILENT(win, ev->src);

                win->cursor_id = 0;
                may_change_mouse_cursor(win);
                break;

            case REQUEST_CURSOR_SET_POS:
                mouse_packet.dx = ev->cur.x - root_mouse_x;
                mouse_packet.dy = ev->cur.y - root_mouse_y;
                mouse_packet.buttons = root_button_state.buttons;

                process_mouse(&mouse_packet);

                break;

            case REQUEST_CURSOR_GET_INFO:
                ev2.type = EVENT_CURSOR_INFO;
                ev2.seqid = ev->seqid;
                ev2.cur.curid = cur_cursor;
                ev2.cur.x = root_mouse_x;
                ev2.cur.y = root_mouse_y;
                ev2.cur.buttons = root_button_state.buttons;
                ev2.src = TO_WINID(GLOB.mypid, 0);
                ev2.dest = ev->src;
                ev2.valid_reply = 1;
                direct_write(clientfd->fd, (void *)&ev2, sizeof(struct event_t));
                break;

            case REQUEST_GRAB_KEYBOARD:
                GET_WINDOW(win, ev->src, EVENT_KEYBOARD_GRABBED);

                if((win->flags & WINDOW_HIDDEN))
                {
                    send_err_event(clientfd->fd, ev->src,
                                   EVENT_KEYBOARD_GRABBED, EINVAL, ev->seqid);
                    break;
                }
                
                grabbed_keyboard_window = win;
                notify_keyboard_grab(win, 1, ev->seqid);
                break;

            case REQUEST_UNGRAB_KEYBOARD:
                GET_WINDOW_SILENT(win, ev->src);
                
                if(grabbed_keyboard_window == win)
                {
                    grabbed_keyboard_window = NULL;
                }

                break;
            
            case REQUEST_GET_INPUT_FOCUS:
                if(grabbed_keyboard_window)
                {
                    win = grabbed_keyboard_window;
                }
                else if(root_window && root_window->focused_child)
                {
                    win = root_window->focused_child;
                }
                else
                {
                    send_err_event(clientfd->fd, ev->src,
                                   EVENT_WINDOW_ATTRIBS, EINVAL, ev->seqid);
                    break;
                }

                ev2.type = EVENT_WINDOW_ATTRIBS;
                ev2.seqid = ev->seqid;
                ev2.winattr.x = win->x;
                ev2.winattr.y = win->y;
                ev2.winattr.w = win->client_w;
                ev2.winattr.h = win->client_h;
                ev2.winattr.flags = win->flags;
                ev2.winattr.winid = win->winid;
                ev2.src = TO_WINID(GLOB.mypid, 0);
                ev2.dest = ev->src;
                ev2.valid_reply = 1;
                direct_write(clientfd->fd, (void *)&ev2, sizeof(struct event_t));
                break;

            case REQUEST_SCREEN_INFO:
                ev2.type = EVENT_SCREEN_INFO;
                ev2.seqid = ev->seqid;
                ev2.screen.rgb_mode = !!(vbe_framebuffer.type);
                ev2.screen.w = vbe_framebuffer.width;
                ev2.screen.h = vbe_framebuffer.height;
                ev2.screen.pixel_width = vbe_framebuffer.pixel_width;
                ev2.screen.red_pos = 
                        vbe_framebuffer.color_info.rgb.red_pos;
                ev2.screen.green_pos = 
                        vbe_framebuffer.color_info.rgb.green_pos;
                ev2.screen.blue_pos = 
                        vbe_framebuffer.color_info.rgb.blue_pos;
                ev2.screen.red_mask_size = 
                        vbe_framebuffer.color_info.rgb.red_mask_size;
                ev2.screen.green_mask_size = 
                        vbe_framebuffer.color_info.rgb.green_mask_size;
                ev2.screen.blue_mask_size = 
                        vbe_framebuffer.color_info.rgb.blue_mask_size;
                ev2.src = TO_WINID(GLOB.mypid, 0);
                ev2.dest = ev->src;
                ev2.valid_reply = 1;
                direct_write(clientfd->fd, (void *)&ev2, sizeof(struct event_t));
                break;

            case REQUEST_COLOR_PALETTE:
                {
                    if(vbe_framebuffer.type != 0)   // not palette-indexed
                    {
                        send_err_event(clientfd->fd, ev->src,
                                       EVENT_COLOR_PALETTE_DATA, EINVAL,
                                       ev->seqid);
                        break;
                    }

                    size_t datasz = GLOB.screen.color_count *
                                        sizeof(struct rgba_color_t);
                    size_t bufsz = sizeof(struct event_res_t) + datasz;
                    struct event_res_t *evbuf = malloc(bufsz + 1);

                    if(!evbuf)
                    {
                        send_err_event(clientfd->fd, ev->src,
                                       EVENT_COLOR_PALETTE_DATA, EINVAL,
                                       ev->seqid);
                        break;
                    }

                    A_memset((void *)evbuf, 0, bufsz);
                    A_memcpy((void *)evbuf->data, GLOB.screen.palette, datasz);

                    evbuf->type = EVENT_COLOR_PALETTE_DATA;
                    evbuf->seqid = ev->seqid;
                    evbuf->datasz = datasz;
                    evbuf->src = TO_WINID(GLOB.mypid, 0);
                    evbuf->dest = ev->src;
                    evbuf->valid_reply = 1;
                    evbuf->palette.color_count = GLOB.screen.color_count;
                    direct_write(clientfd->fd, (void *)evbuf, bufsz);

                    free((void *)evbuf);
                }

                break;

            case REQUEST_COLOR_THEME_GET:
                send_theme_data(ev->src, ev->seqid, clientfd->fd);
                break;

            case REQUEST_COLOR_THEME_SET:
                {
                    // Get the new theme
                    struct event_res_t *evbuf = (struct event_res_t *)ev;
                    uint8_t count = evbuf->palette.color_count;

                    if(count == 0)
                    {
                        break;
                    }

                    if(count > THEME_COLOR_LAST)
                    {
                        count = THEME_COLOR_LAST;
                    }

                    A_memcpy(GLOB.themecolor, evbuf->data, count * sizeof(uint32_t));

                    // Now broadcast it to all apps
                    broadcast_new_theme();
                }

                break;

            case REQUEST_BIND_KEY:
                server_key_bind(ev->keybind.key, ev->keybind.modifiers,
                                ev->keybind.action, ev->src);
                break;

            case REQUEST_UNBIND_KEY:
                server_key_unbind(ev->keybind.key, ev->keybind.modifiers, ev->src);
                break;

            case REQUEST_SET_DESKTOP_BOUNDS:
                if(ev->rect.top >= 0 &&
                   ev->rect.left >= 0 &&
                   ev->rect.bottom < GLOB.screen.h &&
                   ev->rect.right < GLOB.screen.w)
                {
                    desktop_bounds.top = ev->rect.top;
                    desktop_bounds.left = ev->rect.left;
                    desktop_bounds.bottom = ev->rect.bottom;
                    desktop_bounds.right = ev->rect.right;
                    //ungrab_mouse();
                }

                break;
            
            case REQUEST_GET_MODIFIER_KEYS:
                ev2.type = EVENT_MODIFIER_KEYS;
                ev2.seqid = ev->seqid;
                ev2.key.modifiers = modifiers;
                ev2.key.code = 0;
                ev2.src = TO_WINID(GLOB.mypid, 0);
                ev2.dest = ev->src;
                ev2.valid_reply = 1;
                direct_write(clientfd->fd, (void *)&ev2, sizeof(struct event_t));
                break;

            case REQUEST_GET_KEYS_STATE:
                {
                    char buf[32];
                    
                    key_state_bitmap(buf);
                    ev2.type = EVENT_KEYS_STATE;
                    ev2.seqid = ev->seqid;
                    memcpy(&ev2.keybmp.bits, buf, 32);
                    ev2.src = TO_WINID(GLOB.mypid, 0);
                    ev2.dest = ev->src;
                    ev2.valid_reply = 1;
                    direct_write(clientfd->fd, (void *)&ev2, sizeof(struct event_t));
                }

                break;

            case REQUEST_RESOURCE_LOAD:
                {
                    struct event_res_t *evres = (struct event_res_t *)ev;

                    if(!(res = server_resource_load(evres->data)))
                    {
                        send_err_event(clientfd->fd, ev->src,
                                       EVENT_RESOURCE_LOADED, EINVAL, ev->seqid);
                        break;
                    }

                    send_res_load_event(clientfd->fd, evres, res);
                }

                break;
            
            case REQUEST_RESOURCE_GET:
                {
                    struct event_res_t *evres = (struct event_res_t *)ev;

                    if(!(res = server_resource_get(evres->resid)))
                    {
                        send_err_event(clientfd->fd, ev->src,
                                       EVENT_RESOURCE_LOADED, EINVAL, ev->seqid);
                        break;
                    }

                    send_res_load_event(clientfd->fd, evres, res);
                }

                break;

            case REQUEST_RESOURCE_UNLOAD:
                server_resource_unload(((struct event_res_t *)ev)->resid);
                break;

            /*
            case REQUEST_LOAD_ICON:
                if(!(res = resource_load((char *)((struct event_buf_t *)ev)->buf)))
                {
                    send_err_event(ev->src, REQUEST_LOAD_ICON, ENOENT);
                    break;
                }
            */
            
            case REQUEST_CLIPBOARD_SET:
                {
                    struct event_res_t *evres = (struct event_res_t *)ev;
                    size_t bytes = evres->datasz;
                    
                    if(bytes == 0 || server_clipboard_set(evres) != bytes)
                    {
                        send_err_event(clientfd->fd, ev->src,
                                       EVENT_CLIPBOARD_SET, EINVAL, ev->seqid);
                        break;
                    }

                    ev2.clipboard.sz = bytes;
                    ev2.clipboard.fmt = evres->clipboard.fmt;
                    ev2.type = EVENT_CLIPBOARD_SET;
                    ev2.seqid = ev->seqid;
                    ev2.src = TO_WINID(GLOB.mypid, 0);
                    ev2.dest = ev->src;
                    ev2.valid_reply = 1;
                    direct_write(clientfd->fd, (void *)&ev2, sizeof(struct event_t));
                }

                break;
            
            case REQUEST_CLIPBOARD_GET:
                {
                    void *data;
                    size_t datasz;
                    
                    data = server_clipboard_get(ev->clipboard.fmt, &datasz);

                    size_t bufsz = sizeof(struct event_res_t) + datasz;
                    struct event_res_t *evbuf = malloc(bufsz + 1);

                    if(!evbuf)
                    {
                        send_err_event(clientfd->fd, ev->src,
                                       EVENT_CLIPBOARD_DATA, ENOMEM, ev->seqid);
                        break;
                    }

                    A_memset((void *)evbuf, 0, bufsz);
                    
                    if(data)
                    {
                        A_memcpy((void *)evbuf->data, data, datasz);
                    }
                    
                    evbuf->type = EVENT_CLIPBOARD_DATA;
                    evbuf->seqid = ev->seqid;
                    evbuf->datasz = datasz;
                    evbuf->src = TO_WINID(GLOB.mypid, 0);
                    evbuf->dest = ev->src;
                    evbuf->valid_reply = 1;
                    evbuf->clipboard.fmt = ev->clipboard.fmt;
                    direct_write(clientfd->fd, (void *)evbuf, bufsz);

                    free((void *)evbuf);
                }

                break;
            
            case REQUEST_CLIPBOARD_QUERY:
                ev2.clipboard.sz = server_clipboard_query_size(ev->clipboard.fmt);
                ev2.clipboard.fmt = ev->clipboard.fmt;
                ev2.type = EVENT_CLIPBOARD_HAS_DATA;
                ev2.seqid = ev->seqid;
                ev2.src = TO_WINID(GLOB.mypid, 0);
                ev2.dest = ev->src;
                ev2.valid_reply = 1;
                direct_write(clientfd->fd, (void *)&ev2, sizeof(struct event_t));
                break;

            /*
             * Event forwarding.
             *
             * Clients cannot talk to each other, as they only have open sockets
             * with the server. We just grab the destination client's socket and
             * forward the message to them.
             */
            
            case EVENT_CHILD_WINDOW_CREATED:
            case EVENT_CHILD_WINDOW_SHOWN:
            case EVENT_CHILD_WINDOW_HIDDEN:
            case EVENT_CHILD_WINDOW_RAISED:
            case EVENT_CHILD_WINDOW_DESTROYED:
            case EVENT_CHILD_WINDOW_TITLE_SET:
            case EVENT_MENU_SELECTED:
            case EVENT_KEY_PRESS:
                GET_WINDOW_SILENT(win, ev->dest);
                direct_write(win->clientfd->fd, (void *)ev, sz);
                break;

            /*
             * Same for application-private requests and events. Just forward.
             */

            default:
                if(ev->type >= REQUEST_APPLICATION_PRIVATE)
                {
                    GET_WINDOW_SILENT(win, ev->dest);
                    direct_write(win->clientfd->fd, (void *)ev, sz);
                }
                break;
        }
}


void *conn_listener(void *_server_sockfd)
{
    int server_sockfd = (int)(uintptr_t)_server_sockfd;
    
    while(1)
    {
        int client;
        struct sockaddr_storage addr;
        socklen_t addrlen = sizeof(struct sockaddr_storage);
        
        if((client = accept(server_sockfd, 
                            (struct sockaddr *)&addr, &addrlen)) >= 0)
        {
            clientfds[client].fd = client;
            clientfds[client].clients = 0;
            FD_SET(client, &openfds);
            
            if(client > maxopenfd)
            {
                maxopenfd = client;
            }
        }
    }

    return NULL;
}



void *conn_alive_checker(void *unused)
{
    UNUSED(unused);

    int i;
    char c;
    int res;
    
    while(1)
    {
        sleep(1);
        
        for(i = 0; i < NR_OPEN; i++)
        {
            if(clientfds[i].fd > 0)
            {
                if((res = recv(clientfds[i].fd, &c, 1, MSG_DONTWAIT|MSG_PEEK)) < 0)
                {
                    if(errno == ENOTCONN || 
                       errno == ECONNRESET ||
                       errno == ECONNREFUSED ||
                       errno == EADDRNOTAVAIL ||
                       errno == EINVAL)
                    {
                        // client disconnected
                        //__asm__ __volatile__("xchg %%bx, %%bx"::);
                        //client_disconnected(&clientfds[i]);
                        __atomic_store_n(&clientfds[i].flags, 1, __ATOMIC_SEQ_CST);
                    }
                }
            }
        }
    }
}


static struct termios orig_termios;

void tty_reset(void)
{
    tcsetattr(0, TCSAFLUSH, &orig_termios);
    ioctl(0, VT_RAW_INPUT, 0);
    ioctl(0, VT_GRAPHICS_MODE, 0);

    // show the tty's cursor
    ioctl(GLOB.fbfd, FB_SET_CURSOR, 1);
    
    // enable automatic screen update
    ioctl(GLOB.fbfd, FB_INVALIDATE_SCREEN, 1);
}

void tty_atexit(void)
{
    tty_reset();
}

void tty_raw(char *myname)
{
    struct termios raw;

    raw = orig_termios;

    // input modes - no break, no CR to NL, 
    // no parity check, no strip char, no start/stop output control
    raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | INLCR | IGNCR | ICRNL |
                     INPCK | ISTRIP | IXON);

    // output modes - no post processing such as NL to CR+NL
    raw.c_oflag &= ~(OPOST);

    // control modes - set 8 bit chars
    raw.c_cflag &= ~(CSIZE | PARENB);
    raw.c_cflag |= (CS8);

    // local modes - echo off, canonical off (no erase with 
    // backspace, ^U,...),  no extended functions, no signal chars (^Z,^C)
    raw.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

    // control chars - min number of bytes and timer
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0;
    //raw.c_cc[VMIN] = 5; raw.c_cc[VTIME] = 8; /* after 5 bytes or .8 seconds
    //                                            after first byte seen      */
    //raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0; /* immediate - anything       */
    //raw.c_cc[VMIN] = 2; raw.c_cc[VTIME] = 0; /* after two bytes, no timer  */
    //raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 8; /* after a byte or .8 seconds */

    // put terminal in raw mode
    if(tcsetattr(0, TCSAFLUSH, &raw) < 0)
    {
        fprintf(stderr, "%s: cannot set tty raw mode: %s\n",
                        myname, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    ioctl(0, VT_RAW_INPUT, 1);
    ioctl(0, VT_GRAPHICS_MODE, 1);
}



#define open_or_die(fd, file, mode)                     \
    if((fd = open(file, mode)) < 0)                     \
    {                                                   \
        fprintf(stderr, "%s: failed to open '%s': %s\n",\
                        argv[0], file, strerror(errno));\
        exit(EXIT_FAILURE);                             \
    }

#define SET_SIGACTION(signum, handler)      \
    act.sa_handler = handler;               \
    (void)sigaction(signum, &act, NULL);


int main(int argc, char **argv)
{
    uintptr_t backbuf_addr;
    struct sigaction act;

    memset(&act, 0, sizeof(struct sigaction));
    act.sa_flags = SA_RESTART;

    GLOB.mypid = getpid();

    // now setup our SIGINT and SIGHUP signal handlers, which we use to handle
    // system shutdown and reboot
    SET_SIGACTION(SIGINT, sigint_handler);
    SET_SIGACTION(SIGHUP, sighup_handler);
    SET_SIGACTION(SIGCHLD, sigchld_handler);
    SET_SIGACTION(SIGALRM, sig_handler);
    SET_SIGACTION(SIGPWR, sig_handler);
    SET_SIGACTION(SIGWINCH, sigwinch_handler);
    SET_SIGACTION(SIGUSR1, sig_handler);
    SET_SIGACTION(SIGUSR2, sig_handler);
    SET_SIGACTION(SIGSTOP, sig_handler);
    SET_SIGACTION(SIGTSTP, sig_handler);
    SET_SIGACTION(SIGCONT, sig_handler);
    SET_SIGACTION(SIGQUIT, sig_handler);

    act.sa_sigaction = sigsegv_handler;
    (void)sigaction(SIGSEGV, &act, NULL);

    if(!isatty(0))
    {
        fprintf(stderr, "%s: input file is not a tty\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    dup2(0, 1);
    dup2(0, 2);

    if(tcsetpgrp(0, GLOB.mypid) < 0)
    {
        exit(EXIT_FAILURE);
    }
    
    // store current tty settings
    if(tcgetattr(0, &orig_termios) < 0)
    {
        fprintf(stderr, "%s: cannot get tty attributes: %s\n",
                        argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    atexit(tty_atexit);
    tty_raw(argv[0]);

    GLOB.evbufsz = 0x1000;
    GLOB.evbuf_internal = malloc(GLOB.evbufsz);
    
    open_or_die(GLOB.fbfd, "/dev/fb0", O_RDONLY | O_NOATIME);
    open_or_die(GLOB.mousefd, "/dev/mouse0", O_RDONLY | O_NOATIME /* |O_NOBLOCK */);

    // we have to re-map the screen backbuffer as we unmapped everything in
    // the execve() call!

    if(ioctl(GLOB.fbfd, FB_MAP_VBE_BACKBUF, &backbuf_addr) != 0)
    {
        fprintf(stderr, "%s: failed to map VBE back buffer: %s\n",
                        argv[0], strerror(errno));
        close(GLOB.fbfd);
        exit(EXIT_FAILURE);
    }

    if(ioctl(GLOB.fbfd, FB_GET_VBE_BUF, &vbe_framebuffer) != 0)
    {
        fprintf(stderr, "%s: failed to get VBE info: %s\n",
                        argv[0], strerror(errno));
        close(GLOB.fbfd);
        exit(EXIT_FAILURE);
    }

    vbe_framebuffer.back_buffer = (uint8_t *)backbuf_addr;

    // if this is a palette-indexed mode, get the palette
    if(vbe_framebuffer.type == 0)
    {
        // each color is 4-bytes long
        GLOB.screen.color_count = 
                vbe_framebuffer.color_info.indexed.palette_num_colors;
        GLOB.screen.palette = malloc(GLOB.screen.color_count *
                                        sizeof(struct rgba_color_t));

        if(ioctl(GLOB.fbfd, FB_GET_VBE_PALETTE, GLOB.screen.palette) != 0)
        {
            fprintf(stderr, "%s: failed to get VBE color palette: %s\n",
                            argv[0], strerror(errno));
            close(GLOB.fbfd);
            exit(EXIT_FAILURE);
        }
    }

    // hide the tty's cursor
    //ioctl(GLOB.fbfd, FB_SET_CURSOR, 0);
    write(0, "\033[?25l", 6);
    
    // disable automatic screen update - we will update it when we want to
    ioctl(GLOB.fbfd, FB_INVALIDATE_SCREEN, 0);

    GLOB.screen.w = vbe_framebuffer.width;
    GLOB.screen.h = vbe_framebuffer.height;
    GLOB.screen.pixel_width = vbe_framebuffer.pixel_width;
    GLOB.screen.red_pos = vbe_framebuffer.color_info.rgb.red_pos;
    GLOB.screen.green_pos = vbe_framebuffer.color_info.rgb.green_pos;
    GLOB.screen.blue_pos = vbe_framebuffer.color_info.rgb.blue_pos;
    GLOB.screen.red_mask_size = vbe_framebuffer.color_info.rgb.red_mask_size;
    GLOB.screen.green_mask_size = vbe_framebuffer.color_info.rgb.green_mask_size;
    GLOB.screen.blue_mask_size = vbe_framebuffer.color_info.rgb.blue_mask_size;

    desktop_bounds.top = 0;
    desktop_bounds.left = 0;
    desktop_bounds.bottom = GLOB.screen.h - 1;
    desktop_bounds.right = GLOB.screen.w - 1;

    gc = gc_new(vbe_framebuffer.width, vbe_framebuffer.height,
                vbe_framebuffer.pixel_width, vbe_framebuffer.back_buffer,
                vbe_framebuffer.memsize, vbe_framebuffer.pitch, &GLOB.screen);

    //Do a few initializations first
    server_init_resources();
    server_init_theme();
    
    prep_mouse_cursor(gc);
    prep_window_controlbox();
    prep_rect_cache();
    prep_list_cache();
    prep_listnode_cache();
    
    ungrab_mouse();
    
    gc_set_font(gc, GLOB.sysfont.data ? &GLOB.sysfont : &GLOB.mono);
    
    server_login(argv[0]);

#define DESKTOP_EXE             "/bin/desktop/desktop"

    if(!fork())
    {
        char *argv[] = { DESKTOP_EXE, NULL };

        nice(40);
        close(GLOB.mousefd);
        close(GLOB.fbfd);
        munmap(vbe_framebuffer.back_buffer, vbe_framebuffer.memsize);
        execvp(DESKTOP_EXE, argv);
        exit(EXIT_FAILURE);
    }
    
    pthread_t thread;
    
    if(pthread_create(&thread, NULL, screen_updater, NULL) != 0)
    {
        fprintf(stderr, "%s: failed to create screen updater thread\n", 
                        argv[0]);
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&openfds);
    FD_SET(0, &openfds);
    FD_SET(GLOB.mousefd, &openfds);
    maxopenfd = GLOB.mousefd;
    
    A_memset(clientfds, 0, sizeof(clientfds));
    
    int server_sockfd;
    struct sockaddr_un server_addr;
    
    if((server_sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
    {
        fprintf(stderr, "%s: failed to create socket: %s\n", 
                        argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, "/var/run/guiserver");
    
    if(bind(server_sockfd, (struct sockaddr *)&server_addr, 
            sizeof(struct sockaddr_un)) != 0)
    {
        fprintf(stderr, "%s: failed to bind socket: %s\n", 
                        argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if(listen(server_sockfd, 128) != 0)
    {
        fprintf(stderr, "%s: failed to listen to socket: %s\n", 
                        argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&thread, NULL, conn_listener, 
                      (void *)(uintptr_t)server_sockfd) != 0)
    {
        fprintf(stderr, "%s: failed to create connection listener thread\n",
                        argv[0]);
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&thread, NULL, conn_alive_checker, NULL) != 0)
    {
        fprintf(stderr, "%s: failed to create connection checker thread\n", 
                        argv[0]);
        exit(EXIT_FAILURE);
    }

    while(1)
    {
        fd_set rdfs;
        char key[2];   // key[0] = flags, key[1] = keycode
        int i;
        struct mouse_packet_t mouse_packet;
        struct timeval tv;


        if(received_sigwinch)
        {
            received_sigwinch = 0;

            // hide the tty's cursor
            //ioctl(GLOB.fbfd, FB_SET_CURSOR, 0);
            write(0, "\033[?25l", 6);
    
            // disable automatic screen update - we will update it when we want to
            ioctl(GLOB.fbfd, FB_INVALIDATE_SCREEN, 0);

            /*
             * TODO: Get the new screen size and update our records accordingly.
             *       Also, send a WINCH signal to all windows to inform of the
             *       new screen coordinates.
             */

            // Do a dirty update for the desktop, which will, in turn, do a 
            // dirty update for all affected child windows
            server_window_paint(gc, root_window, NULL /* &dirty_list */, 
                                FLAG_PAINT_CHILDREN | FLAG_PAINT_BORDER);
    
            draw_mouse_cursor(0);

            invalidate_screen_rect(0, 0, GLOB.screen.h - 1, GLOB.screen.w - 1);
        }

        mouse_packet.type = 0;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

#define FDS_BITS_ELEMENTS       _howmany(FD_SETSIZE, _NFDBITS)

        for(i = 0; i < FDS_BITS_ELEMENTS; i++)
        {
            rdfs.__fds_bits[i] = openfds.__fds_bits[i];
        }

#undef FDS_BITS_ELEMENTS

        i = select(maxopenfd + 1, &rdfs, NULL, NULL, &tv);

        if(i <= 0)
        {
            continue;
        }
        
        if(FD_ISSET(0, &rdfs))
        {
            if(direct_read(0, (void *)key, 2) == 2)
            {
                server_process_key(gc, key);
            }
        }

        if(FD_ISSET(GLOB.mousefd, &rdfs))
        {
            if(direct_read(GLOB.mousefd, &mouse_packet,
                    sizeof(struct mouse_packet_t)) ==
                        sizeof(struct mouse_packet_t))
            {
                process_mouse(&mouse_packet);
            }
        }
        
        for(i = 0; i < NR_OPEN; i++)
        {
            if(FD_ISSET(i, &rdfs) && clientfds[i].fd > 0)
            {
                service_client(&clientfds[i]);
            }
            else
            {
                if(__atomic_load_n(&clientfds[i].flags, __ATOMIC_SEQ_CST))
                {
                    client_disconnected(&clientfds[i]);
                }
            }
        }
    }
}

