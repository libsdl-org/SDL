/**
 * # CategoryWGPU
 *
 * Experimental video support for WebGPU.
 */

#ifndef SDL_wgpu_h_
#define SDL_wgpu_h_

#include <SDL3/SDL_video.h>

#include <SDL3/SDL_begin_code.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WEBGPU_H_
#define WGPU_OBJECT_ATTRIBUTE(object) typedef struct object##Impl *object;

WGPU_OBJECT_ATTRIBUTE(WGPUSurface)
WGPU_OBJECT_ATTRIBUTE(WGPUInstance)

#undef WGPU_OBJECT_ATTRIBUTE
#endif

/**
 * Create a new WebGPU surface.
 * This will not configure the surface.
 *
 * \param window the window which the surface will attach itself to.
 * \param instance a WebGPU instance.
 * \param surface a pointer to a WGPUSurface handle to output the newly
 *                created WebGPU surface.
 *
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \since This is part of SDL PR #16020, and is therefore not available in regular SDL.
 *
 * \sa SDL_WGPU_DestroySurface
 */
extern SDL_DECLSPEC bool SDLCALL SDL_WGPU_CreateSurface(SDL_Window *window, WGPUInstance instance, WGPUSurface *surface);

/**
 * Destroy a WebGPU surface.
 *
 * \param surface the surface to destroy.
 *
 * \since This is part of SDL PR #16020, and is therefore not available in regular SDL.
 *
 * \sa SDL_WGPU_CreateSurface
 */
extern SDL_DECLSPEC void SDLCALL SDL_WGPU_DestroySurface(WGPUSurface surface);

#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif // SDL_wgpu_h_
