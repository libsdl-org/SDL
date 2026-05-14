/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_gameinput_h_
#define SDL_gameinput_h_

#ifdef HAVE_GAMEINPUT_H

#ifdef HAVE_GCC_DIAGNOSTIC_PRAGMA
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#endif

#include <gameinput.h>

#ifdef HAVE_GCC_DIAGNOSTIC_PRAGMA
#pragma GCC diagnostic pop
#endif

#ifndef GAMEINPUT_API_VERSION
#define GAMEINPUT_API_VERSION 0
#endif

#if GAMEINPUT_API_VERSION == 3
using namespace GameInput::v3;
#elif GAMEINPUT_API_VERSION == 2
using namespace GameInput::v2;
#elif GAMEINPUT_API_VERSION == 1
using namespace GameInput::v1;
#endif

// Default value for SDL_HINT_JOYSTICK_GAMEINPUT
#if defined(SDL_PLATFORM_GDK) || (GAMEINPUT_API_VERSION >= 3)
#define SDL_GAMEINPUT_DEFAULT true
#else
#define SDL_GAMEINPUT_DEFAULT false
#endif

extern bool SDL_InitGameInput(IGameInput **ppGameInput);
extern bool SDL_GameInputReady(void);
extern void SDL_QuitGameInput(void);

#endif // HAVE_GAMEINPUT_H

#ifdef __cplusplus
extern "C" {
#endif

extern bool SDL_UsingGameInputForXInputControllers(void);

#ifdef __cplusplus
}
#endif

#endif // SDL_gameinput_h_
