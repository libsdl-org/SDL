/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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
#ifndef _SDL_vulkan_internal_h
#define _SDL_vulkan_internal_h

#include "../SDL_internal.h"

#include "SDL_stdinc.h"

#if defined(SDL_LOADSO_DISABLED)
#undef SDL_VIDEO_VULKAN_SURFACE
#define SDL_VIDEO_VULKAN_SURFACE 0
#endif

#if SDL_VIDEO_DRIVER_ANDROID
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#if SDL_VIDEO_DRIVER_COCOA
#define VK_USE_PLATFORM_MACOS_MVK
#endif
#if SDL_VIDEO_DRIVER_MIR
#define VK_USE_PLATFORM_MIR_KHR
#endif
#if SDL_VIDEO_DRIVER_UIKIT
#define VK_USE_PLATFORM_IOS_MVK
#endif
#if SDL_VIDEO_DRIVER_WAYLAND
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#if SDL_VIDEO_DRIVER_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#if SDL_VIDEO_DRIVER_X11
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XCB_KHR
#endif

#if SDL_VIDEO_VULKAN_SURFACE

/* Need vulkan.h for the following declarations. Must ensure the first
 * inclusion of vulkan has the appropriate USE_PLATFORM defined, hence
 * the above. */
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

extern const char *SDL_Vulkan_GetResultString(VkResult result);

extern VkExtensionProperties *SDL_Vulkan_CreateInstanceExtensionsList(
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties,
    Uint32 *extensionCount); /* free returned list with SDL_free */

/* Implements functionality of SDL_Vulkan_GetInstanceExtensions for a list of
 * names passed in nameCount and names. */
extern SDL_bool SDL_Vulkan_GetInstanceExtensions_Helper(unsigned *userCount,
                                                        const char **userNames,
                                                        unsigned nameCount,
                                                        const char *const *names);

#endif

#endif
/* vi: set ts=4 sw=4 expandtab: */
