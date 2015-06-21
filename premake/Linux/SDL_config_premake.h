/* include/SDL_config.h.  Generated from SDL_config.h.in by configure.  */
/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

#ifndef _SDL_config_linux_h
#define _SDL_config_linux_h

/**
 *  \file SDL_config.h.in
 *
 *  This is a set of defines to configure the SDL features
 */

/* General platform specific identifiers */
#include "SDL_platform.h"

/* Make sure that this isn't included by Visual C++ */
#ifdef _MSC_VER
#error You should run hg revert SDL_config.h 
#endif

/* C language features */
/* #undef const */
/* #undef inline */
/* #undef volatile */

/* C datatypes */
#ifdef __LP64__
#define SIZEOF_VOIDP 8
#else
#define SIZEOF_VOIDP 4
#endif
#define HAVE_GCC_ATOMICS 1
/* #undef HAVE_GCC_SYNC_LOCK_TEST_AND_SET */

/* Comment this if you want to build without any C library requirements */
#define HAVE_LIBC 1
#if HAVE_LIBC

/* Useful headers */
#define HAVE_ALLOCA_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_STDIO_H 1
#define STDC_HEADERS 1
#define HAVE_STDLIB_H 1
#define HAVE_STDARG_H 1
#define HAVE_MALLOC_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_CTYPE_H 1
#define HAVE_MATH_H 1
#define HAVE_ICONV_H 1
#define HAVE_SIGNAL_H 1
/* #undef HAVE_ALTIVEC_H */
/* #undef HAVE_PTHREAD_NP_H */
/* #undef HAVE_LIBUDEV_H */
#define HAVE_DBUS_DBUS_H 1

/* C library functions */
#define HAVE_MALLOC 1
#define HAVE_CALLOC 1
#define HAVE_REALLOC 1
#define HAVE_FREE 1
#define HAVE_ALLOCA 1
#ifndef __WIN32__ /* Don't use C runtime versions of these on Windows */
#define HAVE_GETENV 1
#define HAVE_SETENV 1
#define HAVE_PUTENV 1
#define HAVE_UNSETENV 1
#endif
#define HAVE_QSORT 1
#define HAVE_ABS 1
#define HAVE_BCOPY 1
#define HAVE_MEMSET 1
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE 1
#define HAVE_MEMCMP 1
#define HAVE_STRLEN 1
/* #undef HAVE_STRLCPY */
/* #undef HAVE_STRLCAT */
#define HAVE_STRDUP 1
/* #undef HAVE__STRREV */
/* #undef HAVE__STRUPR */
/* #undef HAVE__STRLWR */
/* #undef HAVE_INDEX */
/* #undef HAVE_RINDEX */
#define HAVE_STRCHR 1
#define HAVE_STRRCHR 1
#define HAVE_STRSTR 1
/* #undef HAVE_ITOA */
/* #undef HAVE__LTOA */
/* #undef HAVE__UITOA */
/* #undef HAVE__ULTOA */
#define HAVE_STRTOL 1
#define HAVE_STRTOUL 1
/* #undef HAVE__I64TOA */
/* #undef HAVE__UI64TOA */
#define HAVE_STRTOLL 1
#define HAVE_STRTOULL 1
#define HAVE_STRTOD 1
#define HAVE_ATOI 1
#define HAVE_ATOF 1
#define HAVE_STRCMP 1
#define HAVE_STRNCMP 1
/* #undef HAVE__STRICMP */
#define HAVE_STRCASECMP 1
/* #undef HAVE__STRNICMP */
#define HAVE_STRNCASECMP 1
#define HAVE_VSSCANF 1
#define HAVE_VSNPRINTF 1
#define HAVE_M_PI /**/
#define HAVE_ATAN 1
#define HAVE_ATAN2 1
#define HAVE_CEIL 1
#define HAVE_COPYSIGN 1
#define HAVE_COS 1
#define HAVE_COSF 1
#define HAVE_FABS 1
#define HAVE_FLOOR 1
#define HAVE_LOG 1
#define HAVE_POW 1
#define HAVE_SCALBN 1
#define HAVE_SIN 1
#define HAVE_SINF 1
#define HAVE_SQRT 1
#define HAVE_FSEEKO 1
#define HAVE_FSEEKO64 1
#define HAVE_SIGACTION 1
#define HAVE_SA_SIGACTION 1
#define HAVE_SETJMP 1
#define HAVE_NANOSLEEP 1
#define HAVE_SYSCONF 1
/* #undef HAVE_SYSCTLBYNAME */
#define HAVE_CLOCK_GETTIME 1
/* #undef HAVE_GETPAGESIZE */
#define HAVE_MPROTECT 1
#define HAVE_ICONV 1
#define HAVE_PTHREAD_SETNAME_NP 1
/* #undef HAVE_PTHREAD_SET_NAME_NP */
#define HAVE_SEM_TIMEDWAIT 1

#else
#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#endif /* HAVE_LIBC */

/* SDL internal assertion support */
/* #undef SDL_DEFAULT_ASSERT_LEVEL */

