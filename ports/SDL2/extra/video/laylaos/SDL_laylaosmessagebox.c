#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_LAYLAOS

#include <gui/client/window.h>
#include <gui/client/button.h>
#include <gui/client/label.h>
#include <kernel/keycodes.h>

#include "SDL.h"
#include "../SDL_sysvideo.h"
#include "SDL_laylaoswindow.h"


#define MAX_BUTTONS             8       /* Maximum number of buttons supported */
#define MAX_TEXT_LINES          32      /* Maximum number of text lines supported */
#define MIN_BUTTON_WIDTH        64      /* Minimum button width */
#define MIN_DIALOG_WIDTH        200     /* Minimum dialog width */
#define MIN_DIALOG_HEIGHT       100     /* Minimum dialog height */


static const SDL_MessageBoxColor g_default_colors[SDL_MESSAGEBOX_COLOR_MAX] =
{
    { 56,  54,  53  }, /* SDL_MESSAGEBOX_COLOR_BACKGROUND, */
    { 209, 207, 205 }, /* SDL_MESSAGEBOX_COLOR_TEXT, */
    { 140, 135, 129 }, /* SDL_MESSAGEBOX_COLOR_BUTTON_BORDER, */
    { 105, 102, 99  }, /* SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND, */
    { 205, 202, 53  }, /* SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED, */
};

#define SDL_MAKE_RGB( _r, _g, _b )  ( ( ( Uint32 )( _r ) << 24 ) | \
                                      ( ( Uint32 )( _g ) << 16 ) | \
                                      ( ( Uint32 )( _b ) << 8  ) | \
                                      ( ( Uint32 )( 0xff ) ) )


typedef struct TextLineData
{
    int width;                          /* Width of this text line */
    int length;                         /* String length of this text line */
    const char *text;                   /* Text for this line */
} TextLineData;

typedef struct SDL_MessageBoxButtonDataLAYLAOS
{
    //int x, y;                           /* Text position */
    int length;                         /* Text length */
    int text_width;                     /* Text width */

    SDL_Rect rect;                      /* Rectangle for entire button */

    const SDL_MessageBoxButtonData *buttondata;   /* Button data from caller */
} SDL_MessageBoxButtonDataLAYLAOS;

typedef struct SDL_MessageBoxDataLAYLAOS
{
    struct window_t *window, *owner;

    int dialog_width;                   /* Dialog box width. */
    int dialog_height;                  /* Dialog box height. */

    //struct font_t *font;
    //int xtext, ytext;                   /* Text position to start drawing at. */
    SDL_Rect text_rect;
    int numlines;                       /* Count of Text lines. */
    int text_height;                    /* Height for text lines. */
    TextLineData linedata[MAX_TEXT_LINES];

    int *pbuttonid;                     /* Pointer to user return buttonid value. */

    SDL_bool has_focus;
    char last_key_pressed;
    volatile SDL_bool close_dialog;

    int button_press_index;             /* Index into buttondata/buttonpos for button which is pressed (or -1). */
    int mouse_over_index;               /* Index into buttondata/buttonpos for button mouse is over (or -1). */

    int numbuttons;                     /* Count of buttons. */
    const SDL_MessageBoxButtonData *buttondata;
    SDL_MessageBoxButtonDataLAYLAOS buttonpos[MAX_BUTTONS];

    Uint32 color[SDL_MESSAGEBOX_COLOR_MAX];

    const SDL_MessageBoxData *messageboxdata;
} SDL_MessageBoxDataLAYLAOS;


/* Maximum helper for ints. */
static SDL_INLINE int
IntMax(int a, int b)
{
    return (a > b) ? a : b;
}


/* Return width and height for a string. */
static void
GetTextWidthHeight(SDL_MessageBoxDataLAYLAOS *data, const char *str,
                   int nbytes, int *pwidth, int *pheight)
{
    *pwidth = __global_gui_data.mono.charw * nbytes;
    *pheight = __global_gui_data.mono.charh;
}


