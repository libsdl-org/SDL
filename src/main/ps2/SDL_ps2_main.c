/*
    SDL_ps2_main.c, fjtrujy@gmail.com 
*/

#include "SDL_config.h"

#ifdef __PS2__

#include "SDL_main.h"
#include "SDL_error.h"

#include <sifrpc.h>
#include <iopcontrol.h>
#include <sbv_patches.h>

#ifdef main
    #undef main
#endif

static void prepare_IOP()
{
   SifInitRpc(0);
   sbv_patch_enable_lmb();
   sbv_patch_disable_prefix_check();
}

int main(int argc, char *argv[])
{
    prepare_IOP();

    return SDL_main(argc, argv);
}

#endif /* _EE */

/* vi: set ts=4 sw=4 expandtab: */
