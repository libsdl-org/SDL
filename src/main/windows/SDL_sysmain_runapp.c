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

#ifdef SDL_PLATFORM_WIN32

#include "../../core/windows/SDL_windows.h"

/* Win32-specific SDL_RunApp(), which does most of the SDL_main work,
  based on SDL_windows_main.c, placed in the public domain by Sam Lantinga  4/13/98 */

#include <shellapi.h> // CommandLineToArgvW()

static int OutOfMemory(void)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Out of memory - aborting", NULL);
    return -1;
}

static int ErrorProcessingCommandLine(void)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Error processing command line arguments - aborting", NULL);
    return -1;
}

int MINGW32_FORCEALIGN SDL_RunApp(int caller_argc, char *caller_argv[], SDL_main_func mainFunction, void * reserved)
{
    int result;
    (void)reserved;

    // If the provided argv is valid, we pass it to the main function as-is, since it's probably what the user wants.
    // Otherwise, we take a NULL argv as an instruction for SDL to parse the command line into an argv.
    // On Windows, when SDL provides the main entry point, argv is always NULL.
    if (caller_argv && caller_argc >= 0) {
        result = mainFunction(caller_argc, caller_argv);
    } else {
        // We need to be careful about how we allocate/free memory here. We can't use SDL_alloc()/SDL_free()
        // because the application might have used SDL_SetMemoryFunctions() to change the allocator.
        LPWSTR *argvw = NULL;
        char **argv = NULL;

        const LPWSTR command_line = GetCommandLineW();

        // Because of how the Windows command line is structured, we know for sure that the buffer size required to
        // store all argument strings converted to UTF-8 (with null terminators) is guaranteed to be less than or equal
        // to the size of the original command line string converted to UTF-8.
        const int argdata_size = WideCharToMultiByte(CP_UTF8, 0, command_line, -1, NULL, 0, NULL, NULL); // Includes the null terminator
        if (!argdata_size) {
            result = ErrorProcessingCommandLine();
            goto cleanup;
        }

        int argc;
        argvw = CommandLineToArgvW(command_line, &argc);
        if (!argvw || argc < 0) {
            result = OutOfMemory();
            goto cleanup;
        }

        // Allocate argv followed by the argument string buffer as one contiguous allocation.
        argv = (char **)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (argc + 1) * sizeof(*argv) + argdata_size);
        if (!argv) {
            result = OutOfMemory();
            goto cleanup;
        }
        char *argdata = ((char *)argv) + (argc + 1) * sizeof(*argv);
        int argdata_index = 0;

        for (int i = 0; i < argc; ++i) {
            const int bytes_written = WideCharToMultiByte(CP_UTF8, 0, argvw[i], -1, argdata + argdata_index, argdata_size - argdata_index, NULL, NULL);
            if (!bytes_written) {
                result = ErrorProcessingCommandLine();
                goto cleanup;
            }
            argv[i] = argdata + argdata_index;
            argdata_index += bytes_written;
        }
        argv[argc] = NULL;

        SDL_SetMainReady();
        result = mainFunction(argc, argv);

    cleanup:
        HeapFree(GetProcessHeap(), 0, argv);
        LocalFree(argvw);
    }

    return result;
}

#endif // SDL_PLATFORM_WIN32
