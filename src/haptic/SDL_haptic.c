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

#include "SDL_syshaptic.h"
#include "SDL_haptic_c.h"
#include "../joystick/SDL_joystick_c.h" /* For SDL_IsJoystickValid */

static SDL_Haptic *SDL_haptics = NULL;
static char SDL_haptic_magic;

#define CHECK_HAPTIC_MAGIC(haptic, retval)                  \
    if (!haptic || haptic->magic != &SDL_haptic_magic) {    \
        SDL_InvalidParamError("haptic");                    \
        return retval;                                      \
    }

int SDL_InitHaptics(void)
{
    int status;

    status = SDL_SYS_HapticInit();
    if (status >= 0) {
        status = 0;
    }

    return status;
}

static SDL_bool SDL_GetHapticIndex(SDL_HapticID instance_id, int *driver_index)
{
    int num_haptics, device_index;

    if (instance_id > 0) {
        num_haptics = SDL_SYS_NumHaptics();
        for (device_index = 0; device_index < num_haptics; ++device_index) {
            SDL_HapticID haptic_id = SDL_SYS_HapticInstanceID(device_index);
            if (haptic_id == instance_id) {
                *driver_index = device_index;
                return SDL_TRUE;
            }
        }
    }

    SDL_SetError("Haptic device %" SDL_PRIu32 " not found", instance_id);
    return SDL_FALSE;
}

SDL_HapticID *SDL_GetHaptics(int *count)
{
    int device_index;
    int haptic_index = 0, num_haptics = 0;
    SDL_HapticID *haptics;

    num_haptics = SDL_SYS_NumHaptics();

    haptics = (SDL_HapticID *)SDL_malloc((num_haptics + 1) * sizeof(*haptics));
    if (haptics) {
        if (count) {
            *count = num_haptics;
        }

        for (device_index = 0; device_index < num_haptics; ++device_index) {
            haptics[haptic_index] = SDL_SYS_HapticInstanceID(device_index);
            SDL_assert(haptics[haptic_index] > 0);
            ++haptic_index;
        }
        haptics[haptic_index] = 0;
    } else {
        if (count) {
            *count = 0;
        }
    }

    return haptics;
}

const char *SDL_GetHapticInstanceName(SDL_HapticID instance_id)
{
    int device_index;
    const char *name = NULL;

    if (SDL_GetHapticIndex(instance_id, &device_index)) {
        name = SDL_SYS_HapticName(device_index);
    }
    return name;
}

SDL_Haptic *SDL_OpenHaptic(SDL_HapticID instance_id)
{
    SDL_Haptic *haptic;
    SDL_Haptic *hapticlist;
    const char *name;
    int device_index = 0;

    if (!SDL_GetHapticIndex(instance_id, &device_index)) {
        return NULL;
    }

    hapticlist = SDL_haptics;
    /* If the haptic device is already open, return it
     * it is important that we have a single haptic device for each instance id
     */
    while (hapticlist) {
        if (instance_id == hapticlist->instance_id) {
            haptic = hapticlist;
            ++haptic->ref_count;
            return haptic;
        }
        hapticlist = hapticlist->next;
    }

    /* Create the haptic device */
    haptic = (SDL_Haptic *)SDL_calloc(1, sizeof(*haptic));
    if (!haptic) {
        return NULL;
    }

    /* Initialize the haptic device */
    haptic->magic = &SDL_haptic_magic;
    haptic->instance_id = instance_id;
    haptic->rumble_id = -1;
    if (SDL_SYS_HapticOpen(haptic) < 0) {
        SDL_free(haptic);
        return NULL;
    }

    if (!haptic->name) {
        name = SDL_SYS_HapticName(device_index);
        if (name) {
            haptic->name = SDL_strdup(name);
        }
    }

    /* Add haptic to list */
    ++haptic->ref_count;
    /* Link the haptic in the list */
    haptic->next = SDL_haptics;
    SDL_haptics = haptic;

    /* Disable autocenter and set gain to max. */
    if (haptic->supported & SDL_HAPTIC_GAIN) {
        SDL_SetHapticGain(haptic, 100);
    }
    if (haptic->supported & SDL_HAPTIC_AUTOCENTER) {
        SDL_SetHapticAutocenter(haptic, 0);
    }

    return haptic;
}

