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

#ifndef SDL_sysactionset_h_
#define SDL_sysactionset_h_

#include "./SDL_actionset_c.h"

/* Only one provider can read each device at a time.
 * For example, if Steam Input is running, SDL Input doesn't need to read any
 * events for SDL_GameController devices.
 */
typedef enum
{
    ACTIONSET_DEVICE_NONE =          0x0000,
    ACTIONSET_DEVICE_KEYBOARDMOUSE = 0x0001,
    ACTIONSET_DEVICE_CONTROLLER =    0x0002,
    ACTIONSET_DEVICE_TOUCH =         0x0004,
    ACTIONSET_DEVICE_ALL =           0xFFFF
} ActionSetDeviceMask;

typedef struct SDL_ActionSetProvider
{
    ActionSetDeviceMask (*Init)(ActionSetDeviceMask current_mask);
    void (*Quit)(void);
    void (*Update)(void);
} SDL_ActionSetProvider;

/* Not all of these are available in a given build. Use #ifdefs, etc. */
extern SDL_ActionSetProvider STEAMINPUT_provider;
extern SDL_ActionSetProvider SDLINPUT_provider;

#if defined(__WINDOWS__) || defined(__MACOS__) || defined(__LINUX__)
#define SDL_ACTIONSET_STEAMINPUT
#endif

#endif /* SDL_sysactionset_h_ */
