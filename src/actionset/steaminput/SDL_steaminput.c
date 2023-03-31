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
#include "../SDL_sysactionset.h"

#ifdef SDL_ACTIONSET_STEAMINPUT

#include "SDL_steaminput.h"

#ifdef __WINDOWS__
 #ifdef _WIN64
   #define STEAM_DLL "steam_api64.dll"
 #else
   #define STEAM_DLL "steam_api.dll"
 #endif
#elif defined(__MACOS__)
  #define STEAM_DLL "libsteam_api.dylib"
#else
  #define STEAM_DLL "libsteam_api.so"
#endif

static void *steam_dll = NULL;

static ActionSetDeviceMask STEAMINPUT_Init(ActionSetDeviceMask current_mask)
{
    /* Steam Input only works with game controllers */
    if (current_mask & ACTIONSET_DEVICE_CONTROLLER) {
        return current_mask;
    }

    steam_dll = SDL_LoadObject(STEAM_DLL);
    if (steam_dll == NULL) {
        SDL_SetError("Steamworks library was not found");
        return current_mask;
    }
    /* TODO: Load entry points, unload if any are missing */
    /* TODO: if (!SteamAPI_WasInit()) unload and bail */
    /* TODO: Call ISteamInput_Init(true) */
    /* TODO: Subscribe to SteamInputConfigurationLoaded_t */
    /* TODO: Subscribe to SteamInputDeviceConnected_t */
    /* TODO: Subscribe to SteamInputDeviceDisconnected_t */

    return current_mask | ACTIONSET_DEVICE_CONTROLLER;
}

static void STEAMINPUT_Quit(void)
{
    if (steam_dll == NULL) {
        return;
    }

    /* TODO: Call ISteamInput_Shutdown(), clear entry points */
    SDL_UnloadObject(steam_dll);
    steam_dll = NULL;
}

static void STEAMINPUT_Update(void)
{
    if (steam_dll == NULL) {
        return;
    }

    /* TODO: SteamAPI_ISteamInput_RunFrame() */
    /* TODO: if (!BNewDataAvailable()) return; */
    /* TODO: Poll all actions for active set. FIXME: Can we get events...? */
}

SDL_ActionSetProvider STEAMINPUT_provider = {
    STEAMINPUT_Init,
    STEAMINPUT_Quit,
    STEAMINPUT_Update
};

#endif /* SDL_ACTIONSET_STEAMINPUT */
