/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>
  Atomic KMSDRM backend by Manuel Alfayate Corchete <redwindwanderer@gmail.com>

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

#if SDL_VIDEO_DRIVER_ATOMIC

#define DEBUG_DYNAMIC_ATOMIC 0

#include "SDL_atomicdyn.h"


#ifdef SDL_VIDEO_DRIVER_ATOMIC_DYNAMIC

#include "SDL_name.h"
#include "SDL_loadso.h"

typedef struct
{
    void *lib;
    const char *libname;
} atomicdynlib;

#ifndef SDL_VIDEO_DRIVER_ATOMIC_DYNAMIC
#define SDL_VIDEO_DRIVER_ATOMIC_DYNAMIC NULL
#endif
#ifndef SDL_VIDEO_DRIVER_ATOMIC_DYNAMIC_GBM
#define SDL_VIDEO_DRIVER_ATOMIC_DYNAMIC_GBM NULL
#endif

static atomicdynlib atomiclibs[] = {
    {NULL, SDL_VIDEO_DRIVER_ATOMIC_DYNAMIC_GBM},
    {NULL, SDL_VIDEO_DRIVER_ATOMIC_DYNAMIC}
};

static void *
ATOMIC_GetSym(const char *fnname, int *pHasModule)
{
    int i;
    void *fn = NULL;
    for (i = 0; i < SDL_TABLESIZE(atomiclibs); i++) {
        if (atomiclibs[i].lib != NULL) {
            fn = SDL_LoadFunction(atomiclibs[i].lib, fnname);
            if (fn != NULL)
                break;
        }
    }

#if DEBUG_DYNAMIC_ATOMIC
    if (fn != NULL)
        SDL_Log("ATOMIC: Found '%s' in %s (%p)\n", fnname, atomiclibs[i].libname, fn);
    else
        SDL_Log("ATOMIC: Symbol '%s' NOT FOUND!\n", fnname);
#endif

    if (fn == NULL)
        *pHasModule = 0;  /* kill this module. */

    return fn;
}

#endif /* SDL_VIDEO_DRIVER_ATOMIC_DYNAMIC */

/* Define all the function pointers and wrappers... */
#define SDL_ATOMIC_MODULE(modname) int SDL_ATOMIC_HAVE_##modname = 0;
#define SDL_ATOMIC_SYM(rc,fn,params) SDL_DYNATOMICFN_##fn ATOMIC_##fn = NULL;
#define SDL_ATOMIC_SYM_CONST(type,name) SDL_DYNATOMICCONST_##name ATOMIC_##name = NULL;
#include "SDL_atomicsym.h"

static int atomic_load_refcount = 0;

void
SDL_ATOMIC_UnloadSymbols(void)
{
    /* Don't actually unload if more than one module is using the libs... */
    if (atomic_load_refcount > 0) {
        if (--atomic_load_refcount == 0) {
#ifdef SDL_VIDEO_DRIVER_ATOMIC_DYNAMIC
            int i;
#endif

            /* set all the function pointers to NULL. */
#define SDL_ATOMIC_MODULE(modname) SDL_ATOMIC_HAVE_##modname = 0;
#define SDL_ATOMIC_SYM(rc,fn,params) ATOMIC_##fn = NULL;
#define SDL_ATOMIC_SYM_CONST(type,name) ATOMIC_##name = NULL;
#include "SDL_atomicsym.h"


#ifdef SDL_VIDEO_DRIVER_ATOMIC_DYNAMIC
            for (i = 0; i < SDL_TABLESIZE(atomiclibs); i++) {
                if (atomiclibs[i].lib != NULL) {
                    SDL_UnloadObject(atomiclibs[i].lib);
                    atomiclibs[i].lib = NULL;
                }
            }
#endif
        }
    }
}

/* returns non-zero if all needed symbols were loaded. */
int
SDL_ATOMIC_LoadSymbols(void)
{
    int rc = 1;                 /* always succeed if not using Dynamic ATOMIC stuff. */

    /* deal with multiple modules needing these symbols... */
    if (atomic_load_refcount++ == 0) {
#ifdef SDL_VIDEO_DRIVER_ATOMIC_DYNAMIC
        int i;
        int *thismod = NULL;
        for (i = 0; i < SDL_TABLESIZE(atomiclibs); i++) {
            if (atomiclibs[i].libname != NULL) {
                atomiclibs[i].lib = SDL_LoadObject(atomiclibs[i].libname);
            }
        }

#define SDL_ATOMIC_MODULE(modname) SDL_ATOMIC_HAVE_##modname = 1; /* default yes */
#include "SDL_atomicsym.h"

#define SDL_ATOMIC_MODULE(modname) thismod = &SDL_ATOMIC_HAVE_##modname;
#define SDL_ATOMIC_SYM(rc,fn,params) ATOMIC_##fn = (SDL_DYNATOMICFN_##fn) ATOMIC_GetSym(#fn,thismod);
#define SDL_ATOMIC_SYM_CONST(type,name) ATOMIC_##name = *(SDL_DYNATOMICCONST_##name*) ATOMIC_GetSym(#name,thismod);
#include "SDL_atomicsym.h"

        if ((SDL_ATOMIC_HAVE_LIBDRM) && (SDL_ATOMIC_HAVE_GBM)) {
            /* all required symbols loaded. */
            SDL_ClearError();
        } else {
            /* in case something got loaded... */
            SDL_ATOMIC_UnloadSymbols();
            rc = 0;
        }

#else  /* no dynamic ATOMIC */

#define SDL_ATOMIC_MODULE(modname) SDL_ATOMIC_HAVE_##modname = 1; /* default yes */
#define SDL_ATOMIC_SYM(rc,fn,params) ATOMIC_##fn = fn;
#define SDL_ATOMIC_SYM_CONST(type,name) ATOMIC_##name = name;
#include "SDL_atomicsym.h"

#endif
    }

    return rc;
}

#endif /* SDL_VIDEO_DRIVER_ATOMIC */

/* vi: set ts=4 sw=4 expandtab: */
