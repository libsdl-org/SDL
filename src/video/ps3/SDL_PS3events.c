/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

/* Video driver implementation for PS3 */

#include "../../events/SDL_events_c.h"
#include "SDL_PS3events_c.h"
#include "SDL_PS3keyboard_c.h"
#include "SDL_PS3mouse_c.h"
#include "SDL_PS3video.h"

#include <sysutil/sysutil.h>

static void sysWindowCallback(u64 event, u64 param, void *userdata)
{
    SDL_VideoDevice *_this = (SDL_VideoDevice *)userdata;
    SDL_Window *window = NULL;

    // There should only be one window
    if (_this->num_displays == 1) {
        if (_this->displays[0]->fullscreen_window != NULL) {
            window = _this->displays[0]->fullscreen_window;
        }
    }

    switch (event) {
    case SYSUTIL_MENU_OPEN:
        // XMB opened
        if (window) {
            SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MOUSE_LEAVE, 0, 0);
        }
        break;
    case SYSUTIL_MENU_CLOSE:
        // XMB closed
        if (window) {
            SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MOUSE_ENTER, 0, 0);
        }
        break;
    case SYSUTIL_DRAW_BEGIN:
        break;
    case SYSUTIL_DRAW_END:
        break;
    default:
        break;
    }
}

static void videoOutHandler(u32 slot, u32 videoOut, u32 deviceIndex, u32 event, videoOutDeviceInfo *info, void *userData)
{
    // SDL_VideoDevice *_this = (SDL_VideoDevice*)userData;

    // // Process event
    // switch (event) {
    //     default:
    //         break;
    // }
}

void PS3_PumpEvents(SDL_VideoDevice *_this)
{
    sysUtilCheckCallback();
    // PS3_PumpKeyboard(_this);
    // PS3_PumpMouse();
}

void PS3_InitSysEvent(SDL_VideoDevice *_this)
{
    // This callbach register should be used when application will
    // change resoulution.
    // videoOutRegisterCallback(0, videoOutHandler, _this);

    // Init window events handler.
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT1, sysWindowCallback, _this);

    // PS3_InitKeyboard(_this);
    // PS3_InitMouse();
}

void PS3_QuitSysEvent(SDL_VideoDevice *_this)
{
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT1);
    // videoOutUnregisterCallback(0);
    videoOutConfigure(0, NULL, NULL, 0); // reset video output
    // PS3_QuitKeyboard(_this);
    // PS3_QuitMouse();
}

#endif

/* vi: set ts=4 sw=4 expandtab: */
