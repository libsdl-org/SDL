/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

/*
 * @author Mark Callow, www.edgewise-consulting.com. Based on Jacob Lifshay's
 * SDL_x11vulkan.h.
 */

#include "SDL_internal.h"

#ifndef SDL_androidvulkan_h_
#define SDL_androidvulkan_h_

#include "../SDL_vulkan_internal.h"
#include "../SDL_sysvideo.h"

#if defined(SDL_VIDEO_VULKAN) && defined(SDL_VIDEO_DRIVER_ANDROID)

int Android_Vulkan_LoadLibrary(SDL_VideoDevice *_this, const char *path);
void Android_Vulkan_UnloadLibrary(SDL_VideoDevice *_this);
SDL_bool Android_Vulkan_GetInstanceExtensions(SDL_VideoDevice *_this,
                                              unsigned *count,
                                              const char **names);
SDL_bool Android_Vulkan_CreateSurface(SDL_VideoDevice *_this,
                                      SDL_Window *window,
                                      VkInstance instance,
                                      VkSurfaceKHR *surface);

#endif

#endif /* SDL_androidvulkan_h_ */
