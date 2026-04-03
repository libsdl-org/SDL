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

#ifdef SDL_PLATFORM_PS3

// SDL_RunApp() code for PS3.

#ifdef __cplusplus
extern "C" {
#endif

#include "../SDL_main_callbacks.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

extern SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]);
extern SDL_AppResult SDL_AppIterate(void *appstate);
extern SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
extern void SDL_AppQuit(void *appstate, SDL_AppResult result);

#ifdef __cplusplus
}
#endif

int SDL_RunApp(int argc, char *argv[], SDL_main_func mainFunction, void * reserved)
{
    SDL_SetMainReady();

    // Call callbacks directly — no mainFunction involved to
    // forces correct OPD lookup.
    SDL_AppResult result = SDL_InitMainCallbacks(argc, argv,
        &SDL_AppInit,
        &SDL_AppIterate,
        &SDL_AppEvent,
        &SDL_AppQuit
    );

    while (result == SDL_APP_CONTINUE) {
        result = SDL_IterateMainCallbacks(true);
    }

    SDL_QuitMainCallbacks(result);

    return result == SDL_APP_SUCCESS ? 0 : 1;
}

#endif // SDL_PLATFORM_PS3
