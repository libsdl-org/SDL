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

/* Most platforms that use/need SDL_main have their own SDL_RunApp() implementation.
 * If not, you can special case it here by appending || defined(__YOUR_PLATFORM__) */
#if ( !defined(SDL_MAIN_NEEDED) && !defined(SDL_MAIN_AVAILABLE) ) || defined(__ANDROID__)

DECLSPEC int
SDL_RunApp(int argc, char* argv[], SDL_main_func mainFunction, void * reserved)
{
    char empty[1] = {0};
    char* argvdummy[2] = { empty, NULL };

    (void)reserved;

    if(argv == NULL)
    {
        argc = 0;
        /* make sure argv isn't NULL, in case some user code doesn't like that */
        argv = argvdummy;
    }

    return mainFunction(argc, argv);
}

#endif
