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
#include "SDL_internal.h"

#include <windows.h>

#include "../SDL_syslocale.h"

// using namespace Windows::Graphics::Display;
#include <wchar.h>

bool SDL_SYS_GetPreferredLocales(char *buf, size_t buflen)
{
    WCHAR wbuffer[128] = L"";
    int rc = 0;

    // !!! FIXME: do we not have GetUserPreferredUILanguages on WinPhone or UWP?
#if SDL_WINAPI_FAMILY_PHONE
    rc = GetLocaleInfoEx(LOCALE_NAME_SYSTEM_DEFAULT, LOCALE_SNAME, wbuffer, SDL_arraysize(wbuffer));
#else
    rc = GetSystemDefaultLocaleName(wbuffer, SDL_arraysize(wbuffer));
#endif

    if (rc > 0) {
        // Need to convert LPWSTR to LPSTR, that is wide char to char.
        int i;

        if (((size_t)rc) >= (buflen - 1)) {
            rc = (int)(buflen - 1);
        }
        for (i = 0; i < rc; i++) {
            buf[i] = (char)wbuffer[i]; // assume this was ASCII anyhow.
        }
    }
    return true;
}
