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

#ifdef SDL_AUDIO_DRIVER_WASAPI

#include "../../core/windows/SDL_windows.h"
#include "../../core/windows/SDL_immdevice.h"
#include "../../thread/SDL_systhread.h"
#include "../SDL_sysaudio.h"

#define COBJMACROS
#include <audioclient.h>

#include "SDL_wasapi.h"

// These constants aren't available in older SDKs
#ifndef AUDCLNT_STREAMFLAGS_RATEADJUST
#define AUDCLNT_STREAMFLAGS_RATEADJUST 0x00100000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif

// Some GUIDs we need to know without linking to libraries that aren't available before Vista.
static const IID SDL_IID_IAudioRenderClient = { 0xf294acfc, 0x3146, 0x4483, { 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2 } };
static const IID SDL_IID_IAudioCaptureClient = { 0xc8adbd64, 0xe71e, 0x48a0, { 0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17 } };


// WASAPI is _really_ particular about various things happening on the same thread, for COM and such,
//  so we proxy various stuff to a single background thread to manage.

typedef struct ManagementThreadPendingTask
{
    ManagementThreadTask fn;
    void *userdata;
    int result;
    SDL_Semaphore *task_complete_sem;
    char *errorstr;
    struct ManagementThreadPendingTask *next;
} ManagementThreadPendingTask;

static SDL_Thread *ManagementThread = NULL;
static ManagementThreadPendingTask *ManagementThreadPendingTasks = NULL;
static SDL_Mutex *ManagementThreadLock = NULL;
static SDL_Condition *ManagementThreadCondition = NULL;
static SDL_AtomicInt ManagementThreadShutdown;

static void ManagementThreadMainloop(void)
{
    SDL_LockMutex(ManagementThreadLock);
    ManagementThreadPendingTask *task;
    while (((task = SDL_AtomicGetPtr((void **) &ManagementThreadPendingTasks)) != NULL) || !SDL_AtomicGet(&ManagementThreadShutdown)) {
        if (!task) {
            SDL_WaitCondition(ManagementThreadCondition, ManagementThreadLock); // block until there's something to do.
        } else {
            SDL_AtomicSetPtr((void **) &ManagementThreadPendingTasks, task->next); // take task off the pending list.
            SDL_UnlockMutex(ManagementThreadLock);                       // let other things add to the list while we chew on this task.
            task->result = task->fn(task->userdata);                     // run this task.
            if (task->task_complete_sem) {                               // something waiting on result?
                task->errorstr = SDL_strdup(SDL_GetError());
                SDL_PostSemaphore(task->task_complete_sem);
            } else { // nothing waiting, we're done, free it.
                SDL_free(task);
            }
            SDL_LockMutex(ManagementThreadLock); // regrab the lock so we can get the next task; if nothing to do, we'll release the lock in SDL_WaitCondition.
        }
    }
    SDL_UnlockMutex(ManagementThreadLock); // told to shut down and out of tasks, let go of the lock and return.
}

int WASAPI_ProxyToManagementThread(ManagementThreadTask task, void *userdata, int *wait_on_result)
{
    // We want to block for a result, but we are already running from the management thread! Just run the task now so we don't deadlock.
    if ((wait_on_result) && (SDL_ThreadID() == SDL_GetThreadID(ManagementThread))) {
        *wait_on_result = task(userdata);
        return 0;  // completed!
    }

    if (SDL_AtomicGet(&ManagementThreadShutdown)) {
        return SDL_SetError("Can't add task, we're shutting down");
    }

    ManagementThreadPendingTask *pending = SDL_calloc(1, sizeof(ManagementThreadPendingTask));
    if (!pending) {
        return -1;
    }

    pending->fn = task;
    pending->userdata = userdata;

    if (wait_on_result) {
        pending->task_complete_sem = SDL_CreateSemaphore(0);
        if (!pending->task_complete_sem) {
            SDL_free(pending);
            return -1;
        }
    }

    pending->next = NULL;

    SDL_LockMutex(ManagementThreadLock);

    // add to end of task list.
    ManagementThreadPendingTask *prev = NULL;
    for (ManagementThreadPendingTask *i = SDL_AtomicGetPtr((void **) &ManagementThreadPendingTasks); i; i = i->next) {
        prev = i;
    }

    if (prev) {
        prev->next = pending;
    } else {
        SDL_AtomicSetPtr((void **) &ManagementThreadPendingTasks, pending);
    }

    // task is added to the end of the pending list, let management thread rip!
    SDL_SignalCondition(ManagementThreadCondition);
    SDL_UnlockMutex(ManagementThreadLock);

    if (wait_on_result) {
        SDL_WaitSemaphore(pending->task_complete_sem);
        SDL_DestroySemaphore(pending->task_complete_sem);
        *wait_on_result = pending->result;
        if (pending->errorstr) {
            SDL_SetError("%s", pending->errorstr);
            SDL_free(pending->errorstr);
        }
        SDL_free(pending);
    }

    return 0; // successfully added (and possibly executed)!
}

