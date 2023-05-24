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

/* Allow access to a raw mixing buffer */

#include "SDL_audio_c.h"
#include "SDL_sysaudio.h"
#include "../thread/SDL_systhread.h"
#include "../SDL_utils_c.h"

static SDL_AudioDriver current_audio;
static SDL_AudioDevice *open_devices[16];

/* Available audio drivers */
static const AudioBootStrap *const bootstrap[] = {
#ifdef SDL_AUDIO_DRIVER_PULSEAUDIO
    &PULSEAUDIO_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_ALSA
    &ALSA_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_SNDIO
    &SNDIO_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_NETBSD
    &NETBSDAUDIO_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_WASAPI
    &WASAPI_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_DSOUND
    &DSOUND_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_HAIKU
    &HAIKUAUDIO_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_COREAUDIO
    &COREAUDIO_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_AAUDIO
    &aaudio_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_OPENSLES
    &openslES_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_ANDROID
    &ANDROIDAUDIO_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_PS2
    &PS2AUDIO_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_PSP
    &PSPAUDIO_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_VITA
    &VITAAUD_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_N3DS
    &N3DSAUDIO_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_EMSCRIPTEN
    &EMSCRIPTENAUDIO_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_JACK
    &JACK_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_PIPEWIRE
    &PIPEWIRE_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_OSS
    &DSP_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_QNX
    &QSAAUDIO_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_DISK
    &DISKAUDIO_bootstrap,
#endif
#ifdef SDL_AUDIO_DRIVER_DUMMY
    &DUMMYAUDIO_bootstrap,
#endif
    NULL
};

static SDL_AudioDevice *get_audio_device(SDL_AudioDeviceID id)
{
    id--;
    if ((id >= SDL_arraysize(open_devices)) || (open_devices[id] == NULL)) {
        SDL_SetError("Invalid audio device ID");
        return NULL;
    }

    return open_devices[id];
}

int get_max_num_audio_dev(void)
{
    return SDL_arraysize(open_devices);
}

SDL_AudioDevice *get_audio_dev(SDL_AudioDeviceID id)
{
    return open_devices[id];
}

/* stubs for audio drivers that don't need a specific entry point... */
static void SDL_AudioDetectDevices_Default(void)
{
    /* you have to write your own implementation if these assertions fail. */
    SDL_assert(current_audio.impl.OnlyHasDefaultOutputDevice);
    SDL_assert(current_audio.impl.OnlyHasDefaultCaptureDevice || !current_audio.impl.HasCaptureSupport);

    SDL_AddAudioDevice(SDL_FALSE, DEFAULT_OUTPUT_DEVNAME, NULL, (void *)((size_t)0x1));
    if (current_audio.impl.HasCaptureSupport) {
        SDL_AddAudioDevice(SDL_TRUE, DEFAULT_INPUT_DEVNAME, NULL, (void *)((size_t)0x2));
    }
}

static void SDL_AudioThreadInit_Default(SDL_AudioDevice *_this)
{ /* no-op. */
}

static void SDL_AudioThreadDeinit_Default(SDL_AudioDevice *_this)
{ /* no-op. */
}

static void SDL_AudioWaitDevice_Default(SDL_AudioDevice *_this)
{ /* no-op. */
}

static void SDL_AudioPlayDevice_Default(SDL_AudioDevice *_this)
{ /* no-op. */
}

static Uint8 *SDL_AudioGetDeviceBuf_Default(SDL_AudioDevice *_this)
{
    return NULL;
}

static int SDL_AudioCaptureFromDevice_Default(SDL_AudioDevice *_this, void *buffer, int buflen)
{
    return -1; /* just fail immediately. */
}

static void SDL_AudioFlushCapture_Default(SDL_AudioDevice *_this)
{ /* no-op. */
}

static void SDL_AudioCloseDevice_Default(SDL_AudioDevice *_this)
{ /* no-op. */
}

static void SDL_AudioDeinitialize_Default(void)
{ /* no-op. */
}

static void SDL_AudioFreeDeviceHandle_Default(void *handle)
{ /* no-op. */
}

static int SDL_AudioOpenDevice_Default(SDL_AudioDevice *_this, const char *devname)
{
    return SDL_Unsupported();
}

static void SDL_AudioLockDevice_Default(SDL_AudioDevice *device)
{
    SDL_LockMutex(device->mixer_lock);
}

static void SDL_AudioUnlockDevice_Default(SDL_AudioDevice *device)
{
    SDL_UnlockMutex(device->mixer_lock);
}

static void finish_audio_entry_points_init(void)
{
    /*
     * Fill in stub functions for unused driver entry points. This lets us
     *  blindly call them without having to check for validity first.
     */

#define FILL_STUB(x)                                   \
    if (current_audio.impl.x == NULL) {                \
        current_audio.impl.x = SDL_Audio##x##_Default; \
    }
    FILL_STUB(DetectDevices);
    FILL_STUB(OpenDevice);
    FILL_STUB(ThreadInit);
    FILL_STUB(ThreadDeinit);
    FILL_STUB(WaitDevice);
    FILL_STUB(PlayDevice);
    FILL_STUB(GetDeviceBuf);
    FILL_STUB(CaptureFromDevice);
    FILL_STUB(FlushCapture);
    FILL_STUB(CloseDevice);
    FILL_STUB(LockDevice);
    FILL_STUB(UnlockDevice);
    FILL_STUB(FreeDeviceHandle);
    FILL_STUB(Deinitialize);
#undef FILL_STUB
}

/* device hotplug support... */

static int add_audio_device(const char *name, SDL_AudioSpec *spec, void *handle, SDL_AudioDeviceItem **devices, int *devCount)
{
    int retval = -1;
    SDL_AudioDeviceItem *item;
    const SDL_AudioDeviceItem *i;
    int dupenum = 0;

    SDL_assert(handle != NULL); /* we reserve NULL, audio backends can't use it. */
    SDL_assert(name != NULL);

    item = (SDL_AudioDeviceItem *)SDL_malloc(sizeof(SDL_AudioDeviceItem));
    if (!item) {
        return SDL_OutOfMemory();
    }

    item->original_name = SDL_strdup(name);
    if (!item->original_name) {
        SDL_free(item);
        return SDL_OutOfMemory();
    }

    item->dupenum = 0;
    item->name = item->original_name;
    if (spec != NULL) {
        SDL_copyp(&item->spec, spec);
    } else {
        SDL_zero(item->spec);
    }
    item->handle = handle;

    SDL_LockMutex(current_audio.detectionLock);

    for (i = *devices; i != NULL; i = i->next) {
        if (SDL_strcmp(name, i->original_name) == 0) {
            dupenum = i->dupenum + 1;
            break; /* stop at the highest-numbered dupe. */
        }
    }

    if (dupenum) {
        const size_t len = SDL_strlen(name) + 16;
        char *replacement = (char *)SDL_malloc(len);
        if (!replacement) {
            SDL_UnlockMutex(current_audio.detectionLock);
            SDL_free(item->original_name);
            SDL_free(item);
            return SDL_OutOfMemory();
        }

        (void)SDL_snprintf(replacement, len, "%s (%d)", name, dupenum + 1);
        item->dupenum = dupenum;
        item->name = replacement;
    }

    item->next = *devices;
    *devices = item;
    retval = (*devCount)++; /* !!! FIXME: this should be an atomic increment */

    SDL_UnlockMutex(current_audio.detectionLock);

    return retval;
}

