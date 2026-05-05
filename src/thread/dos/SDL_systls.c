/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_THREAD_DOS

/* DOS thread-local storage — uses per-thread static array indexed by
   the scheduler's thread ID. With only DOS_MAX_THREADS (8) threads,
   a simple static array is efficient and avoids any dynamic allocation. */

#include "../../core/dos/SDL_dos_scheduler.h"
#include "../SDL_thread_c.h"

static SDL_TLSData *tls_data[DOS_MAX_THREADS];

void SDL_SYS_InitTLSData(void)
{
    SDL_memset(tls_data, 0, sizeof(tls_data));
}

SDL_TLSData *SDL_SYS_GetTLSData(void)
{
    int tid = DOS_GetCurrentThreadID();
    if (tid < 0 || tid >= DOS_MAX_THREADS) {
        return NULL;
    }
    return tls_data[tid];
}

bool SDL_SYS_SetTLSData(SDL_TLSData *data)
{
    int tid = DOS_GetCurrentThreadID();
    if (tid < 0 || tid >= DOS_MAX_THREADS) {
        return SDL_SetError("Invalid thread ID for TLS");
    }
    tls_data[tid] = data;
    return true;
}

void SDL_SYS_QuitTLSData(void)
{
    SDL_memset(tls_data, 0, sizeof(tls_data));
}

#endif /* SDL_THREAD_DOS */
