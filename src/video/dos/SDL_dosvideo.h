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

#ifndef SDL_dosvideo_h
#define SDL_dosvideo_h

#include "../../core/dos/SDL_dos.h"
#include "../SDL_sysvideo.h"
#include "SDL_internal.h"

struct SDL_DisplayModeData
{
    // we can add more fields to this, if we want them, later.
    Uint16 mode_id;
    Uint16 attributes;
    Uint16 pitch;
    Uint16 w;
    Uint16 h;
    Uint8 num_planes;
    Uint8 bpp;
    Uint8 memory_model;
    Uint8 num_image_pages;
    Uint8 red_mask_size;
    Uint8 red_mask_pos;
    Uint8 green_mask_size;
    Uint8 green_mask_pos;
    Uint8 blue_mask_size;
    Uint8 blue_mask_pos;
    Uint32 physical_base_addr;

    // VBE 1.2 banked framebuffer fields (used when LFB is not available)
    bool has_lfb;           // true if linear framebuffer is available (VBE 2.0+)
    Uint16 win_granularity; // bank positioning granularity in KB
    Uint16 win_size;        // window size in KB (typically 64)
    Uint16 win_a_segment;   // real-mode segment of window A (typically 0xA000)
    Uint32 win_func_ptr;    // real-mode far pointer to bank-switch function
    Uint8 win_a_attributes; // window A capabilities
};

struct SDL_VideoData
{
    __dpmi_meminfo mapping; // video memory mapping.
    SDL_DisplayMode current_mode;
    DOS_InterruptHook keyboard_interrupt_hook;
    Uint32 palette_version;       // tracks SDL_Palette::version to detect changes
    Uint16 original_vbe_mode;     // VBE mode number at startup
    void *vbe_state_buffer;       // saved VBE state (from VBE 0x4F04)
    Uint32 vbe_state_buffer_size; // size of the state buffer

    // Mouse sensitivity (mickeys per pixel), queried from INT 33h function 0x1B
    float mickeys_per_hpixel; // horizontal mickeys per pixel (default: 8)
    float mickeys_per_vpixel; // vertical mickeys per pixel (default: 16)

    // Page-flipping (double-buffering) state
    int current_page;         // 0 or 1: which page is currently displayed
    Uint32 page_offset[2];    // byte offset of each page within video memory
    bool page_flip_available; // true if mode supports double-buffering
    bool banked_mode;         // true if current mode uses banked (not LFB) access
};

struct SDL_DisplayData
{
    int unused; // for now
};

struct SDL_WindowData
{
    int framebuffer_vsync;
};

#endif // SDL_dosvideo_h
