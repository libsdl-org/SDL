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

#ifndef _SDL_x11dyn_h
#define _SDL_x11dyn_h

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#if SDL_VIDEO_DRIVER_X11_HAS_XKBKEYCODETOKEYSYM
#include <X11/XKBlib.h>
#endif

/* Apparently some X11 systems can't include this multiple times... */
#ifndef SDL_INCLUDED_XLIBINT_H
#define SDL_INCLUDED_XLIBINT_H 1
#include <X11/Xlibint.h>
#endif

#include <X11/Xproto.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>

#ifndef NO_SHARED_MEMORY
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

#if SDL_VIDEO_DRIVER_X11_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif
#if SDL_VIDEO_DRIVER_X11_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#if SDL_VIDEO_DRIVER_X11_XINPUT2
#include <X11/extensions/XInput2.h>
#endif
#if SDL_VIDEO_DRIVER_X11_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
#if SDL_VIDEO_DRIVER_X11_XSCRNSAVER
#include <X11/extensions/scrnsaver.h>
#endif
#if SDL_VIDEO_DRIVER_X11_XSHAPE
#include <X11/extensions/shape.h>
#endif
#if SDL_VIDEO_DRIVER_X11_XVIDMODE
#include <X11/extensions/xf86vmode.h>
#endif

/*
 * When using the "dynamic X11" functionality, we duplicate all the Xlib
 *  symbols that would be referenced by SDL inside of SDL itself.
 *  These duplicated symbols just serve as passthroughs to the functions
 *  in Xlib, that was dynamically loaded.
 *
 * This allows us to use Xlib as-is when linking against it directly, but
 *  also handles all the strange cases where there was code in the Xlib
 *  headers that may or may not exist or vary on a given platform.
 */
#ifdef __cplusplus
extern "C"
{
#endif

/* evil function signatures... */
    typedef Bool(*SDL_X11_XESetWireToEventRetType) (Display *, XEvent *,
                                                    xEvent *);
    typedef int (*SDL_X11_XSynchronizeRetType) (Display *);
    typedef Status(*SDL_X11_XESetEventToWireRetType) (Display *, XEvent *,
                                                      xEvent *);

    int SDL_X11_LoadSymbols(void);
    void SDL_X11_UnloadSymbols(void);

/* That's really annoying...make these function pointers no matter what. */
#ifdef X_HAVE_UTF8_STRING
    extern XIC(*pXCreateIC) (XIM, ...);
    extern char *(*pXGetICValues) (XIC, ...);
#endif

/* These SDL_X11_HAVE_* flags are here whether you have dynamic X11 or not. */
#define SDL_X11_MODULE(modname) extern int SDL_X11_HAVE_##modname;
#define SDL_X11_SYM(rc,fn,params,args,ret)
#include "SDL_x11sym.h"
#undef SDL_X11_MODULE
#undef SDL_X11_SYM


#ifdef __cplusplus
}
#endif

#endif                          /* !defined _SDL_x11dyn_h */
/* vi: set ts=4 sw=4 expandtab: */
