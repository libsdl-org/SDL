/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

/* This internal header provides access to the vendored OpenXR headers
 * without requiring include path modifications in project files.
 * Similar to SDL_vulkan_internal.h for Vulkan headers.
 */

#ifndef SDL_openxr_internal_h_
#define SDL_openxr_internal_h_

#include "SDL_internal.h"

/* Define platform-specific OpenXR macros BEFORE including openxr headers */
#ifdef SDL_PLATFORM_ANDROID
#include <jni.h>
#define XR_USE_PLATFORM_ANDROID
#endif

/* Include the vendored OpenXR headers using relative path */
#include "../../video/khronos/openxr/openxr.h"
#include "../../video/khronos/openxr/openxr_platform.h"

/* Compatibility: XR_API_VERSION_1_0 was added in OpenXR 1.1.x */
#ifndef XR_API_VERSION_1_0
#define XR_API_VERSION_1_0 XR_MAKE_VERSION(1, 0, XR_VERSION_PATCH(XR_CURRENT_API_VERSION))
#endif

#define SDL_OPENXR_CHECK_VERSION(x, y, z)                                                               \
    (XR_VERSION_MAJOR(XR_CURRENT_API_VERSION) > x ||                                                    \
     (XR_VERSION_MAJOR(XR_CURRENT_API_VERSION) == x && XR_VERSION_MINOR(XR_CURRENT_API_VERSION) > y) || \
     (XR_VERSION_MAJOR(XR_CURRENT_API_VERSION) == x && XR_VERSION_MINOR(XR_CURRENT_API_VERSION) == y && XR_VERSION_PATCH(XR_CURRENT_API_VERSION) >= z))

#endif /* SDL_openxr_internal_h_ */
