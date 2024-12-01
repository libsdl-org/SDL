/*
  Simple DirectMedia Layer
  Copyright (C) 2024 Katharine Chui <katharine.chui@gmail.com>

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
#include "../../SDL_internal.h"

#ifdef SDL_JOYSTICK_HIDAPI

#include "SDL_hidapihaptic_c.h"
#include "SDL_mutex.h"
#include "SDL_error.h"

extern struct _SDL_JoystickDriver SDL_HIDAPI_JoystickDriver;

typedef struct haptic_list_node
{
    SDL_Haptic *haptic;
    struct haptic_list_node *next;
} haptic_list_node;

static haptic_list_node *haptic_list_head = NULL;
static SDL_mutex *haptic_list_mutex = NULL;

SDL_HIDAPI_HapticDriver *drivers[] = {
    #ifdef SDL_HAPTIC_HIDAPI_LG4FF
    &SDL_HIDAPI_HapticDriverLg4ff,
    #endif //SDL_HAPTIC_HIDAPI_LG4FF
};

int SDL_HIDAPI_HapticInit()
{
    haptic_list_head = NULL;
    haptic_list_mutex = SDL_CreateMutex();
    if (haptic_list_mutex == NULL) {
        return SDL_Error(SDL_ENOMEM);
    }
    return 0;
}

SDL_bool SDL_HIDAPI_HapticIsHidapi(SDL_Haptic *haptic)
{
    haptic_list_node *cur;
    SDL_bool ret = SDL_FALSE;

    SDL_LockMutex(haptic_list_mutex);
    cur = haptic_list_head;
    while (cur != NULL) {
        if (cur->haptic == haptic) {
            ret = SDL_TRUE;
            break;
        }
        cur = cur->next;
    }

    SDL_UnlockMutex(haptic_list_mutex);

    return ret;
}


SDL_bool SDL_HIDAPI_JoystickIsHaptic(SDL_Joystick *joystick)
{
    int i;

    SDL_AssertJoysticksLocked();

    if (joystick->driver != &SDL_HIDAPI_JoystickDriver){
        return SDL_FALSE;
    }

    for (i = 0; i < SDL_arraysize(drivers); ++i) {
        if(drivers[i]->JoystickSupported(joystick)) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

int SDL_HIDAPI_HapticOpenFromJoystick(SDL_Haptic *haptic, SDL_Joystick *joystick)
{
    int i;
    SDL_AssertJoysticksLocked();

    if (joystick->driver != &SDL_HIDAPI_JoystickDriver){
        return SDL_SetError("Cannot open hidapi haptic from non hidapi joystick");
    }

    for (i = 0;i < SDL_arraysize(drivers);++i) {
        if(drivers[i]->JoystickSupported(joystick)) {
            SDL_HIDAPI_HapticDevice *device;
            haptic_list_node *list_node;
            // the driver is responsible for calling SDL_SetError
            void *ctx = drivers[i]->Open(joystick);
            if (ctx == NULL) {
                return -1;
            }

            device = SDL_malloc(sizeof(SDL_HIDAPI_HapticDevice));
            if (device == NULL) {
                SDL_HIDAPI_HapticDevice temp;
                temp.ctx = ctx;
                temp.driver = drivers[i];
                temp.joystick = joystick;
                temp.driver->Close(&temp);
                return SDL_Error(SDL_ENOMEM);
            }

            device->driver = drivers[i];
            device->haptic = haptic;
            device->joystick = joystick;
            device->ctx = ctx;

            list_node = SDL_malloc(sizeof(haptic_list_node));
            if(list_node == NULL) {
                device->driver->Close(device);
                SDL_free(device);
                return SDL_Error(SDL_ENOMEM);
            }

            haptic->hwdata = (struct haptic_hwdata *)device;

            // this is outside of the syshaptic driver
            haptic->index = 255;

            haptic->neffects = device->driver->NumEffects(device);
            haptic->nplaying = device->driver->NumEffectsPlaying(device);
            haptic->supported = device->driver->Query(device);
            haptic->naxes = device->driver->NumAxes(device);

            list_node->haptic = haptic;
            list_node->next = NULL;
            
            // grab a joystick ref so that it doesn't get fully destroyed before the haptic is closed
            joystick->ref_count++;

            SDL_LockMutex(haptic_list_mutex);
            if (haptic_list_head == NULL) {
                haptic_list_head = list_node;
            } else {
                haptic_list_node *cur = haptic_list_head;
                while(cur->next != NULL) {
                cur = cur->next;
                }
                cur->next = list_node;
            }
            SDL_UnlockMutex(haptic_list_mutex);

            return 0;
        }
    }

    return SDL_SetError("No supported HIDAPI haptic driver found for joystick");
}

SDL_bool SDL_HIDAPI_JoystickSameHaptic(SDL_Haptic *haptic, SDL_Joystick *joystick)
{
    SDL_HIDAPI_HapticDevice *device;

    SDL_AssertJoysticksLocked();
    if (joystick->driver != &SDL_HIDAPI_JoystickDriver){
        return SDL_FALSE;
    }

    device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;

    if (joystick == device->joystick) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

void SDL_HIDAPI_HapticClose(SDL_Haptic *haptic)
{
    haptic_list_node *cur, *last;

    SDL_LockMutex(haptic_list_mutex);

    cur = haptic_list_head;
    while (cur != NULL) {
        if (cur->haptic == haptic) {
            SDL_HIDAPI_HapticDevice *device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;

            device->driver->Close(device);
            
            // a reference was grabbed during open, now release it
            SDL_JoystickClose(device->joystick);

            if (cur == haptic_list_head) {
                haptic_list_head = cur->next;
            } else {
                last->next = cur->next;
            }

            SDL_free(device->ctx);
            SDL_free(device);
            SDL_free(cur);
            SDL_UnlockMutex(haptic_list_mutex);
            return;
        }
        last = cur;
        cur = cur->next;
    }

    SDL_UnlockMutex(haptic_list_mutex);
}

void SDL_HIDAPI_HapticQuit(void)
{
    // the list is cleared in SDL_haptic.c
    if (haptic_list_mutex != NULL) {
        SDL_DestroyMutex(haptic_list_mutex);
        haptic_list_mutex = NULL;
    }
}

int SDL_HIDAPI_HapticNewEffect(SDL_Haptic *haptic, SDL_HapticEffect *base)
{
    SDL_HIDAPI_HapticDevice *device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;
    return device->driver->NewEffect(device, base);
}

int SDL_HIDAPI_HapticUpdateEffect(SDL_Haptic *haptic, int id, SDL_HapticEffect *data)
{
    SDL_HIDAPI_HapticDevice *device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;
    return device->driver->UpdateEffect(device, id, data);
}

int SDL_HIDAPI_HapticRunEffect(SDL_Haptic *haptic, int id, Uint32 iterations)
{
    SDL_HIDAPI_HapticDevice *device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;
    return device->driver->RunEffect(device, id, iterations);
}

int SDL_HIDAPI_HapticStopEffect(SDL_Haptic *haptic, int id)
{
    SDL_HIDAPI_HapticDevice *device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;
    return device->driver->StopEffect(device, id);
}

void SDL_HIDAPI_HapticDestroyEffect(SDL_Haptic *haptic, int id)
{
    SDL_HIDAPI_HapticDevice *device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;
    return device->driver->DestroyEffect(device, id);
}

int SDL_HIDAPI_HapticGetEffectStatus(SDL_Haptic *haptic, int id)
{
    SDL_HIDAPI_HapticDevice *device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;
    return device->driver->GetEffectStatus(device, id);
}

int SDL_HIDAPI_HapticSetGain(SDL_Haptic *haptic, int gain)
{
    SDL_HIDAPI_HapticDevice *device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;
    return device->driver->SetGain(device, gain);
}

int SDL_HIDAPI_HapticSetAutocenter(SDL_Haptic *haptic, int autocenter)
{
    SDL_HIDAPI_HapticDevice *device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;
    return device->driver->SetAutocenter(device, autocenter);
}

int SDL_HIDAPI_HapticPause(SDL_Haptic *haptic)
{
    SDL_HIDAPI_HapticDevice *device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;
    return device->driver->Pause(device);
}

int SDL_HIDAPI_HapticUnpause(SDL_Haptic *haptic)
{
    SDL_HIDAPI_HapticDevice *device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;
    return device->driver->Unpause(device);
}

int SDL_HIDAPI_HapticStopAll(SDL_Haptic *haptic)
{
    SDL_HIDAPI_HapticDevice *device = (SDL_HIDAPI_HapticDevice *)haptic->hwdata;
    return device->driver->StopAll(device);
}

#endif //SDL_JOYSTICK_HIDAPI