/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#include <emscripten.h>

// For Emscripten, we let you use SDL_HINT_MAIN_CALLBACK_RATE, because it might be useful to drop it super-low for
//  things like loopwave that don't really do much but wait on the audio device, but be warned that browser timers
//  are super-unreliable in modern times, so you likely won't hit your desired callback rate with good precision.
// Almost all apps should leave this alone, so we can use requestAnimationFrame, which is intended to run reliably
//  at the refresh rate of the user's display.
static Uint32 callback_rate_increment = 0;
static bool iterate_after_waitevent = false;
static bool callback_rate_changed = false;
static void SDLCALL MainCallbackRateHintChanged(void *userdata, const char *name, const char *oldValue, const char *newValue)
{
    callback_rate_changed = true;
    iterate_after_waitevent = newValue && (SDL_strcmp(newValue, "waitevent") == 0);
    if (iterate_after_waitevent) {
        callback_rate_increment = 0;
    } else {
        const double callback_rate = newValue ? SDL_atof(newValue) : 0.0;
        if (callback_rate > 0.0) {
            callback_rate_increment = (Uint32) SDL_NS_TO_MS((double) SDL_NS_PER_SECOND / callback_rate);
        } else {
            callback_rate_increment = 0;
        }
    }
}

// just tell us when any new event is pushed on the queue, so we can check a flag for "waitevent" mode.
static bool saw_new_event = false;
static bool SDLCALL EmscriptenMainCallbackEventWatcher(void *userdata, SDL_Event *event)
{
    saw_new_event = true;
    return true;
}

static void EmscriptenInternalMainloop(void)
{
    // callback rate changed? Update emscripten's mainloop iteration speed.
    if (callback_rate_changed) {
        callback_rate_changed = false;
        if (callback_rate_increment == 0) {
            emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
        } else {
            emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, callback_rate_increment);
        }
    }

    if (iterate_after_waitevent) {
        SDL_PumpEvents();
        if (!saw_new_event) {
            // do nothing yet. Note that we're still going to iterate here because we can't block,
            // but we can stop the app's iteration from progressing until there's an event.
            return;
        }
        saw_new_event = false;
    }

    const SDL_AppResult rc = SDL_IterateMainCallbacks(!iterate_after_waitevent);
    if (rc != SDL_APP_CONTINUE) {
        SDL_QuitMainCallbacks(rc);
        emscripten_cancel_main_loop();  // kill" the mainloop, so it stops calling back into it.
        exit((rc == SDL_APP_FAILURE) ? 1 : 0);  // hopefully this takes down everything else, too.
    }
}

int SDL_EnterAppMainCallbacks(int argc, char *argv[], SDL_AppInit_func appinit, SDL_AppIterate_func appiter, SDL_AppEvent_func appevent, SDL_AppQuit_func appquit)
{
    SDL_AppResult rc = SDL_InitMainCallbacks(argc, argv, appinit, appiter, appevent, appquit);
    if (rc == SDL_APP_CONTINUE) {
        if (!SDL_AddEventWatch(EmscriptenMainCallbackEventWatcher, NULL)) {
            rc = SDL_APP_FAILURE;
        } else {
            SDL_AddHintCallback(SDL_HINT_MAIN_CALLBACK_RATE, MainCallbackRateHintChanged, NULL);
            callback_rate_changed = false;
            emscripten_set_main_loop(EmscriptenInternalMainloop, 0, 0);  // don't throw an exception since we do an orderly return.
            if (callback_rate_increment > 0.0) {
                emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, callback_rate_increment);
            }
        }
    } else {
        SDL_QuitMainCallbacks(rc);
    }
    return (rc == SDL_APP_FAILURE) ? 1 : 0;
}

