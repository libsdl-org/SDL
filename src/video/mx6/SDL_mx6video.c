/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_MX6

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"

#ifdef SDL_INPUT_LINUXEV
#include "../../core/linux/SDL_evdev.h"
#endif

#include "SDL_mx6video.h"
#include "SDL_mx6events_c.h"
#include "SDL_mx6opengles.h"

static int
MX6_Available(void)
{
    return 1;
}

static void
MX6_Destroy(SDL_VideoDevice * device)
{
    if (device->driverdata != NULL) {
        device->driverdata = NULL;
    }
}

static SDL_VideoDevice *
MX6_Create()
{
    SDL_VideoDevice *device;
    SDL_VideoData *phdata;

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Initialize internal data */
    phdata = (SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
    if (phdata == NULL) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }

    device->driverdata = phdata;

    /* Setup amount of available displays and current display */
    device->num_displays = 0;

    /* Set device free function */
    device->free = MX6_Destroy;

    /* Setup all functions which we can handle */
    device->VideoInit = MX6_VideoInit;
    device->VideoQuit = MX6_VideoQuit;
    device->GetDisplayModes = MX6_GetDisplayModes;
    device->SetDisplayMode = MX6_SetDisplayMode;
    device->CreateWindow = MX6_CreateWindow;
    device->CreateWindowFrom = MX6_CreateWindowFrom;
    device->SetWindowTitle = MX6_SetWindowTitle;
    device->SetWindowIcon = MX6_SetWindowIcon;
    device->SetWindowPosition = MX6_SetWindowPosition;
    device->SetWindowSize = MX6_SetWindowSize;
    device->ShowWindow = MX6_ShowWindow;
    device->HideWindow = MX6_HideWindow;
    device->RaiseWindow = MX6_RaiseWindow;
    device->MaximizeWindow = MX6_MaximizeWindow;
    device->MinimizeWindow = MX6_MinimizeWindow;
    device->RestoreWindow = MX6_RestoreWindow;
    device->SetWindowGrab = MX6_SetWindowGrab;
    device->DestroyWindow = MX6_DestroyWindow;
    device->GetWindowWMInfo = MX6_GetWindowWMInfo;

    device->GL_LoadLibrary = MX6_GLES_LoadLibrary;
    device->GL_GetProcAddress = MX6_GLES_GetProcAddress;
    device->GL_UnloadLibrary = MX6_GLES_UnloadLibrary;
    device->GL_CreateContext = MX6_GLES_CreateContext;
    device->GL_MakeCurrent = MX6_GLES_MakeCurrent;
    device->GL_SetSwapInterval = MX6_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = MX6_GLES_GetSwapInterval;
    device->GL_SwapWindow = MX6_GLES_SwapWindow;
    device->GL_DeleteContext = MX6_GLES_DeleteContext;

    device->PumpEvents = MX6_PumpEvents;

    return device;
}

VideoBootStrap MX6_bootstrap = {
    "MX6",
    "Freescale i.MX6 Video Driver",
    MX6_Available,
    MX6_Create
};

static void
MX6_UpdateDisplay(_THIS)
{
    SDL_VideoDisplay *display = &_this->displays[0];
    SDL_DisplayData *data = (SDL_DisplayData*)display->driverdata;
    EGLNativeDisplayType native_display = egl_viv_data->fbGetDisplayByIndex(0);
    SDL_DisplayMode current_mode;
    int pitch, bpp;
    unsigned long pixels;

    /* Store the native EGL display */
    data->native_display = native_display;

    SDL_zero(current_mode);
    egl_viv_data->fbGetDisplayInfo(native_display, &current_mode.w, &current_mode.h, &pixels, &pitch, &bpp);
    /* FIXME: How do we query refresh rate? */
    current_mode.refresh_rate = 60;

    switch (bpp)
    {
    default: /* Is another format used? */
    case 16:
        current_mode.format = SDL_PIXELFORMAT_RGB565;
        break;
    }

    display->desktop_mode = current_mode;
    display->current_mode = current_mode;
}

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int
MX6_VideoInit(_THIS)
{
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;
    SDL_DisplayData *data;
    
    data = (SDL_DisplayData *) SDL_calloc(1, sizeof(SDL_DisplayData));
    if (data == NULL) {
        return SDL_OutOfMemory();
    }

    SDL_zero(display);
    SDL_zero(current_mode);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = data;
    SDL_AddVideoDisplay(&display);

    if (SDL_GL_LoadLibrary(NULL) < 0) {
        return -1;
    }
    MX6_UpdateDisplay(_this);

#ifdef SDL_INPUT_LINUXEV    
    SDL_EVDEV_Init();
#endif    

    return 1;
}

void
MX6_VideoQuit(_THIS)
{
#ifdef SDL_INPUT_LINUXEV    
    SDL_EVDEV_Quit();
#endif    
}

void
MX6_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    /* Only one display mode available, the current one */
    SDL_AddDisplayMode(display, &display->current_mode);
}

int
MX6_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

int
MX6_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_DisplayData *displaydata;
    SDL_WindowData *wdata;
    
    displaydata = SDL_GetDisplayDriverData(0);
    
    /* Allocate window internal data */
    wdata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (wdata == NULL) {
        return SDL_OutOfMemory();
    }

    /* Setup driver data for this window */
    window->driverdata = wdata;
    window->flags |= SDL_WINDOW_OPENGL;
    
    if (!_this->egl_data) {
        return -1;
    }

    wdata->native_window = egl_viv_data->fbCreateWindow(displaydata->native_display, window->x, window->y, window->w, window->h);    
    if (!wdata->native_window) {
        return SDL_SetError("MX6: Can't create native window");
    }

    wdata->egl_surface = SDL_EGL_CreateSurface(_this, wdata->native_window);
    if (wdata->egl_surface == EGL_NO_SURFACE) {
        return SDL_SetError("MX6: Can't create EGL surface");
    }

    /* Window has been successfully created */
    return 0;
}

void
MX6_DestroyWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata;
    
    wdata = window->driverdata;
    if (wdata) {
        SDL_EGL_DestroySurface(_this, wdata->egl_surface);
    }

    if (egl_viv_data) {
        egl_viv_data->fbDestroyWindow(wdata->native_window);
    }
}

int
MX6_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    return -1;
}

void
MX6_SetWindowTitle(_THIS, SDL_Window * window)
{
}
void
MX6_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
}
void
MX6_SetWindowPosition(_THIS, SDL_Window * window)
{
}
void
MX6_SetWindowSize(_THIS, SDL_Window * window)
{
}
void
MX6_ShowWindow(_THIS, SDL_Window * window)
{
}
void
MX6_HideWindow(_THIS, SDL_Window * window)
{
}
void
MX6_RaiseWindow(_THIS, SDL_Window * window)
{
}
void
MX6_MaximizeWindow(_THIS, SDL_Window * window)
{
}
void
MX6_MinimizeWindow(_THIS, SDL_Window * window)
{
}
void
MX6_RestoreWindow(_THIS, SDL_Window * window)
{
}
void
MX6_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
SDL_bool
MX6_GetWindowWMInfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
{
    return SDL_TRUE;
}

#endif /* SDL_VIDEO_DRIVER_MX6 */

/* vi: set ts=4 sw=4 expandtab: */
