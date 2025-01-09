/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

#if defined(__WIN32__) || defined(__WINRT__)

#include "SDL_windows.h"
#include "SDL_error.h"
#include "SDL_assert.h"

#include <objbase.h>  /* for CoInitialize/CoUninitialize (Win32 only) */

#ifndef _WIN32_WINNT_VISTA
#define _WIN32_WINNT_VISTA  0x0600
#endif


/* Sets an error message based on GetLastError() */
int
WIN_SetErrorFromHRESULT(const char *prefix, HRESULT hr)
{
    TCHAR buffer[1024];
    char *message;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, hr, 0,
                  buffer, SDL_arraysize(buffer), NULL);
    message = WIN_StringToUTF8(buffer);
    SDL_SetError("%s%s%s", prefix ? prefix : "", prefix ? ": " : "", message);
    SDL_free(message);
    return -1;
}

/* Sets an error message based on GetLastError() */
int
WIN_SetError(const char *prefix)
{
    return WIN_SetErrorFromHRESULT(prefix, GetLastError());
}

HRESULT
WIN_CoInitialize(void)
{
    /* SDL handles any threading model, so initialize with the default, which
       is compatible with OLE and if that doesn't work, try multi-threaded mode.

       If you need multi-threaded mode, call CoInitializeEx() before SDL_Init()
    */
#ifdef __WINRT__
    /* DLudwig: On WinRT, it is assumed that COM was initialized in main().
       CoInitializeEx is available (not CoInitialize though), however
       on WinRT, main() is typically declared with the [MTAThread]
       attribute, which, AFAIK, should initialize COM.
    */
    return S_OK;
#else
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE) {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    }

    /* S_FALSE means success, but someone else already initialized. */
    /* You still need to call CoUninitialize in this case! */
    if (hr == S_FALSE) {
        return S_OK;
    }

    return hr;
#endif
}

void
WIN_CoUninitialize(void)
{
#ifndef __WINRT__
    CoUninitialize();
#endif
}

#ifndef __WINRT__
static BOOL
IsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
    OSVERSIONINFOEXW osvi;
    DWORDLONG const dwlConditionMask = VerSetConditionMask(
        VerSetConditionMask(
        VerSetConditionMask(
        0, VER_MAJORVERSION, VER_GREATER_EQUAL ),
        VER_MINORVERSION, VER_GREATER_EQUAL ),
        VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL );

    SDL_zero(osvi);
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = wMajorVersion;
    osvi.dwMinorVersion = wMinorVersion;
    osvi.wServicePackMajor = wServicePackMajor;

    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}
#endif

BOOL WIN_IsWindowsVistaOrGreater()
{
#ifdef __WINRT__
    return TRUE;
#else
    return IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_VISTA), LOBYTE(_WIN32_WINNT_VISTA), 0);
#endif
}

#endif /* __WIN32__ */

/* vi: set ts=4 sw=4 expandtab: */
