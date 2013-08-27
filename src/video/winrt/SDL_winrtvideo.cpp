/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2012 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_config.h"

#if SDL_VIDEO_DRIVER_WINRT

/* WinRT SDL video driver implementation

   Initial work on this was done by David Ludwig (dludwig@pobox.com), and
   was based off of SDL's "dummy" video driver.
 */

/* Windows includes */
#include <agile.h>
using namespace Windows::UI::Core;


/* SDL includes */
extern "C" {
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "../../render/SDL_sysrender.h"
#include "SDL_syswm.h"
}

#include "../../core/winrt/SDL_winrtapp.h"
#include "SDL_winrtevents_c.h"
#include "SDL_winrtmouse.h"

extern SDL_WinRTApp ^ SDL_WinRTGlobalApp;


/* Initialization/Query functions */
static int WINRT_VideoInit(_THIS);
static int WINRT_InitModes(_THIS);
static int WINRT_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
static void WINRT_VideoQuit(_THIS);


/* Window functions */
static int WINRT_CreateWindow(_THIS, SDL_Window * window);
static void WINRT_DestroyWindow(_THIS, SDL_Window * window);
static SDL_bool WINRT_GetWindowWMInfo(_THIS, SDL_Window * window, SDL_SysWMinfo * info);


/* Internal window data */
struct SDL_WindowData
{
    SDL_Window *sdlWindow;
    Platform::Agile<Windows::UI::Core::CoreWindow> coreWindow;
};


/* WinRT driver bootstrap functions */

static int
WINRT_Available(void)
{
    return (1);
}

static void
WINRT_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_WinRTGlobalApp->SetSDLVideoDevice(NULL);
    SDL_free(device);
}

static SDL_VideoDevice *
WINRT_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        if (device) {
            SDL_free(device);
        }
        return (0);
    }

    /* Set the function pointers */
    device->VideoInit = WINRT_VideoInit;
    device->VideoQuit = WINRT_VideoQuit;
    device->CreateWindow = WINRT_CreateWindow;
    device->DestroyWindow = WINRT_DestroyWindow;
    device->SetDisplayMode = WINRT_SetDisplayMode;
    device->PumpEvents = WINRT_PumpEvents;
    device->GetWindowWMInfo = WINRT_GetWindowWMInfo;
    device->free = WINRT_DeleteDevice;
    SDL_WinRTGlobalApp->SetSDLVideoDevice(device);

    return device;
}

#define WINRTVID_DRIVER_NAME "winrt"
VideoBootStrap WINRT_bootstrap = {
    WINRTVID_DRIVER_NAME, "SDL Windows RT video driver",
    WINRT_Available, WINRT_CreateDevice
};

int
WINRT_VideoInit(_THIS)
{
    // TODO, WinRT: consider adding a hack to wait (here) for the app's orientation to finish getting set (before the initial display mode is set up)

    if (WINRT_InitModes(_this) < 0) {
        return -1;
    }
    WINRT_InitMouse(_this);

    return 0;
}

static int
WINRT_InitModes(_THIS)
{
    SDL_DisplayMode mode = SDL_WinRTGlobalApp->CalcCurrentDisplayMode();
    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }

    SDL_AddDisplayMode(&_this->displays[0], &mode);
    return 0;
}

static int
WINRT_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

void
WINRT_VideoQuit(_THIS)
{
    WINRT_QuitMouse(_this);
}

int
WINRT_CreateWindow(_THIS, SDL_Window * window)
{
    // Make sure that only one window gets created, at least until multimonitor
    // support is added.
    if (SDL_WinRTGlobalApp->GetSDLWindow() != NULL) {
        SDL_SetError("WinRT only supports one window");
        return -1;
    }

    SDL_WindowData *data = new SDL_WindowData;
    if (!data) {
        SDL_OutOfMemory();
        return -1;
    }
    window->driverdata = data;
    data->sdlWindow = window;
    data->coreWindow = CoreWindow::GetForCurrentThread();

    /* Make sure the window is considered to be positioned at {0,0},
       and is considered fullscreen, shown, and the like.
    */
    window->x = 0;
    window->y = 0;
    window->flags =
        SDL_WINDOW_FULLSCREEN |
        SDL_WINDOW_SHOWN |
        SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_MAXIMIZED |
        SDL_WINDOW_INPUT_GRABBED;

    /* WinRT does not, as of this writing, appear to support app-adjustable
       window sizes.  Set the window size to whatever the native WinRT
       CoreWindow is set at.

       TODO, WinRT: if and when non-fullscreen XAML control support is added to SDL, consider making those resizable via SDL_Window's interfaces.
    */
    window->w = _this->displays[0].current_mode.w;
    window->h = _this->displays[0].current_mode.h;
 
    /* Make sure the WinRT app's IFramworkView can post events on
       behalf of SDL:
    */
    SDL_WinRTGlobalApp->SetSDLWindow(window);

    /* All done! */
    return 0;
}

void
WINRT_DestroyWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData * data = (SDL_WindowData *) window->driverdata;

    if (SDL_WinRTGlobalApp->GetSDLWindow() == window) {
        SDL_WinRTGlobalApp->SetSDLWindow(NULL);
    }

    if (data) {
        // Delete the internal window data:
        delete data;
        data = NULL;
    }
}

SDL_bool
WINRT_GetWindowWMInfo(_THIS, SDL_Window * window, SDL_SysWMinfo * info)
{
    SDL_WindowData * data = (SDL_WindowData *) window->driverdata;

    if (info->version.major <= SDL_MAJOR_VERSION) {
        info->subsystem = SDL_SYSWM_WINDOWSRT;
        info->info.winrt.window = reinterpret_cast<IUnknown *>(data->coreWindow.Get());
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }
    return SDL_FALSE;
}

#endif /* SDL_VIDEO_DRIVER_WINRT */

/* vi: set ts=4 sw=4 expandtab: */
