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

// We only build the real source if the EnableTestFFmpeg trait is enabled, since it requires the FFmpeg headers.
// The Vulkan code paths are only enabled if Vulkan support for the video subsystem is enabled as well.

#ifdef SDL_SWIFTPM_TEST_FFMPEG_ENABLED
#ifdef SDL_SWIFTPM_VIDEO_VULKAN_ENABLED
#define FFMPEG_VULKAN_SUPPORT 1
#endif
#include "../../../../test/testffmpeg_vulkan.c"
#endif