static int ManagementThreadPrepare(void)
{
    if (WASAPI_PlatformInit() == -1) {
        return -1;
    }

    ManagementThreadLock = SDL_CreateMutex();
    if (!ManagementThreadLock) {
        WASAPI_PlatformDeinit();
        return -1;
    }

    ManagementThreadCondition = SDL_CreateCondition();
    if (!ManagementThreadCondition) {
        SDL_DestroyMutex(ManagementThreadLock);
        ManagementThreadLock = NULL;
        WASAPI_PlatformDeinit();
        return -1;
    }

    return 0;
}

typedef struct
{
    char *errorstr;
    SDL_Semaphore *ready_sem;
} ManagementThreadEntryData;

static int ManagementThreadEntry(void *userdata)
{
    ManagementThreadEntryData *data = (ManagementThreadEntryData *)userdata;

    if (ManagementThreadPrepare() < 0) {
        data->errorstr = SDL_strdup(SDL_GetError());
        SDL_PostSemaphore(data->ready_sem); // unblock calling thread.
        return 0;
    }

    SDL_PostSemaphore(data->ready_sem); // unblock calling thread.
    ManagementThreadMainloop();

    WASAPI_PlatformDeinit();
    return 0;
}

static int InitManagementThread(void)
{
    ManagementThreadEntryData mgmtdata;
    SDL_zero(mgmtdata);
    mgmtdata.ready_sem = SDL_CreateSemaphore(0);
    if (!mgmtdata.ready_sem) {
        return -1;
    }

    SDL_AtomicSetPtr((void **) &ManagementThreadPendingTasks, NULL);
    SDL_AtomicSet(&ManagementThreadShutdown, 0);
    ManagementThread = SDL_CreateThreadInternal(ManagementThreadEntry, "SDLWASAPIMgmt", 256 * 1024, &mgmtdata); // !!! FIXME: maybe even smaller stack size?
    if (!ManagementThread) {
        return -1;
    }

    SDL_WaitSemaphore(mgmtdata.ready_sem);
    SDL_DestroySemaphore(mgmtdata.ready_sem);

    if (mgmtdata.errorstr) {
        SDL_WaitThread(ManagementThread, NULL);
        ManagementThread = NULL;
        SDL_SetError("%s", mgmtdata.errorstr);
        SDL_free(mgmtdata.errorstr);
        return -1;
    }

    return 0;
}

static void DeinitManagementThread(void)
{
    if (ManagementThread) {
        SDL_AtomicSet(&ManagementThreadShutdown, 1);
        SDL_LockMutex(ManagementThreadLock);
        SDL_SignalCondition(ManagementThreadCondition);
        SDL_UnlockMutex(ManagementThreadLock);
        SDL_WaitThread(ManagementThread, NULL);
        ManagementThread = NULL;
    }

    SDL_assert(SDL_AtomicGetPtr((void **) &ManagementThreadPendingTasks) == NULL);

    SDL_DestroyCondition(ManagementThreadCondition);
    SDL_DestroyMutex(ManagementThreadLock);
    ManagementThreadCondition = NULL;
    ManagementThreadLock = NULL;
    SDL_AtomicSet(&ManagementThreadShutdown, 0);
}

