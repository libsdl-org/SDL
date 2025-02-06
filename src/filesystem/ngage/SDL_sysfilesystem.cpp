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
#ifdef __cplusplus
extern "C" {
#endif

#include "SDL_internal.h"

#ifdef __cplusplus
}
#endif

#include <e32base.h>
#include <e32std.h>
#include <f32file.h>
#include <utf.h>

#ifdef __cplusplus
extern "C" {
#endif

void NGAGE_GetAppPath(char* path)
{
    TBuf<512> aPath;

    TFileName fullExePath = RProcess().FileName();

    TParsePtrC parser(fullExePath);
    aPath.Copy(parser.DriveAndPath());

    TBuf8<512> utf8Path; // Temporary buffer for UTF-8 data.
    CnvUtfConverter::ConvertFromUnicodeToUtf8(utf8Path, aPath);

    // Copy UTF-8 data to the provided char* buffer.
    strncpy(path, (const char*)utf8Path.Ptr(), utf8Path.Length());
    path[utf8Path.Length()] = '\0';

    // Replace backslashes with forward slashes.
    for (int i = 0; i < utf8Path.Length(); i++)
    {
        if (path[i] == '\\')
        {
            path[i] = '/';
        }
    }
}

#ifdef __cplusplus
}
#endif