#ifndef SDL_AUDIO_DRIVER_DUMMY
#define SDL_AUDIO_DRIVER_DUMMY 1
#endif
#ifndef SDL_AUDIO_DRIVER_DISK
#define SDL_AUDIO_DRIVER_DISK 1
#endif
#ifndef SDL_VIDEO_DRIVER_DUMMY
#define SDL_VIDEO_DRIVER_DUMMY 1
#endif
#ifndef SDL_VIDEO_RENDER_OGL
#define SDL_VIDEO_RENDER_OGL 1
#endif
#ifndef SDL_VIDEO_OPENGL
#define SDL_VIDEO_OPENGL 1
#endif
#ifndef SDL_VIDEO_OPENGL_GLX
#define SDL_VIDEO_OPENGL_GLX 1
#endif
#ifndef SDL_LOADSO_DLOPEN
#define SDL_LOADSO_DLOPEN 1
#endif
#ifndef SDL_AUDIO_DRIVER_ALSA
#define SDL_AUDIO_DRIVER_ALSA 1
#endif
#ifndef SDL_AUDIO_DRIVER_ALSA_DYNAMIC
#define SDL_AUDIO_DRIVER_ALSA_DYNAMIC "libasound.so"
#endif
#ifndef SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC
#define SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC "libpulse-simple.so"
#endif
#ifndef SDL_AUDIO_DRIVER_PULSEAUDIO
#define SDL_AUDIO_DRIVER_PULSEAUDIO 1
#endif
#ifndef SDL_AUDIO_DRIVER_ESD
#define SDL_AUDIO_DRIVER_ESD 1
#endif
#ifndef SDL_AUDIO_DRIVER_ESD_DYNAMIC
#define SDL_AUDIO_DRIVER_ESD_DYNAMIC "libesd.so"
#endif
#ifndef SDL_AUDIO_DRIVER_NAS
#define SDL_AUDIO_DRIVER_NAS 1
#endif
#ifndef SDL_AUDIO_DRIVER_NAS_DYNAMIC
#define SDL_AUDIO_DRIVER_NAS_DYNAMIC "libaudio.so"
#endif
#ifndef SDL_AUDIO_DRIVER_OSS
#define SDL_AUDIO_DRIVER_OSS 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_XINERAMA
#define SDL_VIDEO_DRIVER_X11_XINERAMA 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11
#define SDL_VIDEO_DRIVER_X11 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XEXT
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XEXT "libXext.so"
#endif
#ifndef SDL_VIDEO_DRIVER_X11_XCURSOR
#define SDL_VIDEO_DRIVER_X11_XCURSOR 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_XDBE
#define SDL_VIDEO_DRIVER_X11_XDBE 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_HAS_XKBKEYCODETOKEYSYM
#define SDL_VIDEO_DRIVER_X11_HAS_XKBKEYCODETOKEYSYM 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XINPUT2
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XINPUT2 "libXi.so"
#endif
#ifndef SDL_VIDEO_DRIVER_X11_XVIDMODE
#define SDL_VIDEO_DRIVER_X11_XVIDMODE 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XINERAMA
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XINERAMA "libXinerama.so"
#endif
#ifndef SDL_VIDEO_DRIVER_X11_CONST_PARAM_XEXTADDDISPLAY
#define SDL_VIDEO_DRIVER_X11_CONST_PARAM_XEXTADDDISPLAY 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC
#define SDL_VIDEO_DRIVER_X11_DYNAMIC "libX11.so"
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XSS
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XSS "libXss.so"
#endif
#ifndef SDL_VIDEO_DRIVER_X11_XINPUT2
#define SDL_VIDEO_DRIVER_X11_XINPUT2 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS
#define SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_XSCRNSAVER
#define SDL_VIDEO_DRIVER_X11_XSCRNSAVER 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_XSHAPE
#define SDL_VIDEO_DRIVER_X11_XSHAPE 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
#define SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_XRANDR
#define SDL_VIDEO_DRIVER_X11_XRANDR 1
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XVIDMODE
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XVIDMODE "libXxf86vm.so"
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XCURSOR
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XCURSOR "libXcursor.so"
#endif
#ifndef SDL_VIDEO_DRIVER_X11_DYNAMIC_XRANDR
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XRANDR "libXrandr.so"
#endif
#ifndef SDL_INPUT_LINUXEV
#define SDL_INPUT_LINUXEV 1
#endif
#ifndef SDL_HAPTIC_LINUX
#define SDL_HAPTIC_LINUX 1
#endif
#ifndef SDL_THREAD_PTHREAD_RECURSIVE_MUTEX
#define SDL_THREAD_PTHREAD_RECURSIVE_MUTEX 1
#endif
#ifndef SDL_JOYSTICK_LINUX
#define SDL_JOYSTICK_LINUX 1
#endif
#ifndef SDL_THREAD_PTHREAD
#define SDL_THREAD_PTHREAD 1
#endif
#ifndef SDL_POWER_LINUX
#define SDL_POWER_LINUX 1
#endif
#ifndef SDL_TIMER_UNIX
#define SDL_TIMER_UNIX 1
#endif
#ifndef SDL_FILESYSTEM_UNIX
#define SDL_FILESYSTEM_UNIX 1
#endif

/* Enable assembly routines */
#define SDL_ASSEMBLY_ROUTINES 1
/* #undef SDL_ALTIVEC_BLITTERS */

#endif /* _SDL_config_h */
