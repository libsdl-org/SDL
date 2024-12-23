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

/**
 * # CategoryOpenXR
 *
 * Functions for creating OpenXR handles for SDL_gpu contexts.
 *
 * For the most part, OpenXR operates independent of SDL, but 
 * the graphics initialization depends on direct support from SDL_gpu.
 *
 */

#ifndef SDL_openxr_h_
#define SDL_openxr_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_gpu.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

#if defined(OPENXR_H_)
#define NO_SDL_OPENXR_TYPEDEFS 1
#endif /* OPENXR_H_ */

#if !defined(NO_SDL_OPENXR_TYPEDEFS)
#define XR_NULL_HANDLE 0

#if !defined(XR_DEFINE_HANDLE)
    #define XR_DEFINE_HANDLE(object) typedef Uint64 object;
#endif /* XR_DEFINE_HANDLE */

typedef enum XrStructureType {
    XR_TYPE_SESSION_CREATE_INFO = 8,
    XR_TYPE_SWAPCHAIN_CREATE_INFO = 9,
} XrStructureType;

XR_DEFINE_HANDLE(XrInstance)
XR_DEFINE_HANDLE(XrSystemId)
XR_DEFINE_HANDLE(XrSession)
XR_DEFINE_HANDLE(XrSwapchain)

typedef struct {
    XrStructureType type;
    const void* next;
} XrSessionCreateInfo;
typedef struct {
    XrStructureType type;
    const void* next;
} XrSwapchainCreateInfo;

typedef enum XrResult {
    XR_ERROR_FUNCTION_UNSUPPORTED = -7,
    XR_ERROR_HANDLE_INVALID = -12,
} XrResult;
#endif /* NO_SDL_OPENXR_TYPEDEFS */

extern SDL_DECLSPEC bool SDLCALL SDL_XRGPUSupportsProperties(
    SDL_PropertiesID props);

extern SDL_DECLSPEC bool SDLCALL SDL_CreateXRGPUDeviceWithProperties(
    SDL_GPUDevice **device,
    XrInstance *instance,
    XrSystemId *systemId,
    SDL_PropertiesID props);

#define SDL_PROP_GPU_DEVICE_CREATE_XR_VERSION                 "SDL.gpu.device.create.xr.version"
#define SDL_PROP_GPU_DEVICE_CREATE_XR_EXTENSION_COUNT         "SDL.gpu.device.create.xr.extensions.count"
#define SDL_PROP_GPU_DEVICE_CREATE_XR_EXTENSION_NAMES         "SDL.gpu.device.create.xr.extensions.names"
#define SDL_PROP_GPU_DEVICE_CREATE_XR_LAYER_COUNT             "SDL.gpu.device.create.xr.layers.count"
#define SDL_PROP_GPU_DEVICE_CREATE_XR_LAYER_NAMES             "SDL.gpu.device.create.xr.layers.names"
#define SDL_PROP_GPU_DEVICE_CREATE_XR_APPLICATION_NAME        "SDL.gpu.device.create.xr.application.name"
#define SDL_PROP_GPU_DEVICE_CREATE_XR_APPLICATION_VERSION     "SDL.gpu.device.create.xr.application.version"
#define SDL_PROP_GPU_DEVICE_CREATE_XR_ENGINE_NAME             "SDL.gpu.device.create.xr.engine.name"
#define SDL_PROP_GPU_DEVICE_CREATE_XR_ENGINE_VERSION          "SDL.gpu.device.create.xr.engine.version"

extern SDL_DECLSPEC XrResult SDLCALL SDL_CreateGPUXRSession(
    SDL_GPUDevice *device,
    const XrSessionCreateInfo *createinfo,
    XrSession *session);

/* TODO: document this. tl;dr: SDL_gpu picks the format,
         and validates the usageFlags are supported by SDL_gpu,
         but all other fields are fair game */
/* TODO: figure out then document what usageFlags are actually possible to be supported by SDL_gpu */
extern SDL_DECLSPEC XrResult SDLCALL SDL_CreateGPUXRSwapchain(
    SDL_GPUDevice *device,
    XrSession session,
    const XrSwapchainCreateInfo *createinfo, /**< The swapchain create info. */
    SDL_GPUTextureFormat *textureFormat, /**< The texture format that SDL picked. */
    XrSwapchain *swapchain, /**< The created OpenXR swapchain. */
    SDL_GPUTexture ***textures /**< A pointer to where to store the swapchain texture array. */);

extern SDL_DECLSPEC XrResult SDLCALL SDL_DestroyGPUXRSwapchain(SDL_GPUDevice *device, XrSwapchain swapchain, SDL_GPUTexture **swapchainImages);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_openxr_h_ */
