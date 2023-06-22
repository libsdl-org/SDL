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

#ifndef SDL_x11events_h_
#define SDL_x11events_h_

extern void X11_PumpEvents(SDL_VideoDevice *_this);
extern int X11_WaitEventTimeout(SDL_VideoDevice *_this, Sint64 timeoutNS);
extern void X11_SendWakeupEvent(SDL_VideoDevice *_this, SDL_Window *window);
extern int X11_SuspendScreenSaver(SDL_VideoDevice *_this);
extern void X11_ReconcileKeyboardState(SDL_VideoDevice *_this);
extern void X11_GetBorderValues(SDL_WindowData *data);

#endif /* SDL_x11events_h_ */
