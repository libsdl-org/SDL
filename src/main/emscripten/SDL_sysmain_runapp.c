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

#ifdef SDL_PLATFORM_EMSCRIPTEN

#include "../SDL_main_callbacks.h"

#include <emscripten/emscripten.h>

EM_JS_DEPS(sdlrunapp, "$dynCall,$stringToNewUTF8");

EMSCRIPTEN_KEEPALIVE int CallSDLEmscriptenMainFunction(int argc, char *argv[], SDL_main_func mainFunction)
{
    return SDL_CallMainFunction(argc, argv, mainFunction);
}

int SDL_RunApp(int argc, char *argv[], SDL_main_func mainFunction, void * reserved)
{
    (void)reserved;

    // Move any URL params that start with "SDL_" over to environment
    //  variables, so the hint system can pick them up, etc, much like a user
    //  can set them from a shell prompt on a desktop machine. Ignore all
    //  other params, in case the app wants to use them for something.
    MAIN_THREAD_EM_ASM({
        var parms = new URLSearchParams(window.location.search);
        for (const [key, value] of parms) {
            if (key.startsWith("SDL_")) {
                var ckey = stringToNewUTF8(key);
                var cvalue = stringToNewUTF8(value);
                if ((ckey != 0) && (cvalue != 0)) {
                    //console.log("Setting SDL env var '" + key + "' to '" + value + "' ...");
                    dynCall('iiii', $0, [ckey, cvalue, 1]);
                }
                _Emscripten_force_free(ckey);  // these must use free(), not SDL_free()!
                _Emscripten_force_free(cvalue);
            }
        }
    }, SDL_setenv_unsafe);

    #ifdef SDL_EMSCRIPTEN_PERSISTENT_PATH_STRING
    MAIN_THREAD_EM_ASM({
        const persistent_path = UTF8ToString($0);
        const argc = $1;
        const argv = $2;
        const mainFunction = $3;
        //console.log("SDL is automounting persistent storage to '" + persistent_path + "' ...please wait.");
        FS.mkdirTree(persistent_path);
        FS.mount(IDBFS, { autoPersist: true }, persistent_path);
        FS.syncfs(true, function(err) {
            if (err) {
                console.error(`WARNING: Failed to populate persistent store at '${persistent_path}' (${err.name}: ${err.message}). Save games likely lost?`);
            }
            _CallSDLEmscriptenMainFunction(argc, argv, mainFunction);   // error or not, start the actual SDL_main().
        });
    }, SDL_EMSCRIPTEN_PERSISTENT_PATH_STRING, argc, argv, mainFunction);

    // we need to stop running code until FS.syncfs() finishes, but we need the runtime to not clean up.
    // The actual SDL_main/SDL_AppInit() will be called when the sync is done and things will pick back up where they were.
    emscripten_exit_with_live_runtime();
    return 0;
    #else
    return CallSDLEmscriptenMainFunction(argc, argv, mainFunction);
    #endif
}

#endif
