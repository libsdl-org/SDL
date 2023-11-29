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
#include "SDL_internal.h"

#include "SDL_syscamera.h"
#include "SDL_camera_c.h"
#include "../video/SDL_pixels_c.h"
#include "../thread/SDL_systhread.h"

#define DEBUG_CAMERA 1

// list node entries to share frames between SDL and user app
// !!! FIXME: do we need this struct?
typedef struct entry_t
{
    SDL_CameraFrame frame;
} entry_t;

static SDL_CameraDevice *open_devices[16];  // !!! FIXME: remove limit

static void CloseCameraDevice(SDL_CameraDevice *device)
{
    if (!device) {
        return;
    }

    SDL_AtomicSet(&device->shutdown, 1);
    SDL_AtomicSet(&device->enabled, 1);

    if (device->thread != NULL) {
        SDL_WaitThread(device->thread, NULL);
    }
    if (device->device_lock != NULL) {
        SDL_DestroyMutex(device->device_lock);
    }
    if (device->acquiring_lock != NULL) {
        SDL_DestroyMutex(device->acquiring_lock);
    }

    const int n = SDL_arraysize(open_devices);
    for (int i = 0; i < n; i++) {
        if (open_devices[i] == device) {
            open_devices[i] = NULL;
        }
    }

    entry_t *entry = NULL;
    while (device->buffer_queue != NULL) {
        SDL_ListPop(&device->buffer_queue, (void**)&entry);
        if (entry) {
            SDL_CameraFrame f = entry->frame;
            // Release frames not acquired, if any
            if (f.timestampNS) {
                ReleaseFrame(device, &f);
            }
            SDL_free(entry);
        }
    }

    CloseDevice(device);

    SDL_free(device->dev_name);
    SDL_free(device);
}

// Tell if all devices are closed
SDL_bool CheckAllDeviceClosed(void)
{
    const int n = SDL_arraysize(open_devices);
    int all_closed = SDL_TRUE;
    for (int i = 0; i < n; i++) {
        if (open_devices[i]) {
            all_closed = SDL_FALSE;
            break;
        }
    }
    return all_closed;
}

// Tell if at least one device is in playing state
SDL_bool CheckDevicePlaying(void)
{
    const int n = SDL_arraysize(open_devices);
    for (int i = 0; i < n; i++) {
        if (open_devices[i]) {
            if (SDL_GetCameraStatus(open_devices[i]) == SDL_CAMERA_PLAYING) {
                return SDL_TRUE;
            }
        }
    }
    return SDL_FALSE;
}

void SDL_CloseCamera(SDL_CameraDevice *device)
{
    if (!device) {
        SDL_InvalidParamError("device");
    } else {
        CloseCameraDevice(device);
    }
}

int SDL_StartCamera(SDL_CameraDevice *device)
{
    if (!device) {
        return SDL_InvalidParamError("device");
    } else if (device->is_spec_set == SDL_FALSE) {
        return SDL_SetError("no spec set");
    } else if (SDL_GetCameraStatus(device) != SDL_CAMERA_INIT) {
        return SDL_SetError("invalid state");
    }

    const int result = StartCamera(device);
    if (result < 0) {
        return result;
    }

    SDL_AtomicSet(&device->enabled, 1);

    return 0;
}

int SDL_GetCameraSpec(SDL_CameraDevice *device, SDL_CameraSpec *spec)
{
    if (!device) {
        return SDL_InvalidParamError("device");
    } else if (!spec) {
        return SDL_InvalidParamError("spec");
    }

    SDL_zerop(spec);
    return GetDeviceSpec(device, spec);
}

int SDL_StopCamera(SDL_CameraDevice *device)
{
    if (!device) {
        return SDL_InvalidParamError("device");
    } else if (SDL_GetCameraStatus(device) != SDL_CAMERA_PLAYING) {
        return SDL_SetError("invalid state");
    }

    SDL_AtomicSet(&device->enabled, 0);
    SDL_AtomicSet(&device->shutdown, 1);

    SDL_LockMutex(device->acquiring_lock);
    const int retval = StopCamera(device);
    SDL_UnlockMutex(device->acquiring_lock);

    return (retval < 0) ? -1 : 0;
}