static inline uint32_t simple_request(int event, winid_t dest, winid_t src)
{
    struct event_t ev;
    uint32_t seqid = __next_seqid();

    ev.seqid = seqid;
    ev.type = event;
    ev.src = src;
    ev.dest = dest;
    ev.valid_reply = 1;
    write(__global_gui_data.serverfd, (void *)&ev, sizeof(struct event_t));

    return seqid;
}


/* Initialize SDL_MessageBoxData structure and Display, etc. */
static int
LAYLAOS_MessageBoxInit(SDL_MessageBoxDataLAYLAOS *data,
                       const SDL_MessageBoxData *messageboxdata,
                       int *pbuttonid)
{
    int i;
    int numbuttons = messageboxdata->numbuttons;
    const SDL_MessageBoxButtonData *buttondata = messageboxdata->buttons;
    const SDL_MessageBoxColor *colorhints;

    if(numbuttons > MAX_BUTTONS)
    {
        return SDL_SetError("Too many buttons (%d max allowed)", MAX_BUTTONS);
    }

    data->dialog_width = MIN_DIALOG_WIDTH;
    data->dialog_height = MIN_DIALOG_HEIGHT;
    data->messageboxdata = messageboxdata;
    data->buttondata = buttondata;
    data->numbuttons = numbuttons;
    data->pbuttonid = pbuttonid;
    //data->font = &__global_gui_data.mono;
    data->last_key_pressed = 0;

    if(messageboxdata->colorScheme)
    {
        colorhints = messageboxdata->colorScheme->colors;
    }
    else
    {
        colorhints = g_default_colors;
    }

    /* Convert our SDL_MessageBoxColor r,g,b values to packed RGB format. */
    for(i = 0; i < SDL_MESSAGEBOX_COLOR_MAX; i++)
    {
        data->color[i] = SDL_MAKE_RGB(colorhints[i].r, colorhints[i].g, colorhints[i].b);
    }

    return 0;
}


