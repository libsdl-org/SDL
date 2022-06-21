/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#include "SDL_system.h"
#include "../windows/SDL_windows.h"
#include <XGameRuntime.h>

static XTaskQueueHandle GDK_GlobalTaskQueue;

extern "C" DECLSPEC int
SDL_GDKGetTaskQueue(XTaskQueueHandle * outTaskQueue)
{
    /* If this is the first call, first create the global task queue. */
    if (!GDK_GlobalTaskQueue) {
        HRESULT hr;

        hr = XTaskQueueCreate(XTaskQueueDispatchMode::ThreadPool,
            XTaskQueueDispatchMode::Manual,
            &GDK_GlobalTaskQueue
            );
        if (FAILED(hr)) {
            SDL_SetError("[GDK] Could not create global task queue");
            return -1;
        }

        /* The initial call gets the non-duplicated handle so they can clean it up */
        *outTaskQueue = GDK_GlobalTaskQueue;
    } else {
        /* Duplicate the global task queue handle into outTaskQueue */
        if (FAILED(XTaskQueueDuplicateHandle(GDK_GlobalTaskQueue, outTaskQueue))) {
            SDL_SetError("[GDK] Unable to acquire global task queue");
            return -1;
        }
    }

    return 0;
}

extern "C" void
GDK_DispatchTaskQueue(void)
{
    /* If there is no global task queue, don't do anything.
     * This gives the option to opt-out for those who want to handle everything themselves.
     */
    if (GDK_GlobalTaskQueue) {
        /* Dispatch any callbacks which are ready. */
        while (XTaskQueueDispatch(GDK_GlobalTaskQueue, XTaskQueuePort::Completion, 0)) {
        }
    }
}
