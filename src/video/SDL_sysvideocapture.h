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

#ifndef SDL_sysvideocapture_h_
#define SDL_sysvideocapture_h_

#include "../SDL_list.h"

/* The SDL video_capture driver */
typedef struct SDL_VideoCaptureDevice SDL_VideoCaptureDevice;

/* Define the SDL video_capture driver structure */
struct SDL_VideoCaptureDevice
{
    /* * * */
    /* Data common to all devices */

    /* The device's current video_capture specification */
    SDL_VideoCaptureSpec spec;

    /* Device name */
    char *dev_name;

    /* Current state flags */
    SDL_AtomicInt shutdown;
    SDL_AtomicInt enabled;
    SDL_bool is_spec_set;

    /* A mutex for locking the queue buffers */
    SDL_Mutex *device_lock;
    SDL_Mutex *acquiring_lock;

    /* A thread to feed the video_capture device */
    SDL_Thread *thread;
    SDL_ThreadID threadid;

    /* Queued buffers (if app not using callback). */
    SDL_ListNode *buffer_queue;

    /* * * */
    /* Data private to this driver */
    struct SDL_PrivateVideoCaptureData *hidden;
};

extern int SDL_SYS_VideoCaptureInit(void);
extern int SDL_SYS_VideoCaptureQuit(void);

extern int OpenDevice(SDL_VideoCaptureDevice *_this);
extern void CloseDevice(SDL_VideoCaptureDevice *_this);

extern int InitDevice(SDL_VideoCaptureDevice *_this);

extern int GetDeviceSpec(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureSpec *spec);

extern int StartCapture(SDL_VideoCaptureDevice *_this);
extern int StopCapture(SDL_VideoCaptureDevice *_this);

extern int AcquireFrame(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureFrame *frame);
extern int ReleaseFrame(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureFrame *frame);

extern int GetNumFormats(SDL_VideoCaptureDevice *_this);
extern int GetFormat(SDL_VideoCaptureDevice *_this, int index, Uint32 *format);

extern int GetNumFrameSizes(SDL_VideoCaptureDevice *_this, Uint32 format);
extern int GetFrameSize(SDL_VideoCaptureDevice *_this, Uint32 format, int index, int *width, int *height);

extern int GetDeviceName(SDL_VideoCaptureDeviceID instance_id, char *buf, int size);
extern SDL_VideoCaptureDeviceID *GetVideoCaptureDevices(int *count);

extern SDL_bool check_all_device_closed(void);
extern SDL_bool check_device_playing(void);

#endif /* SDL_sysvideocapture_h_ */
