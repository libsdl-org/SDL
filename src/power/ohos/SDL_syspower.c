/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_POWER_DISABLED
#ifdef SDL_POWER_OHOS

#include "../SDL_syspower.h"

#include "../../core/ohos/SDL_ohos.h"

bool SDL_GetPowerInfo_OHOS(SDL_PowerState *state, int *seconds, int *percent)
{
    if (!OHOS_IsBatteryPresent()) {
        *state = SDL_POWERSTATE_NO_BATTERY;
        return true;
    }
    else {
        *seconds = -1;
        *percent = OHOS_GetBatteryPercent();
        
        *state = OHOS_IsBatteryCharging() ? SDL_POWERSTATE_CHARGING : (OHOS_IsBatteryCharged() ? SDL_POWERSTATE_CHARGED : SDL_POWERSTATE_ON_BATTERY);
    }
    return true;
}

#endif // SDL_POWER_OHOS
#endif // SDL_POWER_DISABLED