/* Calculate and initialize text and button locations. */
static int
LAYLAOS_MessageBoxInitPositions(SDL_MessageBoxDataLAYLAOS *data)
{
    int i;
    int ybuttons;
    int text_width_max = 0;
    int button_text_height = 0;
    int button_width = MIN_BUTTON_WIDTH;
    const SDL_MessageBoxData *messageboxdata = data->messageboxdata;

    /* Go over text and break linefeeds into separate lines. */
    if(messageboxdata->message && messageboxdata->message[0])
    {
        const char *text = messageboxdata->message;
        TextLineData *plinedata = data->linedata;

        for(i = 0; i < MAX_TEXT_LINES; i++, plinedata++)
        {
            int height;
            char *lf = SDL_strchr((char *)text, '\n');

            data->numlines++;

            /* Only grab length up to lf if it exists and isn't the last line. */
            plinedata->length = (lf && (i < MAX_TEXT_LINES - 1)) ? (lf - text) : SDL_strlen(text);
            plinedata->text = text;

            GetTextWidthHeight(data, text, plinedata->length, &plinedata->width, &height);

            /* Text and widths are the largest we've ever seen. */
            data->text_height = IntMax(data->text_height, height);
            text_width_max = IntMax(text_width_max, plinedata->width);

            if(lf && (lf > text) && (lf[-1] == '\r'))
            {
                plinedata->length--;
            }

            text += plinedata->length + 1;

            /* Break if there are no more linefeeds. */
            if(!lf)
            {
                break;
            }
        }

        /* Bump up the text height slightly. */
        data->text_height += 2;
    }

    /* Loop through all buttons and calculate the button widths and height. */
    for(i = 0; i < data->numbuttons; i++)
    {
        int height;

        data->buttonpos[i].buttondata = &data->buttondata[i];
        data->buttonpos[i].length = SDL_strlen(data->buttondata[i].text);

        GetTextWidthHeight(data, data->buttondata[i].text,
                           SDL_strlen(data->buttondata[i].text),
                           &data->buttonpos[i].text_width, &height);

        button_width = IntMax(button_width, data->buttonpos[i].text_width);
        button_text_height = IntMax(button_text_height, height);
    }

#if 0

    if(data->numlines)
    {
        /* x,y for this line of text. */
        data->xtext = data->text_height;
        data->ytext = data->text_height + data->text_height;

        /* Bump button y down to bottom of text. */
        ybuttons = 3 * data->ytext / 2 + (data->numlines - 1) * data->text_height;

        /* Bump the dialog box width and height up if needed. */
        data->dialog_width = IntMax(data->dialog_width, 2 * data->xtext + text_width_max);
        data->dialog_height = IntMax(data->dialog_height, ybuttons);
    }
    else
    {
        /* Button y starts at height of button text. */
        ybuttons = button_text_height;
    }

#endif

#define DIALOG_PADDING          __global_gui_data.mono.charh

    data->text_rect.x = DIALOG_PADDING;
    data->text_rect.y = DIALOG_PADDING;
    data->text_rect.w = text_width_max;
    data->text_rect.h = data->numlines ? (data->numlines * data->text_height) :
                                         __global_gui_data.mono.charh;
    ybuttons = (DIALOG_PADDING * 2) + data->text_rect.h;
    /* Bump the dialog box width and height up if needed. */
    data->dialog_width = IntMax(data->dialog_width, (2 * DIALOG_PADDING) + text_width_max);
    data->dialog_height = IntMax(data->dialog_height,
                                 ybuttons + button_text_height + DIALOG_PADDING);

    if(data->numbuttons)
    {
        int x, y;
        int width_of_buttons;
        int button_spacing = button_text_height;
        int button_height = button_text_height + __global_gui_data.mono.charh;

        /* Bump button width up a bit. */
        button_width += button_text_height;

        /* Get width of all buttons lined up. */
        width_of_buttons = data->numbuttons * button_width + (data->numbuttons - 1) * button_spacing;

        /* Bump up dialog width and height if buttons are wider than text. */
        data->dialog_width = IntMax(data->dialog_width, width_of_buttons + 2 * button_spacing);
        data->dialog_height = IntMax(data->dialog_height, ybuttons + button_height + DIALOG_PADDING);

        /* Location for first button. */
        x = (data->dialog_width - width_of_buttons) / 2;
        y = ybuttons /* + (data->dialog_height - ybuttons - button_height) / 2 */;

        for(i = 0; i < data->numbuttons; i++)
        {
            /* Button coordinates. */
            data->buttonpos[i].rect.x = x;
            data->buttonpos[i].rect.y = y;
            data->buttonpos[i].rect.w = button_width;
            data->buttonpos[i].rect.h = button_height;

            /* Button text coordinates. */
            //data->buttonpos[i].x = x + (button_width - data->buttonpos[i].text_width) / 2;
            //data->buttonpos[i].y = y + (button_height - button_text_height - 1) / 2 + button_text_height;

            /* Scoot over for next button. */
            x += button_width + button_spacing;
        }
    }

    return 0;
}


static void LAYLAOS_MessageBoxButtonHandler(struct button_t *button, int x, int y)
{
    struct window_t *window = (struct window_t *)button->window.parent;
    SDL_MessageBoxDataLAYLAOS *data = window->internal_data;
    
    //data->button_press_index = button->internal_data;
    // *data->pbuttonid = data->button_press_index;
    *data->pbuttonid = (int)(intptr_t)button->internal_data;
    data->close_dialog = SDL_TRUE; 
}


