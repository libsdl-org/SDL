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
#ifndef SDL_internal_h_
#define SDL_internal_h_

/* Many of SDL's features require _GNU_SOURCE on various platforms */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Need this so Linux systems define fseek64o, ftell64o and off64_t */
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1
#endif

/* This is for a variable-length array at the end of a struct:
    struct x { int y; char z[SDL_VARIABLE_LENGTH_ARRAY]; };
   Use this because GCC 2 needs different magic than other compilers. */
#if (defined(__GNUC__) && (__GNUC__ <= 2)) || defined(__CC_ARM) || defined(__cplusplus)
#define SDL_VARIABLE_LENGTH_ARRAY 1
#else
#define SDL_VARIABLE_LENGTH_ARRAY
#endif

#if (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))) || defined(__clang__)
#define HAVE_GCC_DIAGNOSTIC_PRAGMA 1
#endif

#ifdef _MSC_VER /* We use constant comparison for generated code */
#pragma warning(disable : 6326)
#endif

#ifdef _MSC_VER /* SDL_MAX_SMALL_ALLOC_STACKSIZE is smaller than _ALLOCA_S_THRESHOLD and should be generally safe */
#pragma warning(disable : 6255)
#endif
#define SDL_MAX_SMALL_ALLOC_STACKSIZE          128
#define SDL_small_alloc(type, count, pisstack) ((*(pisstack) = ((sizeof(type) * (count)) < SDL_MAX_SMALL_ALLOC_STACKSIZE)), (*(pisstack) ? SDL_stack_alloc(type, count) : (type *)SDL_malloc(sizeof(type) * (count))))
#define SDL_small_free(ptr, isstack) \
    if ((isstack)) {                 \
        SDL_stack_free(ptr);         \
    } else {                         \
        SDL_free(ptr);               \
    }

#include "build_config/SDL_build_config.h"

#include "dynapi/SDL_dynapi.h"

#if SDL_DYNAMIC_API
#include "dynapi/SDL_dynapi_overrides.h"
/* force SDL_DECLSPEC off...it's all internal symbols now.
   These will have actual #defines during SDL_dynapi.c only */
#ifdef SDL_DECLSPEC
#undef SDL_DECLSPEC
#endif
#define SDL_DECLSPEC
#endif

#ifdef SDL_PLATFORM_APPLE
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1 /* for memset_pattern4() */
#endif
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#elif defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STRING_H
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_FLOAT_H
#include <float.h>
#endif

/* If you run into a warning that O_CLOEXEC is redefined, update the SDL configuration header for your platform to add HAVE_O_CLOEXEC */
#ifndef HAVE_O_CLOEXEC
#define O_CLOEXEC 0
#endif

/* A few #defines to reduce SDL footprint.
   Only effective when library is statically linked.
   You have to manually edit this file. */
#ifndef SDL_LEAN_AND_MEAN
#define SDL_LEAN_AND_MEAN 0
#endif

/* Optimized functions from 'SDL_blit_0.c'
   - blit with source bits_per_pixel < 8, palette */
#ifndef SDL_HAVE_BLIT_0
#define SDL_HAVE_BLIT_0 !SDL_LEAN_AND_MEAN
#endif

/* Optimized functions from 'SDL_blit_1.c'
   - blit with source bytes_per_pixel == 1, palette */
#ifndef SDL_HAVE_BLIT_1
#define SDL_HAVE_BLIT_1 !SDL_LEAN_AND_MEAN
#endif

/* Optimized functions from 'SDL_blit_A.c'
   - blit with 'SDL_BLENDMODE_BLEND' blending mode */
#ifndef SDL_HAVE_BLIT_A
#define SDL_HAVE_BLIT_A !SDL_LEAN_AND_MEAN
#endif

/* Optimized functions from 'SDL_blit_N.c'
   - blit with COLORKEY mode, or nothing */
#ifndef SDL_HAVE_BLIT_N
#define SDL_HAVE_BLIT_N !SDL_LEAN_AND_MEAN
#endif

/* Optimized functions from 'SDL_blit_N.c'
   - RGB565 conversion with Lookup tables */
#ifndef SDL_HAVE_BLIT_N_RGB565
#define SDL_HAVE_BLIT_N_RGB565 !SDL_LEAN_AND_MEAN
#endif

/* Optimized functions from 'SDL_blit_AUTO.c'
   - blit with modulate color, modulate alpha, any blending mode
   - scaling or not */
#ifndef SDL_HAVE_BLIT_AUTO
#define SDL_HAVE_BLIT_AUTO !SDL_LEAN_AND_MEAN
#endif

/* Run-Length-Encoding
   - SDL_SetSurfaceColorKey() called with SDL_RLEACCEL flag */
#ifndef SDL_HAVE_RLE
#define SDL_HAVE_RLE !SDL_LEAN_AND_MEAN
#endif

/* Software SDL_Renderer
   - creation of software renderer
   - *not* general blitting functions
   - {blend,draw}{fillrect,line,point} internal functions */
#ifndef SDL_VIDEO_RENDER_SW
#define SDL_VIDEO_RENDER_SW !SDL_LEAN_AND_MEAN
#endif

/* YUV formats
   - handling of YUV surfaces
   - blitting and conversion functions */