static SDL_INLINE int add_capture_device(const char *name, SDL_AudioSpec *spec, void *handle)
{
    SDL_assert(current_audio.impl.HasCaptureSupport);
    return add_audio_device(name, spec, handle, &current_audio.inputDevices, &current_audio.inputDeviceCount);
}

static SDL_INLINE int add_output_device(const char *name, SDL_AudioSpec *spec, void *handle)
{
    return add_audio_device(name, spec, handle, &current_audio.outputDevices, &current_audio.outputDeviceCount);
}

static void free_device_list(SDL_AudioDeviceItem **devices, int *devCount)
{
    SDL_AudioDeviceItem *item, *next;
    for (item = *devices; item != NULL; item = next) {
        next = item->next;
        if (item->handle != NULL) {
            current_audio.impl.FreeDeviceHandle(item->handle);
        }
        /* these two pointers are the same if not a duplicate devname */
        if (item->name != item->original_name) {
            SDL_free(item->name);
        }
        SDL_free(item->original_name);
        SDL_free(item);
    }
    *devices = NULL;
    *devCount = 0;
}

/* The audio backends call this when a new device is plugged in. */
void SDL_AddAudioDevice(const SDL_bool iscapture, const char *name, SDL_AudioSpec *spec, void *handle)
{
    const int device_index = iscapture ? add_capture_device(name, spec, handle) : add_output_device(name, spec, handle);
    if (device_index != -1) {
        /* Post the event, if desired */
        if (SDL_EventEnabled(SDL_EVENT_AUDIO_DEVICE_ADDED)) {
            SDL_Event event;
            event.type = SDL_EVENT_AUDIO_DEVICE_ADDED;
            event.common.timestamp = 0;
            event.adevice.which = device_index;
            event.adevice.iscapture = iscapture;
            SDL_PushEvent(&event);
        }
    }
}

/* The audio backends call this when a currently-opened device is lost. */
void SDL_OpenedAudioDeviceDisconnected(SDL_AudioDevice *device)
{
    SDL_assert(get_audio_device(device->id) == device);

    if (!SDL_AtomicGet(&device->enabled)) {
        return; /* don't report disconnects more than once. */
    }

    if (SDL_AtomicGet(&device->shutdown)) {
        return; /* don't report disconnect if we're trying to close device. */
    }

    /* Ends the audio callback and mark the device as STOPPED, but the
       app still needs to close the device to free resources. */
    current_audio.impl.LockDevice(device);
    SDL_AtomicSet(&device->enabled, 0);
    current_audio.impl.UnlockDevice(device);

    /* Post the event, if desired */
    if (SDL_EventEnabled(SDL_EVENT_AUDIO_DEVICE_REMOVED)) {
        SDL_Event event;
        event.type = SDL_EVENT_AUDIO_DEVICE_REMOVED;
        event.common.timestamp = 0;
        event.adevice.which = device->id;
        event.adevice.iscapture = device->iscapture ? 1 : 0;
        SDL_PushEvent(&event);
    }
}

static void mark_device_removed(void *handle, SDL_AudioDeviceItem *devices, SDL_bool *removedFlag)
{
    SDL_AudioDeviceItem *item;
    SDL_assert(handle != NULL);
    for (item = devices; item != NULL; item = item->next) {
        if (item->handle == handle) {
            item->handle = NULL;
            *removedFlag = SDL_TRUE;
            return;
        }
    }
}

/* The audio backends call this when a device is removed from the system. */
void SDL_RemoveAudioDevice(const SDL_bool iscapture, void *handle)
{
    int device_index;
    SDL_AudioDevice *device = NULL;
    SDL_bool device_was_opened = SDL_FALSE;

    SDL_LockMutex(current_audio.detectionLock);
    if (iscapture) {
        mark_device_removed(handle, current_audio.inputDevices, &current_audio.captureDevicesRemoved);
    } else {
        mark_device_removed(handle, current_audio.outputDevices, &current_audio.outputDevicesRemoved);
    }
    for (device_index = 0; device_index < SDL_arraysize(open_devices); device_index++) {
        device = open_devices[device_index];
        if (device != NULL && device->handle == handle) {
            device_was_opened = SDL_TRUE;
            SDL_OpenedAudioDeviceDisconnected(device);
            break;
        }
    }

    /* Devices that aren't opened, as of 2.24.0, will post an
       SDL_EVENT_AUDIO_DEVICE_REMOVED event with the `which` field set to zero.
       Apps can use this to decide if they need to refresh a list of
       available devices instead of closing an opened one.
       Note that opened devices will send the non-zero event in
       SDL_OpenedAudioDeviceDisconnected(). */
    if (!device_was_opened) {
        if (SDL_EventEnabled(SDL_EVENT_AUDIO_DEVICE_REMOVED)) {
            SDL_Event event;
            event.type = SDL_EVENT_AUDIO_DEVICE_REMOVED;
            event.common.timestamp = 0;
            event.adevice.which = 0;
            event.adevice.iscapture = iscapture ? 1 : 0;
            SDL_PushEvent(&event);
        }
    }

    SDL_UnlockMutex(current_audio.detectionLock);

    current_audio.impl.FreeDeviceHandle(handle);
}

/* buffer queueing support... */

static void SDLCALL SDL_BufferQueueDrainCallback(void *userdata, Uint8 *stream, int len)
{
    /* this function always holds the mixer lock before being called. */
    SDL_AudioDevice *device = (SDL_AudioDevice *)userdata;
    size_t dequeued;

    SDL_assert(device != NULL);     /* this shouldn't ever happen, right?! */
    SDL_assert(!device->iscapture); /* this shouldn't ever happen, right?! */
    SDL_assert(len >= 0);           /* this shouldn't ever happen, right?! */

    dequeued = SDL_ReadFromDataQueue(device->buffer_queue, stream, len);
    stream += dequeued;
    len -= (int)dequeued;

    if (len > 0) { /* fill any remaining space in the stream with silence. */
        SDL_assert(SDL_GetDataQueueSize(device->buffer_queue) == 0);
        SDL_memset(stream, device->callbackspec.silence, len);
    }
}

static void SDLCALL SDL_BufferQueueFillCallback(void *userdata, Uint8 *stream, int len)
{
    /* this function always holds the mixer lock before being called. */
    SDL_AudioDevice *device = (SDL_AudioDevice *)userdata;

    SDL_assert(device != NULL);    /* this shouldn't ever happen, right?! */
    SDL_assert(device->iscapture); /* this shouldn't ever happen, right?! */
    SDL_assert(len >= 0);          /* this shouldn't ever happen, right?! */

    /* note that if this needs to allocate more space and run out of memory,
       we have no choice but to quietly drop the data and hope it works out
       later, but you probably have bigger problems in this case anyhow. */
    SDL_WriteToDataQueue(device->buffer_queue, stream, len);
}

