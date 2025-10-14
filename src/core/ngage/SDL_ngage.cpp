/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#include <e32std.h>
#include <e32svr.h>
#include <hal.h>

#ifdef __cplusplus
extern "C" {
#endif

bool NGAGE_IsClassicModel()
{
    int phone_id;
    HAL::Get(HALData::EMachineUid, phone_id);

    return (0x101f8c19 == phone_id);
}

void NGAGE_printf(const char *fmt, ...)
{
    char buffer[512] = { 0 };

    va_list ap;
    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    va_end(ap);

    TBuf<512> buf;
    buf.Copy(TPtrC8((TText8 *)buffer));

    RDebug::Print(_L("%S"), &buf);
}

void NGAGE_vnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    char buffer[512] = { 0 };

    vsprintf(buffer, fmt, ap);

    TBuf<512> tbuf;
    tbuf.Copy(TPtrC8((TText8 *)buffer));

    RDebug::Print(_L("%S"), &tbuf);

    strncpy(buf, buffer, size - 1);
    buf[size - 1] = '\0';
}

TInt NGAGE_GetFreeHeapMemory()
{
    TInt free = 0;
    return User::Available(free);
}

#ifdef __cplusplus
}
#endif
