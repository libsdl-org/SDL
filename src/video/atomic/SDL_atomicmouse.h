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

#ifndef SDL_ATOMIC_mouse_h_
#define SDL_ATOMIC_mouse_h_

#include <gbm.h>

#define MAX_CURSOR_W 512
#define MAX_CURSOR_H 512

/* Driverdata with driver-side info about the cursor. */
typedef struct _ATOMIC_CursorData
{
    uint16_t       hot_x, hot_y;
    uint16_t       w, h;

    /* The buffer where we store the mouse bitmap ready to be used.
       We get it ready and filled in CreateCursor(), and copy it to a GBM BO in ShowCursor().*/     
    uint32_t *buffer;
    size_t buffer_size;
    size_t buffer_pitch;

} ATOMIC_CursorData;

extern void ATOMIC_InitMouse(_THIS);
extern void ATOMIC_DeinitMouse(_THIS);

extern void ATOMIC_InitCursor();

#endif /* SDL_ATOMIC_mouse_h_ */

/* vi: set ts=4 sw=4 expandtab: */
