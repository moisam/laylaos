#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_LAYLAOS

#include <gui/kbd.h>
#include <gui/keys.h>
#include <gui/client/window.h>
#include <gui/event.h>
#include <gui/mouse.h>

#include "../../events/SDL_events_c.h"

#include "SDL_keycode.h"
#include "SDL_laylaosvideo.h"
#include "SDL_laylaoskeyboard.h"

static void
LAYLAOS_ReconcileKeyboardState(_THIS)
{
    char keys[32];
    int keycode;
    char modifiers = get_modifier_keys();

    /* Sync up the keyboard modifier state */
    SDL_ToggleModState(KMOD_CAPS, (modifiers & MODIFIER_MASK_CAPS) != 0);
    SDL_ToggleModState(KMOD_NUM, (modifiers & MODIFIER_MASK_NUM) != 0);
    
    if(!get_keys_state(keys))
    {
        return;
    }

    keycode = 0;
    
    while(keycode < 256)
    {
        if(LaylaOS_Keycodes[keycode])
        {
            if(keys[keycode / 8] & (1 << (keycode % 8)))
            {
                SDL_SendKeyboardKey(SDL_PRESSED, LaylaOS_Keycodes[keycode]);
            }
            else
            {
                SDL_SendKeyboardKey(SDL_RELEASED, LaylaOS_Keycodes[keycode]);
            }
        }
        
        keycode++;
    }
}


#if 0

void LAYLAOS_CheckWindowState(_THIS, SDL_WindowData *data, uint32_t new_state, int was_fullscreen)
{
    /*
    uint32_t old_state = data->xwindow->state;
    
    data->xwindow->state = new_state;

    if(old_state == new_state)
    {
        return;
    }
    */

    switch(new_state)
    {
        case WINDOW_STATE_NORMAL:
            data->window->flags &= ~(WINDOW_STATE_MAXIMIZED|SDL_WINDOW_MINIMIZED|SDL_WINDOW_FULLSCREEN);
            SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_RESTORED, 0, 0);
            break;

        case WINDOW_STATE_MAXIMIZED:
            data->window->flags |= WINDOW_STATE_MAXIMIZED;
            data->window->flags &= ~(SDL_WINDOW_MINIMIZED|SDL_WINDOW_FULLSCREEN);
            SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_MAXIMIZED, 0, 0);
            break;

        case WINDOW_STATE_MINIMIZED:
            data->window->flags |= SDL_WINDOW_MINIMIZED;
            data->window->flags &= ~(WINDOW_STATE_MAXIMIZED|SDL_WINDOW_FULLSCREEN);
            SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_MINIMIZED, 0, 0);
            break;

        case WINDOW_STATE_FULLSCREEN:
            data->window->flags |= SDL_WINDOW_FULLSCREEN;
            data->window->flags &= ~(WINDOW_STATE_MAXIMIZED|SDL_WINDOW_MINIMIZED);
            /*
             * NOTE: there is no fullscreen event.
             */
            SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_RESTORED, 0, 0);
            break;
    }
    
    if(new_state == WINDOW_STATE_FULLSCREEN)
    {
        //LAYLAOS_SetWindowMouseGrab(_this, data->window, 1);
        //LAYLAOS_SetWindowKeyboardGrab(_this, data->window, 1);
        SDL_SetWindowKeyboardGrab(data->window, 1);
        SDL_SetWindowMouseGrab(data->window, 1);
    }
    else if(was_fullscreen)
    {
        //LAYLAOS_SetWindowMouseGrab(_this, data->window, 0);
        //LAYLAOS_SetWindowKeyboardGrab(_this, data->window, 0);
        SDL_SetWindowKeyboardGrab(data->window, 0);
        SDL_SetWindowMouseGrab(data->window, 0);
    }
}

#endif