// Check spec has valid format and frame size
static int prepare_cameraspec(SDL_CameraDevice *device, const SDL_CameraSpec *desired, SDL_CameraSpec *obtained, int allowed_changes)
{
    // Check format
    const int numfmts = SDL_GetNumCameraFormats(device);
    SDL_bool is_format_valid = SDL_FALSE;

    for (int i = 0; i < numfmts; i++) {
        Uint32 format;
        if (SDL_GetCameraFormat(device, i, &format) == 0) {
            if (format == desired->format && format != SDL_PIXELFORMAT_UNKNOWN) {
                is_format_valid = SDL_TRUE;
                obtained->format = format;
                break;
            }
        }
    }

    if (!is_format_valid) {
        if (allowed_changes) {
            for (int i = 0; i < numfmts; i++) {
                Uint32 format;
                if (SDL_GetCameraFormat(device, i, &format) == 0) {
                    if (format != SDL_PIXELFORMAT_UNKNOWN) {
                        obtained->format = format;
                        is_format_valid = SDL_TRUE;
                        break;
                    }
                }
            }
        } else {
            return SDL_SetError("Not allowed to change the format");
        }
    }

    if (!is_format_valid) {
        return SDL_SetError("Invalid format");
    }

    // Check frame size
    const int numsizes = SDL_GetNumCameraFrameSizes(device, obtained->format);
    SDL_bool is_framesize_valid = SDL_FALSE;

    for (int i = 0; i < numsizes; i++) {
        int w, h;
        if (SDL_GetCameraFrameSize(device, obtained->format, i, &w, &h) == 0) {
            if (desired->width == w && desired->height == h) {
                is_framesize_valid = SDL_TRUE;
                obtained->width = w;
                obtained->height = h;
                break;
            }
        }
    }

    if (!is_framesize_valid) {
        if (allowed_changes) {
            int w, h;
            if (SDL_GetCameraFrameSize(device, obtained->format, 0, &w, &h) == 0) {
                is_framesize_valid = SDL_TRUE;
                obtained->width = w;
                obtained->height = h;
            }
        } else {
            return SDL_SetError("Not allowed to change the frame size");
        }
    }

    if (!is_framesize_valid) {
        return SDL_SetError("Invalid frame size");
    }

    return 0;
}

const char *SDL_GetCameraDeviceName(SDL_CameraDeviceID instance_id)
{
    static char buf[256];
    buf[0] = 0;
    buf[255] = 0;

    if (instance_id == 0) {
        SDL_InvalidParamError("instance_id");
        return NULL;
    }

    if (GetCameraDeviceName(instance_id, buf, sizeof (buf)) < 0) {
        buf[0] = 0;
    }

    return buf;
}


SDL_CameraDeviceID *SDL_GetCameraDevices(int *count)
{
    int dummycount = 0;
    if (!count) {
        count = &dummycount;
    }

    int num = 0;
    SDL_CameraDeviceID *retval = GetCameraDevices(&num);
    if (retval) {
        *count = num;
        return retval;
    }

    // return list of 0 ID, null terminated
    retval = (SDL_CameraDeviceID *)SDL_calloc(1, sizeof(*retval));
    if (retval == NULL) {
        SDL_OutOfMemory();
        *count = 0;
        return NULL;
    }

    retval[0] = 0;
    *count = 0;

    return retval;
}

// Camera thread function
static int SDLCALL SDL_CameraThread(void *devicep)
{
    const int delay = 20;
    SDL_CameraDevice *device = (SDL_CameraDevice *) devicep;

#if DEBUG_CAMERA
    SDL_Log("Start thread 'SDL_CameraThread'");
#endif


#ifdef SDL_VIDEO_DRIVER_ANDROID
    // TODO
    /*
    {
        // Set thread priority to THREAD_PRIORITY_VIDEO
        extern void Android_JNI_CameraSetThreadPriority(int, int);
        Android_JNI_CameraSetThreadPriority(device->iscapture, device);
    }*/
#else
    // The camera capture is always a high priority thread
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
#endif

    // Perform any thread setup
    device->threadid = SDL_GetCurrentThreadID();

    // Init state
    // !!! FIXME: use a semaphore or something
    while (!SDL_AtomicGet(&device->enabled)) {
        SDL_Delay(delay);
    }

    // Loop, filling the camera buffers
    while (!SDL_AtomicGet(&device->shutdown)) {
        SDL_CameraFrame f;
        int ret;

        SDL_zero(f);

        SDL_LockMutex(device->acquiring_lock);
        ret = AcquireFrame(device, &f);
        SDL_UnlockMutex(device->acquiring_lock);

        if (ret == 0) {
            if (f.num_planes == 0) {
                continue;
            }
        }

        if (ret < 0) {
            // Flag it as an error
#if DEBUG_CAMERA
            SDL_Log("dev[%p] error AcquireFrame: %d %s", (void *)device, ret, SDL_GetError());
#endif
            f.num_planes = 0;
        }


        entry_t *entry = SDL_malloc(sizeof (entry_t));
        if (entry == NULL) {
            goto error_mem;
        }

        entry->frame = f;

        SDL_LockMutex(device->device_lock);
        ret = SDL_ListAdd(&device->buffer_queue, entry);
        SDL_UnlockMutex(device->device_lock);

        if (ret < 0) {
            SDL_free(entry);
            goto error_mem;
        }
    }

#if DEBUG_CAMERA
    SDL_Log("dev[%p] End thread 'SDL_CameraThread'", (void *)device);
#endif
    return 0;

error_mem:
#if DEBUG_CAMERA
    SDL_Log("dev[%p] End thread 'SDL_CameraThread' with error: %s", (void *)device, SDL_GetError());
#endif
    SDL_AtomicSet(&device->shutdown, 1);
    SDL_OutOfMemory();  // !!! FIXME: this error isn't accessible since the thread is about to terminate
    return 0;
}

