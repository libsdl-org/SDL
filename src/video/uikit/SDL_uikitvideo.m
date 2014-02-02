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

#if SDL_VIDEO_DRIVER_UIKIT

#import <UIKit/UIKit.h>

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_uikitvideo.h"
#include "SDL_uikitevents.h"
#include "SDL_uikitmodes.h"
#include "SDL_uikitwindow.h"
#include "SDL_uikitopengles.h"

#define UIKITVID_DRIVER_NAME "uikit"

/* Initialization/Query functions */
static int UIKit_VideoInit(_THIS);
static void UIKit_VideoQuit(_THIS);

/* DUMMY driver bootstrap functions */

static int
UIKit_Available(void)
{
    return 1;
}

static void UIKit_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_free(device);
}

static SDL_VideoDevice *
UIKit_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_free(device);
        SDL_OutOfMemory();
        return (0);
    }

    /* Set the function pointers */
    device->VideoInit = UIKit_VideoInit;
    device->VideoQuit = UIKit_VideoQuit;
    device->GetDisplayModes = UIKit_GetDisplayModes;
    device->SetDisplayMode = UIKit_SetDisplayMode;
    device->PumpEvents = UIKit_PumpEvents;
    device->CreateWindow = UIKit_CreateWindow;
    device->ShowWindow = UIKit_ShowWindow;
    device->HideWindow = UIKit_HideWindow;
    device->RaiseWindow = UIKit_RaiseWindow;
    device->SetWindowFullscreen = UIKit_SetWindowFullscreen;
    device->DestroyWindow = UIKit_DestroyWindow;
    device->GetWindowWMInfo = UIKit_GetWindowWMInfo;

    /* !!! FIXME: implement SetWindowBordered */

#if SDL_IPHONE_KEYBOARD
    device->HasScreenKeyboardSupport = UIKit_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard = UIKit_ShowScreenKeyboard;
    device->HideScreenKeyboard = UIKit_HideScreenKeyboard;
    device->IsScreenKeyboardShown = UIKit_IsScreenKeyboardShown;
    device->SetTextInputRect = UIKit_SetTextInputRect;
#endif

    /* OpenGL (ES) functions */
    device->GL_MakeCurrent        = UIKit_GL_MakeCurrent;
    device->GL_SwapWindow        = UIKit_GL_SwapWindow;
    device->GL_CreateContext    = UIKit_GL_CreateContext;
    device->GL_DeleteContext    = UIKit_GL_DeleteContext;
    device->GL_GetProcAddress   = UIKit_GL_GetProcAddress;
    device->GL_LoadLibrary        = UIKit_GL_LoadLibrary;
    device->free = UIKit_DeleteDevice;

    device->gl_config.accelerated = 1;

    return device;
}

VideoBootStrap UIKIT_bootstrap = {
    UIKITVID_DRIVER_NAME, "SDL UIKit video driver",
    UIKit_Available, UIKit_CreateDevice
};


int
UIKit_VideoInit(_THIS)
{
    _this->gl_config.driver_loaded = 1;

    if (UIKit_InitModes(_this) < 0) {
        return -1;
    }
    return 0;
}

void
UIKit_VideoQuit(_THIS)
{
    UIKit_QuitModes(_this);
}

/*
 * iOS log support.
 *
 * This doesn't really have aything to do with the interfaces of the SDL video
 *  subsystem, but we need to stuff this into an Objective-C source code file.
 */

void SDL_NSLog(const char *text)
{
    NSLog(@"%s", text);
}

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
