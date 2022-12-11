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

#ifndef SDL_main_windows_h_
#define SDL_main_windows_h_

#if !defined(SDL_main_h_)
#error "This header should not be included directly, but only via SDL_main.h!"
#endif

/* if someone wants to include SDL_main.h but doesn't want the main handing magic,
   (maybe to call SDL_RegisterApp()) they can #define SDL_MAIN_HANDLED first
   _SDL_MAIN_NOIMPL is for SDL-internal usage (only affects implementation,
   not definition of SDL_MAIN_AVAILABLE etc in SDL_main.h) */
#if !defined(SDL_MAIN_HANDLED) && !defined(_SDL_MAIN_NOIMPL)

#if defined(__WIN32__) || defined(__GDK__)

/* these defines/typedefs are needed for the WinMain() definition */
#ifndef WINAPI
#define WINAPI __stdcall
#endif

#ifdef main
#  undef main
#endif /* main */

#include <SDL3/begin_code.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HINSTANCE__ * HINSTANCE;
typedef char* LPSTR;

#ifndef __GDK__ /* this is only needed for Win32 */

#if defined(_MSC_VER)
/* The VC++ compiler needs main/wmain defined */
# define console_ansi_main main
# if defined(UNICODE) && UNICODE
#  define console_wmain wmain
# endif
#endif

#if defined( UNICODE ) && UNICODE
/* This is where execution begins [console apps, unicode] */
int
console_wmain(int argc, wchar_t *wargv[], wchar_t *wenvp)
{
    return SDL_Win32RunApp(SDL_main, NULL);
}

#else /* ANSI */

/* This is where execution begins [console apps, ansi] */
int
console_ansi_main(int argc, char *argv[])
{
    return SDL_Win32RunApp(SDL_main, NULL);
}
#endif /* UNICODE/ANSI */

#endif /* not __GDK__ */

/* This is where execution begins [windowed apps and GDK] */
int WINAPI
WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
#ifdef __GDK__
    return SDL_GDKRunApp(SDL_main, NULL);
#else
    return SDL_Win32RunApp(SDL_main, NULL);
#endif
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include <SDL3/close_code.h>

/* rename users main() function to SDL_main() so it can be called from the wrapper above */
#define main    SDL_main

/* end of __WIN32__ and __GDK__ impls */
#elif defined(__IOS__) || defined(__TVOS__)

#ifdef main
#  undef main
#endif /* main */

#include <SDL3/begin_code.h>

#ifdef __cplusplus
extern "C" {
#endif

int main(int argc, char *argv[])
{
    return SDL_UIKitRunApp(argc, argv, SDL_main);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include <SDL3/close_code.h>

/* rename users main() function to SDL_main() so it can be called from the wrapper above */
#define main    SDL_main

/* end of __IOS__ and __TVOS__ impls */

/* TODO: remaining platforms */

#endif /* __WIN32__ etc */

#endif /* SDL_MAIN_HANDLED */

#endif /* SDL_main_windows_h_ */

/* vi: set ts=4 sw=4 expandtab: */
