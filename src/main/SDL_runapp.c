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
#include "SDL_main_callbacks.h"

// Add your platform here if you define a custom SDL_RunApp() implementation
#if !defined(SDL_PLATFORM_WIN32) && \
    !defined(SDL_PLATFORM_GDK) && \
    !defined(SDL_PLATFORM_IOS) && \
    !defined(SDL_PLATFORM_TVOS) && \
    !defined(SDL_PLATFORM_EMSCRIPTEN) && \
    !defined(SDL_PLATFORM_PSP) && \
    !defined(SDL_PLATFORM_PS2) && \
    !defined(SDL_PLATFORM_3DS)

int SDL_RunApp(int argc, char *argv[], SDL_main_func mainFunction, void * reserved)
{
    (void)reserved;
    return SDL_CallMainFunction(argc, argv, mainFunction);
}

#endif