static void LAYLAOS_MessageBoxDispatchEvent(struct event_t *ev)
{
    struct window_t *window;
    SDL_MessageBoxDataLAYLAOS *data;
    Uint32 mask;
    
    if(!(window = win_for_winid(ev->dest)))
    {
        return;
    }
    
    data = window->internal_data;
    
    switch(ev->type)
    {
        case EVENT_WINDOW_POS_CHANGED:
            window->x = ev->win.x;
            window->y = ev->win.y;
            break;
        
        case EVENT_WINDOW_GAINED_FOCUS:
            /* Got focus. */
            data->has_focus = SDL_TRUE;
            break;

        case EVENT_WINDOW_LOST_FOCUS:
            /* lost focus. Reset button and mouse info. */
            data->has_focus = SDL_FALSE;
            //data->button_press_index = -1;
            //data->mouse_over_index = -1;
            break;

        case EVENT_MOUSE:
            window_mouseover(window, ev->mouse.x, ev->mouse.y, ev->mouse.buttons);
            break;
        
        case EVENT_MOUSE_EXIT:
            window_mouseexit(window, ev->mouse.buttons);
            break;

        case EVENT_WINDOW_CLOSING:
            //data->button_press_index = -1;
            *data->pbuttonid = data->button_press_index;
            data->close_dialog = SDL_TRUE;
            break;

        case EVENT_KEY_PRESS:
            /* Store key press - we make sure in key release that we got both. */
            if(ev->key.modifiers == 0)
            {
                data->last_key_pressed = ev->key.code;
            }
            break;

        case EVENT_KEY_RELEASE:
            /* If this is a key release for something we didn't get the key down for, then bail. */
            if(ev->key.code != data->last_key_pressed)
            {
                break;
            }
            
            mask = 0;

            if(ev->key.code == KEYCODE_ESC)
            {
                mask = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
            }
            else if(ev->key.code == KEYCODE_ENTER)
            {
                mask = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
            }

            if(mask)
            {
                int i;

                /* Look for first button with this mask set, and return it if found. */
                for(i = 0; i < data->numbuttons; i++)
                {
                    SDL_MessageBoxButtonDataLAYLAOS *buttondataintern = &data->buttonpos[i];
 
                    if(buttondataintern->buttondata->flags & mask)
                    {
                        *data->pbuttonid = buttondataintern->buttondata->buttonid;
                        data->close_dialog = SDL_TRUE;
                        break;
                    }
                }
            }
            break;
    }
}


/* Create and set up our dialog box indow. */
static int
LAYLAOS_MessageBoxCreateWindow(SDL_MessageBoxDataLAYLAOS *data)
{
    int i;
    //struct window_t *owner;
    struct window_attribs_t attribs;
    struct label_t *label;
    const SDL_MessageBoxData *messageboxdata = data->messageboxdata;

    attribs.gravity = WINDOW_ALIGN_CENTERBOTH;
    attribs.x = 0;
    attribs.y = 0;
    attribs.w = data->dialog_width;
    attribs.h = data->dialog_height;
    attribs.flags = WINDOW_NORESIZE | WINDOW_NOMINIMIZE | WINDOW_SKIPTASKBAR;
    
    /*
    if(!_this->windows)
    {
        return SDL_SetError("Couldn't get dialog parent window");
    }

    owner = ((SDL_WindowData *)_this->windows->driverdata)->xwindow;
    */

    if(messageboxdata->window)
    {
        data->owner = ((SDL_WindowData *)messageboxdata->window->driverdata)->xwindow;
    }
    else
    {
        // if no parent was given, create a hidden parent window as
        // the window manager requires every dialog box to have a parent
        if(!(data->owner = window_create(&attribs)))
        {
            return SDL_SetError("Couldn't create background X window");
        }
    }


    if(!(data->window = __window_create(&attribs, WINDOW_TYPE_DIALOG, data->owner->winid)))
    {
        return SDL_SetError("Couldn't create X window");
    }

    data->window->bgcolor = data->color[SDL_MESSAGEBOX_COLOR_BACKGROUND];
    data->window->event_handler = LAYLAOS_MessageBoxDispatchEvent;
    data->window->internal_data = data;
    window_set_title(data->window, (char *)messageboxdata->title);

    if(data->numbuttons)
    {
        struct button_color_t button_colors[3];
        
        button_colors[0].bg = data->color[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND];
        button_colors[1].bg = data->color[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND];
        button_colors[2].bg = data->color[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND];
        button_colors[0].text = data->color[SDL_MESSAGEBOX_COLOR_TEXT];
        button_colors[1].text = data->color[SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED];
        button_colors[2].text = data->color[SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED];
        button_colors[0].border = data->color[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER];
        button_colors[1].border = data->color[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER];
        button_colors[2].border = data->color[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER];

        for(i = 0; i < data->numbuttons; i++)
        {
            struct button_t *button;
            SDL_MessageBoxButtonDataLAYLAOS *buttondataintern = &data->buttonpos[i];
            const SDL_MessageBoxButtonData *buttondata = buttondataintern->buttondata;

            button = button_new(data->window->gc, data->window,
                                                  buttondataintern->rect.x,
                                                  buttondataintern->rect.y,
                                                  buttondataintern->rect.w,
                                                  buttondataintern->rect.h,
                                                  (char *)buttondata->text);

            memcpy(button->colors, button_colors, sizeof(button_colors));
            button->button_click_callback = LAYLAOS_MessageBoxButtonHandler;
            button->internal_data = (void *)(intptr_t)buttondata->buttonid;
        }
    }

    /*
     * TODO: calculate the correct label w & h.
     */
    label = label_new(data->window->gc, data->window,
                      data->text_rect.x, data->text_rect.y,
                      data->text_rect.w, data->text_rect.h,
                      (char *)messageboxdata->message);
    label->window.bgcolor = data->color[SDL_MESSAGEBOX_COLOR_BACKGROUND];
    label->window.fgcolor = data->color[SDL_MESSAGEBOX_COLOR_TEXT];

    window_repaint(data->window);

    //window_show(dialog_window);
    simple_request(REQUEST_DIALOG_SHOW, __global_gui_data.server_winid, data->window->winid);
    data->window->flags &= ~WINDOW_HIDDEN;

    return 0;
}


