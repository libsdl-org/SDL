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

#ifndef _SDL_cocoavideo_h
#define _SDL_cocoavideo_h

#include "SDL_opengl.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>

#include "SDL_keycode.h"
#include "../SDL_sysvideo.h"

#include "SDL_cocoaclipboard.h"
#include "SDL_cocoaevents.h"
#include "SDL_cocoakeyboard.h"
#include "SDL_cocoamodes.h"
#include "SDL_cocoamouse.h"
#include "SDL_cocoaopengl.h"
#include "SDL_cocoawindow.h"

/* Private display data */

@class SDLTranslatorResponder;

typedef struct SDL_VideoData
{
    SInt32 osversion;
    unsigned int modifierFlags;
    void *key_layout;
    SDLTranslatorResponder *fieldEdit;
    NSInteger clipboard_count;
    Uint32 screensaver_activity;
} SDL_VideoData;

/* Utility functions */
extern NSImage * Cocoa_CreateImage(SDL_Surface * surface);

#endif /* _SDL_cocoavideo_h */

/* vi: set ts=4 sw=4 expandtab: */
