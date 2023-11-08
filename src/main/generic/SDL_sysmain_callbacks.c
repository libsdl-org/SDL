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
#include "../SDL_main_callbacks.h"
#include "../../video/SDL_sysvideo.h"

#ifndef __IOS__

static int callback_rate_increment = 0;

static void SDLCALL MainCallbackRateHintChanged(void *userdata, const char *name, const char *oldValue, const char *newValue)
{
    const int callback_rate = newValue ? SDL_atoi(newValue) : 60;
    if (callback_rate > 0) {
        callback_rate_increment = ((Uint64) 1000000000) / ((Uint64) callback_rate);
    } else {
        callback_rate_increment = 0;
    }
}

int SDL_EnterAppMainCallbacks(int argc, char* argv[], SDL_AppInit_func appinit, SDL_AppIterate_func appiter, SDL_AppEvent_func appevent, SDL_AppQuit_func appquit)
{
    int rc = SDL_InitMainCallbacks(argc, argv, appinit, appiter, appevent, appquit);
    if (rc == 0) {
        SDL_AddHintCallback(SDL_HINT_MAIN_CALLBACK_RATE, MainCallbackRateHintChanged, NULL);

        Uint64 next_iteration = callback_rate_increment ? (SDL_GetTicksNS() + callback_rate_increment) : 0;

        while ((rc = SDL_IterateMainCallbacks(SDL_TRUE)) == 0) {
            // !!! FIXME: this can be made more complicated if we decide to
            // !!! FIXME: optionally hand off callback responsibility to the
            // !!! FIXME: video subsystem (for example, if Wayland has a
            // !!! FIXME: protocol to drive an animation loop, maybe we hand
            // !!! FIXME: off to them here if/when the video subsystem becomes
            // !!! FIXME: initialized).

            // !!! FIXME: maybe respect this hint even if there _is_ a window.
            // if there's no window, try to run at about 60fps (or whatever rate
            //  the hint requested). This makes this not eat all the CPU in
            //  simple things like loopwave. If there's a window, we run as fast
            //  as possible, which means we'll clamp to vsync in common cases,
            //  and won't be restrained to vsync if the app is doing a benchmark
            //  or doesn't want to be, based on how they've set up that window.
            if ((callback_rate_increment == 0) || SDL_HasWindows()) {
                next_iteration = 0; // just clear the timer and run at the pace the video subsystem allows.
            } else {
                const Uint64 now = SDL_GetTicksNS();
                if (next_iteration > now) { // Running faster than the limit, sleep a little.
                    SDL_DelayNS(next_iteration - now);
                } else {
                    next_iteration = now; // running behind (or just lost the window)...reset the timer.
                }
                next_iteration += callback_rate_increment;
            }
        }

        SDL_DelHintCallback(SDL_HINT_MAIN_CALLBACK_RATE, MainCallbackRateHintChanged, NULL);
    }
    SDL_QuitMainCallbacks();

    return (rc < 0) ? 1 : 0;
}

#endif // !__IOS__
