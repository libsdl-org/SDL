/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL.h"
#include "../SDL_internal.h"

/**
 * \brief Report the alignment this system needs for SIMD allocations.
 *
 * This will return the minimum number of bytes to which a pointer must be
 *  aligned to be compatible with SIMD instructions on the current machine.
 *  For example, if the machine supports SSE only, it will return 16, but if
 *  it supports AVX-512F, it'll return 64 (etc). This only reports values for
 *  instruction sets SDL knows about, so if your SDL build doesn't have
 *  SDL_HasAVX512F(), then it might return 16 for the SSE support it sees and
 *  not 64 for the AVX-512 instructions that exist but SDL doesn't know about.
 *  Plan accordingly.
 */
extern size_t SDL_SIMDGetAlignment(void);

/**
 * \brief Allocate memory in a SIMD-friendly way.
 *
 * This will allocate a block of memory that is suitable for use with SIMD
 *  instructions. Specifically, it will be properly aligned and padded for
 *  the system's supported vector instructions.
 *
 * The memory returned will be padded such that it is safe to read or write
 *  an incomplete vector at the end of the memory block. This can be useful
 *  so you don't have to drop back to a scalar fallback at the end of your
 *  SIMD processing loop to deal with the final elements without overflowing
 *  the allocated buffer.
 *
 * You must free this memory with SDL_FreeSIMD(), not free() or SDL_free()
 *  or delete[], etc.
 *
 * Note that SDL will only deal with SIMD instruction sets it is aware of;
 *  for example, SDL 2.0.8 knows that SSE wants 16-byte vectors
 *  (SDL_HasSSE()), and AVX2 wants 32 bytes (SDL_HasAVX2()), but doesn't
 *  know that AVX-512 wants 64. To be clear: if you can't decide to use an
 *  instruction set with an SDL_Has*() function, don't use that instruction
 *  set with memory allocated through here.
 *
 * SDL_AllocSIMD(0) will return a non-NULL pointer, assuming the system isn't
 *  out of memory.
 *
 *  \param len The length, in bytes, of the block to allocated. The actual
 *             allocated block might be larger due to padding, etc.
 * \return Pointer to newly-allocated block, NULL if out of memory.
 *
 * \sa SDL_SIMDAlignment
 * \sa SDL_SIMDFree
 */
extern void * SDL_SIMDAlloc(const size_t len);

/**
 * \brief Deallocate memory obtained from SDL_SIMDAlloc
 *
 * It is not valid to use this function on a pointer from anything but
 *  SDL_SIMDAlloc(). It can't be used on pointers from malloc, realloc,
 *  SDL_malloc, memalign, new[], etc.
 *
 * However, SDL_SIMDFree(NULL) is a legal no-op.
 *
 * \sa SDL_SIMDAlloc
 */
extern void SDL_SIMDFree(void *ptr);

/* vi: set ts=4 sw=4 expandtab: */