typedef struct
{
    SDL_AudioDevice **default_output;
    SDL_AudioDevice **default_capture;
} mgmtthrtask_DetectDevicesData;

static int mgmtthrtask_DetectDevices(void *userdata)
{
    mgmtthrtask_DetectDevicesData *data = (mgmtthrtask_DetectDevicesData *)userdata;
    WASAPI_EnumerateEndpoints(data->default_output, data->default_capture);
    return 0;
}

static void WASAPI_DetectDevices(SDL_AudioDevice **default_output, SDL_AudioDevice **default_capture)
{
    int rc;
    // this blocks because it needs to finish before the audio subsystem inits
    mgmtthrtask_DetectDevicesData data = { default_output, default_capture };
    WASAPI_ProxyToManagementThread(mgmtthrtask_DetectDevices, &data, &rc);
}

static int mgmtthrtask_DisconnectDevice(void *userdata)
{
    SDL_AudioDeviceDisconnected((SDL_AudioDevice *)userdata);
    return 0;
}

void WASAPI_DisconnectDevice(SDL_AudioDevice *device)
{
    int rc;  // block on this; don't disconnect while holding the device lock!
    WASAPI_ProxyToManagementThread(mgmtthrtask_DisconnectDevice, device, &rc);
}

static SDL_bool WasapiFailed(SDL_AudioDevice *device, const HRESULT err)
{
    if (err == S_OK) {
        return SDL_FALSE;
    } else if (err == AUDCLNT_E_DEVICE_INVALIDATED) {
        device->hidden->device_lost = SDL_TRUE;
    } else {
        device->hidden->device_dead = SDL_TRUE;
    }

    return SDL_TRUE;
}

static int mgmtthrtask_StopAndReleaseClient(void *userdata)
{
    IAudioClient *client = (IAudioClient *) userdata;
    IAudioClient_Stop(client);
    IAudioClient_Release(client);
    return 0;
}

static int mgmtthrtask_ReleaseCaptureClient(void *userdata)
{
    IAudioCaptureClient_Release((IAudioCaptureClient *)userdata);
    return 0;
}

static int mgmtthrtask_ReleaseRenderClient(void *userdata)
{
    IAudioRenderClient_Release((IAudioRenderClient *)userdata);
    return 0;
}

static int mgmtthrtask_CoTaskMemFree(void *userdata)
{
    CoTaskMemFree(userdata);
    return 0;
}

static int mgmtthrtask_PlatformDeleteActivationHandler(void *userdata)
{
    WASAPI_PlatformDeleteActivationHandler(userdata);
    return 0;
}

static int mgmtthrtask_CloseHandle(void *userdata)
{
    CloseHandle((HANDLE) userdata);
    return 0;
}

static void ResetWasapiDevice(SDL_AudioDevice *device)
{
    if (!device || !device->hidden) {
        return;
    }

    // just queue up all the tasks in the management thread and don't block.
    // We don't care when any of these actually get free'd.

    if (device->hidden->client) {
        IAudioClient *client = device->hidden->client;
        device->hidden->client = NULL;
        WASAPI_ProxyToManagementThread(mgmtthrtask_StopAndReleaseClient, client, NULL);
    }

    if (device->hidden->render) {
        IAudioRenderClient *render = device->hidden->render;
        device->hidden->render = NULL;
        WASAPI_ProxyToManagementThread(mgmtthrtask_ReleaseRenderClient, render, NULL);
    }

    if (device->hidden->capture) {
        IAudioCaptureClient *capture = device->hidden->capture;
        device->hidden->capture = NULL;
        WASAPI_ProxyToManagementThread(mgmtthrtask_ReleaseCaptureClient, capture, NULL);
    }

    if (device->hidden->waveformat) {
        void *ptr = device->hidden->waveformat;
        device->hidden->waveformat = NULL;
        WASAPI_ProxyToManagementThread(mgmtthrtask_CoTaskMemFree, ptr, NULL);
    }

    if (device->hidden->activation_handler) {
        void *activation_handler = device->hidden->activation_handler;
        device->hidden->activation_handler = NULL;
        WASAPI_ProxyToManagementThread(mgmtthrtask_PlatformDeleteActivationHandler, activation_handler, NULL);
    }

    if (device->hidden->event) {
        HANDLE event = device->hidden->event;
        device->hidden->event = NULL;
        WASAPI_ProxyToManagementThread(mgmtthrtask_CloseHandle, (void *) event, NULL);
    }
}

