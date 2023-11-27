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

#include "SDL3/SDL.h"
#include "SDL3/SDL_video_capture.h"
#include "SDL_sysvideocapture.h"
#include "SDL_video_capture_c.h"
#include "SDL_pixels_c.h"
#include "../thread/SDL_systhread.h"

#define DEBUG_VIDEO_CAPTURE_CAPTURE 1


#ifdef SDL_VIDEO_CAPTURE
/* list node entries to share frames between SDL and user app */
typedef struct entry_t
{
    SDL_VideoCaptureFrame frame;
} entry_t;

static SDL_VideoCaptureDevice *open_devices[16];

static void
close_device(SDL_VideoCaptureDevice *device)
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

    {
        int i, n = SDL_arraysize(open_devices);
        for (i = 0; i < n; i++) {
            if (open_devices[i] == device) {
                open_devices[i] = NULL;
            }
        }
    }

    {
        entry_t *entry = NULL;
        while (device->buffer_queue != NULL) {
            SDL_ListPop(&device->buffer_queue, (void**)&entry);
            if (entry) {
                SDL_VideoCaptureFrame f = entry->frame;
                /* Release frames not acquired, if any */
                if (f.timestampNS) {
                    ReleaseFrame(device, &f);
                }
                SDL_free(entry);
            }
        }
    }

    CloseDevice(device);

    SDL_free(device->dev_name);
    SDL_free(device);
}

/* Tell if all device are closed */
SDL_bool check_all_device_closed(void)
{
    int i, n = SDL_arraysize(open_devices);
    int all_closed = SDL_TRUE;
    for (i = 0; i < n; i++) {
        if (open_devices[i]) {
            all_closed = SDL_FALSE;
            break;
        }
    }
    return all_closed;
}

/* Tell if at least one device is in playing state */
SDL_bool check_device_playing(void)
{
    int i, n = SDL_arraysize(open_devices);
    for (i = 0; i < n; i++) {
        if (open_devices[i]) {
            if (SDL_GetVideoCaptureStatus(open_devices[i]) == SDL_VIDEO_CAPTURE_PLAYING) {
                return SDL_TRUE;
            }
        }
    }
    return SDL_FALSE;
}


#endif /* SDL_VIDEO_CAPTURE */

void
SDL_CloseVideoCapture(SDL_VideoCaptureDevice *device)
{
#ifdef SDL_VIDEO_CAPTURE
    if (!device) {
        SDL_InvalidParamError("device");
        return;
    }
    close_device(device);
#endif
}

int
SDL_StartVideoCapture(SDL_VideoCaptureDevice *device)
{
#ifdef SDL_VIDEO_CAPTURE
    SDL_VideoCaptureStatus status;
    int result;
    if (!device) {
        return SDL_InvalidParamError("device");
    }

    if (device->is_spec_set == SDL_FALSE) {
        return SDL_SetError("no spec set");
    }

    status = SDL_GetVideoCaptureStatus(device);
    if (status != SDL_VIDEO_CAPTURE_INIT) {
        return SDL_SetError("invalid state");
    }

    result = StartCapture(device);
    if (result < 0) {
        return result;
    }

    SDL_AtomicSet(&device->enabled, 1);

    return 0;
#else
    return SDL_Unsupported();
#endif
}

int
SDL_GetVideoCaptureSpec(SDL_VideoCaptureDevice *device, SDL_VideoCaptureSpec *spec)
{
#ifdef SDL_VIDEO_CAPTURE
    if (!device) {
        return SDL_InvalidParamError("device");
    }

    if (!spec) {
        return SDL_InvalidParamError("spec");
    }

    SDL_zerop(spec);

    return GetDeviceSpec(device, spec);
#else
    return SDL_Unsupported();
#endif
}

int
SDL_StopVideoCapture(SDL_VideoCaptureDevice *device)
{
#ifdef SDL_VIDEO_CAPTURE
    SDL_VideoCaptureStatus status;
    int ret;
    if (!device) {
        return SDL_InvalidParamError("device");
    }

    status = SDL_GetVideoCaptureStatus(device);

    if (status != SDL_VIDEO_CAPTURE_PLAYING) {
        return SDL_SetError("invalid state");
    }

    SDL_AtomicSet(&device->enabled, 0);
    SDL_AtomicSet(&device->shutdown, 1);

    SDL_LockMutex(device->acquiring_lock);
    ret = StopCapture(device);
    SDL_UnlockMutex(device->acquiring_lock);

    if (ret < 0) {
        return -1;
    }

    return 0;
#else
    return SDL_Unsupported();
#endif
}

