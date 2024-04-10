/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License,Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "../../SDL_internal.h"
#ifdef SDL_VIDEO_DRIVER_OHOS
#if SDL_VIDEO_DRIVER_OHOS
#endif
/* OHOS SDL video driver implementation */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_hints.h"
#include "SDL_ohosvideo.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "../../core/ohos/SDL_ohos.h"
#include "SDL_ohosgl.h"
#include "SDL_ohoswindow.h"
#include "SDL_keyboard.h"
#include "SDL_ohosevents.h"
#include "SDL_ohosvulkan.h"

#define OHOS_VID_DRIVER_NAME "OHOS"

/* Initialization/Query functions */
static int OHOS_VideoInit(SDL_VideoDevice *_this);
static void OHOS_VideoQuit(SDL_VideoDevice *_this);
int OHOS_GetDisplayDPI(SDL_VideoDevice *_this, SDL_VideoDisplay *display, float *ddpi, float *hdpi, float *vdpi);

#include "../SDL_egl_c.h"
#define OHOS_GLES_GetProcAddress SDL_EGL_GetProcAddress
#define OHOS_GLES_UnloadLibrary SDL_EGL_UnloadLibrary
#define OHOS_GLES_SetSwapInterval SDL_EGL_SetSwapInterval
#define OHOS_GLES_GetSwapInterval SDL_EGL_GetSwapInterval
#define OHOS_GLES_DeleteContext SDL_EGL_DeleteContext

/* OHOS driver bootstrap functions */


/* These are filled in with real values in OHOS_SetScreenResolution on init (before SDL_main()) */
int g_ohosSurfaceWidth           = 0;
int g_ohosSurfaceHeight          = 0;
int g_ohosDeviceWidth            = 0;
int g_ohosDeviceHeight           = 0;
static Uint32 OHOS_ScreenFormat = SDL_PIXELFORMAT_UNKNOWN;
static int OHOS_ScreenRate      = 0;
SDL_sem *OHOS_PauseSem          = NULL;
SDL_sem *OHOS_ResumeSem         = NULL;
SDL_mutex *OHOS_PageMutex       = NULL;

static int OHOS_Available(void)
{
    return 1;
}

static void OHOS_SuspendScreenSaver(SDL_VideoDevice *_this)
{
}

static void OHOS_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device->driverdata);
    SDL_free(device);
}

static void OHOS_SetDevice(SDL_VideoDevice *device)
{
    SDL_bool block_on_pause;
    
    /* Set the function pointers */
    device->VideoInit = OHOS_VideoInit;
    device->VideoQuit = OHOS_VideoQuit;
    block_on_pause = SDL_GetHintBoolean(SDL_HINT_OHOS_BLOCK_ON_PAUSE, SDL_TRUE);
    if (block_on_pause) {
        device->PumpEvents = OHOS_PUMPEVENTS_Blocking;
    } else {
        device->PumpEvents = OHOS_PUMPEVENTS_NonBlocking;
    }

    device->GetDisplayDPI = OHOS_GetDisplayDPI;
    device->CreateSDLWindow = OHOS_CreateWindow;
    device->CreateSDLWindowFrom = OHOS_CreateWindowFrom;
    device->SetWindowTitle = OHOS_SetWindowTitle;
    device->SetWindowFullscreen = OHOS_SetWindowFullscreen;
    device->MinimizeWindow = OHOS_MinimizeWindow;
    device->DestroyWindow = OHOS_DestroyWindow;
    device->GetWindowWMInfo = OHOS_GetWindowWMInfo;
    device->free = OHOS_DeleteDevice;

    /* GL pointers */
    device->GL_LoadLibrary = OHOS_GLES_LoadLibrary;
    device->GL_GetProcAddress = OHOS_GLES_GetProcAddress;
    device->GL_UnloadLibrary = OHOS_GLES_UnloadLibrary;
    device->GL_CreateContext = OHOS_GLES_CreateContext;
    device->GL_MakeCurrent = OHOS_GLES_MakeCurrent;
    device->GL_SetSwapInterval = OHOS_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = OHOS_GLES_GetSwapInterval;
    device->GL_SwapWindow = OHOS_GLES_SwapWindow;
    device->GL_DeleteContext = OHOS_GLES_DeleteContext;

#if SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = OHOS_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = OHOS_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = OHOS_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = OHOS_Vulkan_CreateSurface;
#endif

    /* Screensaver */
    device->SuspendScreenSaver = OHOS_SuspendScreenSaver;
}

