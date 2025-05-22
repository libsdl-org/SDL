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
#include "SDL_runapp.h"

// Most platforms that use/need SDL_main have their own SDL_RunApp() implementations.
// If not, you can special case it here by appending '|| defined(__YOUR_PLATFORM__)'.
#if ( !defined(SDL_MAIN_NEEDED) && !defined(SDL_MAIN_AVAILABLE) ) || defined(SDL_PLATFORM_ANDROID)

int SDL_RunApp(int argc, char* argv[], SDL_main_func mainFunction, void * reserved)
{
    (void)reserved;

    return SDL_CallMain(argc, argv, mainFunction);
}

#endif

// Most platforms receive a standard argv via their native entry points
// and don't provide any other (reliable) ways of getting the command-line arguments.
// For those platforms, we only try to ensure that the argv is not NULL
// (which it might be if the user calls SDL_RunApp() directly instead of using SDL_main).
// Platforms that don't use standard argc/argv entry points but provide other ways of
// getting the command-line arguments have their own SDL_CallMain() implementations.
#ifndef SDL_PLATFORM_WINDOWS

int SDL_CallMain(int argc, char* argv[], SDL_main_func mainFunction)
{
    char dummyargv0[] = { 'S', 'D', 'L', '_', 'a', 'p', 'p', '\0' };
    char *dummyargv[2] = { dummyargv0, NULL };

    if (!argv || argc < 0) {
        argc = 1;
        argv = dummyargv;
    }

    return mainFunction(argc, argv);
}

#endif
