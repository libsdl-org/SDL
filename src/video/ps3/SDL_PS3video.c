/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2010 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
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

#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_PS3video.h"
#include "SDL_PS3events_c.h"
#include "SDL_PS3modes_c.h"

#include <stdlib.h>
#include <assert.h>

#include <rsx/gcm_sys.h>
#include <rsx/resc.h>
#include <rsx/mm.h>
#include <rsx/rsx.h>

#define PS3VID_DRIVER_NAME "ps3"

#define CB_SIZE        0x200000   // 2MB command buffer
#define RSX_BUFFER_SIZE 0x1000000  // 16MB — must be power of 2

static bool PS3_VideoInit(SDL_VideoDevice *_this);
static void PS3_VideoQuit(SDL_VideoDevice *_this);

static void initializeGPU(SDL_DeviceData * devdata);

static int PS3_Available(void)
{
    return (1);
}

static void PS3_DeleteDevice(SDL_VideoDevice * device)
{
    deprintf (1, "PS3_DeleteDevice( %p)\n", device); fflush(stdout);
    SDL_free(device);
}

bool PS3_VideoInit(SDL_VideoDevice *_this)
{
    PS3_InitSysEvent(_this);

    PS3_InitModes(_this);

    // Wait for VSYNC to flip
    gcmSetFlipMode(RESC_DISPLAY_VSYNC);

    return true;
}

void PS3_VideoQuit(SDL_VideoDevice *_this)
{
    PS3_QuitModes(_this);
    PS3_QuitSysEvent(_this);
    SDL_free( _this->internal);
}

void initializeGPU( SDL_DeviceData * devdata)
{
    deprintf (1, "initializeGPU()\n");

    // Use system malloc with manual alignment instead of rsxMemalign
    void *raw = malloc(32*1024*1024 + (1024*1024));  // extra for alignment

    if (!raw) {
        deprintf(1, "initializeGPU: malloc FAILED\n");
        return;
    }

    // Manually align to 1MB boundary
    void *host_addr = (void *)(((uintptr_t)raw + (1024*1024 - 1)) & ~(uintptr_t)(1024*1024 - 1));
    if (!host_addr) {
        deprintf(1, "initializeGPU: rsxMemalign FAILED\n");
        return;
    }
    
    devdata->_CommandBuffer = NULL;
    int ret = gcmInitBody(&devdata->_CommandBuffer, 0x200000, 32*1024*1024, host_addr);
    
    if (ret != 0 || devdata->_CommandBuffer == NULL) {
        deprintf(1, "initializeGPU: gcmInitBody FAILED ret=%x\n", ret);
        return;
    }

    assert(devdata->_CommandBuffer != NULL);
}

bool PS3_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_WindowData *wdata;

    // Allocate window internal data
    wdata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (wdata == NULL) {
        return false;
    }

    // Setup driver data for this window
    window->internal = wdata;
    SDL_SetKeyboardFocus(window);

    return true;
}

int PS3_CreateWindowFrom(SDL_VideoDevice *_this, SDL_Window * window, const void *data)
{
    return SDL_Unsupported();
}

void PS3_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window * window)
{
}

bool PS3_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window * window)
{
    return true;
}

void PS3_SetWindowSize(SDL_VideoDevice *_this, SDL_Window * window)
{
}

void PS3_ShowWindow(SDL_VideoDevice *_this, SDL_Window * window)
{
}

void PS3_HideWindow(SDL_VideoDevice *_this, SDL_Window * window)
{
}

void PS3_RaiseWindow(SDL_VideoDevice *_this, SDL_Window * window)
{
}

void PS3_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window * window)
{
}

void PS3_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window * window)
{
}

void PS3_RestoreWindow(SDL_VideoDevice *_this, SDL_Window * window)
{
}

void PS3_DestroyWindow(SDL_VideoDevice *_this, SDL_Window * window)
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
    deprintf (1, "PS3_CreateDevice( %s )\n", device->name);

    // Initialize all variables that we clean on shutdown
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device) {
        SDL_memset(device, 0, (sizeof *device));
    }
    else {
        SDL_OutOfMemory();
        SDL_free(device);
        return (0);
    }

    SDL_DeviceData *devdata = (SDL_DeviceData*)SDL_calloc(1, sizeof(SDL_DeviceData));
    if (!devdata) {
        SDL_OutOfMemory();
        return false;
    }

    // Initialize GPU before any videoOut calls
    initializeGPU(devdata);

    device->internal = (SDL_DeviceData*) devdata;
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

/* vi: set ts=4 sw=4 expandtab: */
