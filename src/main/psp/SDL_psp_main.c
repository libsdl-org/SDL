/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_main.h"
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <pspthreadman.h>
#include <stdlib.h>
#include <stdio.h>

/* If application's main() is redefined as SDL_main, and libSDLmain is
   linked, then this file will create the standard exit callback,
   define the PSP_MODULE_INFO macro, and exit back to the browser when
   the program is finished.

   You can still override other parameters in your own code if you
   desire, such as PSP_HEAP_SIZE_KB, PSP_MAIN_THREAD_ATTR,
   PSP_MAIN_THREAD_STACK_SIZE, etc.
*/

PSP_MODULE_INFO("SDL App", 0, 1, 1);

int sdl_psp_exit_callback(int arg1, int arg2, void *common)
{
    exit(0);
    return 0;
}

int sdl_psp_callback_thread(SceSize args, void *argp)
{
    int cbid;
    cbid = sceKernelCreateCallback("Exit Callback",
                       sdl_psp_exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int sdl_psp_setup_callbacks(void)
{
    int thid = 0;
    thid = sceKernelCreateThread("update_thread",
                     sdl_psp_callback_thread, 0x11, 0xFA0, 0, 0);
    if(thid >= 0)
        sceKernelStartThread(thid, 0, 0);
    return thid;
}

int main(int argc, char *argv[])
{
    pspDebugScreenInit();
    sdl_psp_setup_callbacks();

    /* Register sceKernelExitGame() to be called when we exit */
    atexit(sceKernelExitGame);

    SDL_SetMainReady();

    (void)SDL_main(argc, argv);
    return 0;
}
