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

#ifndef SDL_IMMDEVICE_H
#define SDL_IMMDEVICE_H

#define COBJMACROS
#include <mmdeviceapi.h>
#include <mmreg.h>

typedef struct SDL_AudioDevice SDL_AudioDevice; // this is defined in src/audio/SDL_sysaudio.h

typedef struct SDL_IMMDevice_callbacks
{
    void (*audio_device_disconnected)(SDL_AudioDevice *device);
    void (*default_audio_device_changed)(SDL_AudioDevice *new_default_device);
} SDL_IMMDevice_callbacks;

int SDL_IMMDevice_Init(const SDL_IMMDevice_callbacks *callbacks);
void SDL_IMMDevice_Quit(void);
int SDL_IMMDevice_Get(SDL_AudioDevice *device, IMMDevice **immdevice, SDL_bool iscapture);
void SDL_IMMDevice_EnumerateEndpoints(SDL_AudioDevice **default_output, SDL_AudioDevice **default_capture);
LPGUID SDL_IMMDevice_GetDirectSoundGUID(SDL_AudioDevice *device);
LPCWSTR SDL_IMMDevice_GetDevID(SDL_AudioDevice *device);
void SDL_IMMDevice_FreeDeviceHandle(SDL_AudioDevice *device);

#endif /* SDL_IMMDEVICE_H */
