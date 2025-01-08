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

/* *INDENT-OFF* */ // clang-format off

#include "openxr/openxr.h"

#ifndef SDL_OPENXR_SYM
#define SDL_OPENXR_SYM(name)
#endif

#ifndef SDL_OPENXR_INSTANCE_SYM
#define SDL_OPENXR_INSTANCE_SYM(name)
#endif

SDL_OPENXR_SYM(xrGetInstanceProcAddr)
SDL_OPENXR_SYM(xrCreateInstance)
SDL_OPENXR_SYM(xrEnumerateInstanceExtensionProperties)
SDL_OPENXR_INSTANCE_SYM(xrEnumerateSwapchainFormats)
SDL_OPENXR_INSTANCE_SYM(xrCreateSession)
SDL_OPENXR_INSTANCE_SYM(xrGetSystem)
SDL_OPENXR_INSTANCE_SYM(xrCreateSwapchain)
SDL_OPENXR_INSTANCE_SYM(xrEnumerateSwapchainImages)
SDL_OPENXR_INSTANCE_SYM(xrDestroyInstance)
SDL_OPENXR_INSTANCE_SYM(xrDestroySwapchain)

#undef SDL_OPENXR_SYM
#undef SDL_OPENXR_INSTANCE_SYM

/* *INDENT-ON* */ // clang-format on