#ifdef SDL_VIDEO_CAPTURE

/* Check spec has valid format and frame size */
static int
prepare_video_capturespec(SDL_VideoCaptureDevice *device, const SDL_VideoCaptureSpec *desired, SDL_VideoCaptureSpec *obtained, int allowed_changes)
{
    /* Check format */
    {
        int i, num = SDL_GetNumVideoCaptureFormats(device);
        int is_format_valid = 0;

        for (i = 0; i < num; i++) {
            Uint32 format;
            if (SDL_GetVideoCaptureFormat(device, i, &format) == 0) {
                if (format == desired->format && format != SDL_PIXELFORMAT_UNKNOWN) {
                    is_format_valid = 1;
                    obtained->format = format;
                    break;
                }
            }
        }

        if (!is_format_valid) {
            if (allowed_changes) {
                for (i = 0; i < num; i++) {
                    Uint32 format;
                    if (SDL_GetVideoCaptureFormat(device, i, &format) == 0) {
                        if (format != SDL_PIXELFORMAT_UNKNOWN) {
                            obtained->format = format;
                            is_format_valid = 1;
                            break;
                        }
                    }
                }

            } else {
                SDL_SetError("Not allowed to change the format");
                return -1;
            }
        }

        if (!is_format_valid) {
            SDL_SetError("Invalid format");
            return -1;
        }
    }

    /* Check frame size */
    {
        int i, num = SDL_GetNumVideoCaptureFrameSizes(device, obtained->format);
        int is_framesize_valid = 0;

        for (i = 0; i < num; i++) {
            int w, h;
            if (SDL_GetVideoCaptureFrameSize(device, obtained->format, i, &w, &h) == 0) {
                if (desired->width == w && desired->height == h) {
                    is_framesize_valid = 1;
                    obtained->width = w;
                    obtained->height = h;
                    break;
                }
            }
        }

        if (!is_framesize_valid) {
            if (allowed_changes) {
                int w, h;
                if (SDL_GetVideoCaptureFrameSize(device, obtained->format, 0, &w, &h) == 0) {
                    is_framesize_valid = 1;
                    obtained->width = w;
                    obtained->height = h;
                }
            } else {
                SDL_SetError("Not allowed to change the frame size");
                return -1;
            }
        }

        if (!is_framesize_valid) {
            SDL_SetError("Invalid frame size");
            return -1;
        }

    }

    return 0;
}

#endif /* SDL_VIDEO_CAPTURE */

const char *
SDL_GetVideoCaptureDeviceName(SDL_VideoCaptureDeviceID instance_id)
{
#ifdef SDL_VIDEO_CAPTURE
    static char buf[256];
    buf[0] = 0;
    buf[255] = 0;

    if (instance_id == 0) {
        SDL_InvalidParamError("instance_id");
        return NULL;
    }

    if (GetDeviceName(instance_id, buf, sizeof (buf)) < 0) {
        buf[0] = 0;
    }
    return buf;
#else
    SDL_Unsupported();
    return NULL;
#endif
}


SDL_VideoCaptureDeviceID *
SDL_GetVideoCaptureDevices(int *count)
{

    int num = 0;
    SDL_VideoCaptureDeviceID *ret = NULL;
#ifdef SDL_VIDEO_CAPTURE
    ret = GetVideoCaptureDevices(&num);
#endif

    if (ret) {
        if (count) {
            *count = num;
        }
        return ret;
    }

    /* return list of 0 ID, null terminated */
    num = 0;
    ret = (SDL_VideoCaptureDeviceID *)SDL_malloc((num + 1) * sizeof(*ret));

    if (ret == NULL) {
        SDL_OutOfMemory();
        if (count) {
            *count = 0;
        }
        return NULL;
    }

    ret[num] = 0;
    if (count) {
        *count = num;
    }

    return ret;
}

#ifdef SDL_VIDEO_CAPTURE