SDL_CameraDevice *SDL_OpenCamera(SDL_CameraDeviceID instance_id)
{
    const int n = SDL_arraysize(open_devices);
    SDL_CameraDevice *device = NULL;
    const char *device_name = NULL;
    int id = -1;

    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_SetError("Video subsystem is not initialized");
        goto error;
    }

    // !!! FIXME: there is a race condition here if two devices open from two threads at once.
    // Find an available device ID...
    for (int i = 0; i < n; i++) {
        if (open_devices[i] == NULL) {
            id = i;
            break;
        }
    }

    if (id == -1) {
        SDL_SetError("Too many open camera devices");
        goto error;
    }

    if (instance_id != 0) {
        device_name = SDL_GetCameraDeviceName(instance_id);
        if (device_name == NULL) {
            goto error;
        }
    } else {
        SDL_CameraDeviceID *devices = SDL_GetCameraDevices(NULL);
        if (devices && devices[0]) {
            device_name = SDL_GetCameraDeviceName(devices[0]);
            SDL_free(devices);
        }
    }

#if 0
    // FIXME do we need this ?
    // Let the user override.
    {
        const char *dev = SDL_getenv("SDL_CAMERA_DEVICE_NAME");
        if (dev && dev[0]) {
            device_name = dev;
        }
    }
#endif

    if (device_name == NULL) {
        goto error;
    }

    device = (SDL_CameraDevice *) SDL_calloc(1, sizeof (SDL_CameraDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        goto error;
    }
    device->dev_name = SDL_strdup(device_name);

    SDL_AtomicSet(&device->shutdown, 0);
    SDL_AtomicSet(&device->enabled, 0);

    device->device_lock = SDL_CreateMutex();
    if (device->device_lock == NULL) {
        SDL_SetError("Couldn't create acquiring_lock");
        goto error;
    }

    device->acquiring_lock = SDL_CreateMutex();
    if (device->acquiring_lock == NULL) {
        SDL_SetError("Couldn't create acquiring_lock");
        goto error;
    }

    if (OpenDevice(device) < 0) {
        goto error;
    }

    // empty
    device->buffer_queue = NULL;
    open_devices[id] = device;  // add it to our list of open devices.


    // Start the camera thread
    char threadname[64];
    SDL_snprintf(threadname, sizeof (threadname), "SDLCamera%d", id);
    device->thread = SDL_CreateThreadInternal(SDL_CameraThread, threadname, 0, device);
    if (device->thread == NULL) {
        SDL_SetError("Couldn't create camera thread");
        goto error;
    }

    return device;

error:
    CloseCameraDevice(device);
    return NULL;
}

int SDL_SetCameraSpec(SDL_CameraDevice *device, const SDL_CameraSpec *desired, SDL_CameraSpec *obtained, int allowed_changes)
{
    SDL_CameraSpec _obtained;
    SDL_CameraSpec _desired;
    int result;

    if (!device) {
        return SDL_InvalidParamError("device");
    } else if (device->is_spec_set == SDL_TRUE) {
        return SDL_SetError("already configured");
    }

    if (!desired) {
        SDL_zero(_desired);
        desired = &_desired;
        allowed_changes = SDL_CAMERA_ALLOW_ANY_CHANGE;
    } else {
        // in case desired == obtained
        _desired = *desired;
        desired = &_desired;
    }

    if (!obtained) {
        obtained = &_obtained;
    }

    SDL_zerop(obtained);

    if (prepare_cameraspec(device, desired, obtained, allowed_changes) < 0) {
        return -1;
    }

    device->spec = *obtained;

    result = InitDevice(device);
    if (result < 0) {
        return result;
    }

    *obtained = device->spec;

    device->is_spec_set = SDL_TRUE;

    return 0;
}

