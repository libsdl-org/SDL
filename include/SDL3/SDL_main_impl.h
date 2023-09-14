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

#ifndef SDL_main_impl_h_
#define SDL_main_impl_h_

#ifndef SDL_main_h_
#error "This header should not be included directly, but only via SDL_main.h!"
#endif

/* if someone wants to include SDL_main.h but doesn't want the main handing magic,
   (maybe to call SDL_RegisterApp()) they can #define SDL_MAIN_HANDLED first
   SDL_MAIN_NOIMPL is for SDL-internal usage (only affects implementation,
   not definition of SDL_MAIN_AVAILABLE etc in SDL_main.h) and if the user wants
   to have the SDL_main implementation (from this header) in another source file
   than their main() function, for example if SDL_main requires C++
   and main() is implemented in plain C */
#if !defined(SDL_MAIN_HANDLED) && !defined(SDL_MAIN_NOIMPL)

/* the implementations below must be able to use the implement real main(), nothing renamed
   (the user's main() will be renamed to SDL_main so it can be called from here) */
#ifdef main
#  undef main
#endif /* main */

#ifdef SDL_MAIN_USE_CALLBACKS

#if 0
    /* currently there are no platforms that _need_ a magic entry point here
       for callbacks, but if one shows up, implement it here. */

#else /* use a standard SDL_main, which the app SHOULD NOT ALSO SUPPLY. */

/* this define makes the normal SDL_main entry point stuff work...we just provide SDL_main() instead of the app. */
#define SDL_MAIN_CALLBACK_STANDARD 1

int SDL_main(int argc, char **argv)
{
    return SDL_EnterAppMainCallbacks(argc, argv, SDL_AppInit, SDL_AppIterate, SDL_AppEvent, SDL_AppQuit);
}

#endif  /* platform-specific tests */

#endif  /* SDL_MAIN_USE_CALLBACKS */


/* set up the usual SDL_main stuff if we're not using callbacks or if we are but need the normal entry point. */
#if !defined(SDL_MAIN_USE_CALLBACKS) || defined(SDL_MAIN_CALLBACK_STANDARD)

#if defined(__WIN32__) || defined(__GDK__)

/* these defines/typedefs are needed for the WinMain() definition */
#ifndef WINAPI
#define WINAPI __stdcall
#endif

#include <SDL3/SDL_begin_code.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HINSTANCE__ * HINSTANCE;
typedef char* LPSTR;
typedef wchar_t* PWSTR;

/* The VC++ compiler needs main/wmain defined, but not for GDK */
#if defined(_MSC_VER) && !defined(__GDK__)

/* This is where execution begins [console apps] */
#if defined( UNICODE ) && UNICODE
int wmain(int argc, wchar_t *wargv[], wchar_t *wenvp)
{
    (void)argc;
    (void)wargv;
    (void)wenvp;
    return SDL_RunApp(0, NULL, SDL_main, NULL);
}
#else /* ANSI */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    return SDL_RunApp(0, NULL, SDL_main, NULL);
}
#endif /* UNICODE */

#endif /* _MSC_VER && ! __GDK__ */

/* This is where execution begins [windowed apps and GDK] */
#if defined( UNICODE ) && UNICODE
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR szCmdLine, int sw)
#else /* ANSI */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
#endif
{
    (void)hInst;
    (void)hPrev;
    (void)szCmdLine;
    (void)sw;
    return SDL_RunApp(0, NULL, SDL_main, NULL);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include <SDL3/SDL_close_code.h>

/* end of __WIN32__ and __GDK__ impls */
#elif defined(__WINRT__)

/* WinRT main based on SDL_winrt_main_NonXAML.cpp, placed in the public domain by David Ludwig  3/13/14 */

#include <wrl.h>

/* At least one file in any SDL/WinRT app appears to require compilation
   with C++/CX, otherwise a Windows Metadata file won't get created, and
   an APPX0702 build error can appear shortly after linking.

   The following set of preprocessor code forces this file to be compiled
   as C++/CX, which appears to cause Visual C++ 2012's build tools to
   create this .winmd file, and will help allow builds of SDL/WinRT apps
   to proceed without error.

   If other files in an app's project enable C++/CX compilation, then it might
   be possible for the .cpp file including SDL_main.h to be compiled without /ZW,
   for Visual C++'s build tools to create a winmd file, and for the app to
   build without APPX0702 errors.  In this case, if
   SDL_WINRT_METADATA_FILE_AVAILABLE is defined as a C/C++ macro, then
   the #error (to force C++/CX compilation) will be disabled.

   Please note that /ZW can be specified on a file-by-file basis.  To do this,
   right click on the file in Visual C++, click Properties, then change the
   setting through the dialog that comes up.
*/
#ifndef SDL_WINRT_METADATA_FILE_AVAILABLE
#if !defined(__cplusplus) || !defined(__cplusplus_winrt)
#error The C++ file that includes SDL_main.h must be compiled as C++ code with /ZW, otherwise build errors due to missing .winmd files can occur.
#endif
#endif

/* Prevent MSVC++ from warning about threading models when defining our
   custom WinMain.  The threading model will instead be set via a direct
   call to Windows::Foundation::Initialize (rather than via an attributed
   function).

   To note, this warning (C4447) does not seem to come up unless this file
   is compiled with C++/CX enabled (via the /ZW compiler flag).
*/
#ifdef _MSC_VER
#pragma warning(disable : 4447)
#endif

/* Make sure the function to initialize the Windows Runtime gets linked in. */
#ifdef _MSC_VER
#pragma comment(lib, "runtimeobject.lib")
#endif

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return SDL_RunApp(0, NULL, SDL_main, NULL);
}

/* end of WinRT impl */
#elif defined(__NGAGE__)

/* same typedef as in ngage SDKs e32def.h */
typedef signed int TInt;
/* TODO: if it turns out that this only works when built as C++,
         move __NGAGE__ into the C++ section in SDL_main.h */
TInt E32Main()
{
    return SDL_RunApp(0, NULL, SDL_main, NULL);
}

/* end of __NGAGE__ impl */

#else /* platforms that use a standard main() and just call SDL_RunApp(), like iOS and 3DS */

#include <SDL3/SDL_begin_code.h>

#ifdef __cplusplus
extern "C" {
#endif

int main(int argc, char *argv[])
{
    return SDL_RunApp(argc, argv, SDL_main, NULL);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include <SDL3/SDL_close_code.h>

/* end of impls for standard-conforming platforms */

#endif /* __WIN32__ etc */

#endif /* !defined(SDL_MAIN_USE_CALLBACKS) || defined(SDL_MAIN_CALLBACK_STANDARD) */

/* rename users main() function to SDL_main() so it can be called from the wrappers above */
#define main    SDL_main

#endif /* SDL_MAIN_HANDLED */

#endif /* SDL_main_impl_h_ */
