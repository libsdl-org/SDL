#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "SDL_syswm.h"
#include "SDL_main.h"
#include "SDL_vulkan.h"

#include <stdlib.h>

#define SDL_DYNAPI_PROC(ret, name, fun_args, call_args, opt_ret) \
    ret name fun_args                                            \
    {                                                            \
        abort();                                                 \
    }
#include "dynapi/SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