int SDL_AcquireCameraFrame(SDL_CameraDevice *device, SDL_CameraFrame *frame)
{
    if (!device) {
        return SDL_InvalidParamError("device");
    } else if (!frame) {
        return SDL_InvalidParamError("frame");
    }

    SDL_zerop(frame);

    if (device->thread == NULL) {
        int ret;

        // Wait for a frame
        while ((ret = AcquireFrame(device, frame)) == 0) {
            if (frame->num_planes) {
                return 0;
            }
        }
        return -1;
    } else {
        entry_t *entry = NULL;

        SDL_LockMutex(device->device_lock);
        SDL_ListPop(&device->buffer_queue, (void**)&entry);
        SDL_UnlockMutex(device->device_lock);

        if (entry) {
            *frame = entry->frame;
            SDL_free(entry);

            // Error from thread
            if (frame->num_planes == 0 && frame->timestampNS == 0) {
                return SDL_SetError("error from acquisition thread");
            }
        } else {
            // Queue is empty. Not an error.
        }
    }

    return 0;
}

int SDL_ReleaseCameraFrame(SDL_CameraDevice *device, SDL_CameraFrame *frame)
{
    if (!device) {
        return SDL_InvalidParamError("device");
    } else if (frame == NULL) {
        return SDL_InvalidParamError("frame");
    } else if (ReleaseFrame(device, frame) < 0) {
        return -1;
    }

    SDL_zerop(frame);
    return 0;
}

int SDL_GetNumCameraFormats(SDL_CameraDevice *device)
{
    if (!device) {
        return SDL_InvalidParamError("device");
    }
    return GetNumFormats(device);
}

int SDL_GetCameraFormat(SDL_CameraDevice *device, int index, Uint32 *format)
{
    if (!device) {
        return SDL_InvalidParamError("device");
    } else if (!format) {
        return SDL_InvalidParamError("format");
    }
    *format = 0;
    return GetFormat(device, index, format);
}

int SDL_GetNumCameraFrameSizes(SDL_CameraDevice *device, Uint32 format)
{
    if (!device) {
        return SDL_InvalidParamError("device");
    }
    return GetNumFrameSizes(device, format);
}

int SDL_GetCameraFrameSize(SDL_CameraDevice *device, Uint32 format, int index, int *width, int *height)
{
    if (!device) {
        return SDL_InvalidParamError("device");
    } else if (!width) {
        return SDL_InvalidParamError("width");
    } else if (!height) {
        return SDL_InvalidParamError("height");
    }
    *width = *height = 0;
    return GetFrameSize(device, format, index, width, height);
}

SDL_CameraDevice *SDL_OpenCameraWithSpec(SDL_CameraDeviceID instance_id, const SDL_CameraSpec *desired, SDL_CameraSpec *obtained, int allowed_changes)
{
    SDL_CameraDevice *device;

    if ((device = SDL_OpenCamera(instance_id)) == NULL) {
        return NULL;
    }

    if (SDL_SetCameraSpec(device, desired, obtained, allowed_changes) < 0) {
        SDL_CloseCamera(device);
        return NULL;
    }
    return device;
}

SDL_CameraStatus SDL_GetCameraStatus(SDL_CameraDevice *device)
{
    if (device == NULL) {
        return SDL_CAMERA_INIT;
    } else if (device->is_spec_set == SDL_FALSE) {
        return SDL_CAMERA_INIT;
    } else if (SDL_AtomicGet(&device->shutdown)) {
        return SDL_CAMERA_STOPPED;
    } else if (SDL_AtomicGet(&device->enabled)) {
        return SDL_CAMERA_PLAYING;
    }
    return SDL_CAMERA_INIT;
}

int SDL_CameraInit(void)
{
    SDL_zeroa(open_devices);
    SDL_SYS_CameraInit();
    return 0;
}

void SDL_QuitCamera(void)
{
    const int n = SDL_arraysize(open_devices);
    for (int i = 0; i < n; i++) {
        CloseCameraDevice(open_devices[i]);
    }

    SDL_zeroa(open_devices);

    SDL_SYS_CameraQuit();
}

