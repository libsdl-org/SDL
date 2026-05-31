/**
 * # CategoryWGPU
 *
 * Experimental support for the wgpu-native C bindings.
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
 * Create a new WGPU surface.
 *
 * \param window The window which the surface will attach itself to.
 * \param instance The WGPU instance.
 *
 * \returns A WGPUSurface. Can be NULL if there was a failure.
 */
extern SDL_DECLSPEC WGPUSurface SDLCALL SDL_WGPU_CreateSurface(SDL_Window *window, WGPUInstance instance);

#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif // SDL_wgpu_h_