/* Video capture thread function */
static int SDLCALL
SDL_CaptureVideoThread(void *devicep)
{
    const int delay = 20;
    SDL_VideoCaptureDevice *device = (SDL_VideoCaptureDevice *) devicep;

#if DEBUG_VIDEO_CAPTURE_CAPTURE
    SDL_Log("Start thread 'SDL_CaptureVideo'");
#endif


#ifdef SDL_VIDEO_DRIVER_ANDROID
    // TODO
    /*
    {
        // Set thread priority to THREAD_PRIORITY_VIDEO
        extern void Android_JNI_VideoCaptureSetThreadPriority(int, int);
        Android_JNI_VideoCaptureSetThreadPriority(device->iscapture, device);
    }*/
#else
    /* The video_capture mixing is always a high priority thread */
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
#endif

    /* Perform any thread setup */
    device->threadid = SDL_ThreadID();

    /* Init state */
    while (!SDL_AtomicGet(&device->enabled)) {
        SDL_Delay(delay);
    }

    /* Loop, filling the video_capture buffers */
    while (!SDL_AtomicGet(&device->shutdown)) {
        SDL_VideoCaptureFrame f;
        int ret;
        entry_t *entry;

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
            /* Flag it as an error */
#if DEBUG_VIDEO_CAPTURE_CAPTURE
            SDL_Log("dev[%p] error AcquireFrame: %d %s", (void *)device, ret, SDL_GetError());
#endif
            f.num_planes = 0;
        }


        entry = SDL_malloc(sizeof (entry_t));
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

#if DEBUG_VIDEO_CAPTURE_CAPTURE
    SDL_Log("dev[%p] End thread 'SDL_CaptureVideo'", (void *)device);
#endif
    return 0;

error_mem:
#if DEBUG_VIDEO_CAPTURE_CAPTURE
    SDL_Log("dev[%p] End thread 'SDL_CaptureVideo' with error: %s", (void *)device, SDL_GetError());
#endif
    SDL_AtomicSet(&device->shutdown, 1);
    SDL_OutOfMemory();
    return 0;
}
#endif

SDL_VideoCaptureDevice *
SDL_OpenVideoCapture(SDL_VideoCaptureDeviceID instance_id)
{
#ifdef SDL_VIDEO_CAPTURE
    int i, n = SDL_arraysize(open_devices);
    int id = -1;
    SDL_VideoCaptureDevice *device = NULL;
    const char *device_name = NULL;

    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_SetError("Video subsystem is not initialized");
        goto error;
    }

    /* !!! FIXME: there is a race condition here if two devices open from two threads at once. */
    /* Find an available device ID... */
    for (i = 0; i < n; i++) {
        if (open_devices[i] == NULL) {
            id = i;
            break;
        }
    }

    if (id == -1) {
        SDL_SetError("Too many open video capture devices");
        goto error;
    }

    if (instance_id != 0) {
        device_name = SDL_GetVideoCaptureDeviceName(instance_id);
        if (device_name == NULL) {
            goto error;
        }
    } else {
        SDL_VideoCaptureDeviceID *devices = SDL_GetVideoCaptureDevices(NULL);
        if (devices && devices[0]) {
            device_name = SDL_GetVideoCaptureDeviceName(devices[0]);
            SDL_free(devices);
        }
    }

#if 0
    // FIXME do we need this ?
    /* Let the user override. */
    {
        const char *dev = SDL_getenv("SDL_VIDEO_CAPTURE_DEVICE_NAME");
        if (dev && dev[0]) {
            device_name = dev;
        }
    }
#endif

    if (device_name == NULL) {
        goto error;
    }

    device = (SDL_VideoCaptureDevice *) SDL_calloc(1, sizeof (SDL_VideoCaptureDevice));
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

    /* empty */
    device->buffer_queue = NULL;
    open_devices[id] = device;  /* add it to our list of open devices. */


    /* Start the video_capture thread */
    {
        const size_t stacksize = 64 * 1024;
        char threadname[64];

        SDL_snprintf(threadname, sizeof (threadname), "SDLVideoC%d", id);
        device->thread = SDL_CreateThreadInternal(SDL_CaptureVideoThread, threadname, stacksize, device);

        if (device->thread == NULL) {
            SDL_SetError("Couldn't create video_capture thread");
            goto error;
        }
    }

    return device;

error:
    close_device(device);
    return NULL;
#else
    SDL_Unsupported();
    return NULL;
#endif /* SDL_VIDEO_CAPTURE */
}

