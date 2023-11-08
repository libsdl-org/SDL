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
#include "SDL_main_callbacks.h"

static SDL_AppEvent_func SDL_main_event_callback;
static SDL_AppIterate_func SDL_main_iteration_callback;
static SDL_AppQuit_func SDL_main_quit_callback;
static SDL_AtomicInt apprc;  // use an atomic, since events might land from any thread and we don't want to wrap this all in a mutex. A CAS makes sure we only move from zero once.

// Return true if this event needs to be processed before returning from the event watcher
static SDL_bool ShouldDispatchImmediately(SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_TERMINATING:
    case SDL_EVENT_LOW_MEMORY:
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
    case SDL_EVENT_DID_ENTER_BACKGROUND:
    case SDL_EVENT_WILL_ENTER_FOREGROUND:
    case SDL_EVENT_DID_ENTER_FOREGROUND:
        return SDL_TRUE;
    default:
        return SDL_FALSE;
    }
}

static void SDL_DispatchMainCallbackEvent(SDL_Event *event)
{
    if (SDL_AtomicGet(&apprc) == 0) { // if already quitting, don't send the event to the app.
        SDL_AtomicCAS(&apprc, 0, SDL_main_event_callback(event));
    }
}

static void SDL_DispatchMainCallbackEvents()
{
    SDL_Event events[16];

    for (;;) {
        int count = SDL_PeepEvents(events, SDL_arraysize(events), SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
        if (count <= 0) {
            break;
        }
        for (int i = 0; i < count; ++i) {
            SDL_Event *event = &events[i];
            if (!ShouldDispatchImmediately(event)) {
                SDL_DispatchMainCallbackEvent(event);
            }
        }
    }
}

static int SDLCALL SDL_MainCallbackEventWatcher(void *userdata, SDL_Event *event)
{
    if (ShouldDispatchImmediately(event)) {
        // Make sure any currently queued events are processed then dispatch this before continuing
        SDL_DispatchMainCallbackEvents();
        SDL_DispatchMainCallbackEvent(event);
    } else {
        // We'll process this event later from the main event queue
    }
    return 0;
}

SDL_bool SDL_HasMainCallbacks()
{
    if (SDL_main_iteration_callback) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

int SDL_InitMainCallbacks(int argc, char* argv[], SDL_AppInit_func appinit, SDL_AppIterate_func appiter, SDL_AppEvent_func appevent, SDL_AppQuit_func appquit)
{
    SDL_main_iteration_callback = appiter;
    SDL_main_event_callback = appevent;
    SDL_main_quit_callback = appquit;
    SDL_AtomicSet(&apprc, 0);

    const int rc = appinit(argc, argv);
    if (SDL_AtomicCAS(&apprc, 0, rc) && (rc == 0)) {  // bounce if SDL_AppInit already said abort, otherwise...
        // make sure we definitely have events initialized, even if the app didn't do it.
        if (SDL_InitSubSystem(SDL_INIT_EVENTS) == -1) {
            SDL_AtomicSet(&apprc, -1);
            return -1;
        }

        if (SDL_AddEventWatch(SDL_MainCallbackEventWatcher, NULL) < 0) {
            SDL_AtomicSet(&apprc, -1);
            return -1;
        }
    }

    return SDL_AtomicGet(&apprc);
}

int SDL_IterateMainCallbacks(SDL_bool pump_events)
{
    if (pump_events) {
        SDL_PumpEvents();
    }
    SDL_DispatchMainCallbackEvents();

    int rc = SDL_AtomicGet(&apprc);
    if (rc == 0) {
        rc = SDL_main_iteration_callback();
        if (!SDL_AtomicCAS(&apprc, 0, rc)) {
            rc = SDL_AtomicGet(&apprc); // something else already set a quit result, keep that.
        }
    }
    return rc;
}

void SDL_QuitMainCallbacks(void)
{
    SDL_DelEventWatch(SDL_MainCallbackEventWatcher, NULL);
    SDL_main_quit_callback();

    // for symmetry, you should explicitly Quit what you Init, but we might come through here uninitialized and SDL_Quit() will clear everything anyhow.
    //SDL_QuitSubSystem(SDL_INIT_EVENTS);

    SDL_Quit();
}

