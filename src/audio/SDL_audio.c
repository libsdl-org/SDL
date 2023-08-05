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

#include "SDL_audio_c.h"
#include "SDL_sysaudio.h"
#include "../thread/SDL_systhread.h"
#include "../SDL_utils_c.h"

// Available audio drivers
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
    &AAUDIO_bootstrap,
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

static SDL_AudioDriver current_audio;

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

const char *SDL_GetCurrentAudioDriver(void)
{
    return current_audio.name;
}

// device management and hotplug...


/* SDL_AudioDevice, in SDL3, represents a piece of physical hardware, whether it is in use or not, so these objects exist as long as
   the system-level device is available.

   Physical devices get destroyed for three reasons:
    - They were lost to the system (a USB cable is kicked out, etc).
    - They failed for some other unlikely reason at the API level (which is _also_ probably a USB cable being kicked out).
    - We are shutting down, so all allocated resources are being freed.

   They are _not_ destroyed because we are done using them (when we "close" a playing device).
*/
static void ClosePhysicalAudioDevice(SDL_AudioDevice *device);


// the loop in assign_audio_device_instance_id relies on this being true.
SDL_COMPILE_TIME_ASSERT(check_lowest_audio_default_value, SDL_AUDIO_DEVICE_DEFAULT_CAPTURE < SDL_AUDIO_DEVICE_DEFAULT_OUTPUT);

static SDL_AudioDeviceID assign_audio_device_instance_id(SDL_bool iscapture, SDL_bool islogical)
{
    /* Assign an instance id! Start at 2, in case there are things from the SDL2 era that still think 1 is a special value.
       There's no reasonable scenario where this rolls over, but just in case, we wrap it in a loop.
       Also, make sure we don't assign SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, etc. */

    // The bottom two bits of the instance id tells you if it's an output device (1<<0), and if it's a physical device (1<<1). Make sure these are right.
    const SDL_AudioDeviceID required_mask = (iscapture ? 0 : (1<<0)) | (islogical ? 0 : (1<<1));

    SDL_AudioDeviceID instance_id;
    do {
        instance_id = (SDL_AudioDeviceID) (SDL_AtomicIncRef(&current_audio.last_device_instance_id) + 1);
    } while ( (instance_id < 2) || (instance_id >= SDL_AUDIO_DEVICE_DEFAULT_CAPTURE) || ((instance_id & 0x3) != required_mask) );
    return instance_id;
}

// this assumes you hold the _physical_ device lock for this logical device! This will not unlock the lock or close the physical device!
static void DestroyLogicalAudioDevice(SDL_LogicalAudioDevice *logdev)
{
    // remove ourselves from the physical device's list of logical devices.
    if (logdev->next) {
        logdev->next->prev = logdev->prev;
    }
    if (logdev->prev) {
        logdev->prev->next = logdev->next;
    }
    if (logdev->physical_device->logical_devices == logdev) {
        logdev->physical_device->logical_devices = logdev->next;
    }

    // unbind any still-bound streams...
    SDL_AudioStream *next;
    for (SDL_AudioStream *stream = logdev->bound_streams; stream != NULL; stream = next) {
        SDL_LockMutex(stream->lock);
        next = stream->next_binding;
        stream->next_binding = NULL;
        stream->prev_binding = NULL;
        stream->bound_device = NULL;
        SDL_UnlockMutex(stream->lock);
    }

    SDL_free(logdev);
}

// this must not be called while `device` is still in a device list, or while a device's audio thread is still running (except if the thread calls this while shutting down). */
static void DestroyPhysicalAudioDevice(SDL_AudioDevice *device)
{
    if (!device) {
        return;
    }

    // Destroy any logical devices that still exist...
    SDL_LockMutex(device->lock);
    while (device->logical_devices != NULL) {
        DestroyLogicalAudioDevice(device->logical_devices);
    }
    SDL_UnlockMutex(device->lock);

    // it's safe to not hold the lock for this (we can't anyhow, or the audio thread won't quit), because we shouldn't be in the device list at this point.
    ClosePhysicalAudioDevice(device);

    current_audio.impl.FreeDeviceHandle(device);

    SDL_DestroyMutex(device->lock);
    SDL_free(device->work_buffer);
    SDL_free(device->name);
    SDL_free(device);
}

