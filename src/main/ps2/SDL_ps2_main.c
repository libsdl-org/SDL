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
#include <ps2_fileXio_driver.h>
#include <ps2_memcard_driver.h>

#ifdef main
    #undef main
#endif

static void prepare_IOP()
{
   SifInitRpc(0);
   sbv_patch_enable_lmb();
   sbv_patch_disable_prefix_check();
}

static void init_drivers() {
    init_fileXio_driver();
    init_memcard_driver(true);
}

static void deinit_drivers() {
    deinit_memcard_driver(true);
    deinit_fileXio_driver();
}

int main(int argc, char *argv[])
{
    int res;
    prepare_IOP();
    init_drivers();

    res = SDL_main(argc, argv);

    deinit_drivers();    
    
    return res;
}

#endif /* _EE */

/* vi: set ts=4 sw=4 expandtab: */
