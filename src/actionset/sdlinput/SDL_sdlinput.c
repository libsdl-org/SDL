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
#include "SDL_sdlinput.h"

static ActionSetDeviceMask sdlinput_mask = ACTIONSET_DEVICE_NONE;

static int SDLINPUT_EventWatch(void *userdata, SDL_Event *event)
{
    /* TODO: Handle keyboard, mouse, and controller events, based on mask */
    /* FIXME: Can we get keyboard/mouse hotplugging? */
    return 0;
}

static ActionSetDeviceMask SDLINPUT_Init(ActionSetDeviceMask current_mask)
{
    if (!(current_mask & ACTIONSET_DEVICE_KEYBOARDMOUSE)) {
        sdlinput_mask |= ACTIONSET_DEVICE_KEYBOARDMOUSE;
    }
    if (!(current_mask & ACTIONSET_DEVICE_CONTROLLER)) {
        sdlinput_mask |= ACTIONSET_DEVICE_CONTROLLER;
    }
    if (!(current_mask & ACTIONSET_DEVICE_TOUCH)) {
        sdlinput_mask |= ACTIONSET_DEVICE_TOUCH;
    }

    if (sdlinput_mask == ACTIONSET_DEVICE_NONE) {
        return current_mask;
    }

    /* Do NOT open devices here! Let the event watcher do that! */
    SDL_AddEventWatch(SDLINPUT_EventWatch, NULL);

    return current_mask
         | ACTIONSET_DEVICE_KEYBOARDMOUSE
         | ACTIONSET_DEVICE_CONTROLLER
         | ACTIONSET_DEVICE_TOUCH;
}

static void SDLINPUT_Quit(void)
{
    SDL_DelEventWatch(SDLINPUT_EventWatch, NULL);
    sdlinput_mask = ACTIONSET_DEVICE_NONE;
}

static void SDLINPUT_Update(void)
{
    /* Nothing to do, the event watch handles everything for us */
}

SDL_ActionSetProvider SDLINPUT_provider = {
    SDLINPUT_Init,
    SDLINPUT_Quit,
    SDLINPUT_Update
};
