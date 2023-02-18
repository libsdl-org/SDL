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

#ifndef SDL_build_config_h_
#define SDL_build_config_h_

#include <SDL3/SDL_platform_defines.h>

/**
 *  \file SDL_build_config.h
 *
 *  \brief This is a set of defines to configure the SDL features
 */

/* Add any platform that doesn't build using the configure system. */
#if defined(__WIN32__)
#include "SDL_build_config_windows.h"
#elif defined(__WINRT__)
#include "SDL_build_config_winrt.h"
#elif defined(__WINGDK__)
#include "SDL_build_config_wingdk.h"
#elif defined(__XBOXONE__) || defined(__XBOXSERIES__)
#include "SDL_build_config_xbox.h"
#elif defined(__MACOS__)
#include "SDL_build_config_macos.h"
#elif defined(__IOS__)
#include "SDL_build_config_ios.h"
#elif defined(__ANDROID__)
#include "SDL_build_config_android.h"
#elif defined(__EMSCRIPTEN__)
#include "SDL_build_config_emscripten.h"
#elif defined(__NGAGE__)
#include "SDL_build_config_ngage.h"
#else
/* This is a minimal configuration just to get SDL running on new platforms. */
#include "SDL_build_config_minimal.h"
#endif /* platform config */

#ifdef USING_GENERATED_CONFIG_H
#error Wrong SDL_build_config.h, check your include path?
#endif

#endif /* SDL_build_config_h_ */
