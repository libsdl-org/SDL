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

#ifdef SDL_PLATFORM_WINDOWS

#include "../SDL_runapp.h"
#include "../../core/windows/SDL_windows.h"

#include <shellapi.h> // CommandLineToArgvW()

static int OutOfMemory(void)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Out of memory - aborting", NULL);
    return -1;
}

static int ErrorProcessingCommandLine(void)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Error processing command line arguments", NULL);
    return -1;
}

// If the provided argv is NULL (which it will be when using SDL_main), this function parses
// the command-line string for the current process into an argv and uses that instead.
// Otherwise, the provided argv is used as-is (since that's probably what the user wants).
int SDL_CallMain(int caller_argc, char* caller_argv[], SDL_main_func mainFunction)
{
    int result, argc;
    LPWSTR *argvw = NULL;
    char **argv = NULL;

    // Note that we need to be careful about how we allocate/free memory in this function. If the application calls
    // SDL_SetMemoryFunctions(), we can't rely on SDL_free() to use the same allocator after SDL_main() returns.

    if (!caller_argv || caller_argc < 0) {
        // If the passed argv is NULL (or argc is negative), SDL should get the command-line arguments
        // using GetCommandLineW() and convert them to argc and argv before calling mainFunction().

        // Because of how the Windows command-line works, we know for sure that the buffer size required to store all
        // argument strings converted to UTF-8 (with null terminators) is guaranteed to be less than or equal to the
        // size of the original command-line string converted to UTF-8.
        const int argdata_size = WideCharToMultiByte(CP_UTF8, 0, GetCommandLineW(), -1, NULL, 0, NULL, NULL); // Includes the null terminator
        if (!argdata_size) {
            result = ErrorProcessingCommandLine();
            goto cleanup;
        }

        argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
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

        argvw = NULL;

        caller_argc = argc;
        caller_argv = argv;
    }

    result = mainFunction(caller_argc, caller_argv);

cleanup:

    HeapFree(GetProcessHeap(), 0, argv);
    LocalFree(argvw);

    return result;
}

// GDK uses the same SDL_CallMain() implementation as desktop Windows but has its own SDL_RunApp() implementation.
#ifndef SDL_PLATFORM_GDK

int SDL_RunApp(int argc, char* argv[], SDL_main_func mainFunction, void * reserved)
{
    (void)reserved;

    SDL_SetMainReady();

    return SDL_CallMain(argc, argv, mainFunction);
}

#endif // !SDL_PLATFORM_GDK

#endif // SDL_PLATFORM_WINDOWS