static int mgmtthrtask_ActivateDevice(void *userdata)
{
    return WASAPI_ActivateDevice((SDL_AudioDevice *)userdata);
}

static int ActivateWasapiDevice(SDL_AudioDevice *device)
{
    // this blocks because we're either being notified from a background thread or we're running during device open,
    //  both of which won't deadlock vs the device thread.
    int rc = -1;
    return ((WASAPI_ProxyToManagementThread(mgmtthrtask_ActivateDevice, device, &rc) < 0) || (rc < 0)) ? -1 : 0;
}

// do not call when holding the device lock!
static SDL_bool RecoverWasapiDevice(SDL_AudioDevice *device)
{
    ResetWasapiDevice(device); // dump the lost device's handles.

    // This handles a non-default device that simply had its format changed in the Windows Control Panel.
    if (ActivateWasapiDevice(device) == -1) {
        WASAPI_DisconnectDevice(device);
        return SDL_FALSE;
    }

    device->hidden->device_lost = SDL_FALSE;

    return SDL_TRUE; // okay, carry on with new device details!
}

// do not call when holding the device lock!
static SDL_bool RecoverWasapiIfLost(SDL_AudioDevice *device)
{
    if (SDL_AtomicGet(&device->shutdown)) {
        return SDL_FALSE; // already failed.
    } else if (device->hidden->device_dead) {  // had a fatal error elsewhere, clean up and quit
        IAudioClient_Stop(device->hidden->client);
        WASAPI_DisconnectDevice(device);
        SDL_assert(SDL_AtomicGet(&device->shutdown));  // so we don't come back through here.
        return SDL_FALSE; // already failed.
    } else if (SDL_AtomicGet(&device->zombie)) {
        return SDL_FALSE;  // we're already dead, so just leave and let the Zombie implementations take over.
    } else if (!device->hidden->client) {
        return SDL_TRUE; // still waiting for activation.
    }

    return device->hidden->device_lost ? RecoverWasapiDevice(device) : SDL_TRUE;
}

static Uint8 *WASAPI_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    // get an endpoint buffer from WASAPI.
    BYTE *buffer = NULL;

    if (device->hidden->render) {
        if (WasapiFailed(device, IAudioRenderClient_GetBuffer(device->hidden->render, device->sample_frames, &buffer))) {
            SDL_assert(buffer == NULL);
            if (device->hidden->device_lost) {  // just use an available buffer, we won't be playing it anyhow.
                *buffer_size = 0;  // we'll recover during WaitDevice and try again.
            }
        }
    }

    return (Uint8 *)buffer;
}

static int WASAPI_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    if (device->hidden->render) { // definitely activated?
        // WasapiFailed() will mark the device for reacquisition or removal elsewhere.
        WasapiFailed(device, IAudioRenderClient_ReleaseBuffer(device->hidden->render, device->sample_frames, 0));
    }
    return 0;
}

