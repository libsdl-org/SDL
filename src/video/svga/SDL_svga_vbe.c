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

#ifdef SDL_VIDEO_DRIVER_SVGA

#include "SDL_svga_vbe.h"

#include <dpmi.h>
#include <go32.h>
#include <libc/dosio.h>
#include <string.h>

/* Check the DPMI registers for an error after a VBE function call. */
/* Returns -128 if the function is not supported or the negated VBE error code. */
/* TODO: Create named macro definitions for possible error values. */
#define RETURN_IF_VBE_CALL_FAILED(regs) \
    if ((regs).h.al != 0x4F) return SDL_MIN_SINT8; \
    if ((regs).h.ah != 0) return -(regs).h.ah;

int
SVGA_GetVBEInfo(VBEInfo * info)
{
    __dpmi_regs r;

    dosmemput("VBE2", 4, __tb);

    r.x.ax = 0x4F00;
    r.x.es = __tb_segment;
    r.x.di = __tb_offset;

    __dpmi_int(0x10, &r);

    RETURN_IF_VBE_CALL_FAILED(r);

    dosmemget(__tb, sizeof(*info), info);

    /* Unexpected signature */
    if (strncmp(info->vbe_signature, "VESA", 4) != 0) {
        return -1;
    }

    return 0;
}

VBEMode
SVGA_GetVBEModeAtIndex(const VBEInfo * info, int index)
{
    VBEMode mode;

    dosmemget(VBE_FLAT_PTR(info->video_mode_ptr) + index * sizeof(mode), sizeof(mode), &mode);

    return mode;
}

int
SVGA_GetVBEModeInfo(VBEMode mode, VBEModeInfo * info)
{
    __dpmi_regs r;

    r.x.ax = 0x4F01;
    r.x.cx = mode;
    r.x.es = __tb_segment;
    r.x.di = __tb_offset;

    __dpmi_int(0x10, &r);

    RETURN_IF_VBE_CALL_FAILED(r);

    dosmemget(__tb, sizeof(*info), info);

    return 0;
}

int
SVGA_GetCurrentVBEMode(VBEMode * mode, VBEModeInfo * info)
{
    __dpmi_regs r;

    r.x.ax = 0x4F03;

    __dpmi_int(0x10, &r);

    RETURN_IF_VBE_CALL_FAILED(r);

    *mode = r.x.bx & 0x3FFF; /* High bits are status flags. */

    if (!info) {
        return 0;
    }

    return SVGA_GetVBEModeInfo(*mode, info);
}

int
SVGA_SetVBEMode(VBEMode mode)
{
    __dpmi_regs r;

    mode &= 0x01FF; /* Mode number bit mask. */
    mode |= 0x4000; /* Linear frame buffer flag. */

    r.x.ax = 0x4F02;
    r.x.bx = mode;
    r.x.es = 0;
    r.x.di = 0;

    __dpmi_int(0x10, &r);

    RETURN_IF_VBE_CALL_FAILED(r);

    return 0;
}

int
SVGA_GetState(void **state)
{
    size_t state_size;
    __dpmi_regs r;

    r.x.ax = 0x4F04;
    r.h.dl = 0;   /* Get state buffer size. */
    r.x.cx = 0xF; /* All states. */

    __dpmi_int(0x10, &r);

    RETURN_IF_VBE_CALL_FAILED(r);

    /* Calculate state buffer size. */
    state_size = r.x.bx * 64;

    /* Check that transfer buffer is big enough. */
    if (state_size > __tb_size) {
        return SDL_OutOfMemory();
    }

    r.h.dl = 1; /* Save state. */
    r.x.es = __tb_segment;
    r.x.bx = __tb_offset;

    __dpmi_int(0x10, &r);

    RETURN_IF_VBE_CALL_FAILED(r);

    /* Allocate state buffer. */
    *state = SDL_calloc(1, state_size);
    if (!*state) {
        return SDL_OutOfMemory();
    }

    /* Copy state data from DOS transfer buffer. */
    dosmemget(__tb, state_size, *state);

    return state_size;
}

int
SVGA_SetState(const void *state, size_t size)
{
    __dpmi_regs r;

    /* Check that transfer buffer is big enough. */
    if (size > __tb_size) {
        return SDL_OutOfMemory();
    }

    /* Copy state data into DOS transfer buffer. */
    dosmemput(state, size, __tb);

    r.x.ax = 0x4F04;
    r.h.dl = 2;   /* Restore state. */
    r.x.cx = 0xF; /* All states. */
    r.x.es = __tb_segment;
    r.x.bx = __tb_offset;

    __dpmi_int(0x10, &r);

    RETURN_IF_VBE_CALL_FAILED(r);

    return 0;
}