/* Loop and handle message box event messages until something kills it. */
static int
LAYLAOS_MessageBoxLoop(SDL_MessageBoxDataLAYLAOS *data)
{
    data->button_press_index = -1;  /* Reset what button is currently depressed. */
    data->mouse_over_index = -1;    /* Reset what button the mouse is over. */
    data->has_focus = SDL_TRUE;
    data->close_dialog = SDL_FALSE;

    while(!data->close_dialog)
    {
        struct event_t *ev = NULL;
        
        if((ev = next_event_for_seqid(data->window /* NULL */, 0, 0)))
        {
            //if(win_for_winid(ev->dest) == data->window)
            {
                LAYLAOS_MessageBoxDispatchEvent(ev);
            }
            
            // TODO: we should place the event back into queue if not ours?
            free(ev);
        }
    }

    return 0;
}


/* Free SDL_MessageBoxData data. */
static void
LAYLAOS_MessageBoxShutdown(SDL_MessageBoxDataLAYLAOS *data)
{
    const SDL_MessageBoxData *messageboxdata = data->messageboxdata;

    if(data->window != NULL)
    {
        window_destroy_children(data->window);
        window_destroy(data->window);
        data->window = NULL;
    }
    
    // destroy the background window, if we creatd one
    if(messageboxdata->window == NULL)
    {
        window_destroy(data->owner);
        data->owner = NULL;
    }
}


/* Display a message box. */
int
LAYLAOS_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
    int ret;
    SDL_MessageBoxDataLAYLAOS data;

    SDL_zero(data);

    /* Initialize the return buttonid value to -1 (for error or dialogbox closed). */
    *buttonid = -1;

    /* Init and display the message box. */
    ret = LAYLAOS_MessageBoxInit(&data, messageboxdata, buttonid);

    if(ret != -1)
    {
        ret = LAYLAOS_MessageBoxInitPositions(&data);

        if(ret != -1)
        {
            ret = LAYLAOS_MessageBoxCreateWindow(&data);

            if(ret != -1)
            {
                ret = LAYLAOS_MessageBoxLoop(&data);
            }
        }
    }

    LAYLAOS_MessageBoxShutdown(&data);

    return ret;
}

#endif /* SDL_VIDEO_DRIVER_LAYLAOS */

/* vi: set ts=4 sw=4 expandtab: */
