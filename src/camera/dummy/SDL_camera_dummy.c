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

#ifdef SDL_CAMERA_DRIVER_DUMMY

#include "../SDL_syscamera.h"

static int DUMMYCAMERA_OpenDevice(SDL_CameraDevice *device, const SDL_CameraSpec *spec)
{
    return SDL_Unsupported();
}

static void DUMMYCAMERA_CloseDevice(SDL_CameraDevice *device)
{
}

static int DUMMYCAMERA_WaitDevice(SDL_CameraDevice *device)
{
    return SDL_Unsupported();
}

static int DUMMYCAMERA_AcquireFrame(SDL_CameraDevice *device, SDL_Surface *frame, Uint64 *timestampNS)
{
    return SDL_Unsupported();
}

static void DUMMYCAMERA_ReleaseFrame(SDL_CameraDevice *device, SDL_Surface *frame)
{
}

static void DUMMYCAMERA_DetectDevices(void)
{
}

static void DUMMYCAMERA_FreeDeviceHandle(SDL_CameraDevice *device)
{
}

static void DUMMYCAMERA_Deinitialize(void)
{
}

static SDL_bool DUMMYCAMERA_Init(SDL_CameraDriverImpl *impl)
{
    impl->DetectDevices = DUMMYCAMERA_DetectDevices;
    impl->OpenDevice = DUMMYCAMERA_OpenDevice;
    impl->CloseDevice = DUMMYCAMERA_CloseDevice;
    impl->WaitDevice = DUMMYCAMERA_WaitDevice;
    impl->AcquireFrame = DUMMYCAMERA_AcquireFrame;
    impl->ReleaseFrame = DUMMYCAMERA_ReleaseFrame;
    impl->FreeDeviceHandle = DUMMYCAMERA_FreeDeviceHandle;
    impl->Deinitialize = DUMMYCAMERA_Deinitialize;

    return SDL_TRUE;
}

CameraBootStrap DUMMYCAMERA_bootstrap = {
    "dummy", "SDL dummy camera driver", DUMMYCAMERA_Init, SDL_TRUE
};

#endif  // SDL_CAMERA_DRIVER_DUMMY
