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

#define PFN_xrGetInstanceProcAddr SDL_FunctionPointer
#endif /* NO_SDL_OPENXR_TYPEDEFS */

/**
 * Creates an OpenXR session. The OpenXR system ID is pulled from the passed GPU context.
 * 
 * \param device a GPU context.
 * \param createinfo the create info for the OpenXR session, sans the system ID.
 * \param session an OpenXR session created for the given **device**.
 * \returns the result of the call.
 * 
 * \sa SDL_CreateGPUDeviceWithProperties
 */
extern SDL_DECLSPEC XrResult SDLCALL SDL_CreateGPUXRSession(
    SDL_GPUDevice *device,
    const XrSessionCreateInfo *createinfo,
    XrSession *session);

/**
 * Creates an OpenXR swapchain.
 * 
 * \param device a GPU context.
 * \param session an OpenXR session created for the given **device**.
 * \param createinfo the create info for the OpenXR swapchain, sans the format.
 * \param textureFormat a pointer to store the format of the created swapchain.
 * \param swapchain a pointer to store the created OpenXR swapchain.
 * \param textures a pointer to store the list of created swapchain images.
 * \returns the result of the call.
 * 
 * \sa SDL_CreateGPUDeviceWithProperties
 * \sa SDL_CreateGPUXRSession
 * \sa SDL_DestroyGPUXRSwapchain
 */

/* TODO: figure out then document what usageFlags are actually possible to be supported by SDL_gpu */
extern SDL_DECLSPEC XrResult SDLCALL SDL_CreateGPUXRSwapchain(
    SDL_GPUDevice *device,
    XrSession session,
    const XrSwapchainCreateInfo *createinfo, 
    SDL_GPUTextureFormat *textureFormat,
    XrSwapchain *swapchain,
    SDL_GPUTexture ***textures);

/**
 * Destroys and OpenXR swapchain previously returned by SDL_CreateGPUXRSwapchain.
 * 
 * \param device a GPU context.
 * \param swapchain a swapchain previously returned by SDL_CreateGPUXRSwapchain.
 * \param swapchainImages an array of swapchain images returned by the same call to SDL_CreateGPUXRSwapchain.
 * \returns the result of the call.
 * 
 * \sa SDL_CreateGPUDeviceWithProperties
 * \sa SDL_CreateGPUXRSession
 * \sa SDL_CreateGPUXRSwapchain
 */
extern SDL_DECLSPEC XrResult SDLCALL SDL_DestroyGPUXRSwapchain(SDL_GPUDevice *device, XrSwapchain swapchain, SDL_GPUTexture **swapchainImages);

/**
 * Dynamically load the OpenXR loader. This can be called at any time.
 * 
 * SDL keeps a reference count of the OpenXR loader, calling this function multiple 
 * times will increment that count, rather than loading the library multiple times.
 * 
 * If not called, this will be implicitly called when creating a GPU device with OpenXR.
 * 
 * This function will use the platform default OpenXR loader name, 
 * unless the `SDL_HINT_OPENXR_SONAMES` hint is set.
 * 
 * \returns whether the call succeeded or not.
 * 
 * \threadsafety This function is not thread safe.
 * 
 * \sa SDL_HINT_OPENXR_SONAMES
 */
extern SDL_DECLSPEC bool SDLCALL SDL_OpenXR_LoadLibrary(void);

/**
 * Unload the OpenXR loader previously loaded by SDL_OpenXR_LoadLibrary.
 * 
 * SDL keeps a reference count of the OpenXR loader, calling this function will decrement that count. 
 * Once the reference count reaches zero, the library is unloaded.
 * 
 * \threadsafety This function is not thread safe.
 */
extern SDL_DECLSPEC void SDLCALL SDL_OpenXR_UnloadLibrary(void);

/**
 * Get the address of the `xrGetInstanceProcAddr` function.
 * 
 * This should be called after either calling SDL_OpenXR_LoadLibrary() or
 * creating an OpenXR SDL_GPUDevice.
 * 
 * The actual type of the returned function pointer is PFN_xrGetInstanceProcAddr, 
 * but that isn't always available. You should include the OpenXR headers before this header, 
 * or cast the return value of this function to the correct type.
 * 
 * \returns the function pointer for `xrGetInstanceProcAddr` or NULL on
 *          failure; call SDL_GetError() for more information.
 */
extern SDL_DECLSPEC PFN_xrGetInstanceProcAddr SDLCALL SDL_OpenXR_GetXrGetInstanceProcAddr(void);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_openxr_h_ */