int
SVGA_SetDisplayStart(int x, int y)
{
    __dpmi_regs r;

    r.x.ax = 0x4F07;
    r.x.bx = 0x80; /* Set start and wait for vertical retrace. */
    r.x.cx = x;
    r.x.dx = y;

    __dpmi_int(0x10, &r);

    RETURN_IF_VBE_CALL_FAILED(r);

    return 0;
}

int
SVGA_SetDACPaletteFormat(int bits)
{
    __dpmi_regs r;

    r.x.ax = 0x4F08;
    r.h.bl = 0; /* Flag to set format */
    r.h.bh = bits;

    __dpmi_int(0x10, &r);

    if (r.h.al != 0x4F) {
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "VBE: Failed to set DAC palette format to %d bits, got al=%02x ah=%02x bh=%d; will assume 6-bit color channels",
                     (int)bits, (int)r.h.al, (int)r.h.ah, (int)r.h.bh);
    }

    RETURN_IF_VBE_CALL_FAILED(r);

    return r.h.bh;
}

int
SVGA_GetPaletteData(SDL_Color * colors, int num_colors, Uint8 palette_dac_bits)
{
    int i;
    __dpmi_regs r;

    r.x.ax = 0x4F09;
    r.h.bl = 1; /* Flag to get colors */
    r.x.cx = num_colors;
    r.x.dx = 0; /* First color */
    r.x.es = __tb_segment;
    r.x.di = __tb_offset;

    __dpmi_int(0x10, &r);

    RETURN_IF_VBE_CALL_FAILED(r);

    dosmemget(__tb, num_colors * sizeof(*colors), colors);

    /*
       Palette color components are stored in BGRA order, where
       A is the alignment byte.
    */
    for (i = 0; i < num_colors; i++) {
        Uint8 temp = colors[i].r;
        colors[i].r = colors[i].b;
        colors[i].b = temp;
        colors[i].a = SDL_ALPHA_OPAQUE;
        if (palette_dac_bits == 6) {
            colors[i].r <<= 2;
            colors[i].g <<= 2;
            colors[i].b <<= 2;
        }
    }

    return 0;
}

int SVGA_SetPaletteData(SDL_Color *colors, int num_colors, Uint8 palette_dac_bits)
{
    int i;
    __dpmi_regs r;
    Uint8 bgr_colors[256 * 4];

    if (num_colors > 256) {
        SDL_SetError("Too many palette colors");
        return -1;
    }

    if (palette_dac_bits == 8) {
        for (i = 0; i < num_colors; i++) {
            bgr_colors[i * 4] = colors[i].b;
            bgr_colors[i * 4 + 1] = colors[i].g;
            bgr_colors[i * 4 + 2] = colors[i].r;
            bgr_colors[i * 4 + 3] = 0;
        }
    } else {
        for (i = 0; i < num_colors; i++) {
            bgr_colors[i * 4] = colors[i].b >> 2;
            bgr_colors[i * 4 + 1] = colors[i].g >> 2;
            bgr_colors[i * 4 + 2] = colors[i].r >> 2;
            bgr_colors[i * 4 + 3] = 0;
        }
    }

    r.x.ax = 0x4F09;
    /*
       Flag to set colors.

       Note that according to https://www.phatcode.net/res/221/files/vbe20.pdf 4.12 (page 37),
       on some systems this flag should be 0x80 and that can be determined using the Capabilities
       field, which we do not yet do.
     */
    r.h.bl = 0x0;
    r.x.cx = num_colors;
    r.x.dx = 0; /* First color */
    r.x.es = __tb_segment;
    r.x.di = __tb_offset;

    dosmemput(bgr_colors, num_colors * 4, __tb);
    __dpmi_int(0x10, &r);

    RETURN_IF_VBE_CALL_FAILED(r);

    return 0;
}

SDL_PixelFormatEnum
SVGA_GetPixelFormat(const VBEModeInfo * info)
{
    if (info->memory_model == VBE_MEM_MODEL_PACKED) {
        switch (info->bits_per_pixel) {
            /* FIXME: Is it MSB or LSB? */
            case 1: return SDL_PIXELFORMAT_INDEX1MSB;
            case 4: return SDL_PIXELFORMAT_INDEX4MSB;
            case 8: return SDL_PIXELFORMAT_INDEX8;
        }
    }
    if (info->memory_model == VBE_MEM_MODEL_DIRECT) {
        Uint32 r = ~(~(Uint32)0 << info->red_mask_size) << info->red_field_position;
        Uint32 g = ~(~(Uint32)0 << info->green_mask_size) << info->green_field_position;
        Uint32 b = ~(~(Uint32)0 << info->blue_mask_size) << info->blue_field_position;
        return SDL_MasksToPixelFormatEnum(info->bits_per_pixel, r, g, b, 0);
    }
    return SDL_PIXELFORMAT_UNKNOWN;
}

#endif /* SDL_VIDEO_DRIVER_SVGA */

/* vi: set ts=4 sw=4 expandtab: */
