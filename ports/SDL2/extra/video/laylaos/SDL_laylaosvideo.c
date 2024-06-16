
/*
typedef struct
{
    char DeviceName[32];
} SDL_DisplayData;

typedef struct
{
    unsigned int w, h;
} SDL_DisplayModeData;
*/


/////////////////////////

#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_LAYLAOS

#include <gui/gui.h>

#define LAYLAOSVID_DRIVER_NAME  "LaylaOS"

#define GLOB                    __global_gui_data

#include "SDL_laylaosshape.h"
#include "SDL_laylaosevents.h"
#include "SDL_laylaosframebuffer.h"
#include "SDL_laylaosclipboard.h"
#include "SDL_laylaoswindow.h"
#include "SDL_laylaosvideo.h"
#include "SDL_laylaoskeyboard.h"
#include "SDL_laylaosmouse.h"
#include "SDL_laylaosmodes.h"
#include "SDL_laylaosvulkan.h"


int LAYLAOS_VideoInit(_THIS);
void LAYLAOS_VideoQuit(_THIS);


void
LAYLAOS_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_free(device->driverdata);
    SDL_free(device);
}

SDL_VideoDevice *
LAYLAOS_CreateDevice(void)
{
    SDL_VideoDevice *device;
    SDL_VideoData *data;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));

    if(!device)
    {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (struct SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));

    if(!data)
    {
        SDL_free(device);
        SDL_OutOfMemory();
        return NULL;
    }

    device->driverdata = data;

    /* Set the function pointers */
    device->VideoInit = LAYLAOS_VideoInit;
    device->VideoQuit = LAYLAOS_VideoQuit;
    device->GetDisplayBounds = LAYLAOS_GetDisplayBounds;
    device->GetDisplayUsableBounds = LAYLAOS_GetDisplayUsableBounds;
    device->GetDisplayDPI = NULL;   // LAYLAOS_GetDisplayDPI;
    device->GetDisplayModes = LAYLAOS_GetDisplayModes;
    device->SetDisplayMode = LAYLAOS_SetDisplayMode;
    device->PumpEvents = LAYLAOS_PumpEvents;

    device->WaitEventTimeout = LAYLAOS_WaitEventTimeout;
    device->SendWakeupEvent = NULL;     // WIN_SendWakeupEvent;
    device->SuspendScreenSaver = NULL;      // WIN_SuspendScreenSaver;

    device->CreateSDLWindow = LAYLAOS_CreateWindow;
    device->CreateSDLWindowFrom = LAYLAOS_CreateWindowFrom;
    device->SetWindowTitle = LAYLAOS_SetWindowTitle;
    device->SetWindowIcon = LAYLAOS_SetWindowIcon;
    device->SetWindowPosition = LAYLAOS_SetWindowPosition;
    device->SetWindowSize = LAYLAOS_SetWindowSize;

    device->GetWindowBordersSize = LAYLAOS_GetWindowBordersSize;
    device->GetWindowSizeInPixels = LAYLAOS_GetWindowSizeInPixels;

    device->SetWindowOpacity = LAYLAOS_SetWindowOpacity;
    device->ShowWindow = LAYLAOS_ShowWindow;
    device->HideWindow = LAYLAOS_HideWindow;
    device->RaiseWindow = LAYLAOS_RaiseWindow;
    device->MaximizeWindow = LAYLAOS_MaximizeWindow;
    device->MinimizeWindow = LAYLAOS_MinimizeWindow;
    device->RestoreWindow = LAYLAOS_RestoreWindow;
    device->SetWindowBordered = LAYLAOS_SetWindowBordered;
    device->SetWindowResizable = LAYLAOS_SetWindowResizable;

    device->SetWindowAlwaysOnTop = LAYLAOS_SetWindowAlwaysOnTop;

    device->SetWindowFullscreen = LAYLAOS_SetWindowFullscreen;
    device->SetWindowGammaRamp = LAYLAOS_SetWindowGammaRamp;
    device->GetWindowGammaRamp = LAYLAOS_GetWindowGammaRamp;

    device->SetWindowMouseRect = LAYLAOS_SetWindowMouseRect;
    device->SetWindowMouseGrab = LAYLAOS_SetWindowMouseGrab;
    device->SetWindowKeyboardGrab = LAYLAOS_SetWindowKeyboardGrab;

    //device->SetWindowGrab = LAYLAOS_SetWindowGrab;
    device->DestroyWindow = LAYLAOS_DestroyWindow;
    device->GetWindowWMInfo = LAYLAOS_GetWindowWMInfo;
    device->CreateWindowFramebuffer = LAYLAOS_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = LAYLAOS_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = LAYLAOS_DestroyWindowFramebuffer;
    device->OnWindowEnter = LAYLAOS_OnWindowEnter;
    device->SetWindowHitTest = LAYLAOS_SetWindowHitTest;

    device->AcceptDragAndDrop = LAYLAOS_AcceptDragAndDrop;
    device->FlashWindow = LAYLAOS_FlashWindow;

    device->shape_driver.CreateShaper = LAYLAOS_CreateShaper;
    device->shape_driver.SetWindowShape = LAYLAOS_SetWindowShape;
    device->shape_driver.ResizeWindowShape = LAYLAOS_ResizeWindowShape;

    device->StartTextInput = LAYLAOS_StartTextInput;
    device->StopTextInput = LAYLAOS_StopTextInput;
    device->SetTextInputRect = LAYLAOS_SetTextInputRect;

    device->ClearComposition = NULL;    //WIN_ClearComposition;
    device->IsTextInputShown = NULL;    //WIN_IsTextInputShown;

    device->SetClipboardText = LAYLAOS_SetClipboardText;
    device->GetClipboardText = LAYLAOS_GetClipboardText;
    device->HasClipboardText = LAYLAOS_HasClipboardText;

#if SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = LAYLAOS_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = LAYLAOS_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = LAYLAOS_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = LAYLAOS_Vulkan_CreateSurface;
#endif

    device->free = LAYLAOS_DeleteDevice;

    return device;
}


VideoBootStrap LAYLAOS_bootstrap = {
    LAYLAOSVID_DRIVER_NAME, "SDL LaylaOS video driver", LAYLAOS_CreateDevice
};


int
LAYLAOS_VideoInit(_THIS)
{
    char *dummy_argv[] = { "SDL", NULL };

    gui_init(1, dummy_argv);

    if(LAYLAOS_InitModes(_this) < 0)
    {
        close(GLOB.serverfd);
        GLOB.serverfd = -1;
        return -1;
    }

    LAYLAOS_InitKeyboard(_this);
    LAYLAOS_InitMouse(_this);

    return 0;
}


void
LAYLAOS_VideoQuit(_THIS)
{
    LAYLAOS_QuitKeyboard(_this);
    LAYLAOS_QuitMouse(_this);

    close(GLOB.serverfd);
    GLOB.serverfd = -1;
}

#endif /* SDL_VIDEO_DRIVER_LAYLAOS */

/* vim: set ts=4 sw=4 expandtab: */
