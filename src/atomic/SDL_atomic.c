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

#include "SDL_atomic.h"

/* Note that we undefine the atomic operations here, in case they are
   defined as compiler intrinsics while building SDL but the library user
   doesn't have that compiler.  That way we always have a working set of
   atomic operations built into the library.
*/
#undef SDL_AtomicCAS
#undef SDL_AtomicCASPtr

/*
  If any of the operations are not provided then we must emulate some
  of them. That means we need a nice implementation of spin locks
  that avoids the "one big lock" problem. We use a vector of spin
  locks and pick which one to use based on the address of the operand
  of the function.

  To generate the index of the lock we first shift by 3 bits to get
  rid on the zero bits that result from 32 and 64 bit allignment of
  data. We then mask off all but 5 bits and use those 5 bits as an
  index into the table.

  Picking the lock this way insures that accesses to the same data at
  the same time will go to the same lock. OTOH, accesses to different
  data have only a 1/32 chance of hitting the same lock. That should
  pretty much eliminate the chances of several atomic operations on
  different data from waiting on the same "big lock". If it isn't
  then the table of locks can be expanded to a new size so long as
  the new size is a power of two.

  Contributed by Bob Pendleton, bob@pendleton.com
*/

static SDL_SpinLock locks[32];

static SDL_INLINE void
enterLock(void *a)
{
    uintptr_t index = ((((uintptr_t)a) >> 3) & 0x1f);

    SDL_AtomicLock(&locks[index]);
}

static SDL_INLINE void
leaveLock(void *a)
{
    uintptr_t index = ((((uintptr_t)a) >> 3) & 0x1f);

    SDL_AtomicUnlock(&locks[index]);
}

DECLSPEC SDL_bool SDLCALL
SDL_AtomicCAS(SDL_atomic_t *a, int oldval, int newval)
{
    SDL_bool retval = SDL_FALSE;

    enterLock(a);
    if (a->value == oldval) {
        a->value = newval;
        retval = SDL_TRUE;
    }
    leaveLock(a);

    return retval;
}

DECLSPEC SDL_bool SDLCALL
SDL_AtomicCASPtr(void **a, void *oldval, void *newval)
{
    SDL_bool retval = SDL_FALSE;

    enterLock(a);
    if (*a == oldval) {
        *a = newval;
        retval = SDL_TRUE;
    }
    leaveLock(a);

    return retval;
}

#if defined(__GNUC__) && defined(__arm__) && \
   (defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6T2__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__))
__asm__(
"   .align 2\n"
"   .globl _SDL_MemoryBarrierRelease\n"
"   .globl _SDL_MemoryBarrierAcquire\n"
"_SDL_MemoryBarrierRelease:\n"
"_SDL_MemoryBarrierAcquire:\n"
"   mov r0, #0\n"
"   mcr p15, 0, r0, c7, c10, 5\n"
"   bx lr\n"
);
#endif /* __GNUC__ && __arm__ && ARMV6 */

/* vi: set ts=4 sw=4 expandtab: */
