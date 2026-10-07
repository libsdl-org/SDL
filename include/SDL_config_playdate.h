#ifndef SDL2_SDL_CONFIG_PLAYDATE_H
#define SDL2_SDL_CONFIG_PLAYDATE_H

/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_config_playdate_h_
#define SDL_config_playdate_h_
#define SDL_config_h_

#include <stdint.h>

#include "SDL_platform.h"

/* Most everything except Visual Studio 2008 and earlier has stdint.h now */
#if defined(_MSC_VER) && (_MSC_VER < 1600)
/* Here are some reasonable defaults */
typedef unsigned int size_t;
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
typedef unsigned long uintptr_t;
#else
#define HAVE_STDINT_H 1
#endif /* Visual Studio 2008 */

#ifdef __GNUC__
#define HAVE_GCC_SYNC_LOCK_TEST_AND_SET 1
#endif

// Disable dynamic lookup, needs investigation.
#ifdef SDL_DYNAMIC_API
#undef  SDL_DYNAMIC_API
#define SDL_DYNAMIC_API 0
#endif

// Must un-define osx bc it gets picked up when compiling for simulator.
#ifdef TARGET_SIMULATOR
#undef __APPLE__
#undef __MACOSX__
#endif /* TARGET_SIMULATOR */

#ifdef SDL_LEAN_AND_MEAN
#undef SDL_LEAN_AND_MEAN
#define SDL_LEAN_AND_MEAN 1
#endif

#define HAVE_STDIO_H 1
#define LACKS_SYS_MMAN_H 1
#define HAVE_LIBC 0
#define HAVE_INTTYPES_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define STDC_HEADERS 1
#define HAVE_STRING_H 1
#define HAVE_MATH_H 1
#define HAVE_LIMITS_H 1
#define LACKS_SYS_MMAN_H 1

#ifdef HAVE_MMAP
#undef  HAVE_MMAP
#define HAVE_MMAP 0
#endif

#define HAVE_MALLOC 0
#define HAVE_MORECORE 0

#undef SDL_VIDEO_RENDER_SW
#define SDL_VIDEO_RENDER_SW 1

/* Enable the dummy audio driver (src/audio/dummy/\*.c) */
#define SDL_AUDIO_DRIVER_DUMMY 1

/* Enable the joystick driver */
#define SDL_JOYSTICK_PLAYDATE 1

/* No haptics on Playdate */
#define SDL_HAPTIC_DISABLED 1

/* Enable the sensor driver */
#define SDL_SENSOR_PLAYDATE 1

/* TODO: investigate */
#define SDL_LOADSO_DISABLED 1

/* TODO: investigate */
#define SDL_THREADS_DISABLED 1

/* TODO: investigation, but i think is bc of timers */
#define SDL_TIMER_PLAYDATE 1

/* Enable the video driver */
#define SDL_VIDEO_DRIVER_PLAYDATE  1

/* Enable system power support */
#define SDL_POWER_PLAYDATE 1

#endif /* SDL_config_playdate_h_ */

#endif //SDL2_SDL_CONFIG_PLAYDATE_H
