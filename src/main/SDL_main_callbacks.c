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

static SDL_AppEvent_func appevent;
static SDL_AppIterate_func appiter;
static SDL_AppQuit_func appquit;
static SDL_AtomicInt apprc;  // use an atomic, since events might land from any thread and we don't want to wrap this all in a mutex. A CAS makes sure we only move from zero once.

static int SDLCALL EventWatcher(void *userdata, SDL_Event *event)
{
    if (SDL_AtomicGet(&apprc) == 0) {  // if already quitting, don't send the event to the app.
        SDL_AtomicCAS(&apprc, 0, appevent(event));
    }
    return 0;
}

int SDL_InitMainCallbacks(int argc, char* argv[], SDL_AppInit_func appinit, SDL_AppIterate_func _appiter, SDL_AppEvent_func _appevent, SDL_AppQuit_func _appquit)
{
    appiter = _appiter;
    appevent = _appevent;
    appquit = _appquit;
    SDL_AtomicSet(&apprc, 0);

    const int rc = appinit(argc, argv);
    if (SDL_AtomicCAS(&apprc, 0, rc) && (rc == 0)) {  // bounce if SDL_AppInit already said abort, otherwise...
        // drain any initial events that might have arrived before we added a watcher.
        SDL_Event event;
        SDL_Event *pending_events = NULL;
        int total_pending_events = 0;
        while (SDL_PollEvent(&event)) {
            void *ptr = SDL_realloc(pending_events, sizeof (SDL_Event) * (total_pending_events + 1));
            if (!ptr) {
                SDL_OutOfMemory();
                SDL_free(pending_events);
                SDL_AtomicSet(&apprc, -1);
                return -1;
            }
            pending_events = (SDL_Event *) ptr;
            SDL_copyp(&pending_events[total_pending_events], &event);
            total_pending_events++;
        }

        SDL_AddEventWatch(EventWatcher, NULL);  // !!! FIXME: this should really return an error.

        for (int i = 0; i < total_pending_events; i++) {
            SDL_PushEvent(&pending_events[i]);
        }

        SDL_free(pending_events);
    }

    return SDL_AtomicGet(&apprc);
}

int SDL_IterateMainCallbacks(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // just empty the queue, EventWatcher sends the events to the app.
        switch (event.type) {
            case SDL_EVENT_DROP_FILE:
            case SDL_EVENT_DROP_TEXT:
                SDL_free(event.drop.file);
                break;
        }
    }

    int rc = appiter();
    if (!SDL_AtomicCAS(&apprc, 0, rc)) {
        rc = SDL_AtomicGet(&apprc);  // something else already set a quit result, keep that.
    }

    return rc;
}

void SDL_QuitMainCallbacks(void)
{
    SDL_DelEventWatch(EventWatcher, NULL);
    appquit();
    SDL_Quit();
}