static void
LAYLAOS_DispatchEvent(_THIS, struct event_t *ev)
{
    SDL_VideoData *videodata = (SDL_VideoData *)_this->driverdata;
    SDL_WindowData *data;
    int i;

    if(!videodata)
    {
        return;
    }

    if(ev->type == 0)
    {
        return;
    }

    data = NULL;

    if(videodata && videodata->windowlist)
    {
        for(i = 0; i < videodata->numwindows; ++i)
        {
            if((videodata->windowlist[i] != NULL) &&
               (videodata->windowlist[i]->xwindow->winid == ev->dest))
            {
                data = videodata->windowlist[i];
                break;
            }
        }
    }

    if(!data)
    {
        return;
    }

//#define ISFULLSCR           (data->window->flags & SDL_WINDOW_FULLSCREEN)

    //__asm__ __volatile__("xchg %%bx, %%bx"::);

    switch(ev->type)
    {
        case EVENT_WINDOW_SHOWN:
            //LAYLAOS_CheckWindowState(_this, data, ev->winst.state, ISFULLSCR);
            SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_SHOWN, 0, 0);
            break;

        case EVENT_WINDOW_HIDDEN:
            //LAYLAOS_CheckWindowState(_this, data, ev->winst.state, ISFULLSCR);
            SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_HIDDEN, 0, 0);
            break;

        case EVENT_WINDOW_RAISED:
            //LAYLAOS_CheckWindowState(_this, data, ev->winst.state, ISFULLSCR);
            SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_SHOWN, 0, 0);

            if(SDL_GetKeyboardFocus() != data->window)
            {
                if(get_input_focus() == data->xwindow->winid)
                {
                    SDL_SetKeyboardFocus(data->window);
                    LAYLAOS_ReconcileKeyboardState(_this);
                }
            }

            break;

        case EVENT_WINDOW_GAINED_FOCUS:
            if(SDL_GetKeyboardFocus() != data->window)
            {
                SDL_SetKeyboardFocus(data->window);
                LAYLAOS_ReconcileKeyboardState(_this);
            }
            
            break;

        case EVENT_WINDOW_LOST_FOCUS:
        case EVENT_WINDOW_LOWERED:
            if(SDL_GetKeyboardFocus() == data->window)
            {
                SDL_SetKeyboardFocus(NULL);

                /* In relative mode we are guaranteed to not have mouse focus if we don't have keyboard focus */
                if (SDL_GetMouse()->relative_mode) {
                    SDL_SetMouseFocus(NULL);
                }
            }

            //mouse_ungrab();
            //LAYLAOS_CheckWindowState(_this, data, ev->winst.state, ISFULLSCR);
            break;

        case EVENT_WINDOW_RESIZE_OFFER:
            {
                //struct event_t ev2;
                int16_t x = ev->win.x, y = ev->win.y;
                uint16_t w = ev->win.w, h = ev->win.h;
                
                window_resize(data->xwindow, x, y, w, h);

                // force re-creating window surface so it gets sync'd to our
                // new window buffer
                data->window->surface_valid = SDL_FALSE;
                (void)SDL_GetWindowSurface(data->window);

                SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_MOVED, x, y);
                SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_RESIZED, w, h);
            
                // force repaint
                SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_EXPOSED, 0, 0);

                // get the new window state from the server
                /*
                ev2.type = REQUEST_WINDOW_GET_STATE;
                ev2.seqid = 0;
                ev2.src = data->xwindow->winid;
                ev2.dest = __global_gui_data.server_winid;
                write(__global_gui_data.serverfd, (void *)&ev2, sizeof(struct event_t));
                */
            }

            break;
        
        case EVENT_WINDOW_POS_CHANGED:
            SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_MOVED, ev->win.x, ev->win.y);
            break;

        case EVENT_WINDOW_CLOSING:
            //window_destroy(data->xwindow);
            SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_CLOSE, 0, 0);
            break;
        
        case EVENT_WINDOW_STATE:
            //LAYLAOS_CheckWindowState(_this, data, ev->winst.state, ISFULLSCR);
            break;

        case EVENT_MOUSE:
            {
                SDL_Mouse *mouse = SDL_GetMouse();
                mouse_buttons_t obuttons = data->xwindow->last_button_state;
                mouse_buttons_t nbuttons = ev->mouse.buttons;
                
                data->xwindow->last_button_state = nbuttons;

                if(!mouse->relative_mode || mouse->relative_mode_warp)
                {
                    SDL_SendMouseMotion(data->window, 0, 0, ev->mouse.x, ev->mouse.y);

#define BUTTON_PRESSED(which)   \
    (!(obuttons & MOUSE_ ## which ## _DOWN) &&  \
      (nbuttons & MOUSE_ ## which ## _DOWN))

#define BUTTON_RELEASED(which)  \
    ((obuttons & MOUSE_ ## which ## _DOWN) &&   \
     !(nbuttons & MOUSE_ ## which ## _DOWN))

#define SEND_BUTTON_EVENT(which, ev)            \
    SDL_SendMouseButton(data->window, 0, ev, SDL_BUTTON_ ## which);

                    /* Left button press/release */
                    if(BUTTON_RELEASED(LBUTTON))
                    {
                        SEND_BUTTON_EVENT(LEFT, SDL_RELEASED);
                    }
                    else if(BUTTON_PRESSED(LBUTTON))
                    {
                        SEND_BUTTON_EVENT(LEFT, SDL_PRESSED);
                    }

                    /* Right button press/release */
                    if(BUTTON_RELEASED(RBUTTON))
                    {
                        SEND_BUTTON_EVENT(RIGHT, SDL_RELEASED);
                    }
                    else if(BUTTON_PRESSED(RBUTTON))
                    {
                        SEND_BUTTON_EVENT(RIGHT, SDL_PRESSED);
                    }

                    /* Middle button press/release */
                    if(BUTTON_RELEASED(MBUTTON))
                    {
                        SEND_BUTTON_EVENT(MIDDLE, SDL_RELEASED);
                    }
                    else if(BUTTON_PRESSED(MBUTTON))
                    {
                        SEND_BUTTON_EVENT(MIDDLE, SDL_PRESSED);
                    }

#undef BUTTON_PRESSED
#undef BUTTON_RELEASED
#undef SEND_BUTTON_EVENT

                }
            }

            break;

        case EVENT_MOUSE_ENTER:
            {
                SDL_Mouse *mouse = SDL_GetMouse();

                data->xwindow->last_button_state = ev->mouse.buttons;
                SDL_SetMouseFocus(data->window);
                mouse->last_x = ev->mouse.x;
                mouse->last_y = ev->mouse.y;

                if(!mouse->relative_mode)
                {
                    SDL_SendMouseMotion(data->window, 0, 0, ev->mouse.x, ev->mouse.y);
                }
            }

            break;

        case EVENT_MOUSE_EXIT:
            if(SDL_GetMouseFocus() == data->window &&
               !SDL_GetMouse()->relative_mode &&
               !(data->window->flags & SDL_WINDOW_MOUSE_CAPTURE))
            {
                SDL_SendMouseMotion(data->window, 0, 0, ev->mouse.x, ev->mouse.y);
                SDL_SetMouseFocus(NULL);
            }

            break;
        
        case EVENT_KEY_PRESS:
            {
                SDL_Scancode code = (ev->key.code >= LaylaOS_KeycodeCount) ?
                                    SDL_SCANCODE_UNKNOWN :
                                    LaylaOS_Keycodes[(int)ev->key.code & 0xff];

                if(code != SDL_SCANCODE_UNKNOWN)
                {
                    SDL_SendKeyboardKey(SDL_PRESSED, code);
                }
            }

            break;

        case EVENT_KEY_RELEASE:
            {
                SDL_Scancode code = (ev->key.code >= LaylaOS_KeycodeCount) ?
                                    SDL_SCANCODE_UNKNOWN :
                                    LaylaOS_Keycodes[(int)ev->key.code & 0xff];

                if(code != SDL_SCANCODE_UNKNOWN)
                {
                    SDL_SendKeyboardKey(SDL_RELEASED, code);
                }
            }

            break;
    }
}

void
LAYLAOS_PumpEvents(_THIS)
{
    struct event_t *ev = NULL;

    /* Keep processing pending events */
    while(1)
    //while(pending_events())
    {
        if(!(ev = next_event_for_seqid(NULL, 0, 0)))
        //if(!(ev = next_event()))
        {
            break;
        }

        LAYLAOS_DispatchEvent(_this, ev);
        free(ev);
    }
}

int
LAYLAOS_WaitEventTimeout(_THIS, int timeout)
{
    /* Fail the wait so the caller falls back to polling */
    return -1;
}

#endif /* SDL_VIDEO_DRIVER_LAYLAOS */

/* vi: set ts=4 sw=4 expandtab: */