int
SDL_SetVideoCaptureSpec(SDL_VideoCaptureDevice *device,
        const SDL_VideoCaptureSpec *desired,
        SDL_VideoCaptureSpec *obtained,
        int allowed_changes)
{
#ifdef SDL_VIDEO_CAPTURE
    SDL_VideoCaptureSpec _obtained;
    SDL_VideoCaptureSpec _desired;
    int result;

    if (!device) {
        return SDL_InvalidParamError("device");
    }

    if (device->is_spec_set == SDL_TRUE) {
        return SDL_SetError("already configured");
    }

    if (!desired) {
        SDL_zero(_desired);
        desired = &_desired;
        allowed_changes = SDL_VIDEO_CAPTURE_ALLOW_ANY_CHANGE;
    } else {
        /* in case desired == obtained */
        _desired = *desired;
        desired = &_desired;
    }

    if (!obtained) {
        obtained = &_obtained;
    }

    SDL_zerop(obtained);

    if (prepare_video_capturespec(device, desired, obtained, allowed_changes) < 0) {
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
#else
    SDL_zero(*obtained);
    return SDL_Unsupported();
#endif /* SDL_VIDEO_CAPTURE */
}

int
SDL_AcquireVideoCaptureFrame(SDL_VideoCaptureDevice *device, SDL_VideoCaptureFrame *frame)
{
#ifdef SDL_VIDEO_CAPTURE
    if (!device) {
        return SDL_InvalidParamError("device");
    }

    if (!frame) {
        return SDL_InvalidParamError("frame");
    }

    SDL_zerop(frame);

    if (device->thread == NULL) {
        int ret;

        /* Wait for a frame */
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

            /* Error from thread */
            if (frame->num_planes == 0 && frame->timestampNS == 0) {
                return SDL_SetError("error from acquisition thread");
            }


        } else {
            /* Queue is empty. Not an error. */
        }
    }

    return 0;
#else
    return SDL_Unsupported();
#endif /* SDL_VIDEO_CAPTURE */
}

int
SDL_ReleaseVideoCaptureFrame(SDL_VideoCaptureDevice *device, SDL_VideoCaptureFrame *frame)
{
#ifdef SDL_VIDEO_CAPTURE
    if (!device) {
        return SDL_InvalidParamError("device");
    }

    if (frame == NULL) {
        return SDL_InvalidParamError("frame");
    }

    if (ReleaseFrame(device, frame) < 0) {
        return -1;
    }

    SDL_zerop(frame);

    return 0;
#else
    return SDL_Unsupported();
#endif /* SDL_VIDEO_CAPTURE */
}

int
SDL_GetNumVideoCaptureFormats(SDL_VideoCaptureDevice *device)
{
#ifdef SDL_VIDEO_CAPTURE
    if (!device) {
        return SDL_InvalidParamError("device");
    }
    return GetNumFormats(device);
#else
    return 0;
#endif /* SDL_VIDEO_CAPTURE */
}

int
SDL_GetVideoCaptureFormat(SDL_VideoCaptureDevice *device, int index, Uint32 *format)
{
#ifdef SDL_VIDEO_CAPTURE
    if (!device) {
        return SDL_InvalidParamError("device");
    }
    if (!format) {
        return SDL_InvalidParamError("format");
    }
    *format = 0;
    return GetFormat(device, index, format);
#else
    return SDL_Unsupported();
#endif /* SDL_VIDEO_CAPTURE */
}

int
SDL_GetNumVideoCaptureFrameSizes(SDL_VideoCaptureDevice *device, Uint32 format)
{
#ifdef SDL_VIDEO_CAPTURE
    if (!device) {
        return SDL_InvalidParamError("device");
    }
    return GetNumFrameSizes(device, format);
#else
    return 0;
#endif /* SDL_VIDEO_CAPTURE */
}

int
SDL_GetVideoCaptureFrameSize(SDL_VideoCaptureDevice *device, Uint32 format, int index, int *width, int *height)
{
#ifdef SDL_VIDEO_CAPTURE
    if (!device) {
        return SDL_InvalidParamError("device");
    }
    if (!width) {
        return SDL_InvalidParamError("width");
    }
    if (!height) {
        return SDL_InvalidParamError("height");
    }
    *width = 0;
    *height = 0;
    return GetFrameSize(device, format, index, width, height);
#else
    return SDL_Unsupported();
#endif
}