static int WASAPI_WaitDevice(SDL_AudioDevice *device)
{
    // WaitDevice does not hold the device lock, so check for recovery/disconnect details here.
    while (RecoverWasapiIfLost(device) && device->hidden->client && device->hidden->event) {
        DWORD waitResult = WaitForSingleObjectEx(device->hidden->event, 200, FALSE);
        if (waitResult == WAIT_OBJECT_0) {
            const UINT32 maxpadding = device->sample_frames;
            UINT32 padding = 0;
            if (!WasapiFailed(device, IAudioClient_GetCurrentPadding(device->hidden->client, &padding))) {
                //SDL_Log("WASAPI EVENT! padding=%u maxpadding=%u", (unsigned int)padding, (unsigned int)maxpadding);*/
                if (device->iscapture && (padding > 0)) {
                    break;
                } else if (!device->iscapture && (padding <= maxpadding)) {
                    break;
                }
            }
        } else if (waitResult != WAIT_TIMEOUT) {
            //SDL_Log("WASAPI FAILED EVENT!");*/
            IAudioClient_Stop(device->hidden->client);
            return -1;
        }
    }

    return 0;
}

static int WASAPI_CaptureFromDevice(SDL_AudioDevice *device, void *buffer, int buflen)
{
    BYTE *ptr = NULL;
    UINT32 frames = 0;
    DWORD flags = 0;

    while (device->hidden->capture) {
        const HRESULT ret = IAudioCaptureClient_GetBuffer(device->hidden->capture, &ptr, &frames, &flags, NULL, NULL);
        if (ret == AUDCLNT_S_BUFFER_EMPTY) {
            return 0;  // in theory we should have waited until there was data, but oh well, we'll go back to waiting. Returning 0 is safe in SDL3.
        }

        WasapiFailed(device, ret); // mark device lost/failed if necessary.

        if (ret == S_OK) {
            const int total = ((int)frames) * device->hidden->framesize;
            const int cpy = SDL_min(buflen, total);
            const int leftover = total - cpy;
            const SDL_bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) ? SDL_TRUE : SDL_FALSE;

            SDL_assert(leftover == 0);  // according to MSDN, this isn't everything available, just one "packet" of data per-GetBuffer call.

            if (silent) {
                SDL_memset(buffer, device->silence_value, cpy);
            } else {
                SDL_memcpy(buffer, ptr, cpy);
            }

            WasapiFailed(device, IAudioCaptureClient_ReleaseBuffer(device->hidden->capture, frames));

            return cpy;
        }
    }

    return -1; // unrecoverable error.
}

static void WASAPI_FlushCapture(SDL_AudioDevice *device)
{
    BYTE *ptr = NULL;
    UINT32 frames = 0;
    DWORD flags = 0;

    // just read until we stop getting packets, throwing them away.
    while (!SDL_AtomicGet(&device->shutdown) && device->hidden->capture) {
        const HRESULT ret = IAudioCaptureClient_GetBuffer(device->hidden->capture, &ptr, &frames, &flags, NULL, NULL);
        if (ret == AUDCLNT_S_BUFFER_EMPTY) {
            break; // no more buffered data; we're done.
        } else if (WasapiFailed(device, ret)) {
            break; // failed for some other reason, abort.
        } else if (WasapiFailed(device, IAudioCaptureClient_ReleaseBuffer(device->hidden->capture, frames))) {
            break; // something broke.
        }
    }
}

static void WASAPI_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        ResetWasapiDevice(device);
        SDL_free(device->hidden->devid);
        SDL_free(device->hidden);
        device->hidden = NULL;
    }
}

