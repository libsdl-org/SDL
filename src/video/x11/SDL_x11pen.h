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
#include "../../SDL_internal.h"

#ifndef SDL_x11pen_h_
#define SDL_x11pen_h_

#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2

#include "SDL_x11video.h"
#include "../../events/SDL_pen_c.h"

/* Pressure-sensitive pen */

/* Forward definition for SDL_x11video.h */
struct SDL_VideoData;

/* Function definitions */

/* Detect XINPUT2 devices that are pens / erasers, or update the list after hotplugging */
extern void X11_InitPen(SDL_VideoDevice *_this);

/* Converts XINPUT2 valuators into pen axis information, including normalisation */
extern void X11_PenAxesFromValuators(const SDL_Pen *pen,
                                     const double *input_values, const unsigned char *mask, const int mask_len,
                                     /* out-mode parameters: */
                                     float axis_values[SDL_PEN_NUM_AXES]);

/* Map X11 device ID to pen ID */
extern int X11_PenIDFromDeviceID(int deviceid);

#endif /* SDL_VIDEO_DRIVER_X11_XINPUT2 */

#endif /* SDL_x11pen_h_ */

/* vi: set ts=4 sw=4 expandtab: */
