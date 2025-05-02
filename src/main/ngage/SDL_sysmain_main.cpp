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
#ifdef __cplusplus
extern "C" {
#endif

#include "SDL_internal.h"

extern SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]);
extern SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event);
extern SDL_AppResult SDL_AppIterate(void* appstate);
extern void SDL_AppQuit(void* appstate, SDL_AppResult result);

#ifdef __cplusplus
}
#endif

#include <e32std.h>
#include <estlib.h>
#include <stdlib.h>
#include <stdio.h>

#include "SDL_sysmain_main.hpp"
#include "../../audio/ngage/SDL_ngageaudio.hpp"
#include "../../render/ngage/SDL_render_ngage_c.hpp"

CRenderer *gRenderer = 0;

GLDEF_C TInt E32Main()
{
    // Get args and environment.
    int argc = 1;
    char* argv[] = { "game", NULL };
    char** envp = NULL;

    // Create lvalue variables for __crt0 arguments.
    char** argv_lvalue = argv;
    char** envp_lvalue = envp;

    CTrapCleanup* cleanup = CTrapCleanup::New();
    if (!cleanup)
    {
        return KErrNoMemory;
    }

    TRAPD(err,
    {
        CActiveScheduler* scheduler = new (ELeave) CActiveScheduler();
        CleanupStack::PushL(scheduler);
        CActiveScheduler::Install(scheduler);

        TInt posixErr = SpawnPosixServerThread();
        if (posixErr != KErrNone)
        {
            SDL_Log("Error: Failed to spawn POSIX server thread: %d", posixErr);
            User::Leave(posixErr);
        }

        __crt0(argc, argv_lvalue, envp_lvalue);

        // Increase heap size.
        RHeap* newHeap = User::ChunkHeap(NULL, 7500000, 7500000, KMinHeapGrowBy);
        if (!newHeap)
        {
            SDL_Log("Error: Failed to create new heap");
            User::Leave(KErrNoMemory);
        }
        CleanupStack::PushL(newHeap);

        RHeap* oldHeap = User::SwitchHeap(newHeap);

        TInt targetLatency = 225;
        InitAudio(&targetLatency);

        // Wait until audio is ready.
        while (!AudioIsReady())
        {
            User::After(100000); // 100ms.
        }

        // Create and start the rendering backend.
        gRenderer = CRenderer::NewL();
        CleanupStack::PushL(gRenderer);

        // Create and start the SDL main runner.
        CSDLmain* mainApp = CSDLmain::NewL();
        CleanupStack::PushL(mainApp);
        mainApp->Start();

        // Start the active scheduler to handle events.
        CActiveScheduler::Start();

        CleanupStack::PopAndDestroy(gRenderer);
        CleanupStack::PopAndDestroy(mainApp);

        User::SwitchHeap(oldHeap);

        CleanupStack::PopAndDestroy(newHeap);
        CleanupStack::PopAndDestroy(scheduler);
    });

    if (err != KErrNone)
    {
        SDL_Log("Error: %d", err);
    }

    return err;
}

CSDLmain* CSDLmain::NewL()
{
    CSDLmain* self = new (ELeave) CSDLmain();
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
}

CSDLmain::CSDLmain() : CActive(EPriorityLow) {}

void CSDLmain::ConstructL()
{
    CActiveScheduler::Add(this);
}

CSDLmain::~CSDLmain()
{
    Cancel();
}

void CSDLmain::Start()
{
    SetActive();
    TRequestStatus* status = &iStatus;
    User::RequestComplete(status, KErrNone);
}

void CSDLmain::DoCancel() {}

static bool callbacks_initialized = false;

void CSDLmain::RunL()
{
    if (callbacks_initialized)
    {
        SDL_Event event;

        iResult = SDL_AppIterate(NULL);
        if (iResult != SDL_APP_CONTINUE)
        {
            DeinitAudio();
            SDL_AppQuit(NULL, iResult);
            SDL_Quit();
            CActiveScheduler::Stop();
            return;
        }

        SDL_PumpEvents();
        if (SDL_PollEvent(&event))
        {
            iResult = SDL_AppEvent(NULL, &event);
            if (iResult != SDL_APP_CONTINUE)
            {
                DeinitAudio();
                SDL_AppQuit(NULL, iResult);
                SDL_Quit();
                CActiveScheduler::Stop();
                return;
            }
        }

        Start();
    }
    else
    {
        SDL_SetMainReady();
        SDL_AppInit(NULL, 0, NULL);
        callbacks_initialized = true;
        Start();
    }
}
