/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

#ifndef _SDL_emscriptenvideo_h
#define _SDL_emscriptenvideo_h

#include "../SDL_sysvideo.h"
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <EGL/egl.h>

typedef struct SDL_WindowData
{
#if SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
#endif
    SDL_Window *window;
    SDL_Surface *surface;

    int windowed_width;
    int windowed_height;

    float pixel_ratio;

    SDL_bool external_size;

    int requested_fullscreen_mode;
} SDL_WindowData;

#endif /* _SDL_emscriptenvideo_h */

/* vi: set ts=4 sw=4 expandtab: */