int SDL_QueueAudio(SDL_AudioDeviceID devid, const void *data, Uint32 len)
{
    SDL_AudioDevice *device = get_audio_device(devid);
    int rc = 0;

    if (!device) {
        return -1; /* get_audio_device() will have set the error state */
    } else if (device->iscapture) {
        return SDL_SetError("This is a capture device, queueing not allowed");
    } else if (device->callbackspec.callback != SDL_BufferQueueDrainCallback) {
        return SDL_SetError("Audio device has a callback, queueing not allowed");
    }

    if (len > 0) {
        current_audio.impl.LockDevice(device);
        rc = SDL_WriteToDataQueue(device->buffer_queue, data, len);
        current_audio.impl.UnlockDevice(device);
    }

    return rc;
}

Uint32 SDL_DequeueAudio(SDL_AudioDeviceID devid, void *data, Uint32 len)
{
    SDL_AudioDevice *device = get_audio_device(devid);
    Uint32 rc;

    if ((len == 0) ||                                                     /* nothing to do? */
        (!device) ||                                                      /* called with bogus device id */
        (!device->iscapture) ||                                           /* playback devices can't dequeue */
        (device->callbackspec.callback != SDL_BufferQueueFillCallback)) { /* not set for queueing */
        return 0;                                                         /* just report zero bytes dequeued. */
    }

    current_audio.impl.LockDevice(device);
    rc = (Uint32)SDL_ReadFromDataQueue(device->buffer_queue, data, len);
    current_audio.impl.UnlockDevice(device);
    return rc;
}

Uint32 SDL_GetQueuedAudioSize(SDL_AudioDeviceID devid)
{
    Uint32 retval = 0;
    SDL_AudioDevice *device = get_audio_device(devid);

    if (!device) {
        return 0;
    }

    /* Nothing to do unless we're set up for queueing. */
    if (device->callbackspec.callback == SDL_BufferQueueDrainCallback ||
        device->callbackspec.callback == SDL_BufferQueueFillCallback) {
        current_audio.impl.LockDevice(device);
        retval = (Uint32)SDL_GetDataQueueSize(device->buffer_queue);
        current_audio.impl.UnlockDevice(device);
    }

    return retval;
}

int SDL_ClearQueuedAudio(SDL_AudioDeviceID devid)
{
    SDL_AudioDevice *device = get_audio_device(devid);
    if (!device) {
        return SDL_InvalidParamError("devid");
    }

    /* Blank out the device and release the mutex. Free it afterwards. */
    current_audio.impl.LockDevice(device);

    /* Keep up to two packets in the pool to reduce future memory allocation pressure. */
    SDL_ClearDataQueue(device->buffer_queue, SDL_AUDIOBUFFERQUEUE_PACKETLEN * 2);

    current_audio.impl.UnlockDevice(device);
    return 0;
}

#ifdef SDL_AUDIO_DRIVER_ANDROID
extern void Android_JNI_AudioSetThreadPriority(int, int);
#endif

/* The general mixing thread function */
static int SDLCALL SDL_RunAudio(void *devicep)
{
    SDL_AudioDevice *device = (SDL_AudioDevice *)devicep;
    void *udata = device->callbackspec.userdata;
    SDL_AudioCallback callback = device->callbackspec.callback;
    int data_len = 0;
    Uint8 *data;

    SDL_assert(!device->iscapture);

#ifdef SDL_AUDIO_DRIVER_ANDROID
    {
        /* Set thread priority to THREAD_PRIORITY_AUDIO */
        Android_JNI_AudioSetThreadPriority(device->iscapture, device->id);
    }
#else
    /* The audio mixing is always a high priority thread */
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
#endif

    /* Perform any thread setup */
    device->threadid = SDL_ThreadID();
    current_audio.impl.ThreadInit(device);

    /* Loop, filling the audio buffers */
    while (!SDL_AtomicGet(&device->shutdown)) {
        data_len = device->callbackspec.size;

        /* Fill the current buffer with sound */
        if (!device->stream && SDL_AtomicGet(&device->enabled)) {
            SDL_assert((Uint32)data_len == device->spec.size);
            data = current_audio.impl.GetDeviceBuf(device);
        } else {
            /* if the device isn't enabled, we still write to the
               work_buffer, so the app's callback will fire with
               a regular frequency, in case they depend on that
               for timing or progress. They can use hotplug
               now to know if the device failed.
               Streaming playback uses work_buffer, too. */
            data = NULL;
        }

        if (data == NULL) {
            data = device->work_buffer;
        }

        /* !!! FIXME: this should be LockDevice. */
        SDL_LockMutex(device->mixer_lock);
        if (SDL_AtomicGet(&device->paused)) {
            SDL_memset(data, device->callbackspec.silence, data_len);
        } else {
            callback(udata, data, data_len);
        }
        SDL_UnlockMutex(device->mixer_lock);

        if (device->stream) {
            /* Stream available audio to device, converting/resampling. */
            /* if this fails...oh well. We'll play silence here. */
            SDL_PutAudioStreamData(device->stream, data, data_len);

            while (SDL_GetAudioStreamAvailable(device->stream) >= ((int)device->spec.size)) {
                int got;
                data = SDL_AtomicGet(&device->enabled) ? current_audio.impl.GetDeviceBuf(device) : NULL;
                got = SDL_GetAudioStreamData(device->stream, data ? data : device->work_buffer, device->spec.size);
                SDL_assert((got <= 0) || ((Uint32)got == device->spec.size));

                if (data == NULL) { /* device is having issues... */
                    const Uint32 delay = ((device->spec.samples * 1000) / device->spec.freq);
                    SDL_Delay(delay); /* wait for as long as this buffer would have played. Maybe device recovers later? */
                } else {
                    if ((Uint32)got != device->spec.size) {
                        SDL_memset(data, device->spec.silence, device->spec.size);
                    }
                    current_audio.impl.PlayDevice(device);
                    current_audio.impl.WaitDevice(device);
                }
            }
        } else if (data == device->work_buffer) {
            /* nothing to do; pause like we queued a buffer to play. */
            const Uint32 delay = ((device->spec.samples * 1000) / device->spec.freq);
            SDL_Delay(delay);
        } else { /* writing directly to the device. */
            /* queue this buffer and wait for it to finish playing. */
            current_audio.impl.PlayDevice(device);
            current_audio.impl.WaitDevice(device);
        }
    }

    /* Wait for the audio to drain. */
    SDL_Delay(((device->spec.samples * 1000) / device->spec.freq) * 2);

    current_audio.impl.ThreadDeinit(device);

    return 0;
}

