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

extern "C" {
#include "../SDL_runapp.h"
#include "../../core/gdk/SDL_gdk.h"
#include "../../core/windows/SDL_windows.h"
#include "../../events/SDL_events_c.h"
}
#include <XGameRuntime.h>
#include <xsapi-c/services_c.h>
#include <shellapi.h> // CommandLineToArgvW()
#include <appnotify.h>

extern "C"
int SDL_RunApp(int argc, char* argv[], SDL_main_func mainFunction, void *)
{
    int result;
    HRESULT hr;
    XTaskQueueHandle taskQueue;

    // !!! FIXME: This function does not currently properly deinitialize GDK resources on failure.

    hr = XGameRuntimeInitialize();

    if (SUCCEEDED(hr) && SDL_GetGDKTaskQueue(&taskQueue)) {
        Uint32 titleid = 0;
        char scidBuffer[64];
        XblInitArgs xblArgs;

        XTaskQueueSetCurrentProcessTaskQueue(taskQueue);

        // Try to get the title ID and initialize Xbox Live
        hr = XGameGetXboxTitleId(&titleid);
        if (SUCCEEDED(hr)) {
            SDL_zero(xblArgs);
            xblArgs.queue = taskQueue;
            SDL_snprintf(scidBuffer, 64, "00000000-0000-0000-0000-0000%08X", titleid);
            xblArgs.scid = scidBuffer;
            // !!! FIXME: XblInitialize() should have a corresponding call to XblCleanup() according to the docs.
            hr = XblInitialize(&xblArgs);
        } else {
            SDL_SetError("[GDK] Unable to get titleid. Will not call XblInitialize. Check MicrosoftGame.config!");
        }

        SDL_SetMainReady();

        if (!GDK_RegisterChangeNotifications()) {
            return -1;
        }

        // The common Windows SDL_CallMain() implementation is responsible for handling the argv
        // and substituting it with a new argv parsed from the command-line string, if needed.
        result = SDL_CallMain(argc, argv, mainFunction);

        GDK_UnregisterChangeNotifications();

        // !!! FIXME: This follows the docs exactly, but for some reason still leaks handles on exit?
        // Terminate the task queue and dispatch any pending tasks
        XTaskQueueTerminate(taskQueue, false, nullptr, nullptr);
        while (XTaskQueueDispatch(taskQueue, XTaskQueuePort::Completion, 0))
            ;

        XTaskQueueCloseHandle(taskQueue);

        XGameRuntimeUninitialize();
    } else {
#ifdef SDL_PLATFORM_WINGDK
        if (hr == E_GAMERUNTIME_DLL_NOT_FOUND) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "[GDK] Gaming Runtime library not found (xgameruntime.dll)", NULL);
        } else {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "[GDK] Could not initialize - aborting", NULL);
        }
#else
        SDL_assert_always(0 && "[GDK] Could not initialize - aborting");
#endif
        result = -1;
    }

    return result;
}
