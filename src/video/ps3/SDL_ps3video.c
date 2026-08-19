/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_PS3

/* PS3 SDL video driver implementation, Based on PSL1GHT implementation,
 * itself based on dummy driver.
 *
 * Initial work by Ryan C. Gordon (icculus@icculus.org). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "DUMMY" by Sam Lantinga.
 */

#include "../../events/SDL_events_c.h"
#include "../SDL_pixels_c.h"
#include "../SDL_sysvideo.h"

#include "SDL_ps3events_c.h"
#include "SDL_ps3modes_c.h"
#include "SDL_ps3video.h"

#include <stdlib.h>

#include <rsx/gcm_sys.h>
#include <rsx/mm.h>
#include <rsx/resc.h>
#include <rsx/rsx.h>

#define PS3VID_DRIVER_NAME "ps3"

#define CB_SIZE         0x200000  // 2MB command buffer
#define RSX_BUFFER_SIZE 0x1000000 // 16MB — must be power of 2

bool PS3_VideoInit(SDL_VideoDevice *_this)
{
    PS3_InitSysEvent(_this);

    bool ret = PS3_InitModes(_this);
    if (!ret) {
        return SDL_SetError("PS3 video init");
    }

    // Wait for VSYNC to flip
    gcmSetFlipMode(RESC_DISPLAY_VSYNC);

    return true;
}

void PS3_VideoQuit(SDL_VideoDevice *_this)
{
    PS3_QuitModes(_this);
    PS3_QuitSysEvent(_this);
    SDL_free(_this->internal);
}

static void PS3_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

bool PS3_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_WindowData *wdata;

    // Allocate window internal data
    wdata = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (wdata == NULL) {
        return false;
    }

    // Setup driver data for this window
    window->internal = wdata;
    SDL_SetKeyboardFocus(window);

    return true;
}

int PS3_CreateWindowFrom(SDL_VideoDevice *_this, SDL_Window *window, const void *data)
{
    return SDL_Unsupported();
}

void PS3_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window)
{
}

bool PS3_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window)
{
    return true;
}

void PS3_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
}

void PS3_ShowWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}

void PS3_HideWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}

void PS3_RaiseWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}

void PS3_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}

void PS3_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}

void PS3_RestoreWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}

void PS3_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
}

bool PS3_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    return false;
}

void PS3_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID props)
{
}

void PS3_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
}

static SDL_VideoDevice *PS3_CreateDevice(void)
{
    SDL_VideoDevice *device;

    // Initialize all variables that we clean on shutdown
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device) {
        SDL_memset(device, 0, (sizeof *device));
    } else {
        SDL_OutOfMemory();
        SDL_free(device);
        return (0);
    }

    SDL_VideoData *devdata = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if (!devdata) {
        SDL_OutOfMemory();
        return false;
    }

    // Initialize GPU before any videoOut calls
    gcmInitDefaultFifoMode(GCM_DEFAULT_FIFO_MODE_CONDITIONAL);

    u32 bufferSize = rsxAlign(HOST_ADDR_ALIGNMENT, (DEFAULT_CB_SIZE + HOST_SIZE));
    void *hostAddr = memalign(HOST_ADDR_ALIGNMENT, bufferSize);
    devdata->_CommandBuffer = NULL;

    s32 ret = rsxInit(
        &devdata->_CommandBuffer,
        DEFAULT_CB_SIZE,
        bufferSize,
        hostAddr
    );

    if (ret != 0 || devdata->_CommandBuffer == NULL) {
        return false;
    }

    device->internal = (SDL_VideoData *)devdata;
    device->VideoInit = PS3_VideoInit;
    device->VideoQuit = PS3_VideoQuit;
    device->GetDisplayModes = PS3_GetDisplayModes;
    device->SetDisplayMode = PS3_SetDisplayMode;
    device->CreateSDLWindow = PS3_CreateWindow;
    device->SetWindowTitle = PS3_SetWindowTitle;
    device->SetWindowPosition = PS3_SetWindowPosition;
    device->SetWindowSize = PS3_SetWindowSize;
    device->ShowWindow = PS3_ShowWindow;
    device->HideWindow = PS3_HideWindow;
    device->RaiseWindow = PS3_RaiseWindow;
    device->MaximizeWindow = PS3_MaximizeWindow;
    device->MinimizeWindow = PS3_MinimizeWindow;
    device->RestoreWindow = PS3_RestoreWindow;
    device->DestroyWindow = PS3_DestroyWindow;
    device->HasScreenKeyboardSupport = PS3_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard = PS3_ShowScreenKeyboard;
    device->HideScreenKeyboard = PS3_HideScreenKeyboard;
    device->PumpEvents = PS3_PumpEvents;
    device->free = PS3_DeleteDevice;

    return device;
}

VideoBootStrap PS3_bootstrap = {
    PS3VID_DRIVER_NAME,
    "SDL ps3 video driver",
    PS3_CreateDevice,
    NULL,
    false
};

#endif // SDL_VIDEO_DRIVER_PS3
