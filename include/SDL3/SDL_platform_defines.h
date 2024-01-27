/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

/**
 *  \file SDL_platform_defines.h
 *
 *  Try to get a standard set of platform defines.
 */

#ifndef SDL_platform_defines_h_
#define SDL_platform_defines_h_

#ifdef _AIX
#define SDL_PLATFORM_AIX    1
#endif
#ifdef __HAIKU__
#define SDL_PLATFORM_HAIKU   1
#endif
#if defined(bsdi) || defined(__bsdi) || defined(__bsdi__)
#define SDL_PLATFORM_BSDI    1
#endif
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#define SDL_PLATFORM_FREEBSD 1
#endif
#if defined(hpux) || defined(__hpux) || defined(__hpux__)
#define SDL_PLATFORM_HPUX    1
#endif
#if defined(sgi) || defined(__sgi) || defined(__sgi__) || defined(_SGI_SOURCE)
#define SDL_PLATFORM_IRIX    1
#endif
#if (defined(linux) || defined(__linux) || defined(__linux__))
#define SDL_PLATFORM_LINUX   1
#endif
#if defined(ANDROID) || defined(__ANDROID__)
#undef SDL_PLATFORM_LINUX /* do we need to do this? */
#define SDL_PLATFORM_ANDROID    1
#endif
#ifdef __NGAGE__
#define SDL_PLATFORM_NGAGE 1
#endif

#if defined(__unix__) || defined(__unix) || defined(unix)
#define SDL_PLATFORM_UNIX   1
#endif

#ifdef __APPLE__
#define SDL_PLATFORM_APPLE  1
/* lets us know what version of macOS we're compiling on */
#include <AvailabilityMacros.h>
#include <TargetConditionals.h>

/* Fix building with older SDKs that don't define these
   See this for more information:
   https://stackoverflow.com/questions/12132933/preprocessor-macro-for-os-x-targets
*/
#ifndef TARGET_OS_MACCATALYST
#define TARGET_OS_MACCATALYST 0
#endif
#ifndef TARGET_OS_IOS
#define TARGET_OS_IOS 0
#endif
#ifndef TARGET_OS_IPHONE
#define TARGET_OS_IPHONE 0
#endif
#ifndef TARGET_OS_TV
#define TARGET_OS_TV 0
#endif
#ifndef TARGET_OS_SIMULATOR
#define TARGET_OS_SIMULATOR 0
#endif
#ifndef TARGET_OS_VISION
#define TARGET_OS_VISION 0
#endif

#if TARGET_OS_TV
#define SDL_PLATFORM_TVOS   1
#endif
#if TARGET_OS_VISION
#define SDL_PLATFORM_VISIONOS 1
#endif
#if TARGET_OS_IPHONE
#define SDL_PLATFORM_IOS    1
#else
#define SDL_PLATFORM_MACOS  1
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1070
# error SDL for macOS only supports deploying on 10.7 and above.
#endif /* MAC_OS_X_VERSION_MIN_REQUIRED < 1070 */
#endif /* TARGET_OS_IPHONE */
#endif /* defined(SDL_PLATFORM_APPLE) */

#ifdef __EMSCRIPTEN__
#define SDL_PLATFORM_EMSCRIPTEN 1
#endif
#ifdef __NetBSD__
#define SDL_PLATFORM_NETBSD 1
#endif
#ifdef __OpenBSD__
#define SDL_PLATFORM_OPENBSD    1
#endif
#if defined(__OS2__) || defined(__EMX__)
#define SDL_PLATFORM_OS2    1
#endif
#if defined(osf) || defined(__osf) || defined(__osf__) || defined(_OSF_SOURCE)
#define SDL_PLATFORM_OSF    1
#endif
#ifdef __QNXNTO__
#define SDL_PLATFORM_QNXNTO 1
#endif
#if defined(riscos) || defined(__riscos) || defined(__riscos__)
#define SDL_PLATFORM_RISCOS 1
#endif
#if defined(__sun) && defined(__SVR4)
#define SDL_PLATFORM_SOLARIS    1
#endif

#if defined(__CYGWIN__)
#define SDL_PLATFORM_CYGWIN 1
#endif

#if defined(_WIN32) || defined(SDL_PLATFORM_CYGWIN)
#define SDL_PLATFORM_WINDOWS  1    /* Win32 api and Windows-based OSs */

/* Try to find out if we're compiling for WinRT, GDK or non-WinRT/GDK */
#if defined(_MSC_VER) && defined(__has_include)
#if __has_include(<winapifamily.h>)
#define HAVE_WINAPIFAMILY_H 1
#else
#define HAVE_WINAPIFAMILY_H 0
#endif

/* If _USING_V110_SDK71_ is defined it means we are using the Windows XP toolset. */
#elif defined(_MSC_VER) && (_MSC_VER >= 1700 && !_USING_V110_SDK71_)    /* _MSC_VER == 1700 for Visual Studio 2012 */
#define HAVE_WINAPIFAMILY_H 1
#else
#define HAVE_WINAPIFAMILY_H 0
#endif

#if HAVE_WINAPIFAMILY_H
#include <winapifamily.h>
#define WINAPI_FAMILY_WINRT (!WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP))
#else
#define WINAPI_FAMILY_WINRT 0
#endif /* HAVE_WINAPIFAMILY_H */

#if HAVE_WINAPIFAMILY_H && HAVE_WINAPIFAMILY_H
#define SDL_WINAPI_FAMILY_PHONE (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
#else
#define SDL_WINAPI_FAMILY_PHONE 0
#endif

#if WINAPI_FAMILY_WINRT
#define SDL_PLATFORM_WINRT   1
#elif defined(_GAMING_DESKTOP) /* GDK project configuration always defines _GAMING_XXX */
#define SDL_PLATFORM_WINGDK 1
#elif defined(_GAMING_XBOX_XBOXONE)
#define SDL_PLATFORM_XBOXONE    1
#elif defined(_GAMING_XBOX_SCARLETT)
#define SDL_PLATFORM_XBOXSERIES 1
#else
#define SDL_PLATFORM_WIN32      1
#endif
#endif /* defined(WIN32) || defined(_WIN32) || defined(SDL_PLATFORM_CYGWIN) */

/* This is to support generic "any GDK" separate from a platform-specific GDK */
#if defined(SDL_PLATFORM_WINGDK) || defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES)
#define SDL_PLATFORM_GDK    1
#endif
#ifdef __PSP__
#define SDL_PLATFORM_PSP    1
#endif
#if defined(__PS2__) || defined(PS2)
#define SDL_PLATFORM_PS2    1
#endif

#if defined(__vita__) || defined(__psp2__)
#define SDL_PLATFORM_VITA   1
#endif

#ifdef __3DS__
#undef __3DS__
#define SDL_PLATFORM_3DS    1
#endif

#endif /* SDL_platform_defines_h_ */
