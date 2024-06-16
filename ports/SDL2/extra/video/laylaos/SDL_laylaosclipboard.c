#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_LAYLAOS

#include <limits.h> /* For INT_MAX */
#include <gui/clipboard.h>

#include "SDL_events.h"
#include "SDL_laylaosvideo.h"
#include "SDL_laylaoswindow.h"
#include "../../events/SDL_clipboardevents_c.h"

#define TEXT_FORMAT             CLIPBOARD_FORMAT_TEXT


int
LAYLAOS_SetClipboardText(_THIS, const char *text)
{
    if(!clipboard_set_data(TEXT_FORMAT, (void *)text, SDL_strlen(text) + 1))
    {
        return SDL_SetError("Couldn't set clipboard data");
    }
    
    return 0;
}

char *
LAYLAOS_GetClipboardText(_THIS)
{
    char *text;

    text = NULL;

    if(clipboard_has_data(TEXT_FORMAT))
    {
        text = clipboard_get_data(TEXT_FORMAT);
    }

    if(!text)
    {
        text = SDL_strdup("");
    }

    return text;
}

SDL_bool
LAYLAOS_HasClipboardText(_THIS)
{
    return clipboard_has_data(TEXT_FORMAT) ? SDL_TRUE : SDL_FALSE;
    
    /*
    SDL_bool result = SDL_FALSE;
    char *text = LAYLAOS_GetClipboardText(_this);

    if(text)
    {
        result = text[0] != '\0' ? SDL_TRUE : SDL_FALSE;
        SDL_free(text);
    }

    return result;
    */
}

#endif /* SDL_VIDEO_DRIVER_LAYLAOS */

/* vi: set ts=4 sw=4 expandtab: */