/* !!! FIXME: this needs to deal with device spec changes. */
/* The general capture thread function */
static int SDLCALL SDL_CaptureAudio(void *devicep)
{
    SDL_AudioDevice *device = (SDL_AudioDevice *)devicep;
    const int silence = (int)device->spec.silence;
    const Uint32 delay = ((device->spec.samples * 1000) / device->spec.freq);
    const int data_len = device->spec.size;
    Uint8 *data;
    void *udata = device->callbackspec.userdata;
    SDL_AudioCallback callback = device->callbackspec.callback;

    SDL_assert(device->iscapture);

#ifdef SDL_AUDIO_DRIVER_ANDROID
    {
        /* Set thread priority to THREAD_PRIORITY_AUDIO */
        Android_JNI_AudioSetThreadPriority(device->iscapture, device->id);
    }
#else
    /* The audio mixing is always a high priority thread */
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
#endif

    /* Perform any thread setup */
    device->threadid = SDL_ThreadID();
    current_audio.impl.ThreadInit(device);

    /* Loop, filling the audio buffers */
    while (!SDL_AtomicGet(&device->shutdown)) {
        int still_need;
        Uint8 *ptr;

        if (SDL_AtomicGet(&device->paused)) {
            SDL_Delay(delay); /* just so we don't cook the CPU. */
            if (device->stream) {
                SDL_ClearAudioStream(device->stream);
            }
            current_audio.impl.FlushCapture(device); /* dump anything pending. */
            continue;
        }

        /* Fill the current buffer with sound */
        still_need = data_len;

        /* Use the work_buffer to hold data read from the device. */
        data = device->work_buffer;
        SDL_assert(data != NULL);

        ptr = data;

        /* We still read from the device when "paused" to keep the state sane,
           and block when there isn't data so this thread isn't eating CPU.
           But we don't process it further or call the app's callback. */

        if (!SDL_AtomicGet(&device->enabled)) {
            SDL_Delay(delay); /* try to keep callback firing at normal pace. */
        } else {
            while (still_need > 0) {
                const int rc = current_audio.impl.CaptureFromDevice(device, ptr, still_need);
                SDL_assert(rc <= still_need); /* device should not overflow buffer. :) */
                if (rc > 0) {
                    still_need -= rc;
                    ptr += rc;
                } else { /* uhoh, device failed for some reason! */
                    SDL_OpenedAudioDeviceDisconnected(device);
                    break;
                }
            }
        }

        if (still_need > 0) {
            /* Keep any data we already read, silence the rest. */
            SDL_memset(ptr, silence, still_need);
        }

        if (device->stream) {
            /* if this fails...oh well. */
            SDL_PutAudioStreamData(device->stream, data, data_len);

            while (SDL_GetAudioStreamAvailable(device->stream) >= ((int)device->callbackspec.size)) {
                const int got = SDL_GetAudioStreamData(device->stream, device->work_buffer, device->callbackspec.size);
                SDL_assert((got < 0) || ((Uint32)got == device->callbackspec.size));
                if ((Uint32)got != device->callbackspec.size) {
                    SDL_memset(device->work_buffer, device->spec.silence, device->callbackspec.size);
                }

                /* !!! FIXME: this should be LockDevice. */
                SDL_LockMutex(device->mixer_lock);
                if (!SDL_AtomicGet(&device->paused)) {
                    callback(udata, device->work_buffer, device->callbackspec.size);
                }
                SDL_UnlockMutex(device->mixer_lock);
            }
        } else { /* feeding user callback directly without streaming. */
            /* !!! FIXME: this should be LockDevice. */
            SDL_LockMutex(device->mixer_lock);
            if (!SDL_AtomicGet(&device->paused)) {
                callback(udata, data, device->callbackspec.size);
            }
            SDL_UnlockMutex(device->mixer_lock);
        }
    }

    current_audio.impl.FlushCapture(device);

    current_audio.impl.ThreadDeinit(device);

    return 0;
}

