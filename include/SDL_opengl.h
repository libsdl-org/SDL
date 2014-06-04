/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

/**
 *  \file SDL_opengl.h
 *
 *  This is a simple file to encapsulate the OpenGL API headers.
 */

#ifndef _SDL_opengl_h
#define _SDL_opengl_h

#include "SDL_config.h"

#ifndef __IPHONEOS__

#ifdef __WIN32__
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX                /* Don't defined min() and max() */
#endif
#include <windows.h>
#endif

#ifdef __glext_h_
/* Someone has already included glext.h */
#define NO_SDL_GLEXT
#endif
#ifndef NO_SDL_GLEXT
#define __glext_h_              /* Don't let gl.h include glext.h */
#endif
#if defined(__MACOSX__)
#include <OpenGL/gl.h>          /* Header File For The OpenGL Library */
#define __X_GL_H
#else
#include <GL/gl.h>              /* Header File For The OpenGL Library */
#endif
#ifndef NO_SDL_GLEXT
#undef __glext_h_
#endif

/**
 *  \file SDL_opengl.h
 *
 *  This file is included because glext.h is not available on some systems.
 *  If you don't want this version included, simply define ::NO_SDL_GLEXT.
 *
 *  The latest version is available from:
 *      http://www.opengl.org/registry/
 */

/**
 *  \def NO_SDL_GLEXT
 *
 *  Define this if you have your own version of glext.h and want to disable the
 *  version included in SDL_opengl.h.
 */

#if !defined(NO_SDL_GLEXT) && !defined(GL_GLEXT_LEGACY)
#include "SDL_opengl_glext.h"
#endif /* NO_SDL_GLEXT */

#endif /* !__IPHONEOS__ */

#endif /* _SDL_opengl_h */

/* vi: set ts=4 sw=4 expandtab: */
