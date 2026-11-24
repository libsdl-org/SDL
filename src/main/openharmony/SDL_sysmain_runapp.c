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

#include <hilog/log.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "../SDL_main_callbacks.h"

static int SDL_main_argc = 0;
static char **SDL_main_argv = NULL;
static void *ThreadEntry__SDL_main(void *userdata)
{
    SDL_main_func pMain = (SDL_main_func) userdata;
    pthread_setname_np(pthread_self(), "SDL_main");
    OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "Calling SDL_main!");
    const int rc = pMain(SDL_main_argc, SDL_main_argv);
    OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "SDL_main returned %{public}d! Now terminating the process...", rc);
    exit(rc);
    return NULL;
}

// This is passed to SDL_RunApp. It decides whether libmain.so has the main callbacks or SDL_main,
//  and either calls SDL_EnterAppMainCallbacks or spins a thread and attempts to work with SDL_main().
// This will log something and attempt to terminate the process if things go wrong.
static int RunAppOpenHarmonyMain(int argc, char **argv)
{
    // don't call SDL_Log() in here, we aren't necessarily set up yet! Use hilog directly!!

    const char *sofile = "libmain.so";
    void *lib = dlopen(sofile, RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "Failed loading %{public}s, can't start app! (%{public}s)", sofile, dlerror());
        exit(1);
    }

    SDL_main_func pMain = (SDL_main_func) dlsym(lib, "SDL_main");
    if (pMain) {
        SDL_main_argc = argc;
        SDL_main_argv = argv;
        pthread_t thread;
        const int rc = pthread_create(&thread, NULL, ThreadEntry__SDL_main, pMain);
        if (rc != 0) {
            OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "Failed to create thread to run SDL_main from %{public}s, can't start app! (%{public}s)", sofile, strerror(rc));
            exit(1);
        }

        pthread_detach(thread);  // we won't be waiting on this.
        return 0;  // SDL_main is running in another thread, and this thread returns to the app's core loop thing.
    }

    #define FINDCBFN(nam) \
        SDL_App##nam##_func pApp##nam = (SDL_App##nam##_func) dlsym(lib, "SDL_App" #nam); \
        if (!pApp##nam) { \
            OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "Couldn't find %{public}s in %{public}s, can't start app! (%{public}s)", "SDL_App" #nam, sofile, dlerror()); \
            exit(1); \
        }
    FINDCBFN(Init);
    FINDCBFN(Iterate);
    FINDCBFN(Event);
    FINDCBFN(Quit);
    #undef FINDCBFN

    // this returns after SDL_AppInit, and we'll deal with SDL_AppEvent/SDL_AppIterate in SDL_sysmain_callbacks.c, when we get OnFrame events from SDL's XComponent.
    return SDL_EnterAppMainCallbacks(argc, argv, pAppInit, pAppIterate, pAppEvent, pAppQuit);
}

void SDL_OpenHarmonyMainSurfaceCreated(void)  // src/core/openharmony calls this when our XComponent is ready to go, and we use this to get the actual native app code booted.
{
    static bool booted = false;
    if (!booted) {  // just in case we make more XComponents later...?
        booted = true;
        SDL_RunApp(0, NULL, RunAppOpenHarmonyMain, NULL);
    }
}

#endif

