/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

/**
 *  \file SDL_intrin.h
 *
 *  \brief Header file for CPU intrinsics for SDL
 */

#ifndef SDL_intrin_h_
#define SDL_intrin_h_

#include <SDL3/SDL_stdinc.h>

/* Need to do this here because intrin.h has C++ code in it */
/* Visual Studio 2005 has a bug where intrin.h conflicts with winnt.h */
#if defined(_MSC_VER) && (_MSC_VER >= 1500) && (defined(_M_IX86) || defined(_M_X64))
#ifdef __clang__
/* As of Clang 11, '_m_prefetchw' is conflicting with the winnt.h's version,
   so we define the needed '_m_prefetch' here as a pseudo-header, until the issue is fixed. */

#ifndef __PRFCHWINTRIN_H
#define __PRFCHWINTRIN_H

static __inline__ void __attribute__((__always_inline__, __nodebug__))
_m_prefetch(void *__P)
{
  __builtin_prefetch (__P, 0, 3 /* _MM_HINT_T0 */);
}

#endif /* __PRFCHWINTRIN_H */
#endif /* __clang__ */
#include <intrin.h>
#ifndef _WIN64
#ifndef __MMX__
#define __MMX__
#endif
#endif
#ifndef __SSE__
#define __SSE__
#endif
#ifndef __SSE2__
#define __SSE2__
#endif
#ifndef __SSE3__
#define __SSE3__
#endif
#elif defined(__MINGW64_VERSION_MAJOR)
#include <intrin.h>
#if defined(__ARM_NEON) && !defined(SDL_DISABLE_NEON)
#  include <arm_neon.h>
#endif
#else
/* altivec.h redefining bool causes a number of problems, see bugs 3993 and 4392, so you need to explicitly define SDL_ENABLE_ALTIVEC to have it included. */
#if defined(__ALTIVEC__) && defined(SDL_ENABLE_ALTIVEC)
#include <altivec.h>
#endif
#if !defined(SDL_DISABLE_NEON)
#  if defined(__ARM_NEON)
#    include <arm_neon.h>
#  elif defined(__WINDOWS__) || defined(__WINRT__) || defined(__GDK__)
/* Visual Studio doesn't define __ARM_ARCH, but _M_ARM (if set, always 7), and _M_ARM64 (if set, always 1). */
#    if defined(_M_ARM)
#      include <armintr.h>
#      include <arm_neon.h>
#      define __ARM_NEON 1 /* Set __ARM_NEON so that it can be used elsewhere, at compile time */
#    endif
#    if defined (_M_ARM64)
#      include <arm64intr.h>
#      include <arm64_neon.h>
#      define __ARM_NEON 1 /* Set __ARM_NEON so that it can be used elsewhere, at compile time */
#      define __ARM_ARCH 8
#    endif
#  endif
#endif
#endif /* compiler version */

#if defined(__loongarch_sx) && !defined(SDL_DISABLE_LSX)
#include <lsxintrin.h>
#endif
#if defined(__loongarch_asx) && !defined(SDL_DISABLE_LASX)
#include <lasxintrin.h>
#endif
#if defined(__AVX__) && !defined(SDL_DISABLE_AVX)
#include <immintrin.h>
#endif
#if defined(__MMX__) && !defined(SDL_DISABLE_MMX)
#include <mmintrin.h>
#endif
#if defined(__SSE__) && !defined(SDL_DISABLE_SSE)
#include <xmmintrin.h>
#endif
#if defined(__SSE2__) && !defined(SDL_DISABLE_SSE2)
#include <emmintrin.h>
#endif
#if defined(__SSE3__) && !defined(SDL_DISABLE_SSE3)
#include <pmmintrin.h>
#endif

#endif /* SDL_intrin_h_ */
