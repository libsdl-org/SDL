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
#include "SDL_actionset_c.h"
#include "SDL_sysactionset.h"

/* TODO: Copy over from Codename GigaBrain */

/* Providers should be prioritized as follows:
 * Private Launchers (i.e. EA) > Shared Stores (i.e. Steam, Itch) > OS > SDL
 */
static const SDL_ActionSetProvider *const providers[] = {
#ifdef SDL_ACTIONSET_STEAMINPUT
    &STEAMINPUT_provider,
#endif
    &SDLINPUT_provider /* Unlike other subsystems, SDL is always available, no dummy drivers needed */
};

int SDL_InitActionSet(void)
{
    int i;
    ActionSetDeviceMask current_mask = ACTIONSET_DEVICE_NONE;
    for (i = 0; i < SDL_arraysize(providers); i += 1) {
        current_mask |= providers[i]->Init(current_mask);
    }
    return 0;
}

void SDL_QuitActionSet(void)
{
    int i;
    /* TODO: Close ActionSetDevice array here */
    for (i = 0; i < SDL_arraysize(providers); i += 1) {
        providers[i]->Quit();
    }
}

SDL_bool SDL_ActionSetsOpened(void)
{
    return SDL_FALSE;
}

void SDL_UpdateActionSet(void)
{
    int i;
    for (i = 0; i < SDL_arraysize(providers); i += 1) {
        providers[i]->Update();
    }
}