SDL_VideoCaptureDevice *
SDL_OpenVideoCaptureWithSpec(
        SDL_VideoCaptureDeviceID instance_id,
        const SDL_VideoCaptureSpec *desired,
        SDL_VideoCaptureSpec *obtained,
        int allowed_changes)
{
#ifdef SDL_VIDEO_CAPTURE
    SDL_VideoCaptureDevice *device;

    if ((device = SDL_OpenVideoCapture(instance_id)) == NULL) {
        return NULL;
    }

    if (SDL_SetVideoCaptureSpec(device, desired, obtained, allowed_changes) < 0) {
        SDL_CloseVideoCapture(device);
        return NULL;
    }
    return device;
#else
    SDL_Unsupported();
    return NULL;
#endif
}

SDL_VideoCaptureStatus
SDL_GetVideoCaptureStatus(SDL_VideoCaptureDevice *device)
{
#ifdef SDL_VIDEO_CAPTURE
    if (device == NULL) {
        return SDL_VIDEO_CAPTURE_INIT;
    }

    if (device->is_spec_set == SDL_FALSE) {
        return SDL_VIDEO_CAPTURE_INIT;
    }

    if (SDL_AtomicGet(&device->shutdown)) {
        return SDL_VIDEO_CAPTURE_STOPPED;
    }

    if (SDL_AtomicGet(&device->enabled)) {
        return SDL_VIDEO_CAPTURE_PLAYING;
    }
    return SDL_VIDEO_CAPTURE_INIT;
#else
    SDL_Unsupported();
    return SDL_VIDEO_CAPTURE_FAIL;
#endif
}

int
SDL_VideoCaptureInit(void)
{
#ifdef SDL_VIDEO_CAPTURE
    SDL_zeroa(open_devices);

    SDL_SYS_VideoCaptureInit();
    return 0;
#else
    return 0;
#endif
}

void
SDL_QuitVideoCapture(void)
{
#ifdef SDL_VIDEO_CAPTURE
    int i, n = SDL_arraysize(open_devices);
    for (i = 0; i < n; i++) {
        close_device(open_devices[i]);
    }

    SDL_zeroa(open_devices);

    SDL_SYS_VideoCaptureQuit();
#endif
}

#ifdef SDL_VIDEO_CAPTURE

#if defined(__linux__) && !defined(__ANDROID__)

/* See SDL_video_capture_v4l2.c */

#elif defined(__ANDROID__) && __ANDROID_API__ >= 24

/* See SDL_android_video_capture.c */

#elif defined(__IPHONEOS__) || defined(__MACOS__)

/* See SDL_video_capture_apple.m */
#else

int SDL_SYS_VideoCaptureInit(void)
{
    return 0;
}

int SDL_SYS_VideoCaptureQuit(void)
{
    return 0;
}

int
OpenDevice(SDL_VideoCaptureDevice *_this)
{
    return SDL_SetError("not implemented");
}

void
CloseDevice(SDL_VideoCaptureDevice *_this)
{
    return;
}

int
InitDevice(SDL_VideoCaptureDevice *_this)
{
    size_t size, pitch;
    SDL_CalculateSize(_this->spec.format, _this->spec.width, _this->spec.height, &size, &pitch, SDL_FALSE);
    SDL_Log("Buffer size: %d x %d", _this->spec.width, _this->spec.height);
    return -1;
}

int
GetDeviceSpec(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureSpec *spec)
{
    return SDL_Unsupported();
}

int
StartCapture(SDL_VideoCaptureDevice *_this)
{
    return SDL_Unsupported();
}

int
StopCapture(SDL_VideoCaptureDevice *_this)
{
    return -1;
}

int
AcquireFrame(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureFrame *frame)
{
    return -1;
}

int
ReleaseFrame(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureFrame *frame)
{
    return -1;
}

int
GetNumFormats(SDL_VideoCaptureDevice *_this)
{
    return -1;
}

int
GetFormat(SDL_VideoCaptureDevice *_this, int index, Uint32 *format)
{
    return -1;
}

int
GetNumFrameSizes(SDL_VideoCaptureDevice *_this, Uint32 format)
{
    return -1;
}

int
GetFrameSize(SDL_VideoCaptureDevice *_this, Uint32 format, int index, int *width, int *height)
{
    return -1;
}

int
GetDeviceName(SDL_VideoCaptureDeviceID instance_id, char *buf, int size)
{
    return -1;
}

SDL_VideoCaptureDeviceID *
GetVideoCaptureDevices(int *count)
{
    return NULL;
}

#endif

#endif /* SDL_VIDEO_CAPTURE */
