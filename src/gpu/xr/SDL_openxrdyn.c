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

#ifdef HAVE_GPU_OPENXR

#ifndef SDL_GPU_OPENXR_DYNAMIC
#error OpenXR loader path not set for platform
#endif

#define DEBUG_DYNAMIC_OPENXR 0

#include "SDL_openxrdyn.h"

typedef struct
{
    SDL_SharedObject *lib;
    const char *libname;
} openxrdynlib;

static openxrdynlib openxr_loader = { NULL, SDL_GPU_OPENXR_DYNAMIC };

static void *OPENXR_GetSym(const char *fnname, bool *failed)
{
    void *fn = SDL_LoadFunction(openxr_loader.lib, fnname);

#if DEBUG_DYNAMIC_OPENXR
    if (fn) {
        SDL_Log("OPENXR: Found '%s' in %s (%p)\n", fnname, dynlib->libname, fn);
    } else {
        SDL_Log("OPENXR: Symbol '%s' NOT FOUND!\n", fnname);
    }
#endif

    return fn;
}

// Define all the function pointers and wrappers...
#define SDL_OPENXR_SYM(name)     PFN_##name OPENXR_##name = NULL;
#include "SDL_openxrsym.h"

static int openxr_load_refcount = 0;

void SDL_OPENXR_UnloadLoaderSymbols(void)
{
    // Don't actually unload if more than one module is using the libs...
    if (openxr_load_refcount > 0) {
        if (--openxr_load_refcount == 0) {
            // set all the function pointers to NULL.
#define SDL_OPENXR_SYM(name) OPENXR_##name = NULL;
#include "SDL_openxrsym.h"

            SDL_UnloadObject(openxr_loader.lib);
        }
    }
}

// returns non-zero if all needed symbols were loaded.
bool SDL_OPENXR_LoadLoaderSymbols(void)
{
    bool result = true; // always succeed if not using Dynamic OPENXR stuff.

    // deal with multiple modules (dga, openxr, etc) needing these symbols...
    if (openxr_load_refcount++ == 0) {
        openxr_loader.lib = SDL_LoadObject(openxr_loader.libname);

        if(!openxr_loader.lib)
            return false;

        bool failed = false;

#define SDL_OPENXR_SYM(name)     OPENXR_##name = (PFN_##name)OPENXR_GetSym(#name, &failed);
#include "SDL_openxrsym.h"

        if (failed) {
            // in case something got loaded...
            SDL_OPENXR_UnloadLoaderSymbols();
            result = false;
        }
    }

    return result;
}

XrInstancePfns *SDL_OPENXR_LoadInstanceSymbols(XrInstance instance)
{
    XrResult result;

    XrInstancePfns *pfns = SDL_calloc(1, sizeof(XrInstancePfns));

#define SDL_OPENXR_INSTANCE_SYM(name) \
    result = xrGetInstanceProcAddr(instance, #name, (PFN_xrVoidFunction *)&pfns->name); \
    if(result != XR_SUCCESS) { \
        SDL_free(pfns); \
        return NULL; \
    }
#include "SDL_openxrsym.h"

    return pfns;
}

#endif // HAVE_GPU_OPENXR