static SDL_VideoDevice* OHOS_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;
    SDL_VideoData *data;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
    if (!data) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }

    device->driverdata = data;
    OHOS_SetDevice(device);
    return device;
}

VideoBootStrap OHOS_bootstrap = {
    OHOS_VID_DRIVER_NAME, "SDL OHOS video driver",
    OHOS_Available, OHOS_CreateDevice
};


int OHOS_VideoInit(SDL_VideoDevice *_this)
{
    SDL_VideoData *videodata = (SDL_VideoData *)_this->driverdata;
    int display_index;
    SDL_VideoDisplay *display;
    SDL_DisplayMode mode;

    videodata->isPaused  = SDL_FALSE;
    videodata->isPausing = SDL_FALSE;

    mode.format          = OHOS_ScreenFormat;
    mode.w               = g_ohosDeviceWidth;
    mode.h               = g_ohosDeviceHeight;
    mode.refresh_rate    = OHOS_ScreenRate;
    mode.driverdata      = NULL;

    display_index = SDL_AddBasicVideoDisplay(&mode);
    if (display_index < 0) {
        return -1;
    }
    display = SDL_GetDisplay(display_index);
    display->orientation = OHOS_GetDisplayOrientation();

    SDL_AddDisplayMode(&_this->displays[0], &mode);

    OHOS_InitTouch();

    OHOS_InitMouse();

    /* We're done! */
    return 0;
}

void OHOS_VideoQuit(SDL_VideoDevice *_this)
{
    OHOS_QuitMouse();
    OHOS_QuitTouch();
}

int OHOS_GetDisplayDPI(SDL_VideoDevice *_this, SDL_VideoDisplay *display, float *ddpi, float *hdpi, float *vdpi)
{
    return 0;
}

void OHOS_SetScreenResolution(int deviceWidth, int deviceHeight, Uint32 format, float rate)
{
    OHOS_ScreenFormat  = format;
    OHOS_ScreenRate    = (int)rate;
    g_ohosDeviceWidth   = deviceWidth;
    g_ohosDeviceHeight  = deviceHeight;
}

void OHOS_SetScreenSize(int surfaceWidth, int surfaceHeight)
{
    g_ohosSurfaceWidth  = surfaceWidth;
    g_ohosSurfaceHeight = surfaceHeight;
}


void OHOS_SendResize(SDL_Window *window)
{
    /*
      Update the resolution of the desktop mode, so that the window
      can be properly resized. The screen resolution change can for
      which can happen after VideoInit().
    */
    SDL_VideoDevice *device = SDL_GetVideoDevice();
    if (device && device->num_displays > 0) {
        SDL_VideoDisplay *display          = &device->displays[0];
        display->desktop_mode.format       = OHOS_ScreenFormat;
        display->desktop_mode.w            = g_ohosDeviceWidth;
        display->desktop_mode.h            = g_ohosDeviceHeight;
        display->desktop_mode.refresh_rate = OHOS_ScreenRate;
    }

    if (window) {
        /* Force the current mode to match the resize otherwise the SDL_WINDOWEVENT_RESTORED event
         * will fall back to the old mode */
        SDL_VideoDisplay *display              = SDL_GetDisplayForWindow(window);
        display->display_modes[0].format       = OHOS_ScreenFormat;
        display->display_modes[0].w            = g_ohosDeviceWidth;
        display->display_modes[0].h            = g_ohosDeviceHeight;
        display->display_modes[0].refresh_rate = OHOS_ScreenRate;
        display->current_mode                  = display->display_modes[0];

        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, g_ohosSurfaceWidth, g_ohosSurfaceHeight);
    }
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
