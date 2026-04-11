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

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <lv2/process.h>
#include <sysutil/sysutil.h>

extern SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]);
extern SDL_AppResult SDL_AppIterate(void *appstate);
extern SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
extern void SDL_AppQuit(void *appstate, SDL_AppResult result);

static volatile bool quit_requested = false;

static void program_exit_callback()
{
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
}

static void sysutil_exit_callback(u64 status, u64 param, void *usrdata)
{
    if (status == SYSUTIL_EXIT_GAME) {
        quit_requested = true;
    }
}

#ifdef __cplusplus
}
#endif

int SDL_RunApp(int argc, char *argv[], SDL_main_func mainFunction, void * reserved)
{
    atexit(program_exit_callback);
    // Register XMB callbacks.
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_exit_callback, NULL);

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
        sysUtilCheckCallback();

        result = SDL_IterateMainCallbacks(true);

        if (quit_requested) {
            result = SDL_APP_SUCCESS;
        }
    }
    SDL_QuitMainCallbacks(result);

    sysProcessExit(0);

    return result == SDL_APP_SUCCESS ? 0 : 1;
}

#endif // SDL_PLATFORM_PS3
