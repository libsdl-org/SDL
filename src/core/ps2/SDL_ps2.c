/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef __PS2__

/* SDL_RunApp() code for PS2 based on SDL_ps2_main.c, fjtrujy@gmail.com */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

#include <kernel.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <sbv_patches.h>
#include <ps2_filesystem_driver.h>

__attribute__((weak)) void reset_IOP()
{
    SifInitRpc(0);
    while (!SifIopReset(NULL, 0)) {
    }
    while (!SifIopSync()) {
    }
}

static void prepare_IOP()
{
    reset_IOP();
    SifInitRpc(0);
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
    sbv_patch_fileio();
}

static void init_drivers()
{
	init_ps2_filesystem_driver();
}

static void deinit_drivers()
{
	deinit_ps2_filesystem_driver();
}

DECLSPEC int
SDL_RunApp(int argc, char* argv[], SDL_main_func mainFunction, void * reserved)
{
    int res;
    (void)reserved;

    prepare_IOP();
    init_drivers();

    SDL_SetMainReady();

    res = mainFunction(argc, argv);

    deinit_drivers();

    return res;
}

#endif /* __PS2__ */
