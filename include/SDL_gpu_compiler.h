/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_gpu_compiler_h_
#define SDL_gpu_compiler_h_

/**
 *  \file SDL_gpu_compiler.h
 *
 *  Header for the SDL GPU compiler routines.
 */

#include "SDL_gpu.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* !!! FIXME: this all needs formal (and significantly more robust) documentation. */

/*
 * This builds shader source code into bytecode. One could use this to cook
 * shaders offline, or pass dynamic strings at runtime. This is meant to favor
 * speed over optimization. If one really wants a strong optimizing compiler,
 * one should build an external tool.  :)
 */
int MOJOGPU_CompileShader(const char *src, const Uint32 srclen, const char *type, const char *mainfn, Uint8 **result, Uint32 *resultlen);

/* !!! FIXME: There's probably a lot of other stuff we want to put in here. */

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* SDL_gpu_compiler_h_ */

/* vi: set ts=4 sw=4 expandtab: */
