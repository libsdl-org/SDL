/*
    SDL_ps2_main.c, fjtrujy@gmail.com 
*/

#include "SDL_config.h"

#ifdef __PS2__

#include "SDL_main.h"
#include "SDL_error.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <kernel.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <sbv_patches.h>
#include <ps2_fileXio_driver.h>
#include <ps2_memcard_driver.h>
#include <ps2_usb_driver.h>

#ifdef main
    #undef main
#endif

__attribute__((weak))
void reset_IOP() {
    SifInitRpc(0);
    while(!SifIopReset(NULL, 0)){};
    while(!SifIopSync()){};
}

static void prepare_IOP()
{
    reset_IOP();
    SifInitRpc(0);
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
}

static void init_drivers() {
    init_fileXio_driver();
    init_memcard_driver(true);
    init_usb_driver(true);
}

static void deinit_drivers() {
    deinit_usb_driver(true);
    deinit_memcard_driver(true);
    deinit_fileXio_driver();
}

static void waitUntilDeviceIsReady(char *path)
{
    struct stat buffer;
    int ret = -1;
    int retries = 50;

    while(ret != 0 && retries > 0) {
        ret = stat(path, &buffer);
        /* Wait untill the device is ready */
        nopdelay();

        retries--;
    }
}

int main(int argc, char *argv[])
{
    int res;
    char cwd[FILENAME_MAX];

    prepare_IOP();
    init_drivers();
    
    getcwd(cwd, sizeof(cwd));
    waitUntilDeviceIsReady(cwd);

    res = SDL_main(argc, argv);

    deinit_drivers();    
    
    return res;
}

#endif /* _PS2 */

/* vi: set ts=4 sw=4 expandtab: */