static int mgmtthrtask_PrepDevice(void *userdata)
{
    SDL_AudioDevice *device = (SDL_AudioDevice *)userdata;

    /* !!! FIXME: we could request an exclusive mode stream, which is lower latency;
       !!!  it will write into the kernel's audio buffer directly instead of
       !!!  shared memory that a user-mode mixer then writes to the kernel with
       !!!  everything else. Doing this means any other sound using this device will
       !!!  stop playing, including the user's MP3 player and system notification
       !!!  sounds. You'd probably need to release the device when the app isn't in
       !!!  the foreground, to be a good citizen of the system. It's doable, but it's
       !!!  more work and causes some annoyances, and I don't know what the latency
       !!!  wins actually look like. Maybe add a hint to force exclusive mode at
       !!!  some point. To be sure, defaulting to shared mode is the right thing to
       !!!  do in any case. */
    const AUDCLNT_SHAREMODE sharemode = AUDCLNT_SHAREMODE_SHARED;

    IAudioClient *client = device->hidden->client;
    SDL_assert(client != NULL);

#if defined(__WINRT__) || defined(__GDK__) // CreateEventEx() arrived in Vista, so we need an #ifdef for XP.
    device->hidden->event = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
#else
    device->hidden->event = CreateEventW(NULL, 0, 0, NULL);
#endif

    if (!device->hidden->event) {
        return WIN_SetError("WASAPI can't create an event handle");
    }

    HRESULT ret;

    WAVEFORMATEX *waveformat = NULL;
    ret = IAudioClient_GetMixFormat(client, &waveformat);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't determine mix format", ret);
    }
    SDL_assert(waveformat != NULL);
    device->hidden->waveformat = waveformat;

    SDL_AudioSpec newspec;
    newspec.channels = (Uint8)waveformat->nChannels;

    // Make sure we have a valid format that we can convert to whatever WASAPI wants.
    const SDL_AudioFormat wasapi_format = SDL_WaveFormatExToSDLFormat(waveformat);

    SDL_AudioFormat test_format;
    const SDL_AudioFormat *closefmts = SDL_ClosestAudioFormats(device->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        if (test_format == wasapi_format) {
            newspec.format = test_format;
            break;
        }
    }

    if (!test_format) {
        return SDL_SetError("%s: Unsupported audio format", "wasapi");
    }

    REFERENCE_TIME default_period = 0;
    ret = IAudioClient_GetDevicePeriod(client, &default_period, NULL);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't determine minimum device period", ret);
    }

    DWORD streamflags = 0;

    /* we've gotten reports that WASAPI's resampler introduces distortions, but in the short term
       it fixes some other WASAPI-specific quirks we haven't quite tracked down.
       Refer to bug #6326 for the immediate concern. */
#if 1
    // favor WASAPI's resampler over our own
    if ((DWORD)device->spec.freq != waveformat->nSamplesPerSec) {
        streamflags |= (AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY);
        waveformat->nSamplesPerSec = device->spec.freq;
        waveformat->nAvgBytesPerSec = waveformat->nSamplesPerSec * waveformat->nChannels * (waveformat->wBitsPerSample / 8);
    }
#endif

    newspec.freq = waveformat->nSamplesPerSec;

    streamflags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    ret = IAudioClient_Initialize(client, sharemode, streamflags, 0, 0, waveformat, NULL);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't initialize audio client", ret);
    }

    ret = IAudioClient_SetEventHandle(client, device->hidden->event);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't set event handle", ret);
    }

    UINT32 bufsize = 0; // this is in sample frames, not samples, not bytes.
    ret = IAudioClient_GetBufferSize(client, &bufsize);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't determine buffer size", ret);
    }

    /* Match the callback size to the period size to cut down on the number of
       interrupts waited for in each call to WaitDevice */
    const float period_millis = default_period / 10000.0f;
    const float period_frames = period_millis * newspec.freq / 1000.0f;
    const int new_sample_frames = (int) SDL_ceilf(period_frames);

    // Update the fragment size as size in bytes
    if (SDL_AudioDeviceFormatChangedAlreadyLocked(device, &newspec, new_sample_frames) < 0) {
        return -1;
    }

    device->hidden->framesize = SDL_AUDIO_FRAMESIZE(device->spec);

    if (device->iscapture) {
        IAudioCaptureClient *capture = NULL;
        ret = IAudioClient_GetService(client, &SDL_IID_IAudioCaptureClient, (void **)&capture);
        if (FAILED(ret)) {
            return WIN_SetErrorFromHRESULT("WASAPI can't get capture client service", ret);
        }

        SDL_assert(capture != NULL);
        device->hidden->capture = capture;
        ret = IAudioClient_Start(client);
        if (FAILED(ret)) {
            return WIN_SetErrorFromHRESULT("WASAPI can't start capture", ret);
        }

        WASAPI_FlushCapture(device); // MSDN says you should flush capture endpoint right after startup.
    } else {
        IAudioRenderClient *render = NULL;
        ret = IAudioClient_GetService(client, &SDL_IID_IAudioRenderClient, (void **)&render);
        if (FAILED(ret)) {
            return WIN_SetErrorFromHRESULT("WASAPI can't get render client service", ret);
        }

        SDL_assert(render != NULL);
        device->hidden->render = render;
        ret = IAudioClient_Start(client);
        if (FAILED(ret)) {
            return WIN_SetErrorFromHRESULT("WASAPI can't start playback", ret);
        }
    }

    return 0; // good to go.
}

