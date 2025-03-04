/*
  // Simple DirectMedia Layer
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

/**
 * # CategoryWebGPU
**/

/*
NOTE: Some of the enum conversion functions are a nightmare to manage between the different flavours of webgpu.h

wgpu-native source code pulls it's header from here: https://github.com/webgpu-native/webgpu-headers/blob/main/webgpu.h
however the header provided in the release version of the library: https://github.com/gfx-rs/wgpu-native/releases
have some changes to the enum values that kind of screw everything up.

Gotta wait until the header is finalized to get this fixed.

webgpu.h from webgpu-headers SHOULD work for all backends, except for Emscripten and wgpu-native apparently!
So as I suspected, Dawn would have been the best pick to work with like I had originally done before swapping to wgpu-native out of convenience.
*/

#ifndef __EMSCRIPTEN__
// #include "../../webgpu-headers/webgpu.h"
#include "../../wgpu/include/webgpu/webgpu.h"
#endif // webgpu.h is provided by emscripten when using emcc with -sUSE_WEBGPU=1
 
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

extern SDL_DECLSPEC bool SDLCALL SDL_WebGPU_CreateSurface(SDL_Window *window,
                                                          WGPUInstance instance,
                                                          WGPUSurface *retSurface);
