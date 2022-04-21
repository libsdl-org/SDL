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
#include "../SDL_internal.h"

#ifndef SDL_sysgpu_h_
#define SDL_sysgpu_h_

#define SDL_SUPPRESS_GPU_API_UNSTABLE_WARNING 1  /* !!! FIXME: remove later */
#include "SDL_gpu.h"
#include "../SDL_hashtable.h"

struct SDL_GpuDevice
{
//    SDL_GpuBuffer *(*CreateCPUBuffer)(SDL_GpuDevice *_this, const Uint32 buflen);

};

struct SDL_GpuPipeline
{
    SDL_GpuPipelineDescription desc;
};

struct SDL_GpuSampler
{
    SDL_GpuSamplerDescription desc;
};

/* Multiple mutexes might be overkill, but there's no reason to
   block all caches when one is being accessed. */
struct SDL_GpuStateCache
{
    const char *label;
    SDL_GpuDevice *device;
    SDL_mutex *pipeline_mutex;
    SDL_HashTable *pipeline_cache;
    SDL_mutex *sampler_mutex;
    SDL_HashTable *sampler_cache;
};


#endif /* SDL_sysgpu_h_ */

/* vi: set ts=4 sw=4 expandtab: */