// This is called once a device is activated, possibly asynchronously.
int WASAPI_PrepDevice(SDL_AudioDevice *device)
{
    int rc = 0;
    return (WASAPI_ProxyToManagementThread(mgmtthrtask_PrepDevice, device, &rc) < 0) ? -1 : rc;
}

static int WASAPI_OpenDevice(SDL_AudioDevice *device)
{
    // Initialize all variables that we clean on shutdown
    device->hidden = (struct SDL_PrivateAudioData *) SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) {
        return -1;
    } else if (ActivateWasapiDevice(device) < 0) {
        return -1; // already set error.
    }

    /* Ready, but possibly waiting for async device activation.
       Until activation is successful, we will report silence from capture
       devices and ignore data on playback devices. Upon activation, we'll make
       sure any bound audio streams are adjusted for the final device format. */

    return 0;
}

static void WASAPI_ThreadInit(SDL_AudioDevice *device)
{
    WASAPI_PlatformThreadInit(device);
}

static void WASAPI_ThreadDeinit(SDL_AudioDevice *device)
{
    WASAPI_PlatformThreadDeinit(device);
}

static int mgmtthrtask_FreeDeviceHandle(void *userdata)
{
    WASAPI_PlatformFreeDeviceHandle((SDL_AudioDevice *)userdata);
    return 0;
}

static void WASAPI_FreeDeviceHandle(SDL_AudioDevice *device)
{
    int rc;
    WASAPI_ProxyToManagementThread(mgmtthrtask_FreeDeviceHandle, device, &rc);
}

static int mgmtthrtask_DeinitializeStart(void *userdata)
{
    WASAPI_PlatformDeinitializeStart();
    return 0;
}

static void WASAPI_DeinitializeStart(void)
{
    int rc;
    WASAPI_ProxyToManagementThread(mgmtthrtask_DeinitializeStart, NULL, &rc);
}

static void WASAPI_Deinitialize(void)
{
    DeinitManagementThread();
}

static SDL_bool WASAPI_Init(SDL_AudioDriverImpl *impl)
{
    if (InitManagementThread() < 0) {
        return SDL_FALSE;
    }

    impl->DetectDevices = WASAPI_DetectDevices;
    impl->ThreadInit = WASAPI_ThreadInit;
    impl->ThreadDeinit = WASAPI_ThreadDeinit;
    impl->OpenDevice = WASAPI_OpenDevice;
    impl->PlayDevice = WASAPI_PlayDevice;
    impl->WaitDevice = WASAPI_WaitDevice;
    impl->GetDeviceBuf = WASAPI_GetDeviceBuf;
    impl->WaitCaptureDevice = WASAPI_WaitDevice;
    impl->CaptureFromDevice = WASAPI_CaptureFromDevice;
    impl->FlushCapture = WASAPI_FlushCapture;
    impl->CloseDevice = WASAPI_CloseDevice;
    impl->DeinitializeStart = WASAPI_DeinitializeStart;
    impl->Deinitialize = WASAPI_Deinitialize;
    impl->FreeDeviceHandle = WASAPI_FreeDeviceHandle;

    impl->HasCaptureSupport = SDL_TRUE;

    return SDL_TRUE;
}

AudioBootStrap WASAPI_bootstrap = {
    "wasapi", "WASAPI", WASAPI_Init, SDL_FALSE
};

#endif // SDL_AUDIO_DRIVER_WASAPI
