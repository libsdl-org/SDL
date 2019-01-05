/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2019 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_HAIKU


#ifdef __cplusplus
extern "C" {
#endif

#include "SDL_bkeyboard.h"
#include "SDL_bwindow.h"
#include "SDL_bclipboard.h"
#include "SDL_bvideo.h"
#include "SDL_bopengl.h"
#include "SDL_bmodes.h"
#include "SDL_bframebuffer.h"
#include "SDL_bevents.h"

/* FIXME: Undefined functions */
//    #define HAIKU_PumpEvents NULL
    #define HAIKU_StartTextInput NULL
    #define HAIKU_StopTextInput NULL
    #define HAIKU_SetTextInputRect NULL

//    #define HAIKU_DeleteDevice NULL

/* End undefined functions */

static SDL_VideoDevice *
HAIKU_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;
    /*SDL_VideoData *data;*/

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));

    device->driverdata = NULL; /* FIXME: Is this the cause of some of the
                                  SDL_Quit() errors? */

/* TODO: Figure out if any initialization needs to go here */

    /* Set the function pointers */
    device->VideoInit = HAIKU_VideoInit;
    device->VideoQuit = HAIKU_VideoQuit;
    device->GetDisplayBounds = HAIKU_GetDisplayBounds;
    device->GetDisplayModes = HAIKU_GetDisplayModes;
    device->SetDisplayMode = HAIKU_SetDisplayMode;
    device->PumpEvents = HAIKU_PumpEvents;

    device->CreateSDLWindow = HAIKU_CreateWindow;
    device->CreateSDLWindowFrom = HAIKU_CreateWindowFrom;
    device->SetWindowTitle = HAIKU_SetWindowTitle;
    device->SetWindowIcon = HAIKU_SetWindowIcon;
    device->SetWindowPosition = HAIKU_SetWindowPosition;
    device->SetWindowSize = HAIKU_SetWindowSize;
    device->ShowWindow = HAIKU_ShowWindow;
    device->HideWindow = HAIKU_HideWindow;
    device->RaiseWindow = HAIKU_RaiseWindow;
    device->MaximizeWindow = HAIKU_MaximizeWindow;
    device->MinimizeWindow = HAIKU_MinimizeWindow;
    device->RestoreWindow = HAIKU_RestoreWindow;
    device->SetWindowBordered = HAIKU_SetWindowBordered;
    device->SetWindowResizable = HAIKU_SetWindowResizable;
    device->SetWindowFullscreen = HAIKU_SetWindowFullscreen;
    device->SetWindowGammaRamp = HAIKU_SetWindowGammaRamp;
    device->GetWindowGammaRamp = HAIKU_GetWindowGammaRamp;
    device->SetWindowGrab = HAIKU_SetWindowGrab;
    device->DestroyWindow = HAIKU_DestroyWindow;
    device->GetWindowWMInfo = HAIKU_GetWindowWMInfo;
    device->CreateWindowFramebuffer = HAIKU_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = HAIKU_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = HAIKU_DestroyWindowFramebuffer;
    
    device->shape_driver.CreateShaper = NULL;
    device->shape_driver.SetWindowShape = NULL;
    device->shape_driver.ResizeWindowShape = NULL;

#if SDL_VIDEO_OPENGL
    device->GL_LoadLibrary = HAIKU_GL_LoadLibrary;
    device->GL_GetProcAddress = HAIKU_GL_GetProcAddress;
    device->GL_UnloadLibrary = HAIKU_GL_UnloadLibrary;
    device->GL_CreateContext = HAIKU_GL_CreateContext;
    device->GL_MakeCurrent = HAIKU_GL_MakeCurrent;
    device->GL_SetSwapInterval = HAIKU_GL_SetSwapInterval;
    device->GL_GetSwapInterval = HAIKU_GL_GetSwapInterval;
    device->GL_SwapWindow = HAIKU_GL_SwapWindow;
    device->GL_DeleteContext = HAIKU_GL_DeleteContext;
#endif

    device->StartTextInput = HAIKU_StartTextInput;
    device->StopTextInput = HAIKU_StopTextInput;
    device->SetTextInputRect = HAIKU_SetTextInputRect;

    device->SetClipboardText = HAIKU_SetClipboardText;
    device->GetClipboardText = HAIKU_GetClipboardText;
    device->HasClipboardText = HAIKU_HasClipboardText;

    device->free = HAIKU_DeleteDevice;

    return device;
}

VideoBootStrap HAIKU_bootstrap = {
    "haiku", "Haiku graphics",
    HAIKU_Available, HAIKU_CreateDevice
};

void HAIKU_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_free(device->driverdata);
    SDL_free(device);
}

int HAIKU_VideoInit(_THIS)
{
    /* Initialize the Be Application for appserver interaction */
    if (SDL_InitBeApp() < 0) {
        return -1;
    }
    
    /* Initialize video modes */
    HAIKU_InitModes(_this);

    /* Init the keymap */
    HAIKU_InitOSKeymap();

#if SDL_VIDEO_OPENGL
        /* testgl application doesn't load library, just tries to load symbols */
        /* is it correct? if so we have to load library here */
    HAIKU_GL_LoadLibrary(_this, NULL);
#endif

    /* We're done! */
    return (0);
}

int HAIKU_Available(void)
{
    return (1);
}

void HAIKU_VideoQuit(_THIS)
{

    HAIKU_QuitModes(_this);

    SDL_QuitBeApp();
}

#ifdef __cplusplus
}
#endif

#endif /* SDL_VIDEO_DRIVER_HAIKU */

/* vi: set ts=4 sw=4 expandtab: */