#ifndef SDL_HAVE_YUV
#define SDL_HAVE_YUV !SDL_LEAN_AND_MEAN
#endif

#ifndef SDL_RENDER_DISABLED
/* define the not defined ones as 0 */
#ifndef SDL_VIDEO_RENDER_D3D
#define SDL_VIDEO_RENDER_D3D 0
#endif
#ifndef SDL_VIDEO_RENDER_D3D11
#define SDL_VIDEO_RENDER_D3D11 0
#endif
#ifndef SDL_VIDEO_RENDER_D3D12
#define SDL_VIDEO_RENDER_D3D12 0
#endif
#ifndef SDL_VIDEO_RENDER_METAL
#define SDL_VIDEO_RENDER_METAL 0
#endif
#ifndef SDL_VIDEO_RENDER_OGL
#define SDL_VIDEO_RENDER_OGL  0
#endif
#ifndef SDL_VIDEO_RENDER_OGL_ES2
#define SDL_VIDEO_RENDER_OGL_ES2 0
#endif
#ifndef SDL_VIDEO_RENDER_PS2
#define SDL_VIDEO_RENDER_PS2 0
#endif
#ifndef SDL_VIDEO_RENDER_PSP
#define SDL_VIDEO_RENDER_PSP 0
#endif
#ifndef SDL_VIDEO_RENDER_VITA_GXM
#define SDL_VIDEO_RENDER_VITA_GXM 0
#endif
#ifndef SDL_VIDEO_RENDER_VULKAN
#define SDL_VIDEO_RENDER_VULKAN 0
#endif
#else /* define all as 0 */
#undef SDL_VIDEO_RENDER_SW
#define SDL_VIDEO_RENDER_SW 0
#undef SDL_VIDEO_RENDER_D3D
#define SDL_VIDEO_RENDER_D3D 0
#undef SDL_VIDEO_RENDER_D3D11
#define SDL_VIDEO_RENDER_D3D11 0
#undef SDL_VIDEO_RENDER_D3D12
#define SDL_VIDEO_RENDER_D3D12 0
#undef SDL_VIDEO_RENDER_METAL
#define SDL_VIDEO_RENDER_METAL 0
#undef SDL_VIDEO_RENDER_OGL
#define SDL_VIDEO_RENDER_OGL  0
#undef SDL_VIDEO_RENDER_OGL_ES2
#define SDL_VIDEO_RENDER_OGL_ES2 0
#undef SDL_VIDEO_RENDER_PS2
#define SDL_VIDEO_RENDER_PS2 0
#undef SDL_VIDEO_RENDER_PSP
#define SDL_VIDEO_RENDER_PSP 0
#undef SDL_VIDEO_RENDER_VITA_GXM
#define SDL_VIDEO_RENDER_VITA_GXM 0
#undef SDL_VIDEO_RENDER_VULKAN
#define SDL_VIDEO_RENDER_VULKAN 0
#endif /* SDL_RENDER_DISABLED */

#define SDL_HAS_RENDER_DRIVER \
       (SDL_VIDEO_RENDER_SW       | \
        SDL_VIDEO_RENDER_D3D      | \
        SDL_VIDEO_RENDER_D3D11    | \
        SDL_VIDEO_RENDER_D3D12    | \
        SDL_VIDEO_RENDER_METAL    | \
        SDL_VIDEO_RENDER_OGL      | \
        SDL_VIDEO_RENDER_OGL_ES2  | \
        SDL_VIDEO_RENDER_PS2      | \
        SDL_VIDEO_RENDER_PSP      | \
        SDL_VIDEO_RENDER_VITA_GXM | \
        SDL_VIDEO_RENDER_VULKAN )

#if !defined(SDL_RENDER_DISABLED) && !SDL_HAS_RENDER_DRIVER
#error SDL_RENDER enabled without any backend drivers.
#endif

#if !defined(HAVE_LIBC)
// If not using _any_ C runtime, these have to be defined before SDL_thread.h
// gets included, so internal SDL_CreateThread calls will not try to reference
// the (unavailable and unneeded) _beginthreadex/_endthreadex functions.
#define SDL_BeginThreadFunction NULL
#define SDL_EndThreadFunction NULL
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_intrin.h>

#define SDL_MAIN_NOIMPL /* don't drag in header-only implementation of SDL_main */
#include <SDL3/SDL_main.h>

#include "SDL_utils_c.h"

/* The internal implementations of these functions have up to nanosecond precision.
   We can expose these functions as part of the API if we want to later.
*/
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

extern int SDLCALL SDL_WaitSemaphoreTimeoutNS(SDL_Semaphore *sem, Sint64 timeoutNS);
extern int SDLCALL SDL_WaitConditionTimeoutNS(SDL_Condition *cond, SDL_Mutex *mutex, Sint64 timeoutNS);
extern SDL_bool SDLCALL SDL_WaitEventTimeoutNS(SDL_Event *event, Sint64 timeoutNS);

/* Queue `memory` to be passed to SDL_free once the event queue is emptied.
   this manages the list of pointers to SDL_AllocateEventMemory, but you
   can use it to queue pointers from other subsystems that can die at any
   moment but definitely need to live long enough for the app to copy them
   if they happened to query them in their last moments. */
extern void *SDL_FreeLater(void *memory);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif /* SDL_internal_h_ */
