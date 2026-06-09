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

#ifdef SDL_PLATFORM_OPENHARMONY

#include "../SDL_main_callbacks.h"

void SDL_OpenHarmonyOnFrameCallback(void)   // src/core/openharmony calls this when our XComponent is ready for a new frame, and we call SDL_AppIterate here
{
    if (SDL_HasMainCallbacks()) {  // ignore this if we're using SDL_main().
        const SDL_AppResult rc = SDL_IterateMainCallbacks(true);
        if (rc != SDL_APP_CONTINUE) {
            SDL_Log("SDL_AppIterate said %s! Terminating!", (rc == SDL_APP_FAILURE) ? "SDL_APP_FAILURE" : "SDL_APP_SUCCESS");
            SDL_QuitMainCallbacks(rc);
            exit((rc == SDL_APP_FAILURE) ? 1 : 0);
        }
    }
}

// We hook into this deep into the actual OpenHarmony app lifecycle, see notes elsewhere. Once we return from this, we'll get OnFrame callbacks from our
//  XComponent, which we'll use for SDL_AppIterate, etc.
int SDL_EnterAppMainCallbacks(int argc, char *argv[], SDL_AppInit_func appinit, SDL_AppIterate_func appiter, SDL_AppEvent_func appevent, SDL_AppQuit_func appquit)
{
    const SDL_AppResult rc = SDL_InitMainCallbacks(argc, argv, appinit, appiter, appevent, appquit);
    if (rc == SDL_APP_CONTINUE) {
SDL_Log("SDL_AppInit said continue! We are live!");
        return 0;  // this will fall all the way out of SDL_main (or whatever), where OpenHarmony will keep running the process, passing events through ArkTS, etc...
    }

SDL_Log("SDL_AppInit said %s! Terminating!", (rc == SDL_APP_FAILURE) ? "SDL_APP_FAILURE" : "SDL_APP_SUCCESS");

    // appinit requested quit, just bounce out now.
    SDL_QuitMainCallbacks(rc);
    exit((rc == SDL_APP_FAILURE) ? 1 : 0);

    return 1;  // just in case.
}

#endif

