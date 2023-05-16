/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_cocoaclipboard_h_
#define SDL_cocoaclipboard_h_

/* Forward declaration */
@class SDL_CocoaVideoData;

extern int Cocoa_SetClipboardText(SDL_VideoDevice *_this, const char *text);
extern char *Cocoa_GetClipboardText(SDL_VideoDevice *_this);
extern SDL_bool Cocoa_HasClipboardText(SDL_VideoDevice *_this);
extern void Cocoa_CheckClipboardUpdate(SDL_CocoaVideoData *data);
extern void *Cocoa_GetClipboardData(SDL_VideoDevice *_this, size_t *len, const char *mime_type);
extern SDL_bool Cocoa_HasClipboardData(SDL_VideoDevice *_this, const char *mime_type);
extern int Cocoa_SetClipboardData(SDL_VideoDevice *_this, SDL_ClipboardDataCallback callback, size_t mime_count,
                                  const char **mime_types, void *userdata);


#endif /* SDL_cocoaclipboard_h_ */
