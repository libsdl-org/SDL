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

#ifndef SDL_openxrdyn_h_
#define SDL_openxrdyn_h_

/* Use the internal header for vendored OpenXR includes */
#include "SDL_openxr_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XrInstancePfns
{
#define SDL_OPENXR_INSTANCE_SYM(name) \
    PFN_##name name;
#include "SDL_openxrsym.h"
} XrInstancePfns;

extern XrInstancePfns *SDL_OPENXR_LoadInstanceSymbols(XrInstance instance);

/* Define the function pointers */
#define SDL_OPENXR_SYM(name) \
    extern PFN_##name OPENXR_##name;
#include "SDL_openxrsym.h"

#define xrGetInstanceProcAddr                  OPENXR_xrGetInstanceProcAddr
#define xrEnumerateApiLayerProperties          OPENXR_xrEnumerateApiLayerProperties
#define xrEnumerateInstanceExtensionProperties OPENXR_xrEnumerateInstanceExtensionProperties
#define xrCreateInstance                       OPENXR_xrCreateInstance

#ifdef __cplusplus
}
#endif

#endif // SDL_openxrdyn_h_
