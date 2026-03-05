/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#ifndef SDL_svga_video_h_
#define SDL_svga_video_h_

#include "../SDL_sysvideo.h"

#include "SDL_svga_vbe.h"

typedef struct
{
    VBEInfo vbe_info;
    VBEMode original_mode;
    void *original_state;
    size_t state_size;
} SDL_DeviceData;

typedef struct
{
    VBEMode vbe_mode;
    VBEFarPtr framebuffer_phys_addr;
} SDL_DisplayModeData;

typedef struct
{
    SDL_Palette *last_palette;
    Uint32 last_palette_version;
    Uint32 framebuffer_linear_addr;
    int framebuffer_selector;
    SDL_bool framebuffer_page;
    Uint8 palette_dac_bits;
} SDL_WindowData;

#endif /* SDL_svga_video_h_ */

/* vi: set ts=4 sw=4 expandtab: */