static SDL_AudioDevice *CreatePhysicalAudioDevice(const char *name, SDL_bool iscapture, const SDL_AudioSpec *spec, void *handle, SDL_AudioDevice **devices, SDL_AtomicInt *device_count)
{
    SDL_assert(name != NULL);

    if (SDL_AtomicGet(&current_audio.shutting_down)) {
        return NULL;  // we're shutting down, don't add any devices that are hotplugged at the last possible moment.
    }

    SDL_AudioDevice *device = (SDL_AudioDevice *)SDL_calloc(1, sizeof(SDL_AudioDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    device->name = SDL_strdup(name);
    if (!device->name) {
        SDL_free(device);
        SDL_OutOfMemory();
        return NULL;
    }

    device->lock = SDL_CreateMutex();
    if (!device->lock) {
        SDL_free(device->name);
        SDL_free(device);
        return NULL;
    }

    SDL_AtomicSet(&device->shutdown, 0);
    SDL_AtomicSet(&device->condemned, 0);
    SDL_AtomicSet(&device->zombie, 0);
    device->iscapture = iscapture;
    SDL_memcpy(&device->spec, spec, sizeof (SDL_AudioSpec));
    SDL_memcpy(&device->default_spec, spec, sizeof (SDL_AudioSpec));
    device->silence_value = SDL_GetSilenceValueForFormat(device->spec.format);
    device->handle = handle;
    device->prev = NULL;

    device->instance_id = assign_audio_device_instance_id(iscapture, /*islogical=*/SDL_FALSE);

    SDL_LockRWLockForWriting(current_audio.device_list_lock);

    if (*devices) {
        SDL_assert((*devices)->prev == NULL);
        (*devices)->prev = device;
    }
    device->next = *devices;
    *devices = device;
    SDL_AtomicAdd(device_count, 1);
    SDL_UnlockRWLock(current_audio.device_list_lock);

    return device;
}

static SDL_AudioDevice *CreateAudioCaptureDevice(const char *name, const SDL_AudioSpec *spec, void *handle)
{
    SDL_assert(current_audio.impl.HasCaptureSupport);
    return CreatePhysicalAudioDevice(name, SDL_TRUE, spec, handle, &current_audio.capture_devices, &current_audio.capture_device_count);
}

static SDL_AudioDevice *CreateAudioOutputDevice(const char *name, const SDL_AudioSpec *spec, void *handle)
{
    return CreatePhysicalAudioDevice(name, SDL_FALSE, spec, handle, &current_audio.output_devices, &current_audio.output_device_count);
}

// The audio backends call this when a new device is plugged in.
SDL_AudioDevice *SDL_AddAudioDevice(const SDL_bool iscapture, const char *name, const SDL_AudioSpec *inspec, void *handle)
{
    const SDL_AudioFormat default_format = iscapture ? DEFAULT_AUDIO_CAPTURE_FORMAT : DEFAULT_AUDIO_OUTPUT_FORMAT;
    const int default_channels = iscapture ? DEFAULT_AUDIO_CAPTURE_CHANNELS : DEFAULT_AUDIO_OUTPUT_CHANNELS;
    const int default_freq = iscapture ? DEFAULT_AUDIO_CAPTURE_FREQUENCY : DEFAULT_AUDIO_OUTPUT_FREQUENCY;

    SDL_AudioSpec spec;
    if (!inspec) {
        spec.format = default_format;
        spec.channels = default_channels;
        spec.freq = default_freq;
    } else {
        spec.format = (inspec->format != 0) ? inspec->format : default_format;
        spec.channels = (inspec->channels != 0) ? inspec->channels : default_channels;
        spec.freq = (inspec->freq != 0) ? inspec->freq : default_freq;
    }

    SDL_AudioDevice *device = iscapture ? CreateAudioCaptureDevice(name, &spec, handle) : CreateAudioOutputDevice(name, &spec, handle);
    if (device) {
        // Post the event, if desired
        if (SDL_EventEnabled(SDL_EVENT_AUDIO_DEVICE_ADDED)) {
            SDL_Event event;
            event.type = SDL_EVENT_AUDIO_DEVICE_ADDED;
            event.common.timestamp = 0;
            event.adevice.which = device->instance_id;
            event.adevice.iscapture = iscapture;
            SDL_PushEvent(&event);
        }
    }

    return device;
}

// this _also_ destroys the logical device!
static void DisconnectLogicalAudioDevice(SDL_LogicalAudioDevice *logdev)
{
    if (SDL_EventEnabled(SDL_EVENT_AUDIO_DEVICE_REMOVED)) {
        SDL_Event event;
        SDL_zero(event);
        event.type = SDL_EVENT_AUDIO_DEVICE_REMOVED;
        event.common.timestamp = 0;
        event.adevice.which = logdev->instance_id;
        event.adevice.iscapture = logdev->physical_device->iscapture ? 1 : 0;
        SDL_PushEvent(&event);
    }

    DestroyLogicalAudioDevice(logdev);
}

// Called when a device is removed from the system, or it fails unexpectedly, from any thread, possibly even the audio device's thread.
void SDL_AudioDeviceDisconnected(SDL_AudioDevice *device)
{
    if (!device) {
        return;
    }

    // if the current default device is going down, mark it as dead but keep it around until a replacement is decided upon, so we can migrate logical devices to it.
    if ((device->instance_id == current_audio.default_output_device_id) || (device->instance_id == current_audio.default_capture_device_id)) {
        SDL_LockMutex(device->lock);  // make sure nothing else is messing with the device before continuing.
        SDL_AtomicSet(&device->zombie, 1);
        SDL_AtomicSet(&device->shutdown, 1);  // tell audio thread to terminate, but don't mark it condemned, so the thread won't destroy the device. We'll join on the audio thread later.

        // dump any logical devices that explicitly opened this device. Things that opened the system default can stay.
        SDL_LogicalAudioDevice *next = NULL;
        for (SDL_LogicalAudioDevice *logdev = device->logical_devices; logdev != NULL; logdev = next) {
            next = logdev->next;
            if (!logdev->is_default) {  // if opened as a default, leave it on the zombie device for later migration.
                DisconnectLogicalAudioDevice(logdev);
            }
        }
        SDL_UnlockMutex(device->lock);  // make sure nothing else is messing with the device before continuing.
        return;  // done for now. Come back when a new default device is chosen!
    }

    SDL_bool was_live = SDL_FALSE;

    // take it out of the device list.
    SDL_LockRWLockForWriting(current_audio.device_list_lock);
    SDL_LockMutex(device->lock);  // make sure nothing else is messing with the device before continuing.
    if (device == current_audio.output_devices) {
        SDL_assert(device->prev == NULL);
        current_audio.output_devices = device->next;
        was_live = SDL_TRUE;
    } else if (device == current_audio.capture_devices) {
        SDL_assert(device->prev == NULL);
        current_audio.capture_devices = device->next;
        was_live = SDL_TRUE;
    }
    if (device->prev != NULL) {
        device->prev->next = device->next;
        was_live = SDL_TRUE;
    }
    if (device->next != NULL) {
        device->next->prev = device->prev;
        was_live = SDL_TRUE;
    }

    device->next = NULL;
    device->prev = NULL;

    if (was_live) {
        SDL_AtomicAdd(device->iscapture ? &current_audio.capture_device_count : &current_audio.output_device_count, -1);
    }

    SDL_UnlockRWLock(current_audio.device_list_lock);

    // now device is not in the list, and we own it, so no one should be able to find it again, except the audio thread, which holds a pointer!
    SDL_AtomicSet(&device->condemned, 1);
    SDL_AtomicSet(&device->shutdown, 1);  // tell audio thread to terminate.

    // disconnect each attached logical device, so apps won't find their streams still bound if they get the REMOVED event before the device thread cleans up.
    SDL_LogicalAudioDevice *next;
    for (SDL_LogicalAudioDevice *logdev = device->logical_devices; logdev != NULL; logdev = next) {
        next = logdev->next;
        DisconnectLogicalAudioDevice(logdev);
    }

    // if there's an audio thread, don't free until thread is terminating, otherwise free stuff now.
    const SDL_bool should_destroy = SDL_AtomicGet(&device->thread_alive) ? SDL_FALSE : SDL_TRUE;
    SDL_UnlockMutex(device->lock);

    // Post the event, if we haven't tried to before and if it's desired
    if (was_live && SDL_EventEnabled(SDL_EVENT_AUDIO_DEVICE_REMOVED)) {
        SDL_Event event;
        SDL_zero(event);
        event.type = SDL_EVENT_AUDIO_DEVICE_REMOVED;
        event.common.timestamp = 0;
        event.adevice.which = device->instance_id;
        event.adevice.iscapture = device->iscapture ? 1 : 0;
        SDL_PushEvent(&event);
    }

    if (should_destroy) {
        DestroyPhysicalAudioDevice(device);
    }
}


// stubs for audio drivers that don't need a specific entry point...

static void SDL_AudioThreadDeinit_Default(SDL_AudioDevice *device) { /* no-op. */ }
static void SDL_AudioWaitDevice_Default(SDL_AudioDevice *device) { /* no-op. */ }
static void SDL_AudioPlayDevice_Default(SDL_AudioDevice *device, const Uint8 *buffer, int buffer_size) { /* no-op. */ }
static void SDL_AudioWaitCaptureDevice_Default(SDL_AudioDevice *device) { /* no-op. */ }
static void SDL_AudioFlushCapture_Default(SDL_AudioDevice *device) { /* no-op. */ }
static void SDL_AudioCloseDevice_Default(SDL_AudioDevice *device) { /* no-op. */ }
static void SDL_AudioDeinitialize_Default(void) { /* no-op. */ }
static void SDL_AudioFreeDeviceHandle_Default(SDL_AudioDevice *device) { /* no-op. */ }

static void SDL_AudioThreadInit_Default(SDL_AudioDevice *device)
{
    SDL_SetThreadPriority(device->iscapture ? SDL_THREAD_PRIORITY_HIGH : SDL_THREAD_PRIORITY_TIME_CRITICAL);
}

static void SDL_AudioDetectDevices_Default(SDL_AudioDevice **default_output, SDL_AudioDevice **default_capture)
{
    // you have to write your own implementation if these assertions fail.
    SDL_assert(current_audio.impl.OnlyHasDefaultOutputDevice);
    SDL_assert(current_audio.impl.OnlyHasDefaultCaptureDevice || !current_audio.impl.HasCaptureSupport);

    *default_output = SDL_AddAudioDevice(SDL_FALSE, DEFAULT_OUTPUT_DEVNAME, NULL, (void *)((size_t)0x1));
    if (current_audio.impl.HasCaptureSupport) {
        *default_capture = SDL_AddAudioDevice(SDL_TRUE, DEFAULT_INPUT_DEVNAME, NULL, (void *)((size_t)0x2));
    }
}

static Uint8 *SDL_AudioGetDeviceBuf_Default(SDL_AudioDevice *device, int *buffer_size)
{
    *buffer_size = 0;
    return NULL;
}

static int SDL_AudioCaptureFromDevice_Default(SDL_AudioDevice *device, void *buffer, int buflen)
{
    return SDL_Unsupported();
}

static int SDL_AudioOpenDevice_Default(SDL_AudioDevice *device)
{
    return SDL_Unsupported();
}

// Fill in stub functions for unused driver entry points. This lets us blindly call them without having to check for validity first.
static void CompleteAudioEntryPoints(void)
{
    #define FILL_STUB(x) if (!current_audio.impl.x) { current_audio.impl.x = SDL_Audio##x##_Default; }
    FILL_STUB(DetectDevices);
    FILL_STUB(OpenDevice);
    FILL_STUB(ThreadInit);
    FILL_STUB(ThreadDeinit);
    FILL_STUB(WaitDevice);
    FILL_STUB(PlayDevice);
    FILL_STUB(GetDeviceBuf);
    FILL_STUB(WaitCaptureDevice);
    FILL_STUB(CaptureFromDevice);
    FILL_STUB(FlushCapture);
    FILL_STUB(CloseDevice);
    FILL_STUB(FreeDeviceHandle);
    FILL_STUB(Deinitialize);
    #undef FILL_STUB
}

static SDL_AudioDeviceID GetFirstAddedAudioDeviceID(const SDL_bool iscapture)
{
    // (these are pushed to the front of the linked list as added, so the first device added is last in the list.)
    SDL_LockRWLockForReading(current_audio.device_list_lock);
    SDL_AudioDevice *last = NULL;
    for (SDL_AudioDevice *i = iscapture ? current_audio.capture_devices : current_audio.output_devices; i != NULL; i = i->next) {
        last = i;
    }
    const SDL_AudioDeviceID retval = last ? last->instance_id : 0;
    SDL_UnlockRWLock(current_audio.device_list_lock);
    return retval;
}

// !!! FIXME: the video subsystem does SDL_VideoInit, not SDL_InitVideo. Make this match.
int SDL_InitAudio(const char *driver_name)
{
    if (SDL_GetCurrentAudioDriver()) {
        SDL_QuitAudio(); // shutdown driver if already running.
    }

    SDL_ChooseAudioConverters();

    SDL_RWLock *device_list_lock = SDL_CreateRWLock();  // create this early, so if it fails we don't have to tear down the whole audio subsystem.
    if (!device_list_lock) {
        return -1;
    }

    // Select the proper audio driver
    if (driver_name == NULL) {
        driver_name = SDL_GetHint(SDL_HINT_AUDIO_DRIVER);
    }

    SDL_bool initialized = SDL_FALSE;
    SDL_bool tried_to_init = SDL_FALSE;

    if (driver_name != NULL && *driver_name != 0) {
        char *driver_name_copy = SDL_strdup(driver_name);
        const char *driver_attempt = driver_name_copy;

        if (driver_name_copy == NULL) {
            SDL_DestroyRWLock(device_list_lock);
            return SDL_OutOfMemory();
        }

        while (driver_attempt != NULL && *driver_attempt != 0 && !initialized) {
            char *driver_attempt_end = SDL_strchr(driver_attempt, ',');
            if (driver_attempt_end != NULL) {
                *driver_attempt_end = '\0';
            }

            // SDL 1.2 uses the name "dsound", so we'll support both.
            if (SDL_strcmp(driver_attempt, "dsound") == 0) {
                driver_attempt = "directsound";
            } else if (SDL_strcmp(driver_attempt, "pulse") == 0) {  // likewise, "pulse" was renamed to "pulseaudio"
                driver_attempt = "pulseaudio";
            }

            for (int i = 0; bootstrap[i]; ++i) {
                if (SDL_strcasecmp(bootstrap[i]->name, driver_attempt) == 0) {
                    tried_to_init = SDL_TRUE;
                    SDL_zero(current_audio);
                    SDL_AtomicSet(&current_audio.last_device_instance_id, 2);  // start past 1 because of SDL2's legacy interface.
                    current_audio.device_list_lock = device_list_lock;
                    if (bootstrap[i]->init(&current_audio.impl)) {
                        current_audio.name = bootstrap[i]->name;
                        current_audio.desc = bootstrap[i]->desc;
                        initialized = SDL_TRUE;
                    }
                    break;
                }
            }

            driver_attempt = (driver_attempt_end != NULL) ? (driver_attempt_end + 1) : NULL;
        }

        SDL_free(driver_name_copy);
    } else {
        for (int i = 0; (!initialized) && (bootstrap[i]); ++i) {
            if (bootstrap[i]->demand_only) {
                continue;
            }

            tried_to_init = SDL_TRUE;
            SDL_zero(current_audio);
            SDL_AtomicSet(&current_audio.last_device_instance_id, 2);  // start past 1 because of SDL2's legacy interface.
            current_audio.device_list_lock = device_list_lock;
            if (bootstrap[i]->init(&current_audio.impl)) {
                current_audio.name = bootstrap[i]->name;
                current_audio.desc = bootstrap[i]->desc;
                initialized = SDL_TRUE;
            }
        }
    }

    if (!initialized) {
        // specific drivers will set the error message if they fail, but otherwise we do it here.
        if (!tried_to_init) {
            if (driver_name) {
                SDL_SetError("Audio target '%s' not available", driver_name);
            } else {
                SDL_SetError("No available audio device");
            }
        }

        SDL_zero(current_audio);
        SDL_DestroyRWLock(device_list_lock);
        current_audio.device_list_lock = NULL;
        return -1;  // No driver was available, so fail.
    }

    CompleteAudioEntryPoints();

    // Make sure we have a list of devices available at startup...
    SDL_AudioDevice *default_output = NULL;
    SDL_AudioDevice *default_capture = NULL;
    current_audio.impl.DetectDevices(&default_output, &default_capture);

    // these are only set if default_* is non-NULL, in case the backend just called SDL_DefaultAudioDeviceChanged directly during DetectDevices.
    if (default_output) {
        current_audio.default_output_device_id = default_output->instance_id;
    }
    if (default_capture) {
        current_audio.default_capture_device_id = default_capture->instance_id;
    }

    // If no default was _ever_ specified, just take the first device we see, if any.
    if (!current_audio.default_output_device_id) {
        current_audio.default_output_device_id = GetFirstAddedAudioDeviceID(/*iscapture=*/SDL_FALSE);
    }
    if (!current_audio.default_capture_device_id) {
        current_audio.default_capture_device_id = GetFirstAddedAudioDeviceID(/*iscapture=*/SDL_TRUE);
    }

    return 0;
}

void SDL_QuitAudio(void)
{
    if (!current_audio.name) {  // not initialized?!
        return;
    }

    // merge device lists so we don't have to duplicate work below.
    SDL_LockRWLockForWriting(current_audio.device_list_lock);
    SDL_AtomicSet(&current_audio.shutting_down, 1);
    SDL_AudioDevice *devices = NULL;
    for (SDL_AudioDevice *i = current_audio.output_devices; i != NULL; i = i->next) {
        devices = i;
    }
    if (!devices) {
        devices = current_audio.capture_devices;
    } else {
        SDL_assert(devices->next == NULL);
        devices->next = current_audio.capture_devices;
        devices = current_audio.output_devices;
    }
    current_audio.output_devices = NULL;
    current_audio.capture_devices = NULL;
    SDL_AtomicSet(&current_audio.output_device_count, 0);
    SDL_AtomicSet(&current_audio.capture_device_count, 0);
    SDL_UnlockRWLock(current_audio.device_list_lock);
    
    // mark all devices for shutdown so all threads can begin to terminate.
    for (SDL_AudioDevice *i = devices; i != NULL; i = i->next) {
        SDL_AtomicSet(&i->shutdown, 1);
    }

    // now wait on any audio threads...
    for (SDL_AudioDevice *i = devices; i != NULL; i = i->next) {
        if (i->thread) {
            SDL_assert(!SDL_AtomicGet(&i->condemned));  // these shouldn't have been in the device list still, and thread should have detached.
            SDL_WaitThread(i->thread, NULL);
            i->thread = NULL;
        }
    }

    while (devices) {
        SDL_AudioDevice *next = devices->next;
        DestroyPhysicalAudioDevice(devices);
        devices = next;
    }

    // Free the driver data
    current_audio.impl.Deinitialize();

    SDL_DestroyRWLock(current_audio.device_list_lock);

    SDL_zero(current_audio);
}


void SDL_AudioThreadFinalize(SDL_AudioDevice *device)
{
    if (SDL_AtomicGet(&device->condemned)) {
        if (device->thread) {
            SDL_DetachThread(device->thread);  // no one is waiting for us, just detach ourselves.
            device->thread = NULL;
            SDL_AtomicSet(&device->thread_alive, 0);
        }
        DestroyPhysicalAudioDevice(device);
    }
    SDL_AtomicSet(&device->thread_alive, 0);
}

// Output device thread. This is split into chunks, so backends that need to control this directly can use the pieces they need without duplicating effort.

void SDL_OutputAudioThreadSetup(SDL_AudioDevice *device)
{
    SDL_assert(!device->iscapture);
    current_audio.impl.ThreadInit(device);
}

SDL_bool SDL_OutputAudioThreadIterate(SDL_AudioDevice *device)
{
    SDL_assert(!device->iscapture);

    SDL_LockMutex(device->lock);

    if (SDL_AtomicGet(&device->shutdown)) {
        SDL_UnlockMutex(device->lock);
        return SDL_FALSE;  // we're done, shut it down.
    }

    SDL_bool retval = SDL_TRUE;
    int buffer_size = device->buffer_size;
    Uint8 *mix_buffer = current_audio.impl.GetDeviceBuf(device, &buffer_size);
    if (!mix_buffer) {
        retval = SDL_FALSE;
    } else {
        SDL_assert(buffer_size <= device->buffer_size);  // you can ask for less, but not more.
        SDL_memset(mix_buffer, device->silence_value, buffer_size);  // start with silence.

        for (SDL_LogicalAudioDevice *logdev = device->logical_devices; logdev != NULL; logdev = logdev->next) {
            if (SDL_AtomicGet(&logdev->paused)) {
                continue;  // paused? Skip this logical device.
            }

            for (SDL_AudioStream *stream = logdev->bound_streams; stream != NULL; stream = stream->next_binding) {
                /* this will hold a lock on `stream` while getting. We don't explicitly lock the streams
                   for iterating here because the binding linked list can only change while the device lock is held.
                   (we _do_ lock the stream during binding/unbinding to make sure that two threads can't try to bind
                   the same stream to different devices at the same time, though.) */
                const int br = SDL_GetAudioStreamData(stream, device->work_buffer, buffer_size);
                if (br < 0) {
                    // oh crud, we probably ran out of memory. This is possibly an overreaction to kill the audio device, but it's likely the whole thing is going down in a moment anyhow.
                    retval = SDL_FALSE;
                    break;
                } else if (br > 0) {  // it's okay if we get less than requested, we mix what we have.
                    // !!! FIXME: this needs to mix to float32 or int32, so we don't clip.
                    if (SDL_MixAudioFormat(mix_buffer, device->work_buffer, device->spec.format, br, SDL_MIX_MAXVOLUME) < 0) {  // !!! FIXME: allow streams to specify gain?
                        SDL_assert(!"We probably ended up with some totally unexpected audio format here");
                        retval = SDL_FALSE;  // uh...?
                        break;
                    }
                }
            }
        }

        // !!! FIXME: have PlayDevice return a value and do disconnects in here with it.
        current_audio.impl.PlayDevice(device, mix_buffer, buffer_size);  // this SHOULD NOT BLOCK, as we are holding a lock right now. Block in WaitDevice!
    }

    SDL_UnlockMutex(device->lock);

    if (!retval) {
        SDL_AudioDeviceDisconnected(device);  // doh.
    }

    return retval;
}

void SDL_OutputAudioThreadShutdown(SDL_AudioDevice *device)
{
    SDL_assert(!device->iscapture);
    const int samples = (device->buffer_size / (SDL_AUDIO_BITSIZE(device->spec.format) / 8)) / device->spec.channels;
    // Wait for the audio to drain. !!! FIXME: don't bother waiting if device is lost.
    SDL_Delay(((samples * 1000) / device->spec.freq) * 2);
    current_audio.impl.ThreadDeinit(device);
    SDL_AudioThreadFinalize(device);
}

static int SDLCALL OutputAudioThread(void *devicep)  // thread entry point
{
    SDL_AudioDevice *device = (SDL_AudioDevice *)devicep;
    SDL_assert(device != NULL);
    SDL_assert(!device->iscapture);
    SDL_OutputAudioThreadSetup(device);
    do {
        current_audio.impl.WaitDevice(device);
    } while (SDL_OutputAudioThreadIterate(device));

    SDL_OutputAudioThreadShutdown(device);
    return 0;
}



// Capture device thread. This is split into chunks, so backends that need to control this directly can use the pieces they need without duplicating effort.

void SDL_CaptureAudioThreadSetup(SDL_AudioDevice *device)
{
    SDL_assert(device->iscapture);
    current_audio.impl.ThreadInit(device);
}

SDL_bool SDL_CaptureAudioThreadIterate(SDL_AudioDevice *device)
{
    SDL_assert(device->iscapture);

    SDL_LockMutex(device->lock);

    SDL_bool retval = SDL_TRUE;

    if (SDL_AtomicGet(&device->shutdown)) {
        retval = SDL_FALSE;  // we're done, shut it down.
    } else if (device->logical_devices == NULL) {
        current_audio.impl.FlushCapture(device); // nothing wants data, dump anything pending.
    } else {
        // this SHOULD NOT BLOCK, as we are holding a lock right now. Block in WaitCaptureDevice!
        const int rc = current_audio.impl.CaptureFromDevice(device, device->work_buffer, device->buffer_size);
        if (rc < 0) {  // uhoh, device failed for some reason!
            retval = SDL_FALSE;
        } else if (rc > 0) {  // queue the new data to each bound stream.
            for (SDL_LogicalAudioDevice *logdev = device->logical_devices; logdev != NULL; logdev = logdev->next) {
                if (SDL_AtomicGet(&logdev->paused)) {
                    continue;  // paused? Skip this logical device.
                }

                for (SDL_AudioStream *stream = logdev->bound_streams; stream != NULL; stream = stream->next_binding) {
                    /* this will hold a lock on `stream` while putting. We don't explicitly lock the streams
                       for iterating here because the binding linked list can only change while the device lock is held.
                       (we _do_ lock the stream during binding/unbinding to make sure that two threads can't try to bind
                       the same stream to different devices at the same time, though.) */
                    if (SDL_PutAudioStreamData(stream, device->work_buffer, rc) < 0) {
                        // oh crud, we probably ran out of memory. This is possibly an overreaction to kill the audio device, but it's likely the whole thing is going down in a moment anyhow.
                        retval = SDL_FALSE;
                        break;
                    }
                }
            }
        }
    }

    SDL_UnlockMutex(device->lock);

    if (!retval) {
        SDL_AudioDeviceDisconnected(device);  // doh.
    }

    return retval;
}

void SDL_CaptureAudioThreadShutdown(SDL_AudioDevice *device)
{
    SDL_assert(device->iscapture);
    current_audio.impl.FlushCapture(device);
    current_audio.impl.ThreadDeinit(device);
    SDL_AudioThreadFinalize(device);
}

static int SDLCALL CaptureAudioThread(void *devicep)  // thread entry point
{
    SDL_AudioDevice *device = (SDL_AudioDevice *)devicep;
    SDL_assert(device != NULL);
    SDL_assert(device->iscapture);
    SDL_CaptureAudioThreadSetup(device);

    do {
        current_audio.impl.WaitCaptureDevice(device);
    } while (SDL_CaptureAudioThreadIterate(device));

    SDL_CaptureAudioThreadShutdown(device);
    return 0;
}


static SDL_AudioDeviceID *GetAudioDevices(int *reqcount, SDL_AudioDevice **devices, SDL_AtomicInt *device_count)
{
    if (!SDL_GetCurrentAudioDriver()) {
        SDL_SetError("Audio subsystem is not initialized");
        return NULL;
    }

    SDL_LockRWLockForReading(current_audio.device_list_lock);
    int num_devices = SDL_AtomicGet(device_count);
    SDL_AudioDeviceID *retval = (SDL_AudioDeviceID *) SDL_malloc((num_devices + 1) * sizeof (SDL_AudioDeviceID));
    if (retval == NULL) {
        num_devices = 0;
        SDL_OutOfMemory();
    } else {
        const SDL_AudioDevice *dev = *devices;  // pointer to a pointer so we can dereference it after the lock is held.
        for (int i = 0; i < num_devices; i++) {
            SDL_assert(dev != NULL);
            SDL_assert(!SDL_AtomicGet((SDL_AtomicInt *) &dev->condemned));  // shouldn't be in the list if pending deletion.
            retval[i] = dev->instance_id;
            dev = dev->next;
        }
        SDL_assert(dev == NULL);  // did the whole list?
        retval[num_devices] = 0;  // null-terminated.
    }
    SDL_UnlockRWLock(current_audio.device_list_lock);

    if (reqcount != NULL) {
        *reqcount = num_devices;
    }

    return retval;
}

SDL_AudioDeviceID *SDL_GetAudioOutputDevices(int *count)
{
    return GetAudioDevices(count, &current_audio.output_devices, &current_audio.output_device_count);
}

SDL_AudioDeviceID *SDL_GetAudioCaptureDevices(int *count)
{
    return GetAudioDevices(count, &current_audio.capture_devices, &current_audio.capture_device_count);
}

// If found, this locks _the physical device_ this logical device is associated with, before returning.
static SDL_LogicalAudioDevice *ObtainLogicalAudioDevice(SDL_AudioDeviceID devid)
{
    if (!SDL_GetCurrentAudioDriver()) {
        SDL_SetError("Audio subsystem is not initialized");
        return NULL;
    }

    SDL_LogicalAudioDevice *logdev = NULL;

    const SDL_bool islogical = (devid & (1<<1)) ? SDL_FALSE : SDL_TRUE;
    if (islogical) {  // don't bother looking if it's not a logical device id value.
        const SDL_bool iscapture = (devid & (1<<0)) ? SDL_FALSE : SDL_TRUE;

        SDL_LockRWLockForReading(current_audio.device_list_lock);

        for (SDL_AudioDevice *device = iscapture ? current_audio.capture_devices : current_audio.output_devices; device != NULL; device = device->next) {
            SDL_LockMutex(device->lock);  // caller must unlock if we choose a logical device from this guy.
            SDL_assert(!SDL_AtomicGet(&device->condemned));  // shouldn't be in the list if pending deletion.
            for (logdev = device->logical_devices; logdev != NULL; logdev = logdev->next) {
                if (logdev->instance_id == devid) {
                    break; // found it!
                }
            }
            if (logdev != NULL) {
                break;
            }
            SDL_UnlockMutex(device->lock);  // give up this lock and try the next physical device.
        }

        SDL_UnlockRWLock(current_audio.device_list_lock);
    }

    if (!logdev) {
        SDL_SetError("Invalid audio device instance ID");
    }

    return logdev;
}

/* this finds the physical device associated with `devid` and locks it for use.
   Note that a logical device instance id will return its associated physical device! */
static SDL_AudioDevice *ObtainPhysicalAudioDevice(SDL_AudioDeviceID devid)
{
    // bit #1 of devid is set for physical devices and unset for logical.
    const SDL_bool islogical = (devid & (1<<1)) ? SDL_FALSE : SDL_TRUE;
    if (islogical) {
        SDL_LogicalAudioDevice *logdev = ObtainLogicalAudioDevice(devid);
        if (logdev) {
            return logdev->physical_device;
        }
        return NULL;
    }

    if (!SDL_GetCurrentAudioDriver()) {
        SDL_SetError("Audio subsystem is not initialized");
        return NULL;
    }

    // bit #0 of devid is set for output devices and unset for capture.
    const SDL_bool iscapture = (devid & (1<<0)) ? SDL_FALSE : SDL_TRUE;
    SDL_AudioDevice *dev = NULL;

    SDL_LockRWLockForReading(current_audio.device_list_lock);

    for (dev = iscapture ? current_audio.capture_devices : current_audio.output_devices; dev != NULL; dev = dev->next) {
        if (dev->instance_id == devid) {  // found it?
            SDL_LockMutex(dev->lock);  // caller must unlock.
            SDL_assert(!SDL_AtomicGet(&dev->condemned));  // shouldn't be in the list if pending deletion.
            break;
        }
    }

    SDL_UnlockRWLock(current_audio.device_list_lock);

    if (!dev) {
        SDL_SetError("Invalid audio device instance ID");
    }

    return dev;
}

SDL_AudioDevice *SDL_FindPhysicalAudioDeviceByCallback(SDL_bool (*callback)(SDL_AudioDevice *device, void *userdata), void *userdata)
{
    if (!SDL_GetCurrentAudioDriver()) {
        SDL_SetError("Audio subsystem is not initialized");
        return NULL;
    }

    SDL_LockRWLockForReading(current_audio.device_list_lock);

    SDL_AudioDevice *dev = NULL;
    for (dev = current_audio.output_devices; dev != NULL; dev = dev->next) {
        if (callback(dev, userdata)) {  // found it?
            break;
        }
    }

    if (!dev) {
        // !!! FIXME: code duplication, from above.
        for (dev = current_audio.capture_devices; dev != NULL; dev = dev->next) {
            if (callback(dev, userdata)) {  // found it?
                break;
            }
        }
    }

    SDL_UnlockRWLock(current_audio.device_list_lock);

    if (!dev) {
        SDL_SetError("Device not found");
    }

    SDL_assert(!dev || !SDL_AtomicGet(&dev->condemned));  // shouldn't be in the list if pending deletion.

    return dev;
}

static SDL_bool TestDeviceHandleCallback(SDL_AudioDevice *device, void *handle)
{
    return device->handle == handle;
}

SDL_AudioDevice *SDL_FindPhysicalAudioDeviceByHandle(void *handle)
{
    return SDL_FindPhysicalAudioDeviceByCallback(TestDeviceHandleCallback, handle);
}

char *SDL_GetAudioDeviceName(SDL_AudioDeviceID devid)
{
    SDL_AudioDevice *device = ObtainPhysicalAudioDevice(devid);
    if (!device) {
        return NULL;
    }

    char *retval = SDL_strdup(device->name);
    if (!retval) {
        SDL_OutOfMemory();
    }

    SDL_UnlockMutex(device->lock);

    return retval;
}

int SDL_GetAudioDeviceFormat(SDL_AudioDeviceID devid, SDL_AudioSpec *spec)
{
    if (!spec) {
        return SDL_InvalidParamError("spec");
    }

    SDL_bool is_default = SDL_FALSE;
    if (devid == SDL_AUDIO_DEVICE_DEFAULT_OUTPUT) {
        devid = current_audio.default_output_device_id;
        is_default = SDL_TRUE;
    } else if (devid == SDL_AUDIO_DEVICE_DEFAULT_CAPTURE) {
        devid = current_audio.default_capture_device_id;
        is_default = SDL_TRUE;
    }

    if ((devid == 0) && is_default) {
        return SDL_SetError("No default audio device available");
    }

    SDL_AudioDevice *device = ObtainPhysicalAudioDevice(devid);
    if (!device) {
        return -1;
    }

    SDL_memcpy(spec, &device->spec, sizeof (SDL_AudioSpec));
    SDL_UnlockMutex(device->lock);

    return 0;
}

// this expects the device lock to be held.  !!! FIXME: no it doesn't...?
static void ClosePhysicalAudioDevice(SDL_AudioDevice *device)
{
    SDL_assert(current_audio.impl.ProvidesOwnCallbackThread || ((device->thread == NULL) == (SDL_AtomicGet(&device->thread_alive) == 0)));

    if (SDL_AtomicGet(&device->thread_alive)) {
        SDL_AtomicSet(&device->shutdown, 1);
        if (device->thread != NULL) {
            SDL_WaitThread(device->thread, NULL);
            device->thread = NULL;
        }
        SDL_AtomicSet(&device->thread_alive, 0);
    }

    if (device->is_opened) {
        current_audio.impl.CloseDevice(device);  // if ProvidesOwnCallbackThread, this must join on any existing device thread before returning!
        device->is_opened = SDL_FALSE;
        device->hidden = NULL;  // just in case.
    }

    if (device->work_buffer) {
        SDL_aligned_free(device->work_buffer);
        device->work_buffer = NULL;
    }

    SDL_memcpy(&device->spec, &device->default_spec, sizeof (SDL_AudioSpec));
    device->sample_frames = 0;
    device->silence_value = SDL_GetSilenceValueForFormat(device->spec.format);
    SDL_AtomicSet(&device->shutdown, 0);  // ready to go again.
}

void SDL_CloseAudioDevice(SDL_AudioDeviceID devid)
{
    SDL_LogicalAudioDevice *logdev = ObtainLogicalAudioDevice(devid);
    if (logdev) {  // if NULL, maybe it was already lost?
        SDL_AudioDevice *device = logdev->physical_device;
        DestroyLogicalAudioDevice(logdev);

        if (device->logical_devices == NULL) {  // no more logical devices? Close the physical device, too.
            // !!! FIXME: we _need_ to release this lock, but doing so can cause a race condition if someone opens a device while we're closing it.
            SDL_UnlockMutex(device->lock);  // can't hold the lock or the audio thread will deadlock while we WaitThread it.
            ClosePhysicalAudioDevice(device);
        } else {
            SDL_UnlockMutex(device->lock);  // we're set, let everything go again.
        }
    }
}


static SDL_AudioFormat ParseAudioFormatString(const char *string)
{
    if (string) {
        #define CHECK_FMT_STRING(x) if (SDL_strcmp(string, #x) == 0) { return SDL_AUDIO_##x; }
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
    }
    return 0;
}

static void PrepareAudioFormat(SDL_bool iscapture, SDL_AudioSpec *spec)
{
    if (spec->freq == 0) {
        spec->freq = iscapture ? DEFAULT_AUDIO_CAPTURE_FREQUENCY : DEFAULT_AUDIO_OUTPUT_FREQUENCY;

        const char *env = SDL_getenv("SDL_AUDIO_FREQUENCY");  // !!! FIXME: should be a hint?
        if (env != NULL) {
            const int val = SDL_atoi(env);
            if (val > 0) {
                spec->freq = val;
            }
        }
    }

    if (spec->channels == 0) {
        spec->channels = iscapture ? DEFAULT_AUDIO_CAPTURE_CHANNELS : DEFAULT_AUDIO_OUTPUT_CHANNELS;;
        const char *env = SDL_getenv("SDL_AUDIO_CHANNELS");
        if (env != NULL) {
            const int val = SDL_atoi(env);
            if (val > 0) {
                spec->channels = val;
            }
        }
    }

    if (spec->format == 0) {
        const SDL_AudioFormat val = ParseAudioFormatString(SDL_getenv("SDL_AUDIO_FORMAT"));
        spec->format = (val != 0) ? val : (iscapture ? DEFAULT_AUDIO_CAPTURE_FORMAT : DEFAULT_AUDIO_OUTPUT_FORMAT);
    }
}

static int GetDefaultSampleFramesFromFreq(int freq)
{
    return SDL_powerof2((freq / 1000) * 46);  // Pick the closest power-of-two to ~46 ms at desired frequency
}

void SDL_UpdatedAudioDeviceFormat(SDL_AudioDevice *device)
{
    device->silence_value = SDL_GetSilenceValueForFormat(device->spec.format);
    device->buffer_size = device->sample_frames * (SDL_AUDIO_BITSIZE(device->spec.format) / 8) * device->spec.channels;
}

char *SDL_GetAudioThreadName(SDL_AudioDevice *device, char *buf, size_t buflen)
{
    (void)SDL_snprintf(buf, buflen, "SDLAudio%c%d", (device->iscapture) ? 'C' : 'P', (int) device->instance_id);
    return buf;
}


// this expects the device lock to be held.
static int OpenPhysicalAudioDevice(SDL_AudioDevice *device, const SDL_AudioSpec *inspec)
{
    SDL_assert(!device->is_opened);
    SDL_assert(device->logical_devices == NULL);

    // Just pretend to open a zombie device. It can still collect logical devices on the assumption they will all migrate when the default device is officially changed.
    if (SDL_AtomicGet(&device->zombie)) {
        return 0;  // Braaaaaaaaains.
    }

    SDL_AudioSpec spec;
    SDL_memcpy(&spec, inspec ? inspec : &device->default_spec, sizeof (SDL_AudioSpec));
    PrepareAudioFormat(device->iscapture, &spec);

    /* We allow the device format to change if it's better than the current settings (by various definitions of "better"). This prevents
       something low quality, like an old game using S8/8000Hz audio, from ruining a music thing playing at CD quality that tries to open later.
       (or some VoIP library that opens for mono output ruining your surround-sound game because it got there first).
       These are just requests! The backend may change any of these values during OpenDevice method! */
    device->spec.format = (SDL_AUDIO_BITSIZE(device->default_spec.format) >= SDL_AUDIO_BITSIZE(spec.format)) ? device->default_spec.format : spec.format;
    device->spec.freq = SDL_max(device->default_spec.freq, spec.freq);
    device->spec.channels = SDL_max(device->default_spec.channels, spec.channels);
    device->sample_frames = GetDefaultSampleFramesFromFreq(device->spec.freq);
    SDL_UpdatedAudioDeviceFormat(device);  // start this off sane.

    device->is_opened = SDL_TRUE;  // mark this true even if impl.OpenDevice fails, so we know to clean up.
    if (current_audio.impl.OpenDevice(device) < 0) {
        ClosePhysicalAudioDevice(device);  // clean up anything the backend left half-initialized.
        return -1;
    }

    SDL_UpdatedAudioDeviceFormat(device);  // in case the backend changed things and forgot to call this.

    // Allocate a scratch audio buffer
    device->work_buffer = (Uint8 *)SDL_aligned_alloc(SDL_SIMDGetAlignment(), device->buffer_size);
    if (device->work_buffer == NULL) {
        ClosePhysicalAudioDevice(device);
        return SDL_OutOfMemory();
    }

    // Start the audio thread if necessary
    SDL_AtomicSet(&device->thread_alive, 1);
    if (!current_audio.impl.ProvidesOwnCallbackThread) {
        const size_t stacksize = 0;  // just take the system default, since audio streams might have callbacks.
        char threadname[64];
        SDL_GetAudioThreadName(device, threadname, sizeof (threadname));
        device->thread = SDL_CreateThreadInternal(device->iscapture ? CaptureAudioThread : OutputAudioThread, threadname, stacksize, device);

        if (device->thread == NULL) {
            SDL_AtomicSet(&device->thread_alive, 0);
            ClosePhysicalAudioDevice(device);
            return SDL_SetError("Couldn't create audio thread");
        }
    }

    return 0;
}

SDL_AudioDeviceID SDL_OpenAudioDevice(SDL_AudioDeviceID devid, const SDL_AudioSpec *spec)
{
    if (!SDL_GetCurrentAudioDriver()) {
        SDL_SetError("Audio subsystem is not initialized");
        return 0;
    }

    SDL_bool is_default = SDL_FALSE;
    if (devid == SDL_AUDIO_DEVICE_DEFAULT_OUTPUT) {
        devid = current_audio.default_output_device_id;
        is_default = SDL_TRUE;
    } else if (devid == SDL_AUDIO_DEVICE_DEFAULT_CAPTURE) {
        devid = current_audio.default_capture_device_id;
        is_default = SDL_TRUE;
    }

    if ((devid == 0) && is_default) {
        SDL_SetError("No default audio device available");
        return 0;
    }

    // this will let you use a logical device to make a new logical device on the parent physical device. Could be useful?
    SDL_AudioDevice *device = NULL;
    const SDL_bool islogical = (devid & (1<<1)) ? SDL_FALSE : SDL_TRUE;
    if (!islogical) {
        device = ObtainPhysicalAudioDevice(devid);
    } else {
        SDL_LogicalAudioDevice *logdev = ObtainLogicalAudioDevice(devid);  // this locks the physical device, too.
        if (logdev) {
            is_default = logdev->is_default;  // was the original logical device meant to be a default? Make this one, too.
            device = logdev->physical_device;
        }
    }

    SDL_AudioDeviceID retval = 0;

    if (device) {
        SDL_LogicalAudioDevice *logdev = NULL;
        if (!is_default && SDL_AtomicGet(&device->zombie)) {
            // uhoh, this device is undead, and just waiting for a new default device to be declared so it can hand off to it. Refuse explicit opens.
            SDL_SetError("Device was already lost and can't accept new opens");
        } else if ((logdev = (SDL_LogicalAudioDevice *) SDL_calloc(1, sizeof (SDL_LogicalAudioDevice))) == NULL) {
            SDL_OutOfMemory();
        } else if (!device->is_opened && OpenPhysicalAudioDevice(device, spec) == -1) {  // first thing using this physical device? Open at the OS level...
            SDL_free(logdev);
        } else {
            SDL_AtomicSet(&logdev->paused, 0);
            retval = logdev->instance_id = assign_audio_device_instance_id(device->iscapture, /*islogical=*/SDL_TRUE);
            logdev->physical_device = device;
            logdev->is_default = is_default;
            logdev->next = device->logical_devices;
            if (device->logical_devices) {
                device->logical_devices->prev = logdev;
            }
            device->logical_devices = logdev;
        }
        SDL_UnlockMutex(device->lock);
    }

    return retval;
}

static int SetLogicalAudioDevicePauseState(SDL_AudioDeviceID devid, int value)
{
    SDL_LogicalAudioDevice *logdev = ObtainLogicalAudioDevice(devid);
    if (!logdev) {
        return -1;  // ObtainLogicalAudioDevice will have set an error.
    }
    SDL_AtomicSet(&logdev->paused, value);
    SDL_UnlockMutex(logdev->physical_device->lock);
    return 0;
}

int SDL_PauseAudioDevice(SDL_AudioDeviceID devid)
{
    return SetLogicalAudioDevicePauseState(devid, 1);
}

int SDLCALL SDL_ResumeAudioDevice(SDL_AudioDeviceID devid)
{
    return SetLogicalAudioDevicePauseState(devid, 0);
}

SDL_bool SDL_IsAudioDevicePaused(SDL_AudioDeviceID devid)
{
    SDL_LogicalAudioDevice *logdev = ObtainLogicalAudioDevice(devid);
    SDL_bool retval = SDL_FALSE;
    if (logdev) {
        if (SDL_AtomicGet(&logdev->paused)) {
            retval = SDL_TRUE;
        }
        SDL_UnlockMutex(logdev->physical_device->lock);
    }
    return retval;
}


int SDL_BindAudioStreams(SDL_AudioDeviceID devid, SDL_AudioStream **streams, int num_streams)
{
    const SDL_bool islogical = (devid & (1<<1)) ? SDL_FALSE : SDL_TRUE;
    SDL_LogicalAudioDevice *logdev;

    if (num_streams == 0) {
        return 0;  // nothing to do
    } else if (num_streams < 0) {
        return SDL_InvalidParamError("num_streams");
    } else if (streams == NULL) {
        return SDL_InvalidParamError("streams");
    } else if (!islogical) {
        return SDL_SetError("Audio streams are bound to device ids from SDL_OpenAudioDevice, not raw physical devices");
    } else if ((logdev = ObtainLogicalAudioDevice(devid)) == NULL) {
        return -1;  // ObtainLogicalAudioDevice set the error message.
    }

    // make sure start of list is sane.
    SDL_assert(!logdev->bound_streams || (logdev->bound_streams->prev_binding == NULL));

    SDL_AudioDevice *device = logdev->physical_device;
    int retval = 0;

    // lock all the streams upfront, so we can verify they aren't bound elsewhere and add them all in one block, as this is intended to add everything or nothing.
    for (int i = 0; i < num_streams; i++) {
        SDL_AudioStream *stream = streams[i];
        if (stream == NULL) {
            retval = SDL_SetError("Stream #%d is NULL", i);
        } else {
            SDL_LockMutex(stream->lock);
            SDL_assert((stream->bound_device == NULL) == ((stream->prev_binding == NULL) || (stream->next_binding == NULL)));
            if (stream->bound_device) {
                retval = SDL_SetError("Stream #%d is already bound to a device", i);
            }
        }

        if (retval != 0) {
            int j;
            for (j = 0; j <= i; j++) {
                SDL_UnlockMutex(streams[j]->lock);
            }
            break;
        }
    }

    if (retval == 0) {
        // Now that everything is verified, chain everything together.
        const SDL_bool iscapture = device->iscapture;
        for (int i = 0; i < num_streams; i++) {
            SDL_AudioStream *stream = streams[i];
            SDL_AudioSpec src_spec, dst_spec;

            // set the proper end of the stream to the device's format.
            SDL_GetAudioStreamFormat(stream, &src_spec, &dst_spec);
            if (iscapture) {
                SDL_SetAudioStreamFormat(stream, &device->spec, &dst_spec);
            } else {
                SDL_SetAudioStreamFormat(stream, &src_spec, &device->spec);
            }

            stream->bound_device = logdev;
            stream->prev_binding = NULL;
            stream->next_binding = logdev->bound_streams;
            if (logdev->bound_streams) {
                logdev->bound_streams->prev_binding = stream;
            }
            logdev->bound_streams = stream;

            SDL_UnlockMutex(stream->lock);
        }
    }

    SDL_UnlockMutex(device->lock);

    return retval;
}

int SDL_BindAudioStream(SDL_AudioDeviceID devid, SDL_AudioStream *stream)
{
    return SDL_BindAudioStreams(devid, &stream, 1);
}

void SDL_UnbindAudioStreams(SDL_AudioStream **streams, int num_streams)
{
    /* to prevent deadlock when holding both locks, we _must_ lock the device first, and the stream second, as that is the order the audio thread will do it.
       But this means we have an unlikely, pathological case where a stream could change its binding between when we lookup its bound device and when we lock everything,
       so we double-check here. */
    for (int i = 0; i < num_streams; i++) {
        SDL_AudioStream *stream = streams[i];
        if (!stream) {
            continue;  // nothing to do, it's a NULL stream.
        }

        while (SDL_TRUE) {
            SDL_LockMutex(stream->lock);   // lock to check this and then release it, in case the device isn't locked yet.
            SDL_LogicalAudioDevice *bounddev = stream->bound_device;
            SDL_UnlockMutex(stream->lock);

            // lock in correct order.
            if (bounddev) {
                SDL_LockMutex(bounddev->physical_device->lock);  // this requires recursive mutexes, since we're likely locking the same device multiple times.
            }
            SDL_LockMutex(stream->lock);

            if (bounddev == stream->bound_device) {
                break;  // the binding didn't change in the small window where it could, so we're good.
            } else {
                SDL_UnlockMutex(stream->lock);  // it changed bindings! Try again.
                if (bounddev) {
                    SDL_UnlockMutex(bounddev->physical_device->lock);
                }
            }
        }
    }

    // everything is locked, start unbinding streams.
    for (int i = 0; i < num_streams; i++) {
        SDL_AudioStream *stream = streams[i];
        if (stream && stream->bound_device) {
            if (stream->bound_device->bound_streams == stream) {
                SDL_assert(stream->prev_binding == NULL);
                stream->bound_device->bound_streams = stream->next_binding;
            }
            if (stream->prev_binding) {
                stream->prev_binding->next_binding = stream->next_binding;
            }
            if (stream->next_binding) {
                stream->next_binding->prev_binding = stream->prev_binding;
            }
            stream->prev_binding = stream->next_binding = NULL;
        }
    }

    // Finalize and unlock everything.
    for (int i = 0; i < num_streams; i++) {
        SDL_AudioStream *stream = streams[i];
        if (stream && stream->bound_device) {
            SDL_LogicalAudioDevice *logdev = stream->bound_device;
            stream->bound_device = NULL;
            SDL_UnlockMutex(stream->lock);
            if (logdev) {
                SDL_UnlockMutex(logdev->physical_device->lock);
            }
        }
    }
}

void SDL_UnbindAudioStream(SDL_AudioStream *stream)
{
    SDL_UnbindAudioStreams(&stream, 1);
}

SDL_AudioDeviceID SDL_GetAudioStreamBinding(SDL_AudioStream *stream)
{
    SDL_AudioDeviceID retval = 0;
    if (stream) {
        SDL_LockMutex(stream->lock);
        if (stream->bound_device) {
            retval = stream->bound_device->instance_id;
        }
        SDL_UnlockMutex(stream->lock);
    }
    return retval;
}

SDL_AudioStream *SDL_CreateAndBindAudioStream(SDL_AudioDeviceID devid, const SDL_AudioSpec *spec)
{
    const SDL_bool islogical = (devid & (1<<1)) ? SDL_FALSE : SDL_TRUE;
    if (!islogical) {
        SDL_SetError("Audio streams are bound to device ids from SDL_OpenAudioDevice, not raw physical devices");
        return NULL;
    }

    SDL_AudioStream *stream = NULL;
    SDL_LogicalAudioDevice *logdev = ObtainLogicalAudioDevice(devid);
    if (logdev) {
        SDL_AudioDevice *device = logdev->physical_device;
        if (device->iscapture) {
            stream = SDL_CreateAudioStream(&device->spec, spec);
        } else {
            stream = SDL_CreateAudioStream(spec, &device->spec);
        }

        if (stream) {
            if (SDL_BindAudioStream(devid, stream) == -1) {
                SDL_DestroyAudioStream(stream);
                stream = NULL;
            }
        }
        SDL_UnlockMutex(device->lock);
    }
    return stream;
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
    for (int i = 0; i < NUM_FORMATS; i++) {
        if (format_list[i][0] == format) {
            return &format_list[i][0];
        }
    }
    return &format_list[0][NUM_FORMATS]; // not found; return what looks like a list with only a zero in it.
}

int SDL_GetSilenceValueForFormat(SDL_AudioFormat format)
{
    return (format == SDL_AUDIO_U8) ? 0x80 : 0x00;
}

// called internally by backends when the system default device changes.
void SDL_DefaultAudioDeviceChanged(SDL_AudioDevice *new_default_device)
{
    if (new_default_device == NULL) {  // !!! FIXME: what should we do in this case? Maybe all devices are lost, so there _isn't_ a default?
        return;  // uhoh.
    }

    const SDL_bool iscapture = new_default_device->iscapture;
    const SDL_AudioDeviceID current_devid = iscapture ? current_audio.default_capture_device_id : current_audio.default_output_device_id;

    if (new_default_device->instance_id == current_devid) {
        return;  // this is already the default.
    }

    SDL_LockMutex(new_default_device->lock);

    SDL_AudioDevice *current_default_device = ObtainPhysicalAudioDevice(current_devid);

    /* change the official default ID over while we have locks on both devices, so if something raced to open the default during
       this, it either gets the new device or is ready on the old and can be migrated. */
    if (iscapture) {
        current_audio.default_capture_device_id = new_default_device->instance_id;
    } else {
        current_audio.default_output_device_id = new_default_device->instance_id;
    }

    if (current_default_device) {
        // migrate any logical devices that were opened as a default to the new physical device...

        SDL_assert(current_default_device->iscapture == iscapture);

        // See if we have to open the new physical device, and if so, find the best audiospec for it.
        SDL_AudioSpec spec;
        SDL_bool needs_migration = SDL_FALSE;
        SDL_zero(spec);
        for (SDL_LogicalAudioDevice *logdev = current_default_device->logical_devices; logdev != NULL; logdev = logdev->next) {
            if (logdev->is_default) {
                needs_migration = SDL_TRUE;
                for (SDL_AudioStream *stream = logdev->bound_streams; stream != NULL; stream = stream->next_binding) {
                    const SDL_AudioSpec *streamspec = iscapture ? &stream->dst_spec : &stream->src_spec;
                    if (SDL_AUDIO_BITSIZE(streamspec->format) > SDL_AUDIO_BITSIZE(spec.format)) {
                        spec.format = streamspec->format;
                    }
                    if (streamspec->channels > spec.channels) {
                        spec.channels = streamspec->channels;
                    }
                    if (streamspec->freq > spec.freq) {
                        spec.freq = streamspec->freq;
                    }
                }
            }
        }

        if (needs_migration) {
            if (new_default_device->logical_devices == NULL) {  // New default physical device not been opened yet? Open at the OS level...
                if (OpenPhysicalAudioDevice(new_default_device, &spec) == -1) {
                    needs_migration = SDL_FALSE;  // uhoh, just leave everything on the old default, nothing to be done.
                }
            }
        }

        if (needs_migration) {
            SDL_LogicalAudioDevice *next = NULL;
            for (SDL_LogicalAudioDevice *logdev = current_default_device->logical_devices; logdev != NULL; logdev = next) {
                next = logdev->next;

                if (!logdev->is_default) {
                    continue;  // not opened as a default, leave it on the current physical device.
                }

                // make sure all our streams are targeting the new device's format.
                for (SDL_AudioStream *stream = logdev->bound_streams; stream != NULL; stream = stream->next_binding) {
                    SDL_SetAudioStreamFormat(stream, iscapture ? &new_default_device->spec : NULL, iscapture ? NULL : &new_default_device->spec);
                }

                // now migrate the logical device.
                if (logdev->next) {
                    logdev->next->prev = logdev->prev;
                }
                if (logdev->prev) {
                    logdev->prev->next = logdev->next;
                }
                if (current_default_device->logical_devices == logdev) {
                    current_default_device->logical_devices = logdev->next;
                }

                logdev->physical_device = new_default_device;
                logdev->prev = NULL;
                logdev->next = new_default_device->logical_devices;
                new_default_device->logical_devices = logdev;
            }

            if (current_default_device->logical_devices == NULL) {   // nothing left on the current physical device, close it.
                // !!! FIXME: we _need_ to release this lock, but doing so can cause a race condition if someone opens a device while we're closing it.
                SDL_UnlockMutex(current_default_device->lock);  // can't hold the lock or the audio thread will deadlock while we WaitThread it.
                ClosePhysicalAudioDevice(current_default_device);
                SDL_LockMutex(current_default_device->lock);  // we're about to unlock this again, so make sure the locks match.
            }
        }

        SDL_UnlockMutex(current_default_device->lock);
    }

    SDL_UnlockMutex(new_default_device->lock);

    // was current device already dead and just kept around to migrate to a new default device? Now we can kill it. Aim for the brain.
    if (current_default_device && SDL_AtomicGet(&current_default_device->zombie)) {
        SDL_AudioDeviceDisconnected(current_default_device);  // Call again, now that we're not the default; this will remove from device list, send removal events, and destroy the SDL_AudioDevice.
    }
}

int SDL_AudioDeviceFormatChangedAlreadyLocked(SDL_AudioDevice *device, const SDL_AudioSpec *newspec, int new_sample_frames)
{
    SDL_bool kill_device = SDL_FALSE;

    const int orig_buffer_size = device->buffer_size;
    const SDL_bool iscapture = device->iscapture;

    if ((device->spec.format != newspec->format) || (device->spec.channels != newspec->channels) || (device->spec.freq != newspec->freq)) {
        SDL_memcpy(&device->spec, newspec, sizeof (*newspec));
        for (SDL_LogicalAudioDevice *logdev = device->logical_devices; !kill_device && (logdev != NULL); logdev = logdev->next) {
            for (SDL_AudioStream *stream = logdev->bound_streams; !kill_device && (stream != NULL); stream = stream->next_binding) {
                if (SDL_SetAudioStreamFormat(stream, iscapture ? &device->spec : NULL, iscapture ? NULL : &device->spec) == -1) {
                    kill_device = SDL_TRUE;
                }
            }
        }
    }

    if (!kill_device) {
        device->sample_frames = new_sample_frames;
        SDL_UpdatedAudioDeviceFormat(device);
        if (device->work_buffer && (device->buffer_size > orig_buffer_size)) {
            SDL_aligned_free(device->work_buffer);
            device->work_buffer = (Uint8 *)SDL_aligned_alloc(SDL_SIMDGetAlignment(), device->buffer_size);
            if (!device->work_buffer) {
                kill_device = SDL_TRUE;
            }
        }
    }

    return kill_device ? -1 : 0;
}

int SDL_AudioDeviceFormatChanged(SDL_AudioDevice *device, const SDL_AudioSpec *newspec, int new_sample_frames)
{
    SDL_LockMutex(device->lock);
    const int retval = SDL_AudioDeviceFormatChangedAlreadyLocked(device, newspec, new_sample_frames);
    SDL_UnlockMutex(device->lock);
    return retval;
}

