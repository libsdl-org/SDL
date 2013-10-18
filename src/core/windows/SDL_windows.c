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
#include "SDL_config.h"

#ifdef __WIN32__

#include "SDL_windows.h"
#include "SDL_error.h"
#include "SDL_assert.h"

#include <objbase.h>  /* for CoInitialize/CoUninitialize */

/* Sets an error message based on GetLastError() */
int
WIN_SetError(const char *prefix)
{
    TCHAR buffer[1024];
    char *message;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
                  buffer, SDL_arraysize(buffer), NULL);
    message = WIN_StringToUTF8(buffer);
    SDL_SetError("%s%s%s", prefix ? prefix : "", prefix ? ": " : "", message);
    SDL_free(message);
    return -1;
}

HRESULT
WIN_CoInitialize(void)
{
    const HRESULT hr = CoInitialize(NULL);

    /* S_FALSE means success, but someone else already initialized. */
    /* You still need to call CoUninitialize in this case! */
    if (hr == S_FALSE) {
        return S_OK;
    }

    return hr;
}

void
WIN_CoUninitialize(void)
{
    CoUninitialize();
}

#endif /* __WIN32__ */

/* vi: set ts=4 sw=4 expandtab: */
