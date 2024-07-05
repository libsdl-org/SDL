/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
#include "../windows/SDL_windows.h"
#include "../../events/SDL_events_c.h"
}
#include <XGameRuntime.h>
#include <xsapi-c/services_c.h>
#include <appnotify.h>

static XTaskQueueHandle GDK_GlobalTaskQueue;

PAPPSTATE_REGISTRATION hPLM = {};
PAPPCONSTRAIN_REGISTRATION hCPLM = {};
HANDLE plmSuspendComplete = nullptr;

extern "C"
int SDL_GDKGetTaskQueue(XTaskQueueHandle *outTaskQueue)
{
    /* If this is the first call, first create the global task queue. */
    if (!GDK_GlobalTaskQueue) {
        HRESULT hr;

        hr = XTaskQueueCreate(XTaskQueueDispatchMode::ThreadPool,
                              XTaskQueueDispatchMode::Manual,
                              &GDK_GlobalTaskQueue);
        if (FAILED(hr)) {
            return SDL_SetError("[GDK] Could not create global task queue");
        }

        /* The initial call gets the non-duplicated handle so they can clean it up */
        *outTaskQueue = GDK_GlobalTaskQueue;
    } else {
        /* Duplicate the global task queue handle into outTaskQueue */
        if (FAILED(XTaskQueueDuplicateHandle(GDK_GlobalTaskQueue, outTaskQueue))) {
            return SDL_SetError("[GDK] Unable to acquire global task queue");
        }
    }

    return 0;
}

extern "C"
void GDK_DispatchTaskQueue(void)
{
    /* If there is no global task queue, don't do anything.
     * This gives the option to opt-out for those who want to handle everything themselves.
     */
    if (GDK_GlobalTaskQueue) {
        /* Dispatch any callbacks which are ready. */
        while (XTaskQueueDispatch(GDK_GlobalTaskQueue, XTaskQueuePort::Completion, 0))
            ;
    }
}

extern "C"
void SDL_GDKSuspendComplete()
{
    if (plmSuspendComplete) {
        SetEvent(plmSuspendComplete);
    }
}

extern "C"
int SDL_GDKGetDefaultUser(XUserHandle *outUserHandle)
{
    XAsyncBlock block = { 0 };
    HRESULT result;

    if (FAILED(result = XUserAddAsync(XUserAddOptions::AddDefaultUserAllowingUI, &block))) {
        return WIN_SetErrorFromHRESULT("XUserAddAsync", result);
    }

    do {
        result = XUserAddResult(&block, outUserHandle);
    } while (result == E_PENDING);
    if (FAILED(result)) {
        return WIN_SetErrorFromHRESULT("XUserAddResult", result);
    }

    return 0;
}
