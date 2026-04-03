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

#ifndef SDL_build_config_ps3_h_
#define SDL_build_config_ps3_h_

#include <SDL3/SDL_platform_defines.h>

#ifdef __GNUC__
#define HAVE_GCC_SYNC_LOCK_TEST_AND_SET 1
#endif

#define SIZEOF_VOIDP 8
#define HAVE_GCC_ATOMICS    1

/* Comment this if you want to build without any C library requirements */
#define HAVE_LIBC 1

#if HAVE_LIBC
#define HAVE_ALLOCA_H       1
#define HAVE_SYS_TYPES_H    1
#define HAVE_STDIO_H    1
#define STDC_HEADERS    1
#define HAVE_STDLIB_H   1
#define HAVE_STDARG_H   1
#define HAVE_STRING_H   1
#define HAVE_STRINGS_H  1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H   1
#define HAVE_CTYPE_H    1
#define HAVE_MATH_H     1
#define HAVE_ICONV_H    1
#define HAVE_SIGNAL_H   1
#define HAVE_ALTIVEC_H  1

/* C library functions */
#define HAVE_MALLOC 1
#define HAVE_CALLOC 1
#define HAVE_REALLOC    1
#define HAVE_FREE   1
#define HAVE_ALLOCA 1
#ifndef __WIN32__ /* Don't use C runtime versions of these on Windows */
#define HAVE_GETENV 1
#define HAVE_SETENV 1
#define HAVE_PUTENV 1
#define HAVE_UNSETENV   1
#endif
#define HAVE_QSORT  1
#define HAVE_ABS    1
#define HAVE_BCOPY  1
#define HAVE_MEMSET 1
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE    1
/* #undef HAVE_MEMCMP */
#define HAVE_STRLEN 1
#define HAVE_STRLCPY    1
#define HAVE_STRLCAT    1
#define HAVE_STRDUP 1
#define HAVE_STRCHR 1
#define HAVE_STRRCHR    1
#define HAVE_STRSTR 1
#define HAVE_STRTOL 1
#define HAVE_STRTOUL    1
#define HAVE_STRTOLL    1
#define HAVE_STRTOULL   1
/* #undef HAVE_STRTOD */
#define HAVE_ATOI   1
#define HAVE_ATOF   1
#define HAVE_STRCMP 1
#define HAVE_STRNCMP    1
#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_SSCANF 1
#define HAVE_SNPRINTF   1
#define HAVE_VSNPRINTF  1
#define HAVE_M_PI   1
#define HAVE_ATAN   1
#define HAVE_ATAN2  1
#define HAVE_CEIL   1
#define HAVE_COPYSIGN   1
#define HAVE_COS    1
#define HAVE_COSF   1
#define HAVE_FABS   1
#define HAVE_FLOOR  1
#define HAVE_LOG    1
/* #undef HAVE_POW  */
#define HAVE_SCALBN 1
#define HAVE_SIN    1
#define HAVE_SINF   1
#define HAVE_SQRT   1
#define HAVE_SETJMP 1
/* #undef HAVE_NANOSLEEP */
/* #define HAVE_SYSCONF  1 */
/* #define HAVE_SIGACTION    1 */


#else
#include <stdarg.h>
#endif /* HAVE_LIBC */

/* PS3 isn't that sophisticated */
/* #define LACKS_SYS_MMAN_H 1 */

/* Enable the stub for HAPTIC
/* PS3 doesn't have haptic device (src/haptic/dummy/\*.c) */
#define SDL_HAPTIC_DISABLED    1

/* PS3 can't load shared object (src/loadso/dummy/\*.c) */
#define SDL_LOADSO_DISABLED    1

/* Enable the stub audio driver (src/audio/ps3/\*.c) */
#define SDL_AUDIO_DRIVER_PS3    1

/* Enable the stub joystick driver (src/joystick/ps3/\*.c) */
#define SDL_JOYSTICK_PS3        1

/* Enable the stub thread support (src/thread/ps3/\*.c) */
#define SDL_THREAD_PS3  1

/* Enable the stub timer support (src/timer/ps3/\*.c) */
#define SDL_TIMER_PS3  1

/* PS3 video dirver */
#define SDL_VIDEO_DRIVER_PS3   1

/* PS3 render dirver */
#define SDL_VIDEO_RENDER_PS3   1
#define SDL_VIDEO_OPENGL   0
#define HAVE_SDL_VIDEO   1

/* Enable assembly routines */
#define SDL_ASSEMBLY_ROUTINES 1
#define SDL_ALTIVEC_BLITTERS 1

#endif /* SDL_build_config_ps3_h_ */
