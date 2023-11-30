/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PSP

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"

/* PSP declarations */
#include "SDL_pspvideo.h"
#include "SDL_pspevents_c.h"
#include "SDL_pspgl_c.h"

/* unused
static SDL_bool PSP_initialized = SDL_FALSE;
*/

static void PSP_Destroy(SDL_VideoDevice *device)
{
    SDL_free(device->driverdata);
    SDL_free(device);
}

static SDL_VideoDevice *PSP_Create()
{
    SDL_VideoDevice *device;
    SDL_VideoData *phdata;
    SDL_GLDriverData *gldata;

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }

    /* Initialize internal PSP specific data */
    phdata = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if (!phdata) {
        SDL_free(device);
        return NULL;
    }

    gldata = (SDL_GLDriverData *)SDL_calloc(1, sizeof(SDL_GLDriverData));
    if (!gldata) {
        SDL_free(device);
        SDL_free(phdata);
        return NULL;
    }
    device->gl_data = gldata;

    device->driverdata = phdata;

    phdata->egl_initialized = SDL_TRUE;

    /* Setup amount of available displays */
    device->num_displays = 0;

    /* Set device free function */
    device->free = PSP_Destroy;

    /* Setup all functions which we can handle */
    device->VideoInit = PSP_VideoInit;
    device->VideoQuit = PSP_VideoQuit;
    device->GetDisplayModes = PSP_GetDisplayModes;
    device->SetDisplayMode = PSP_SetDisplayMode;
    device->CreateSDLWindow = PSP_CreateWindow;
    device->SetWindowTitle = PSP_SetWindowTitle;
    device->SetWindowPosition = PSP_SetWindowPosition;
    device->SetWindowSize = PSP_SetWindowSize;
    device->ShowWindow = PSP_ShowWindow;
    device->HideWindow = PSP_HideWindow;
    device->RaiseWindow = PSP_RaiseWindow;
    device->MaximizeWindow = PSP_MaximizeWindow;
    device->MinimizeWindow = PSP_MinimizeWindow;
    device->RestoreWindow = PSP_RestoreWindow;
    device->DestroyWindow = PSP_DestroyWindow;
    device->GL_LoadLibrary = PSP_GL_LoadLibrary;
    device->GL_GetProcAddress = PSP_GL_GetProcAddress;
    device->GL_UnloadLibrary = PSP_GL_UnloadLibrary;
    device->GL_CreateContext = PSP_GL_CreateContext;
    device->GL_MakeCurrent = PSP_GL_MakeCurrent;
    device->GL_SetSwapInterval = PSP_GL_SetSwapInterval;
    device->GL_GetSwapInterval = PSP_GL_GetSwapInterval;
    device->GL_SwapWindow = PSP_GL_SwapWindow;
    device->GL_DeleteContext = PSP_GL_DeleteContext;
    device->HasScreenKeyboardSupport = PSP_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard = PSP_ShowScreenKeyboard;
    device->HideScreenKeyboard = PSP_HideScreenKeyboard;
    device->IsScreenKeyboardShown = PSP_IsScreenKeyboardShown;

    device->PumpEvents = PSP_PumpEvents;

    return device;
}

VideoBootStrap PSP_bootstrap = {
    "PSP",
    "PSP Video Driver",
    PSP_Create
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int PSP_VideoInit(SDL_VideoDevice *_this)
{
    SDL_DisplayMode mode;

    if (PSP_EventInit(_this) == -1) {
        return -1;  /* error string would already be set */
    }

    SDL_zero(mode);
    mode.w = 480;
    mode.h = 272;
    mode.refresh_rate = 60.0f;

    /* 32 bpp for default */
    mode.format = SDL_PIXELFORMAT_ABGR8888;

    if (SDL_AddBasicVideoDisplay(&mode) == 0) {
        return -1;
    }
    return 0;
}

void PSP_VideoQuit(SDL_VideoDevice *_this)
{
    PSP_EventQuit(_this);
}

int PSP_GetDisplayModes(SDL_VideoDevice *_this, SDL_VideoDisplay *display)
{
    SDL_DisplayMode mode;

    SDL_zero(mode);
    mode.w = 480;
    mode.h = 272;
    mode.refresh_rate = 60.0f;

    /* 32 bpp for default */
    mode.format = SDL_PIXELFORMAT_ABGR8888;
    SDL_AddFullscreenDisplayMode(display, &mode);

    /* 16 bpp secondary mode */
    mode.format = SDL_PIXELFORMAT_BGR565;
    SDL_AddFullscreenDisplayMode(display, &mode);
    return 0;
}

int PSP_SetDisplayMode(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}

#define EGLCHK(stmt)                           \
    do {                                       \
        EGLint err;                            \
                                               \
        stmt;                                  \
        err = eglGetError();                   \
        if (err != EGL_SUCCESS) {              \
            SDL_SetError("EGL error %d", err); \
            return 0;                          \
        }                                      \
    } while (0)

int PSP_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_WindowData *wdata;

    /* Allocate window internal data */
    wdata = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!wdata) {
        return -1;
    }

    /* Setup driver data for this window */
    window->driverdata = wdata;

    SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    return 0;
}

void PSP_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window)
{
}
int PSP_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window)
{
    return SDL_Unsupported();
}
void PSP_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
}
void PSP_ShowWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}
void PSP_HideWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}
void PSP_RaiseWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}
void PSP_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}
void PSP_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}
void PSP_RestoreWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}
void PSP_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}

/* TO Write Me */
SDL_bool PSP_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    return SDL_FALSE;
}
void PSP_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
}
void PSP_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
}
SDL_bool PSP_IsScreenKeyboardShown(SDL_VideoDevice *_this, SDL_Window *window)
{
    return SDL_FALSE;
}

#endif /* SDL_VIDEO_DRIVER_PSP */
