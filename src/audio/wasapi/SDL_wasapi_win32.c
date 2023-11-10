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

/* This is code that Windows uses to talk to WASAPI-related system APIs.
   This is for non-WinRT desktop apps. The C++/CX implementation of these
   functions, exclusive to WinRT, are in SDL_wasapi_winrt.cpp.
   The code in SDL_wasapi.c is used by both standard Windows and WinRT builds
   to deal with audio and calls into these functions. */

#if defined(SDL_AUDIO_DRIVER_WASAPI) && !defined(__WINRT__)

#include "../../core/windows/SDL_windows.h"
#include "../../core/windows/SDL_immdevice.h"
#include "../SDL_sysaudio.h"

#include <audioclient.h>

#include "SDL_wasapi.h"

// handle to Avrt.dll--Vista and later!--for flagging the callback thread as "Pro Audio" (low latency).
static HMODULE libavrt = NULL;
typedef HANDLE(WINAPI *pfnAvSetMmThreadCharacteristicsW)(LPCWSTR, LPDWORD);
typedef BOOL(WINAPI *pfnAvRevertMmThreadCharacteristics)(HANDLE);
static pfnAvSetMmThreadCharacteristicsW pAvSetMmThreadCharacteristicsW = NULL;
static pfnAvRevertMmThreadCharacteristics pAvRevertMmThreadCharacteristics = NULL;

static SDL_bool immdevice_initialized = SDL_FALSE;

// Some GUIDs we need to know without linking to libraries that aren't available before Vista.
static const IID SDL_IID_IAudioClient = { 0x1cb9ad4c, 0xdbfa, 0x4c32, { 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2 } };

static int mgmtthrtask_AudioDeviceDisconnected(void *userdata)
{
    SDL_AudioDeviceDisconnected((SDL_AudioDevice *)userdata);
    return 0;
}

static void WASAPI_AudioDeviceDisconnected(SDL_AudioDevice *device)
{
    // don't wait on this, IMMDevice's own thread needs to return or everything will deadlock.
    WASAPI_ProxyToManagementThread(mgmtthrtask_AudioDeviceDisconnected, device, NULL);
}

static int mgmtthrtask_DefaultAudioDeviceChanged(void *userdata)
{
    SDL_DefaultAudioDeviceChanged((SDL_AudioDevice *) userdata);
    return 0;
}

static void WASAPI_DefaultAudioDeviceChanged(SDL_AudioDevice *new_default_device)
{
    // don't wait on this, IMMDevice's own thread needs to return or everything will deadlock.
    WASAPI_ProxyToManagementThread(mgmtthrtask_DefaultAudioDeviceChanged, new_default_device, NULL);
}

int WASAPI_PlatformInit(void)
{
    const SDL_IMMDevice_callbacks callbacks = { WASAPI_AudioDeviceDisconnected, WASAPI_DefaultAudioDeviceChanged };
    if (FAILED(WIN_CoInitialize())) {
        return SDL_SetError("CoInitialize() failed");
    } else if (SDL_IMMDevice_Init(&callbacks) < 0) {
        return -1; // Error string is set by SDL_IMMDevice_Init
    }

    immdevice_initialized = SDL_TRUE;

    libavrt = LoadLibrary(TEXT("avrt.dll")); // this library is available in Vista and later. No WinXP, so have to LoadLibrary to use it for now!
    if (libavrt) {
        pAvSetMmThreadCharacteristicsW = (pfnAvSetMmThreadCharacteristicsW)GetProcAddress(libavrt, "AvSetMmThreadCharacteristicsW");
        pAvRevertMmThreadCharacteristics = (pfnAvRevertMmThreadCharacteristics)GetProcAddress(libavrt, "AvRevertMmThreadCharacteristics");
    }

    return 0;
}

static void StopWasapiHotplug(void)
{
    if (immdevice_initialized) {
        SDL_IMMDevice_Quit();
        immdevice_initialized = SDL_FALSE;
    }
}

void WASAPI_PlatformDeinit(void)
{
    if (libavrt) {
        FreeLibrary(libavrt);
        libavrt = NULL;
    }

    pAvSetMmThreadCharacteristicsW = NULL;
    pAvRevertMmThreadCharacteristics = NULL;

    StopWasapiHotplug();

    WIN_CoUninitialize();
}

void WASAPI_PlatformDeinitializeStart(void)
{
    StopWasapiHotplug();
}

void WASAPI_PlatformThreadInit(SDL_AudioDevice *device)
{
    // this thread uses COM.
    if (SUCCEEDED(WIN_CoInitialize())) { // can't report errors, hope it worked!
        device->hidden->coinitialized = SDL_TRUE;
    }

    // Set this thread to very high "Pro Audio" priority.
    if (pAvSetMmThreadCharacteristicsW) {
        DWORD idx = 0;
        device->hidden->task = pAvSetMmThreadCharacteristicsW(L"Pro Audio", &idx);
    } else {
        SDL_SetThreadPriority(device->iscapture ? SDL_THREAD_PRIORITY_HIGH : SDL_THREAD_PRIORITY_TIME_CRITICAL);
    }

}

void WASAPI_PlatformThreadDeinit(SDL_AudioDevice *device)
{
    // Set this thread back to normal priority.
    if (device->hidden->task && pAvRevertMmThreadCharacteristics) {
        pAvRevertMmThreadCharacteristics(device->hidden->task);
        device->hidden->task = NULL;
    }

    if (device->hidden->coinitialized) {
        WIN_CoUninitialize();
        device->hidden->coinitialized = SDL_FALSE;
    }
}

int WASAPI_ActivateDevice(SDL_AudioDevice *device)
{
    IMMDevice *immdevice = NULL;
    if (SDL_IMMDevice_Get(device, &immdevice, device->iscapture) < 0) {
        device->hidden->client = NULL;
        return -1; // This is already set by SDL_IMMDevice_Get
    }

    // this is _not_ async in standard win32, yay!
    HRESULT ret = IMMDevice_Activate(immdevice, &SDL_IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&device->hidden->client);
    IMMDevice_Release(immdevice);

    if (FAILED(ret)) {
        SDL_assert(device->hidden->client == NULL);
        return WIN_SetErrorFromHRESULT("WASAPI can't activate audio endpoint", ret);
    }

    SDL_assert(device->hidden->client != NULL);
    if (WASAPI_PrepDevice(device) == -1) { // not async, fire it right away.
        return -1;
    }

    return 0; // good to go.
}

void WASAPI_EnumerateEndpoints(SDL_AudioDevice **default_output, SDL_AudioDevice **default_capture)
{
    SDL_IMMDevice_EnumerateEndpoints(default_output, default_capture);
}

void WASAPI_PlatformDeleteActivationHandler(void *handler)
{
    // not asynchronous.
    SDL_assert(!"This function should have only been called on WinRT.");
}

void WASAPI_PlatformFreeDeviceHandle(SDL_AudioDevice *device)
{
    SDL_IMMDevice_FreeDeviceHandle(device);
}

#endif // SDL_AUDIO_DRIVER_WASAPI && !defined(__WINRT__)
