/*
   based on SDL_ngage_main.c, originally for SDL 1.2 by Hannu Viitala
*/

#include "SDL_internal.h"

#ifdef SDL_PLATFORM_NGAGE

#include <e32std.h>
#include <e32def.h>
#include <e32svr.h>
#include <e32base.h>
#include <estlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <w32std.h>
#include <apgtask.h>


int SDL_RunApp(int argc_, char* argv_[], SDL_main_func mainFunction, void * reserved)
{
    (void)argc_; (void)argv_; (void)reserved;
    /*  Get the clean-up stack */
    CTrapCleanup *cleanup = CTrapCleanup::New();

    /* Arrange for multi-threaded operation */
    SpawnPosixServerThread();

    /* Get args and environment */
    int argc = 0;
    char **argv = 0;
    char **envp = 0;

    __crt0(argc, argv, envp);

    /* Start the application! */

    /* Create stdlib */
    _REENT;

    /* Set process and thread priority and name */

    RThread currentThread;
    RProcess thisProcess;
    TParse exeName;
    exeName.Set(thisProcess.FileName(), NULL, NULL);
    currentThread.Rename(exeName.Name());
    currentThread.SetProcessPriority(EPriorityLow);
    currentThread.SetPriority(EPriorityMuchLess);

    /* Increase heap size */
    RHeap *newHeap = NULL;
    RHeap *oldHeap = NULL;
    TInt heapSize = 7500000;
    int ret;

    newHeap = User::ChunkHeap(NULL, heapSize, heapSize, KMinHeapGrowBy);

    if (!newHeap) {
        ret = 3;
        goto cleanup;
    } else {
        oldHeap = User::SwitchHeap(newHeap);
        /* Call stdlib main */
        SDL_SetMainReady();
        ret = mainFunction(argc, argv);
    }

cleanup:
    _cleanup();

    CloseSTDLIB();
    delete cleanup;
    return ret;
}

#endif // SDL_PLATFORM_NGAGE
