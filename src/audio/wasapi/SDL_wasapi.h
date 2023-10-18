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

#ifndef SDL_wasapi_h_
#define SDL_wasapi_h_

#ifdef __cplusplus
extern "C" {
#endif

#include "../SDL_sysaudio.h"

struct SDL_PrivateAudioData
{
    WCHAR *devid;
    WAVEFORMATEX *waveformat;
    IAudioClient *client;
    IAudioRenderClient *render;
    IAudioCaptureClient *capture;
    HANDLE event;
    HANDLE task;
    SDL_bool coinitialized;
    int framesize;
    SDL_bool device_lost;
    SDL_bool device_dead;
    void *activation_handler;
};

// win32 and winrt implementations call into these.
int WASAPI_PrepDevice(SDL_AudioDevice *device);
void WASAPI_DisconnectDevice(SDL_AudioDevice *device);  // don't hold the device lock when calling this!


// BE CAREFUL: if you are holding the device lock and proxy to the management thread with wait_until_complete, and grab the lock again, you will deadlock.
typedef int (*ManagementThreadTask)(void *userdata);
int WASAPI_ProxyToManagementThread(ManagementThreadTask task, void *userdata, int *wait_until_complete);

// These are functions that are implemented differently for Windows vs WinRT.
// UNLESS OTHERWISE NOTED THESE ALL HAPPEN ON THE MANAGEMENT THREAD.
int WASAPI_PlatformInit(void);
void WASAPI_PlatformDeinit(void);
void WASAPI_PlatformDeinitializeStart(void);
void WASAPI_EnumerateEndpoints(SDL_AudioDevice **default_output, SDL_AudioDevice **default_capture);
int WASAPI_ActivateDevice(SDL_AudioDevice *device);
void WASAPI_PlatformThreadInit(SDL_AudioDevice *device);  // this happens on the audio device thread, not the management thread.
void WASAPI_PlatformThreadDeinit(SDL_AudioDevice *device);  // this happens on the audio device thread, not the management thread.
void WASAPI_PlatformDeleteActivationHandler(void *handler);
void WASAPI_PlatformFreeDeviceHandle(SDL_AudioDevice *device);

#ifdef __cplusplus
}
#endif

#endif // SDL_wasapi_h_
