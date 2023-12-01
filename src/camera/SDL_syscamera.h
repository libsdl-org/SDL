/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
#include "../SDL_internal.h"

#ifndef SDL_syscamera_h_
#define SDL_syscamera_h_

#include "../SDL_list.h"

#define DEBUG_CAMERA 1

// The SDL camera driver
typedef struct SDL_CameraDevice SDL_CameraDevice;

// Define the SDL camera driver structure
struct SDL_CameraDevice
{
    // The device's current camera specification
    SDL_CameraSpec spec;

    // Device name
    char *dev_name;

    // Current state flags
    SDL_AtomicInt shutdown;
    SDL_AtomicInt enabled;
    SDL_bool is_spec_set;

    // A mutex for locking the queue buffers
    SDL_Mutex *device_lock;
    SDL_Mutex *acquiring_lock;

    // A thread to feed the camera device
    SDL_Thread *thread;
    SDL_ThreadID threadid;

    // Queued buffers (if app not using callback).
    SDL_ListNode *buffer_queue;

    // Data private to this driver
    struct SDL_PrivateCameraData *hidden;
};

typedef struct SDL_CameraDriverImpl
{
    void (*DetectDevices)(void);
    int (*OpenDevice)(SDL_CameraDevice *_this);
    void (*CloseDevice)(SDL_CameraDevice *_this);
    int (*InitDevice)(SDL_CameraDevice *_this);
    int (*GetDeviceSpec)(SDL_CameraDevice *_this, SDL_CameraSpec *spec);
    int (*StartCamera)(SDL_CameraDevice *_this);
    int (*StopCamera)(SDL_CameraDevice *_this);
    int (*AcquireFrame)(SDL_CameraDevice *_this, SDL_CameraFrame *frame);
    int (*ReleaseFrame)(SDL_CameraDevice *_this, SDL_CameraFrame *frame);
    int (*GetNumFormats)(SDL_CameraDevice *_this);
    int (*GetFormat)(SDL_CameraDevice *_this, int index, Uint32 *format);
    int (*GetNumFrameSizes)(SDL_CameraDevice *_this, Uint32 format);
    int (*GetFrameSize)(SDL_CameraDevice *_this, Uint32 format, int index, int *width, int *height);
    int (*GetDeviceName)(SDL_CameraDeviceID instance_id, char *buf, int size);
    SDL_CameraDeviceID *(*GetDevices)(int *count);
    void (*Deinitialize)(void);
} SDL_CameraDriverImpl;

typedef struct SDL_CameraDriver
{
    const char *name;  // The name of this camera driver
    const char *desc;  // The description of this camera driver
    SDL_CameraDriverImpl impl; // the backend's interface
} SDL_CameraDriver;

typedef struct CameraBootStrap
{
    const char *name;
    const char *desc;
    SDL_bool (*init)(SDL_CameraDriverImpl *impl);
    SDL_bool demand_only; // if SDL_TRUE: request explicitly, or it won't be available.
} CameraBootStrap;

// Not all of these are available in a given build. Use #ifdefs, etc.
extern CameraBootStrap DUMMYCAMERA_bootstrap;
extern CameraBootStrap V4L2_bootstrap;
extern CameraBootStrap COREMEDIA_bootstrap;
extern CameraBootStrap ANDROIDCAMERA_bootstrap;

#endif // SDL_syscamera_h_