static SDL_AudioFormat SDL_ParseAudioFormat(const char *string)
{
#define CHECK_FMT_STRING(x)          \
    if (SDL_strcmp(string, #x) == 0) \
    return SDL_AUDIO_##x
    CHECK_FMT_STRING(U8);
    CHECK_FMT_STRING(S8);
    CHECK_FMT_STRING(S16LSB);
    CHECK_FMT_STRING(S16MSB);
    CHECK_FMT_STRING(S16);
    CHECK_FMT_STRING(S32LSB);
    CHECK_FMT_STRING(S32MSB);
    CHECK_FMT_STRING(S32SYS);
    CHECK_FMT_STRING(S32);
    CHECK_FMT_STRING(F32LSB);
    CHECK_FMT_STRING(F32MSB);
    CHECK_FMT_STRING(F32SYS);
    CHECK_FMT_STRING(F32);
#undef CHECK_FMT_STRING
    return 0;
}

int SDL_GetNumAudioDrivers(void)
{
    return SDL_arraysize(bootstrap) - 1;
}

const char *SDL_GetAudioDriver(int index)
{
    if (index >= 0 && index < SDL_GetNumAudioDrivers()) {
        return bootstrap[index]->name;
    }
    return NULL;
}

int SDL_InitAudio(const char *driver_name)
{
    int i;
    SDL_bool initialized = SDL_FALSE, tried_to_init = SDL_FALSE;

    if (SDL_GetCurrentAudioDriver()) {
        SDL_QuitAudio(); /* shutdown driver if already running. */
    }

    SDL_zeroa(open_devices);

    /* Select the proper audio driver */
    if (driver_name == NULL) {
        driver_name = SDL_GetHint(SDL_HINT_AUDIO_DRIVER);
    }

    if (driver_name != NULL && *driver_name != 0) {
        const char *driver_attempt = driver_name;
        while (driver_attempt != NULL && *driver_attempt != 0 && !initialized) {
            const char *driver_attempt_end = SDL_strchr(driver_attempt, ',');
            size_t driver_attempt_len = (driver_attempt_end != NULL) ? (driver_attempt_end - driver_attempt)
                                                                     : SDL_strlen(driver_attempt);
#ifdef SDL_AUDIO_DRIVER_DSOUND
            /* SDL 1.2 uses the name "dsound", so we'll support both. */
            if (driver_attempt_len == SDL_strlen("dsound") &&
                (SDL_strncasecmp(driver_attempt, "dsound", driver_attempt_len) == 0)) {
                driver_attempt = "directsound";
                driver_attempt_len = SDL_strlen("directsound");
            }
#endif

#ifdef SDL_AUDIO_DRIVER_PULSEAUDIO
            /* SDL 1.2 uses the name "pulse", so we'll support both. */
            if (driver_attempt_len == SDL_strlen("pulse") &&
                (SDL_strncasecmp(driver_attempt, "pulse", driver_attempt_len) == 0)) {
                driver_attempt = "pulseaudio";
                driver_attempt_len = SDL_strlen("pulseaudio");
            }
#endif

            for (i = 0; bootstrap[i]; ++i) {
                if ((driver_attempt_len == SDL_strlen(bootstrap[i]->name)) &&
                    (SDL_strncasecmp(bootstrap[i]->name, driver_attempt, driver_attempt_len) == 0)) {
                    tried_to_init = SDL_TRUE;
                    SDL_zero(current_audio);
                    current_audio.name = bootstrap[i]->name;
                    current_audio.desc = bootstrap[i]->desc;
                    initialized = bootstrap[i]->init(&current_audio.impl);
                    break;
                }
            }

            driver_attempt = (driver_attempt_end != NULL) ? (driver_attempt_end + 1) : NULL;
        }
    } else {
        for (i = 0; (!initialized) && (bootstrap[i]); ++i) {
            if (bootstrap[i]->demand_only) {
                continue;
            }

            tried_to_init = SDL_TRUE;
            SDL_zero(current_audio);
            current_audio.name = bootstrap[i]->name;
            current_audio.desc = bootstrap[i]->desc;
            initialized = bootstrap[i]->init(&current_audio.impl);
        }
    }

    if (!initialized) {
        /* specific drivers will set the error message if they fail... */
        if (!tried_to_init) {
            if (driver_name) {
                SDL_SetError("Audio target '%s' not available", driver_name);
            } else {
                SDL_SetError("No available audio device");
            }
        }

        SDL_zero(current_audio);
        return -1; /* No driver was available, so fail. */
    }

    current_audio.detectionLock = SDL_CreateMutex();

    finish_audio_entry_points_init();

    /* Make sure we have a list of devices available at startup. */
    current_audio.impl.DetectDevices();

    return 0;
}

/*
 * Get the current audio driver name
 */
const char *SDL_GetCurrentAudioDriver(void)
{
    return current_audio.name;
}

/* Clean out devices that we've removed but had to keep around for stability. */
static void clean_out_device_list(SDL_AudioDeviceItem **devices, int *devCount, SDL_bool *removedFlag)
{
    SDL_AudioDeviceItem *item = *devices;
    SDL_AudioDeviceItem *prev = NULL;
    int total = 0;

    while (item) {
        SDL_AudioDeviceItem *next = item->next;
        if (item->handle != NULL) {
            total++;
            prev = item;
        } else {
            if (prev) {
                prev->next = next;
            } else {
                *devices = next;
            }
            /* these two pointers are the same if not a duplicate devname */
            if (item->name != item->original_name) {
                SDL_free(item->name);
            }
            SDL_free(item->original_name);
            SDL_free(item);
        }
        item = next;
    }

    *devCount = total;
    *removedFlag = SDL_FALSE;
}

int SDL_GetNumAudioDevices(int iscapture)
{
    int retval = 0;

    if (!SDL_GetCurrentAudioDriver()) {
        return -1;
    }

    SDL_LockMutex(current_audio.detectionLock);
    if (iscapture && current_audio.captureDevicesRemoved) {
        clean_out_device_list(&current_audio.inputDevices, &current_audio.inputDeviceCount, &current_audio.captureDevicesRemoved);
    }

    if (!iscapture && current_audio.outputDevicesRemoved) {
        clean_out_device_list(&current_audio.outputDevices, &current_audio.outputDeviceCount, &current_audio.outputDevicesRemoved);
    }

    retval = iscapture ? current_audio.inputDeviceCount : current_audio.outputDeviceCount;
    SDL_UnlockMutex(current_audio.detectionLock);

    return retval;
}

const char *SDL_GetAudioDeviceName(int index, int iscapture)
{
    SDL_AudioDeviceItem *item;
    int i;
    const char *retval;

    if (!SDL_GetCurrentAudioDriver()) {
        SDL_SetError("Audio subsystem is not initialized");
        return NULL;
    }

    SDL_LockMutex(current_audio.detectionLock);
    item = iscapture ? current_audio.inputDevices : current_audio.outputDevices;
    i = iscapture ? current_audio.inputDeviceCount : current_audio.outputDeviceCount;
    if (index >= 0 && index < i) {
        for (i--; i > index; i--, item = item->next) {
            SDL_assert(item != NULL);
        }
        SDL_assert(item != NULL);
        retval = item->name;
    } else {
        SDL_InvalidParamError("index");
        retval = NULL;
    }
    SDL_UnlockMutex(current_audio.detectionLock);

    return retval;
}

int SDL_GetAudioDeviceSpec(int index, int iscapture, SDL_AudioSpec *spec)
{
    SDL_AudioDeviceItem *item;
    int i, retval;

    if (spec == NULL) {
        return SDL_InvalidParamError("spec");
    }

    if (!SDL_GetCurrentAudioDriver()) {
        return SDL_SetError("Audio subsystem is not initialized");
    }

    SDL_LockMutex(current_audio.detectionLock);
    item = iscapture ? current_audio.inputDevices : current_audio.outputDevices;
    i = iscapture ? current_audio.inputDeviceCount : current_audio.outputDeviceCount;
    if (index >= 0 && index < i) {
        for (i--; i > index; i--, item = item->next) {
            SDL_assert(item != NULL);
        }
        SDL_assert(item != NULL);
        SDL_copyp(spec, &item->spec);
        retval = 0;
    } else {
        retval = SDL_InvalidParamError("index");
    }
    SDL_UnlockMutex(current_audio.detectionLock);

    return retval;
}

int SDL_GetDefaultAudioInfo(char **name, SDL_AudioSpec *spec, int iscapture)
{
    if (spec == NULL) {
        return SDL_InvalidParamError("spec");
    }

    if (!SDL_GetCurrentAudioDriver()) {
        return SDL_SetError("Audio subsystem is not initialized");
    }

    if (current_audio.impl.GetDefaultAudioInfo == NULL) {
        return SDL_Unsupported();
    }
    return current_audio.impl.GetDefaultAudioInfo(name, spec, iscapture);
}

static void close_audio_device(SDL_AudioDevice *device)
{
    if (!device) {
        return;
    }

    /* make sure the device is paused before we do anything else, so the
       audio callback definitely won't fire again. */
    current_audio.impl.LockDevice(device);
    SDL_AtomicSet(&device->paused, 1);
    SDL_AtomicSet(&device->shutdown, 1);
    SDL_AtomicSet(&device->enabled, 0);
    current_audio.impl.UnlockDevice(device);

    if (device->thread != NULL) {
        SDL_WaitThread(device->thread, NULL);
    }
    if (device->mixer_lock != NULL) {
        SDL_DestroyMutex(device->mixer_lock);
    }

    SDL_free(device->work_buffer);
    SDL_DestroyAudioStream(device->stream);

    if (device->id > 0) {
        SDL_AudioDevice *opendev = open_devices[device->id - 1];
        SDL_assert((opendev == device) || (opendev == NULL));
        if (opendev == device) {
            open_devices[device->id - 1] = NULL;
        }
    }

    if (device->hidden != NULL) {
        current_audio.impl.CloseDevice(device);
    }

    SDL_DestroyDataQueue(device->buffer_queue);

    SDL_free(device);
}

static Uint16 GetDefaultSamplesFromFreq(int freq)
{
    /* Pick a default of ~46 ms at desired frequency */
    /* !!! FIXME: remove this when the non-Po2 resampling is in. */
    const Uint16 max_sample = (Uint16)((freq / 1000) * 46);
    Uint16 current_sample = 1;
    while (current_sample < max_sample) {
        current_sample *= 2;
    }
    return current_sample;
}

/*
 * Sanity check desired AudioSpec for SDL_OpenAudio() in (orig).
 *  Fills in a sanitized copy in (prepared).
 *  Returns non-zero if okay, zero on fatal parameters in (orig).
 */
static int prepare_audiospec(const SDL_AudioSpec *orig, SDL_AudioSpec *prepared)
{
    SDL_copyp(prepared, orig);

    if (orig->freq == 0) {
        static const int DEFAULT_FREQ = 22050;
        const char *env = SDL_getenv("SDL_AUDIO_FREQUENCY");
        if (env != NULL) {
            int freq = SDL_atoi(env);
            prepared->freq = freq != 0 ? freq : DEFAULT_FREQ;
        } else {
            prepared->freq = DEFAULT_FREQ;
        }
    }

    if (orig->format == 0) {
        const char *env = SDL_getenv("SDL_AUDIO_FORMAT");
        if (env != NULL) {
            const SDL_AudioFormat format = SDL_ParseAudioFormat(env);
            prepared->format = format != 0 ? format : SDL_AUDIO_S16;
        } else {
            prepared->format = SDL_AUDIO_S16;
        }
    }

    if (orig->channels == 0) {
        const char *env = SDL_getenv("SDL_AUDIO_CHANNELS");
        if (env != NULL) {
            Uint8 channels = (Uint8)SDL_atoi(env);
            prepared->channels = channels != 0 ? channels : 2;
        } else {
            prepared->channels = 2;
        }
    } else if (orig->channels > 8) {
        SDL_SetError("Unsupported number of audio channels.");
        return 0;
    }

    if (orig->samples == 0) {
        const char *env = SDL_getenv("SDL_AUDIO_SAMPLES");
        if (env != NULL) {
            Uint16 samples = (Uint16)SDL_atoi(env);
            prepared->samples = samples != 0 ? samples : GetDefaultSamplesFromFreq(prepared->freq);
        } else {
            prepared->samples = GetDefaultSamplesFromFreq(prepared->freq);
        }
    }

    /* Calculate the silence and size of the audio specification */
    SDL_CalculateAudioSpec(prepared);

    return 1;
}

static SDL_AudioDeviceID open_audio_device(const char *devname, int iscapture,
                                           const SDL_AudioSpec *desired, SDL_AudioSpec *obtained,
                                           int allowed_changes, int min_id)
{
    const SDL_bool is_internal_thread = (desired->callback == NULL);
    SDL_AudioDeviceID id = 0;
    SDL_AudioSpec _obtained;
    SDL_AudioDevice *device;
    SDL_bool build_stream;
    void *handle = NULL;
    int i = 0;

    if (!SDL_GetCurrentAudioDriver()) {
        SDL_SetError("Audio subsystem is not initialized");
        return 0;
    }

    if (iscapture && !current_audio.impl.HasCaptureSupport) {
        SDL_SetError("No capture support");
        return 0;
    }

    SDL_LockMutex(current_audio.detectionLock);
    /* Find an available device ID... */
    for (id = min_id - 1; id < SDL_arraysize(open_devices); id++) {
        if (open_devices[id] == NULL) {
            break;
        }
    }

    if (id == SDL_arraysize(open_devices)) {
        SDL_SetError("Too many open audio devices");
        SDL_UnlockMutex(current_audio.detectionLock);
        return 0;
    }

    if (!obtained) {
        obtained = &_obtained;
    }
    if (!prepare_audiospec(desired, obtained)) {
        SDL_UnlockMutex(current_audio.detectionLock);
        return 0;
    }

    /* If app doesn't care about a specific device, let the user override. */
    if (devname == NULL) {
        devname = SDL_getenv("SDL_AUDIO_DEVICE_NAME");
    }

    /*
     * Catch device names at the high level for the simple case...
     * This lets us have a basic "device enumeration" for systems that
     *  don't have multiple devices, but makes sure the device name is
     *  always NULL when it hits the low level.
     *
     * Also make sure that the simple case prevents multiple simultaneous
     *  opens of the default system device.
     */

    if ((iscapture) && (current_audio.impl.OnlyHasDefaultCaptureDevice)) {
        if ((devname) && (SDL_strcmp(devname, DEFAULT_INPUT_DEVNAME) != 0)) {
            SDL_SetError("No such device");
            SDL_UnlockMutex(current_audio.detectionLock);
            return 0;
        }
        devname = NULL;

        for (i = 0; i < SDL_arraysize(open_devices); i++) {
            if ((open_devices[i]) && (open_devices[i]->iscapture)) {
                SDL_SetError("Audio device already open");
                SDL_UnlockMutex(current_audio.detectionLock);
                return 0;
            }
        }
    } else if ((!iscapture) && (current_audio.impl.OnlyHasDefaultOutputDevice)) {
        if ((devname) && (SDL_strcmp(devname, DEFAULT_OUTPUT_DEVNAME) != 0)) {
            SDL_UnlockMutex(current_audio.detectionLock);
            SDL_SetError("No such device");
            return 0;
        }
        devname = NULL;

        for (i = 0; i < SDL_arraysize(open_devices); i++) {
            if ((open_devices[i]) && (!open_devices[i]->iscapture)) {
                SDL_UnlockMutex(current_audio.detectionLock);
                SDL_SetError("Audio device already open");
                return 0;
            }
        }
    } else if (devname != NULL) {
        /* if the app specifies an exact string, we can pass the backend
           an actual device handle thingey, which saves them the effort of
           figuring out what device this was (such as, reenumerating
           everything again to find the matching human-readable name).
           It might still need to open a device based on the string for,
           say, a network audio server, but this optimizes some cases. */
        SDL_AudioDeviceItem *item;
        for (item = iscapture ? current_audio.inputDevices : current_audio.outputDevices; item; item = item->next) {
            if ((item->handle != NULL) && (SDL_strcmp(item->name, devname) == 0)) {
                handle = item->handle;
                break;
            }
        }
    }

    if (!current_audio.impl.AllowsArbitraryDeviceNames) {
        /* has to be in our device list, or the default device. */
        if ((handle == NULL) && (devname != NULL)) {
            SDL_SetError("No such device.");
            SDL_UnlockMutex(current_audio.detectionLock);
            return 0;
        }
    }

    device = (SDL_AudioDevice *)SDL_calloc(1, sizeof(SDL_AudioDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        SDL_UnlockMutex(current_audio.detectionLock);
        return 0;
    }
    device->id = id + 1;
    device->spec = *obtained;
    device->iscapture = iscapture ? SDL_TRUE : SDL_FALSE;
    device->handle = handle;

    SDL_AtomicSet(&device->shutdown, 0); /* just in case. */
    SDL_AtomicSet(&device->paused, 1);
    SDL_AtomicSet(&device->enabled, 1);

    /* Create a mutex for locking the sound buffers */
    if (current_audio.impl.LockDevice == SDL_AudioLockDevice_Default) {
        device->mixer_lock = SDL_CreateMutex();
        if (device->mixer_lock == NULL) {
            close_audio_device(device);
            SDL_UnlockMutex(current_audio.detectionLock);
            SDL_SetError("Couldn't create mixer lock");
            return 0;
        }
    }

    /* For backends that require a power-of-two value for spec.samples, take the
     * value we got from 'desired' and round up to the nearest value
     */
    if (!current_audio.impl.SupportsNonPow2Samples && device->spec.samples > 0) {
        int samples = SDL_powerof2(device->spec.samples);
        if (samples <= SDL_MAX_UINT16) {
            device->spec.samples = (Uint16)samples;
        } else {
            SDL_SetError("Couldn't hold sample count %d\n", samples);
            return 0;
        }
    }

    if (current_audio.impl.OpenDevice(device, devname) < 0) {
        close_audio_device(device);
        SDL_UnlockMutex(current_audio.detectionLock);
        return 0;
    }

    /* if your target really doesn't need it, set it to 0x1 or something. */
    /* otherwise, close_audio_device() won't call impl.CloseDevice(). */
    SDL_assert(device->hidden != NULL);

    /* See if we need to do any conversion */
    build_stream = SDL_FALSE;
    if (obtained->freq != device->spec.freq) {
        if (allowed_changes & SDL_AUDIO_ALLOW_FREQUENCY_CHANGE) {
            obtained->freq = device->spec.freq;
        } else {
            build_stream = SDL_TRUE;
        }
    }
    if (obtained->format != device->spec.format) {
        if (allowed_changes & SDL_AUDIO_ALLOW_FORMAT_CHANGE) {
            obtained->format = device->spec.format;
        } else {
            build_stream = SDL_TRUE;
        }
    }
    if (obtained->channels != device->spec.channels) {
        if (allowed_changes & SDL_AUDIO_ALLOW_CHANNELS_CHANGE) {
            obtained->channels = device->spec.channels;
        } else {
            build_stream = SDL_TRUE;
        }
    }
    if (device->spec.samples != obtained->samples) {
        if (allowed_changes & SDL_AUDIO_ALLOW_SAMPLES_CHANGE) {
            obtained->samples = device->spec.samples;
        } else {
            build_stream = SDL_TRUE;
        }
    }

    SDL_CalculateAudioSpec(obtained); /* recalc after possible changes. */

    device->callbackspec = *obtained;

    if (build_stream) {
        if (iscapture) {
            device->stream = SDL_CreateAudioStream(device->spec.format,
                                                device->spec.channels, device->spec.freq,
                                                obtained->format, obtained->channels, obtained->freq);
        } else {
            device->stream = SDL_CreateAudioStream(obtained->format, obtained->channels,
                                                obtained->freq, device->spec.format,
                                                device->spec.channels, device->spec.freq);
        }

        if (!device->stream) {
            close_audio_device(device);
            SDL_UnlockMutex(current_audio.detectionLock);
            return 0;
        }
    }

    if (device->spec.callback == NULL) { /* use buffer queueing? */
        /* pool a few packets to start. Enough for two callbacks. */
        device->buffer_queue = SDL_CreateDataQueue(SDL_AUDIOBUFFERQUEUE_PACKETLEN, obtained->size * 2);
        if (!device->buffer_queue) {
            close_audio_device(device);
            SDL_UnlockMutex(current_audio.detectionLock);
            SDL_SetError("Couldn't create audio buffer queue");
            return 0;
        }
        device->callbackspec.callback = iscapture ? SDL_BufferQueueFillCallback : SDL_BufferQueueDrainCallback;
        device->callbackspec.userdata = device;
    }

    /* Allocate a scratch audio buffer */
    device->work_buffer_len = build_stream ? device->callbackspec.size : 0;
    if (device->spec.size > device->work_buffer_len) {
        device->work_buffer_len = device->spec.size;
    }
    SDL_assert(device->work_buffer_len > 0);

    device->work_buffer = (Uint8 *)SDL_malloc(device->work_buffer_len);
    if (device->work_buffer == NULL) {
        close_audio_device(device);
        SDL_UnlockMutex(current_audio.detectionLock);
        SDL_OutOfMemory();
        return 0;
    }

    open_devices[id] = device; /* add it to our list of open devices. */

    /* Start the audio thread if necessary */
    if (!current_audio.impl.ProvidesOwnCallbackThread) {
        /* Start the audio thread */
        /* !!! FIXME: we don't force the audio thread stack size here if it calls into user code, but maybe we should? */
        /* buffer queueing callback only needs a few bytes, so make the stack tiny. */
        const size_t stacksize = is_internal_thread ? 64 * 1024 : 0;
        char threadname[64];

        (void)SDL_snprintf(threadname, sizeof(threadname), "SDLAudio%c%" SDL_PRIu32, (iscapture) ? 'C' : 'P', device->id);
        device->thread = SDL_CreateThreadInternal(iscapture ? SDL_CaptureAudio : SDL_RunAudio, threadname, stacksize, device);

        if (device->thread == NULL) {
            close_audio_device(device);
            SDL_SetError("Couldn't create audio thread");
            SDL_UnlockMutex(current_audio.detectionLock);
            return 0;
        }
    }
    SDL_UnlockMutex(current_audio.detectionLock);

    return device->id;
}

SDL_AudioDeviceID SDL_OpenAudioDevice(const char *device, int iscapture,
                    const SDL_AudioSpec *desired, SDL_AudioSpec *obtained,
                    int allowed_changes)
{
    return open_audio_device(device, iscapture, desired, obtained,
                             allowed_changes, 2);
}

SDL_AudioStatus SDL_GetAudioDeviceStatus(SDL_AudioDeviceID devid)
{
    SDL_AudioDevice *device = get_audio_device(devid);
    SDL_AudioStatus status = SDL_AUDIO_STOPPED;
    if (device && SDL_AtomicGet(&device->enabled)) {
        if (SDL_AtomicGet(&device->paused)) {
            status = SDL_AUDIO_PAUSED;
        } else {
            status = SDL_AUDIO_PLAYING;
        }
    }
    return status;
}

int SDL_PauseAudioDevice(SDL_AudioDeviceID devid)
{
    SDL_AudioDevice *device = get_audio_device(devid);
    if (!device) {
        return SDL_InvalidParamError("devid");
    }
    current_audio.impl.LockDevice(device);
    SDL_AtomicSet(&device->paused, 1);
    current_audio.impl.UnlockDevice(device);
    return 0;
}

int SDL_PlayAudioDevice(SDL_AudioDeviceID devid)
{
    SDL_AudioDevice *device = get_audio_device(devid);
    if (!device) {
        return SDL_InvalidParamError("devid");
    }
    current_audio.impl.LockDevice(device);
    SDL_AtomicSet(&device->paused, 0);
    current_audio.impl.UnlockDevice(device);
    return 0;
}

int SDL_LockAudioDevice(SDL_AudioDeviceID devid)
{
    /* Obtain a lock on the mixing buffers */
    SDL_AudioDevice *device = get_audio_device(devid);
    if (!device) {
        return SDL_InvalidParamError("devid");
    }
    current_audio.impl.LockDevice(device);
    return 0;
}

void SDL_UnlockAudioDevice(SDL_AudioDeviceID devid)
{
    /* Obtain a lock on the mixing buffers */
    SDL_AudioDevice *device = get_audio_device(devid);
    if (!device) {
        return;
    }
    current_audio.impl.UnlockDevice(device);
}

void SDL_CloseAudioDevice(SDL_AudioDeviceID devid)
{
    SDL_AudioDevice *device = get_audio_device(devid);
    if (!device) {
        return;
    }
    close_audio_device(device);
}

void SDL_QuitAudio(void)
{
    SDL_AudioDeviceID i;

    if (!current_audio.name) { /* not initialized?! */
        return;
    }

    for (i = 0; i < SDL_arraysize(open_devices); i++) {
        close_audio_device(open_devices[i]);
    }

    free_device_list(&current_audio.outputDevices, &current_audio.outputDeviceCount);
    free_device_list(&current_audio.inputDevices, &current_audio.inputDeviceCount);

    /* Free the driver data */
    current_audio.impl.Deinitialize();

    SDL_DestroyMutex(current_audio.detectionLock);

    SDL_zero(current_audio);
    SDL_zeroa(open_devices);
}

#define NUM_FORMATS 8
static const SDL_AudioFormat format_list[NUM_FORMATS][NUM_FORMATS + 1] = {
    { SDL_AUDIO_U8, SDL_AUDIO_S8, SDL_AUDIO_S16LSB, SDL_AUDIO_S16MSB, SDL_AUDIO_S32LSB, SDL_AUDIO_S32MSB, SDL_AUDIO_F32LSB, SDL_AUDIO_F32MSB, 0 },
    { SDL_AUDIO_S8, SDL_AUDIO_U8, SDL_AUDIO_S16LSB, SDL_AUDIO_S16MSB, SDL_AUDIO_S32LSB, SDL_AUDIO_S32MSB, SDL_AUDIO_F32LSB, SDL_AUDIO_F32MSB, 0 },
    { SDL_AUDIO_S16LSB, SDL_AUDIO_S16MSB, SDL_AUDIO_S32LSB, SDL_AUDIO_S32MSB, SDL_AUDIO_F32LSB, SDL_AUDIO_F32MSB, SDL_AUDIO_U8, SDL_AUDIO_S8, 0 },
    { SDL_AUDIO_S16MSB, SDL_AUDIO_S16LSB, SDL_AUDIO_S32MSB, SDL_AUDIO_S32LSB, SDL_AUDIO_F32MSB, SDL_AUDIO_F32LSB, SDL_AUDIO_U8, SDL_AUDIO_S8, 0 },
    { SDL_AUDIO_S32LSB, SDL_AUDIO_S32MSB, SDL_AUDIO_F32LSB, SDL_AUDIO_F32MSB, SDL_AUDIO_S16LSB, SDL_AUDIO_S16MSB, SDL_AUDIO_U8, SDL_AUDIO_S8, 0 },
    { SDL_AUDIO_S32MSB, SDL_AUDIO_S32LSB, SDL_AUDIO_F32MSB, SDL_AUDIO_F32LSB, SDL_AUDIO_S16MSB, SDL_AUDIO_S16LSB, SDL_AUDIO_U8, SDL_AUDIO_S8, 0 },
    { SDL_AUDIO_F32LSB, SDL_AUDIO_F32MSB, SDL_AUDIO_S32LSB, SDL_AUDIO_S32MSB, SDL_AUDIO_S16LSB, SDL_AUDIO_S16MSB, SDL_AUDIO_U8, SDL_AUDIO_S8, 0 },
    { SDL_AUDIO_F32MSB, SDL_AUDIO_F32LSB, SDL_AUDIO_S32MSB, SDL_AUDIO_S32LSB, SDL_AUDIO_S16MSB, SDL_AUDIO_S16LSB, SDL_AUDIO_U8, SDL_AUDIO_S8, 0 },
};

const SDL_AudioFormat *SDL_ClosestAudioFormats(SDL_AudioFormat format)
{
    int i;
    for (i = 0; i < NUM_FORMATS; i++) {
        if (format_list[i][0] == format) {
            return &format_list[i][0];
        }
    }
    return &format_list[0][NUM_FORMATS]; /* not found; return what looks like a list with only a zero in it. */
}

Uint8 SDL_GetSilenceValueForFormat(const SDL_AudioFormat format)
{
    return (format == SDL_AUDIO_U8) ? 0x80 : 0x00;
}

void SDL_CalculateAudioSpec(SDL_AudioSpec *spec)
{
    spec->silence = SDL_GetSilenceValueForFormat(spec->format);
    spec->size = SDL_AUDIO_BITSIZE(spec->format) / 8;
    spec->size *= spec->channels;
    spec->size *= spec->samples;
}

/* !!! FIXME: move this to SDL_audiocvt.c */
int SDL_ConvertAudioSamples(SDL_AudioFormat src_format, Uint8 src_channels, int src_rate, const Uint8 *src_data, int src_len,
                            SDL_AudioFormat dst_format, Uint8 dst_channels, int dst_rate, Uint8 **dst_data, int *dst_len)
{
    int ret = -1;
    SDL_AudioStream *stream = NULL;
    Uint8 *dst = NULL;
    int dstlen = 0;

    if (dst_data) {
        *dst_data = NULL;
    }

    if (dst_len) {
        *dst_len = 0;
    }

    if (src_data == NULL) {
        return SDL_InvalidParamError("src_data");
    } else if (src_len < 0) {
        return SDL_InvalidParamError("src_len");
    } else if (dst_data == NULL) {
        return SDL_InvalidParamError("dst_data");
    } else if (dst_len == NULL) {
        return SDL_InvalidParamError("dst_len");
    }

    stream = SDL_CreateAudioStream(src_format, src_channels, src_rate, dst_format, dst_channels, dst_rate);
    if (stream != NULL) {
        if ((SDL_PutAudioStreamData(stream, src_data, src_len) == 0) && (SDL_FlushAudioStream(stream) == 0)) {
            dstlen = SDL_GetAudioStreamAvailable(stream);
            if (dstlen >= 0) {
                dst = (Uint8 *)SDL_malloc(dstlen);
                if (!dst) {
                    SDL_OutOfMemory();
                } else {
                    ret = (SDL_GetAudioStreamData(stream, dst, dstlen) >= 0) ? 0 : -1;
                }
            }
        }
    }

    if (ret == -1) {
        SDL_free(dst);
    } else {
        *dst_data = dst;
        *dst_len = dstlen;
    }

    SDL_DestroyAudioStream(stream);
    return ret;
}