SDL_Haptic *SDL_GetHapticFromInstanceID(SDL_HapticID instance_id)
{
    SDL_Haptic *haptic;

    for (haptic = SDL_haptics; haptic; haptic = haptic->next) {
        if (instance_id == haptic->instance_id) {
            break;
        }
    }
    return haptic;
}

SDL_HapticID SDL_GetHapticInstanceID(SDL_Haptic *haptic)
{
    CHECK_HAPTIC_MAGIC(haptic, 0);

    return haptic->instance_id;
}

const char *SDL_GetHapticName(SDL_Haptic *haptic)
{
    CHECK_HAPTIC_MAGIC(haptic, 0);

    return haptic->name;
}

SDL_bool SDL_IsMouseHaptic(void)
{
    if (SDL_SYS_HapticMouse() < 0) {
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

SDL_Haptic *SDL_OpenHapticFromMouse(void)
{
    int device_index;

    device_index = SDL_SYS_HapticMouse();

    if (device_index < 0) {
        SDL_SetError("Haptic: Mouse isn't a haptic device.");
        return NULL;
    }

    return SDL_OpenHaptic(device_index);
}

SDL_bool SDL_IsJoystickHaptic(SDL_Joystick *joystick)
{
    SDL_bool result = SDL_FALSE;

    SDL_LockJoysticks();
    {
        /* Must be a valid joystick */
        if (SDL_IsJoystickValid(joystick) &&
            !SDL_IsGamepad(SDL_GetJoystickInstanceID(joystick))) {
            if (SDL_SYS_JoystickIsHaptic(joystick) > 0) {
                result = SDL_TRUE;
            }
        }
    }
    SDL_UnlockJoysticks();

    return result;
}

SDL_Haptic *SDL_OpenHapticFromJoystick(SDL_Joystick *joystick)
{
    SDL_Haptic *haptic;
    SDL_Haptic *hapticlist;

    SDL_LockJoysticks();
    {
        /* Must be a valid joystick */
        if (!SDL_IsJoystickValid(joystick)) {
            SDL_SetError("Haptic: Joystick isn't valid.");
            SDL_UnlockJoysticks();
            return NULL;
        }

        /* Joystick must be haptic */
        if (SDL_IsGamepad(SDL_GetJoystickInstanceID(joystick)) ||
            SDL_SYS_JoystickIsHaptic(joystick) <= 0) {
            SDL_SetError("Haptic: Joystick isn't a haptic device.");
            SDL_UnlockJoysticks();
            return NULL;
        }

        hapticlist = SDL_haptics;
        /* Check to see if joystick's haptic is already open */
        while (hapticlist) {
            if (SDL_SYS_JoystickSameHaptic(hapticlist, joystick)) {
                haptic = hapticlist;
                ++haptic->ref_count;
                SDL_UnlockJoysticks();
                return haptic;
            }
            hapticlist = hapticlist->next;
        }

        /* Create the haptic device */
        haptic = (SDL_Haptic *)SDL_calloc(1, sizeof(*haptic));
        if (!haptic) {
            SDL_UnlockJoysticks();
            return NULL;
        }

        /* Initialize the haptic device
         * This function should fill in the instance ID and name.
         */
        haptic->rumble_id = -1;
        if (SDL_SYS_HapticOpenFromJoystick(haptic, joystick) < 0) {
            SDL_SetError("Haptic: SDL_SYS_HapticOpenFromJoystick failed.");
            SDL_free(haptic);
            SDL_UnlockJoysticks();
            return NULL;
        }
        SDL_assert(haptic->instance_id != 0);
    }
    SDL_UnlockJoysticks();

    /* Add haptic to list */
    ++haptic->ref_count;
    /* Link the haptic in the list */
    haptic->next = SDL_haptics;
    SDL_haptics = haptic;

    return haptic;
}

void SDL_CloseHaptic(SDL_Haptic *haptic)
{
    int i;
    SDL_Haptic *hapticlist;
    SDL_Haptic *hapticlistprev;

    CHECK_HAPTIC_MAGIC(haptic,);

    /* Check if it's still in use */
    if (--haptic->ref_count > 0) {
        return;
    }

    /* Close it, properly removing effects if needed */
    for (i = 0; i < haptic->neffects; i++) {
        if (haptic->effects[i].hweffect != NULL) {
            SDL_DestroyHapticEffect(haptic, i);
        }
    }
    SDL_SYS_HapticClose(haptic);
    haptic->magic = NULL;

    /* Remove from the list */
    hapticlist = SDL_haptics;
    hapticlistprev = NULL;
    while (hapticlist) {
        if (haptic == hapticlist) {
            if (hapticlistprev) {
                /* unlink this entry */
                hapticlistprev->next = hapticlist->next;
            } else {
                SDL_haptics = haptic->next;
            }

            break;
        }
        hapticlistprev = hapticlist;
        hapticlist = hapticlist->next;
    }

    /* Free the data associated with this device */
    SDL_free(haptic->name);
    SDL_free(haptic);
}

void SDL_QuitHaptics(void)
{
    while (SDL_haptics) {
        SDL_CloseHaptic(SDL_haptics);
    }

    SDL_SYS_HapticQuit();
}

int SDL_GetMaxHapticEffects(SDL_Haptic *haptic)
{
    CHECK_HAPTIC_MAGIC(haptic, -1);

    return haptic->neffects;
}

int SDL_GetMaxHapticEffectsPlaying(SDL_Haptic *haptic)
{
    CHECK_HAPTIC_MAGIC(haptic, -1);

    return haptic->nplaying;
}

Uint32 SDL_GetHapticFeatures(SDL_Haptic *haptic)
{
    CHECK_HAPTIC_MAGIC(haptic, 0);

    return haptic->supported;
}

int SDL_GetNumHapticAxes(SDL_Haptic *haptic)
{
    CHECK_HAPTIC_MAGIC(haptic, -1);

    return haptic->naxes;
}

SDL_bool SDL_HapticEffectSupported(SDL_Haptic *haptic, const SDL_HapticEffect *effect)
{
    CHECK_HAPTIC_MAGIC(haptic, SDL_FALSE);

    if (!effect) {
        return SDL_FALSE;
    }

    if ((haptic->supported & effect->type) != 0) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

int SDL_CreateHapticEffect(SDL_Haptic *haptic, const SDL_HapticEffect *effect)
{
    int i;

    CHECK_HAPTIC_MAGIC(haptic, -1);

    if (!effect) {
        return SDL_InvalidParamError("effect");
    }

    /* Check to see if effect is supported */
    if (SDL_HapticEffectSupported(haptic, effect) == SDL_FALSE) {
        return SDL_SetError("Haptic: Effect not supported by haptic device.");
    }

    /* See if there's a free slot */
    for (i = 0; i < haptic->neffects; i++) {
        if (haptic->effects[i].hweffect == NULL) {

            /* Now let the backend create the real effect */
            if (SDL_SYS_HapticNewEffect(haptic, &haptic->effects[i], effect) != 0) {
                return -1; /* Backend failed to create effect */
            }

            SDL_memcpy(&haptic->effects[i].effect, effect,
                       sizeof(SDL_HapticEffect));
            return i;
        }
    }

    return SDL_SetError("Haptic: Device has no free space left.");
}

static int ValidEffect(SDL_Haptic *haptic, int effect)
{
    if ((effect < 0) || (effect >= haptic->neffects)) {
        SDL_SetError("Haptic: Invalid effect identifier.");
        return 0;
    }
    return 1;
}

int SDL_UpdateHapticEffect(SDL_Haptic *haptic, int effect, const SDL_HapticEffect *data)
{
    CHECK_HAPTIC_MAGIC(haptic, -1);

    if (!ValidEffect(haptic, effect)) {
        return -1;
    }

    if (!data) {
        return SDL_InvalidParamError("data");
    }

    /* Can't change type dynamically. */
    if (data->type != haptic->effects[effect].effect.type) {
        return SDL_SetError("Haptic: Updating effect type is illegal.");
    }

    /* Updates the effect */
    if (SDL_SYS_HapticUpdateEffect(haptic, &haptic->effects[effect], data) <
        0) {
        return -1;
    }

    SDL_memcpy(&haptic->effects[effect].effect, data,
               sizeof(SDL_HapticEffect));
    return 0;
}

int SDL_RunHapticEffect(SDL_Haptic *haptic, int effect, Uint32 iterations)
{
    CHECK_HAPTIC_MAGIC(haptic, -1);

    if (!ValidEffect(haptic, effect)) {
        return -1;
    }

    /* Run the effect */
    if (SDL_SYS_HapticRunEffect(haptic, &haptic->effects[effect], iterations) < 0) {
        return -1;
    }

    return 0;
}

int SDL_StopHapticEffect(SDL_Haptic *haptic, int effect)
{
    CHECK_HAPTIC_MAGIC(haptic, -1);

    if (!ValidEffect(haptic, effect)) {
        return -1;
    }

    /* Stop the effect */
    if (SDL_SYS_HapticStopEffect(haptic, &haptic->effects[effect]) < 0) {
        return -1;
    }

    return 0;
}

void SDL_DestroyHapticEffect(SDL_Haptic *haptic, int effect)
{
    CHECK_HAPTIC_MAGIC(haptic,);

    if (!ValidEffect(haptic, effect)) {
        return;
    }

    /* Not allocated */
    if (haptic->effects[effect].hweffect == NULL) {
        return;
    }

    SDL_SYS_HapticDestroyEffect(haptic, &haptic->effects[effect]);
}

int SDL_GetHapticEffectStatus(SDL_Haptic *haptic, int effect)
{
    CHECK_HAPTIC_MAGIC(haptic, -1);

    if (!ValidEffect(haptic, effect)) {
        return -1;
    }

    if (!(haptic->supported & SDL_HAPTIC_STATUS)) {
        return SDL_SetError("Haptic: Device does not support status queries.");
    }

    return SDL_SYS_HapticGetEffectStatus(haptic, &haptic->effects[effect]);
}

int SDL_SetHapticGain(SDL_Haptic *haptic, int gain)
{
    const char *env;
    int real_gain, max_gain;

    CHECK_HAPTIC_MAGIC(haptic, -1);

    if (!(haptic->supported & SDL_HAPTIC_GAIN)) {
        return SDL_SetError("Haptic: Device does not support setting gain.");
    }

    if ((gain < 0) || (gain > 100)) {
        return SDL_SetError("Haptic: Gain must be between 0 and 100.");
    }

    /* We use the envvar to get the maximum gain. */
    env = SDL_getenv("SDL_HAPTIC_GAIN_MAX");
    if (env) {
        max_gain = SDL_atoi(env);

        /* Check for sanity. */
        if (max_gain < 0) {
            max_gain = 0;
        } else if (max_gain > 100) {
            max_gain = 100;
        }

        /* We'll scale it linearly with SDL_HAPTIC_GAIN_MAX */
        real_gain = (gain * max_gain) / 100;
    } else {
        real_gain = gain;
    }

    if (SDL_SYS_HapticSetGain(haptic, real_gain) < 0) {
        return -1;
    }

    return 0;
}

int SDL_SetHapticAutocenter(SDL_Haptic *haptic, int autocenter)
{
    CHECK_HAPTIC_MAGIC(haptic, -1);

    if (!(haptic->supported & SDL_HAPTIC_AUTOCENTER)) {
        return SDL_SetError("Haptic: Device does not support setting autocenter.");
    }

    if ((autocenter < 0) || (autocenter > 100)) {
        return SDL_SetError("Haptic: Autocenter must be between 0 and 100.");
    }

    if (SDL_SYS_HapticSetAutocenter(haptic, autocenter) < 0) {
        return -1;
    }

    return 0;
}

int SDL_PauseHaptic(SDL_Haptic *haptic)
{
    CHECK_HAPTIC_MAGIC(haptic, -1);

    if (!(haptic->supported & SDL_HAPTIC_PAUSE)) {
        return SDL_SetError("Haptic: Device does not support setting pausing.");
    }

    return SDL_SYS_HapticPause(haptic);
}

int SDL_ResumeHaptic(SDL_Haptic *haptic)
{
    CHECK_HAPTIC_MAGIC(haptic, -1);

    if (!(haptic->supported & SDL_HAPTIC_PAUSE)) {
        return 0; /* Not going to be paused, so we pretend it's unpaused. */
    }

    return SDL_SYS_HapticUnpause(haptic);
}

int SDL_StopHapticEffects(SDL_Haptic *haptic)
{
    CHECK_HAPTIC_MAGIC(haptic, -1);

    return SDL_SYS_HapticStopAll(haptic);
}

SDL_bool SDL_HapticRumbleSupported(SDL_Haptic *haptic)
{
    CHECK_HAPTIC_MAGIC(haptic, SDL_FALSE);

    /* Most things can use SINE, but XInput only has LEFTRIGHT. */
    return (haptic->supported & (SDL_HAPTIC_SINE | SDL_HAPTIC_LEFTRIGHT)) != 0;
}

int SDL_InitHapticRumble(SDL_Haptic *haptic)
{
    SDL_HapticEffect *efx = &haptic->rumble_effect;

    CHECK_HAPTIC_MAGIC(haptic, -1);

    /* Already allocated. */
    if (haptic->rumble_id >= 0) {
        return 0;
    }

    SDL_zerop(efx);
    if (haptic->supported & SDL_HAPTIC_SINE) {
        efx->type = SDL_HAPTIC_SINE;
        efx->periodic.direction.type = SDL_HAPTIC_CARTESIAN;
        efx->periodic.period = 1000;
        efx->periodic.magnitude = 0x4000;
        efx->periodic.length = 5000;
        efx->periodic.attack_length = 0;
        efx->periodic.fade_length = 0;
    } else if (haptic->supported & SDL_HAPTIC_LEFTRIGHT) { /* XInput? */
        efx->type = SDL_HAPTIC_LEFTRIGHT;
        efx->leftright.length = 5000;
        efx->leftright.large_magnitude = 0x4000;
        efx->leftright.small_magnitude = 0x4000;
    } else {
        return SDL_SetError("Device doesn't support rumble");
    }

    haptic->rumble_id = SDL_CreateHapticEffect(haptic, &haptic->rumble_effect);
    if (haptic->rumble_id >= 0) {
        return 0;
    }
    return -1;
}

int SDL_PlayHapticRumble(SDL_Haptic *haptic, float strength, Uint32 length)
{
    SDL_HapticEffect *efx;
    Sint16 magnitude;

    CHECK_HAPTIC_MAGIC(haptic, -1);

    if (haptic->rumble_id < 0) {
        return SDL_SetError("Haptic: Rumble effect not initialized on haptic device");
    }

    /* Clamp strength. */
    if (strength > 1.0f) {
        strength = 1.0f;
    } else if (strength < 0.0f) {
        strength = 0.0f;
    }
    magnitude = (Sint16)(32767.0f * strength);

    efx = &haptic->rumble_effect;
    if (efx->type == SDL_HAPTIC_SINE) {
        efx->periodic.magnitude = magnitude;
        efx->periodic.length = length;
    } else if (efx->type == SDL_HAPTIC_LEFTRIGHT) {
        efx->leftright.small_magnitude = efx->leftright.large_magnitude = magnitude;
        efx->leftright.length = length;
    } else {
        SDL_assert(!"This should have been caught elsewhere");
    }

    if (SDL_UpdateHapticEffect(haptic, haptic->rumble_id, &haptic->rumble_effect) < 0) {
        return -1;
    }

    return SDL_RunHapticEffect(haptic, haptic->rumble_id, 1);
}

int SDL_StopHapticRumble(SDL_Haptic *haptic)
{
    CHECK_HAPTIC_MAGIC(haptic, -1);

    if (haptic->rumble_id < 0) {
        return SDL_SetError("Haptic: Rumble effect not initialized on haptic device");
    }

    return SDL_StopHapticEffect(haptic, haptic->rumble_id);
}
