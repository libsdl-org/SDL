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

#ifdef SDL_PLATFORM_WIN32

#include "../../core/windows/SDL_windows.h"
#include "../SDL_main_callbacks.h"

/* Win32-specific SDL_RunApp(), which does most of the SDL_main work,
  based on SDL_windows_main.c, placed in the public domain by Sam Lantinga  4/13/98 */

int MINGW32_FORCEALIGN SDL_RunApp(int argc, char *argv[], SDL_main_func mainFunction, void *reserved)
{
    (void)reserved;

    int result = -1;
    void *heap_allocated = NULL;
    const char *args_error = WIN_CheckDefaultArgcArgv(&argc, &argv, &heap_allocated);
    if (args_error) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", args_error, NULL);
    } else {
        result = SDL_CallMainFunction(argc, argv, mainFunction);
        if (heap_allocated) {
            HeapFree(GetProcessHeap(), 0, heap_allocated);
        }
    }
    return result;
}

#endif // SDL_PLATFORM_WIN32
