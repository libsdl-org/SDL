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

#ifdef SDL_THREAD_PS3

/* Semaphores in the PS3 environment */

#include "SDL3/SDL_mutex.h"
#include <sys/sem.h>
#include <ppu-types.h>
#include <sys/systime.h>

/* PS3 LV2 timeout error code */
#ifndef ETIMEDOUT
#define ETIMEDOUT  0x80010006   /* LV2_ETIMEDOUT */
#endif

#ifndef SYS_SEM_NAME_MAX
#define SYS_SEM_NAME_MAX  0x08
#endif

#ifndef SYS_SEM_ID_INVALID
#define SYS_SEM_ID_INVALID  0
#endif

struct SDL_Semaphore
{
    sys_sem_t id;
};

/* Create a counting semaphore */
SDL_Semaphore *SDL_CreateSemaphore(Uint32 initial_value)
{
    SDL_Semaphore *sem = (SDL_Semaphore *) SDL_malloc(sizeof(*sem));
    if (!sem) {
        return NULL;
    }

    sys_sem_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.attr_protocol  = SYS_SEM_ATTR_PROTOCOL;
    attr.attr_pshared   = SYS_SEM_ATTR_PSHARED;
    attr.key            = 0;
    attr.flags          = 0;
    strncpy(attr.name, "SDLsem", SYS_SEM_NAME_MAX);
    // sysSemAttrInitialize(attr);

    int ret = sysSemCreate(&sem->id, &attr, initial_value, 255);
    if (ret != 0) {
        SDL_free(sem);
        return NULL;
    }

    return sem;
}

/* Free the semaphore */
void SDL_DestroySemaphore(SDL_Semaphore * sem)
{
    if (sem) {
        sysSemDestroy(sem->id);
        SDL_free(sem);
    }
}

bool SDL_WaitSemaphoreTimeoutNS(SDL_Semaphore *sem, Sint64 timeoutNS)
{
    if (!sem || sem->id == 0) {
        return false;
    }

    /* No timeout — block forever */
    if (timeoutNS < 0) {
        sysSemWait(sem->id, 0);
        return true;
    }

    /* Try without blocking */
    if (timeoutNS == 0) {
        return sysSemTryWait(sem->id) == 0;
    }

    /* PS3 sys_sem_wait timeout is in microseconds */
    u64 timeoutUS = (u64)(timeoutNS / 1000);

    /* Clamp to at least 1us to avoid accidental non-blocking call */
    if (timeoutUS == 0) {
        timeoutUS = 1;
    }

    int ret = sysSemWait(sem->id, timeoutUS);

    switch (ret) {
        case 0:
            return true;       /* acquired */
        case ETIMEDOUT:
            return false;      /* timed out */
        default:
            SDL_SetError("sys_sem_wait() failed: %d", ret);
            return false;
    }
}

/* Returns the current count of the semaphore */
Uint32 SDL_GetSemaphoreValue(SDL_Semaphore * sem)
{
    s32 val = 0;
    sysSemGetValue(sem->id, &val);
    return (Uint32) val;
}

/* Atomically increases the semaphore's count (not blocking) */
void SDL_SignalSemaphore(SDL_Semaphore * sem)
{
    if (!sem) {
        return;
    }
    
    sysSemPost(sem->id, 1);
}

void SDL_PostSemaphore(SDL_Semaphore *sem)
{
    if (!sem) {
        return;
    }

    if (sem->id == SYS_SEM_ID_INVALID) {
        return;
    }

    sysSemPost(sem->id, 0);
}

#endif // SDL_THREAD_PS3

/* vi: set ts=4 sw=4 expandtab: */
