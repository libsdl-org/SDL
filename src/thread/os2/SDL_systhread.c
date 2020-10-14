/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_THREAD_OS2

/* Thread management routines for SDL */

#include "SDL_thread.h"
#include "../SDL_systhread.h"
#include "../SDL_thread_c.h"
#include "../SDL_systhread.h"
#include "SDL_systls_c.h"
#include "../../core/os2/SDL_os2.h"

#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#include <os2.h>

#ifndef SDL_PASSED_BEGINTHREAD_ENDTHREAD
/* We'll use the C library from this DLL */
#include <process.h>

/* Cygwin gcc-3 ... MingW64 (even with a i386 host) does this like MSVC. */
#if (defined(__MINGW32__) && (__GNUC__ < 4))
typedef unsigned long (__cdecl *pfnSDL_CurrentBeginThread) (void *, unsigned,
        unsigned (__stdcall *func)(void *), void *arg,
        unsigned, unsigned *threadID);
typedef void (__cdecl *pfnSDL_CurrentEndThread)(unsigned code);

#elif defined(__WATCOMC__)
/* This is for Watcom targets except OS2 */
#if __WATCOMC__ < 1240
#define __watcall
#endif
typedef unsigned long (__watcall * pfnSDL_CurrentBeginThread) (void *,
                                                               unsigned,
                                                               unsigned
                                                               (__stdcall *
                                                                func) (void
                                                                       *),
                                                               void *arg,
                                                               unsigned,
                                                               unsigned
                                                               *threadID);
typedef void (__watcall * pfnSDL_CurrentEndThread) (unsigned code);

#else
typedef uintptr_t(__cdecl * pfnSDL_CurrentBeginThread) (void *, unsigned,
                                                        unsigned (__stdcall *
                                                                  func) (void
                                                                         *),
                                                        void *arg, unsigned,
                                                        unsigned *threadID);
typedef void (__cdecl * pfnSDL_CurrentEndThread) (unsigned code);
#endif
#endif /* !SDL_PASSED_BEGINTHREAD_ENDTHREAD */


typedef struct ThreadStartParms {
  void *args;
  pfnSDL_CurrentEndThread pfnCurrentEndThread;
} tThreadStartParms, *pThreadStartParms;

static void RunThread(void *data)
{
  pThreadStartParms           pThreadParms = (pThreadStartParms) data;
  pfnSDL_CurrentEndThread     pfnEndThread = pThreadParms->pfnCurrentEndThread;
  void                        *args = pThreadParms->args;

  SDL_free( pThreadParms );

  if ( ppSDLTLSData != NULL )
    *ppSDLTLSData = NULL;

  SDL_RunThread( args );

  if ( pfnEndThread != NULL )
    pfnEndThread();
}



#ifdef SDL_PASSED_BEGINTHREAD_ENDTHREAD
int
SDL_SYS_CreateThread(SDL_Thread * thread, void *args,
                     pfnSDL_CurrentBeginThread pfnBeginThread,
                     pfnSDL_CurrentEndThread pfnEndThread)
#else
int
SDL_SYS_CreateThread(SDL_Thread * thread, void *args)
#endif /* SDL_PASSED_BEGINTHREAD_ENDTHREAD */
{
  pThreadStartParms    pThreadParms = SDL_malloc( sizeof(tThreadStartParms) );

  if ( pThreadParms == NULL )
    return SDL_OutOfMemory();

  // Save the function which we will have to call to clear the RTL of calling app!
  pThreadParms->pfnCurrentEndThread = pfnEndThread;
  // Also save the real parameters we have to pass to thread function
  pThreadParms->args = args;

  // Start the thread using the runtime library of calling app!
  thread->handle = (SYS_ThreadHandle)
  #ifdef SDL_PASSED_BEGINTHREAD_ENDTHREAD
    ( (size_t) pfnBeginThread( RunThread, NULL, 65535, pThreadParms ) );
  #else
    _beginthread( RunThread, NULL, 65535, pThreadParms );
  #endif

  if ( thread->handle == -1 )
      return SDL_SetError( "Not enough resources to create thread" );

  return 0;
}

void
SDL_SYS_SetupThread(const char *name)
{
    return;
}

SDL_threadID
SDL_ThreadID(void)
{
  PTIB       tib;
  PPIB       pib;

  DosGetInfoBlocks( &tib, &pib );
  return tib->tib_ptib2->tib2_ultid;
}

int
SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority)
{
  ULONG      ulRC;

  ulRC = DosSetPriority( PRTYS_THREAD,
                         priority == SDL_THREAD_PRIORITY_LOW
                           ? PRTYC_IDLETIME
                           : priority == SDL_THREAD_PRIORITY_HIGH
                               ? PRTYC_TIMECRITICAL 
                               : PRTYC_REGULAR ,
                         0, 0 );
  if ( ulRC != NO_ERROR )
    return SDL_SetError( "DosSetPriority() failed, rc = %u", ulRC );

  return 0;
}

void
SDL_SYS_WaitThread(SDL_Thread * thread)
{
  ULONG      ulRC = DosWaitThread( (PTID)&thread->handle, DCWW_WAIT );

  if ( ulRC != NO_ERROR )
    debug( "DosWaitThread() failed, rc = %u", ulRC );
}

void
SDL_SYS_DetachThread(SDL_Thread * thread)
{
  return;
}

#endif /* SDL_THREAD_OS2 */

/* vi: set ts=4 sw=4 expandtab: */
