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
#include "SDL_config.h"

#if SDL_VIDEO_DRIVER_X11

#define DEBUG_DYNAMIC_X11 0

#include "SDL_x11dyn.h"

#if DEBUG_DYNAMIC_X11
#include <stdio.h>
#endif

#ifdef SDL_VIDEO_DRIVER_X11_DYNAMIC

#include "SDL_name.h"
#include "SDL_loadso.h"

typedef struct
{
    void *lib;
    const char *libname;
} x11dynlib;

#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC
#define SDL_VIDEO_DRIVER_X11_DYNAMIC NULL
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XEXT
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XEXT NULL
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XCURSOR
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XCURSOR NULL
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XINERAMA
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XINERAMA NULL
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XINPUT2
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XINPUT2 NULL
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XRANDR
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XRANDR NULL
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XSS
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XSS NULL
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XVIDMODE
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XVIDMODE NULL
#endif

static x11dynlib x11libs[] = {
    {NULL, SDL_VIDEO_DRIVER_X11_DYNAMIC},
    {NULL, SDL_VIDEO_DRIVER_X11_DYNAMIC_XEXT},
    {NULL, SDL_VIDEO_DRIVER_X11_DYNAMIC_XCURSOR},
    {NULL, SDL_VIDEO_DRIVER_X11_DYNAMIC_XINERAMA},
    {NULL, SDL_VIDEO_DRIVER_X11_DYNAMIC_XINPUT2},
    {NULL, SDL_VIDEO_DRIVER_X11_DYNAMIC_XRANDR},
    {NULL, SDL_VIDEO_DRIVER_X11_DYNAMIC_XSS},
    {NULL, SDL_VIDEO_DRIVER_X11_DYNAMIC_XVIDMODE}
};

static void *
X11_GetSym(const char *fnname, int *pHasModule)
{
    int i;
    void *fn = NULL;
    for (i = 0; i < SDL_TABLESIZE(x11libs); i++) {
        if (x11libs[i].lib != NULL) {
            fn = SDL_LoadFunction(x11libs[i].lib, fnname);
            if (fn != NULL)
                break;
        }
    }

#if DEBUG_DYNAMIC_X11
    if (fn != NULL)
        printf("X11: Found '%s' in %s (%p)\n", fnname, x11libs[i].libname, fn);
    else
        printf("X11: Symbol '%s' NOT FOUND!\n", fnname);
#endif

    if (fn == NULL)
        *pHasModule = 0;  /* kill this module. */

    return fn;
}


/* Define all the function pointers and wrappers... */
#define SDL_X11_MODULE(modname)
#define SDL_X11_SYM(rc,fn,params,args,ret) \
    typedef rc (*SDL_DYNX11FN_##fn) params; \
    static SDL_DYNX11FN_##fn p##fn = NULL; \
    rc fn params { ret p##fn args ; }
#include "SDL_x11sym.h"
#undef SDL_X11_MODULE
#undef SDL_X11_SYM
#endif /* SDL_VIDEO_DRIVER_X11_DYNAMIC */

/* Annoying varargs entry point... */
#ifdef X_HAVE_UTF8_STRING
typedef XIC(*SDL_DYNX11FN_XCreateIC) (XIM,...);
SDL_DYNX11FN_XCreateIC pXCreateIC = NULL;
typedef char *(*SDL_DYNX11FN_XGetICValues) (XIC, ...);
SDL_DYNX11FN_XGetICValues pXGetICValues = NULL;
#endif

/* These SDL_X11_HAVE_* flags are here whether you have dynamic X11 or not. */
#define SDL_X11_MODULE(modname) int SDL_X11_HAVE_##modname = 0;
#define SDL_X11_SYM(rc,fn,params,args,ret)
#include "SDL_x11sym.h"
#undef SDL_X11_MODULE
#undef SDL_X11_SYM


static int x11_load_refcount = 0;

void
SDL_X11_UnloadSymbols(void)
{
#ifdef SDL_VIDEO_DRIVER_X11_DYNAMIC
    /* Don't actually unload if more than one module is using the libs... */
    if (x11_load_refcount > 0) {
        if (--x11_load_refcount == 0) {
            int i;

            /* set all the function pointers to NULL. */
#define SDL_X11_MODULE(modname) SDL_X11_HAVE_##modname = 0;
#define SDL_X11_SYM(rc,fn,params,args,ret) p##fn = NULL;
#include "SDL_x11sym.h"
#undef SDL_X11_MODULE
#undef SDL_X11_SYM

#ifdef X_HAVE_UTF8_STRING
            pXCreateIC = NULL;
            pXGetICValues = NULL;
#endif

            for (i = 0; i < SDL_TABLESIZE(x11libs); i++) {
                if (x11libs[i].lib != NULL) {
                    SDL_UnloadObject(x11libs[i].lib);
                    x11libs[i].lib = NULL;
                }
            }
        }
    }
#endif
}

/* returns non-zero if all needed symbols were loaded. */
int
SDL_X11_LoadSymbols(void)
{
    int rc = 1;                 /* always succeed if not using Dynamic X11 stuff. */

#ifdef SDL_VIDEO_DRIVER_X11_DYNAMIC
    /* deal with multiple modules (dga, x11, etc) needing these symbols... */
    if (x11_load_refcount++ == 0) {
        int i;
        int *thismod = NULL;
        for (i = 0; i < SDL_TABLESIZE(x11libs); i++) {
            if (x11libs[i].libname != NULL) {
                x11libs[i].lib = SDL_LoadObject(x11libs[i].libname);
            }
        }

#define SDL_X11_MODULE(modname) SDL_X11_HAVE_##modname = 1; /* default yes */
#define SDL_X11_SYM(a,fn,x,y,z)
#include "SDL_x11sym.h"
#undef SDL_X11_MODULE
#undef SDL_X11_SYM

#define SDL_X11_MODULE(modname) thismod = &SDL_X11_HAVE_##modname;
#define SDL_X11_SYM(a,fn,x,y,z) p##fn = (SDL_DYNX11FN_##fn) X11_GetSym(#fn,thismod);
#include "SDL_x11sym.h"
#undef SDL_X11_MODULE
#undef SDL_X11_SYM

#ifdef X_HAVE_UTF8_STRING
        pXCreateIC = (SDL_DYNX11FN_XCreateIC)
                        X11_GetSym("XCreateIC", &SDL_X11_HAVE_UTF8);
        pXGetICValues = (SDL_DYNX11FN_XGetICValues)
                        X11_GetSym("XGetICValues", &SDL_X11_HAVE_UTF8);
#endif

        if (SDL_X11_HAVE_BASEXLIB) {
            /* all required symbols loaded. */
            SDL_ClearError();
        } else {
            /* in case something got loaded... */
            SDL_X11_UnloadSymbols();
            rc = 0;
        }
    }
#else
#define SDL_X11_MODULE(modname) SDL_X11_HAVE_##modname = 1; /* default yes */
#define SDL_X11_SYM(a,fn,x,y,z)
#include "SDL_x11sym.h"
#undef SDL_X11_MODULE
#undef SDL_X11_SYM

#ifdef X_HAVE_UTF8_STRING
    pXCreateIC = XCreateIC;
    pXGetICValues = XGetICValues;
#endif
#endif

    return rc;
}

#endif /* SDL_VIDEO_DRIVER_X11 */

/* vi: set ts=4 sw=4 expandtab: */
