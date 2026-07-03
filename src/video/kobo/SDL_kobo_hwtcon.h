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

/*
 * Kobo (MediaTek "hwtcon" e-ink controller) update ABI.
 *
 * This file is ADDED for the Kobo port; it is NOT part of upstream SDL2.
 *
 * The constants/structs below are the userspace ioctl ABI of the MediaTek
 * "hwtcon" e-ink driver, as exposed by the Kobo Elipsa 2E kernel
 * (<linux/hwtcon_ioctl_cmd.h>). They are interface facts, transcribed from:
 *   - kobolabs kernel: https://github.com/kobolabs/Kobo-Reader/tree/master/hw/mt8113-elipsa2e
 *   - FBInk's reference header (eink/mtk-kobo.h), by NiLuJe (GPL-2.0)
 * Only the definitions this driver actually uses are reproduced here.
 * If this port is ever distributed, mind the GPL-2.0 origin of the ABI header
 * vs. SDL's zlib license (kernel UAPI is generally usable, but flag it).
 */

#ifndef SDL_kobo_hwtcon_h_
#define SDL_kobo_hwtcon_h_

#include <stdint.h>
#include <linux/ioctl.h>

#define HWTCON_IOCTL_MAGIC_NUMBER 'F'

/* update_mode (matches MXCFB) */
#define HWTCON_UPDATE_MODE_PARTIAL 0x0
#define HWTCON_UPDATE_MODE_FULL    0x1

/* flags */
#define HWTCON_FLAG_USE_DITHERING 0x1

/* waveform_mode values */
enum {
    HWTCON_WAVEFORM_MODE_INIT   = 0,
    HWTCON_WAVEFORM_MODE_DU     = 1,
    HWTCON_WAVEFORM_MODE_GC16   = 2,
    HWTCON_WAVEFORM_MODE_GL16   = 3,
    HWTCON_WAVEFORM_MODE_GLR16  = 4,
    HWTCON_WAVEFORM_MODE_REAGL  = 4,
    HWTCON_WAVEFORM_MODE_A2     = 6,
    HWTCON_WAVEFORM_MODE_GCK16  = 8,
    HWTCON_WAVEFORM_MODE_GLKW16 = 9,
    HWTCON_WAVEFORM_MODE_GCC16  = 10,
    HWTCON_WAVEFORM_MODE_GLRC16 = 11,
    HWTCON_WAVEFORM_MODE_AUTO   = 257
};

struct hwtcon_rect {
    uint32_t top;
    uint32_t left;
    uint32_t width;
    uint32_t height;
};

struct hwtcon_update_marker_data {
    uint32_t update_marker;
    uint32_t collision_test;
};

struct hwtcon_update_data {
    struct hwtcon_rect update_region;
    uint32_t           waveform_mode;
    uint32_t           update_mode;
    uint32_t           update_marker;
    unsigned int       flags;
    int                dither_mode;
};

/* Send an update to refresh (a region of) the e-ink panel. */
#define HWTCON_SEND_UPDATE \
    _IOW(HWTCON_IOCTL_MAGIC_NUMBER, 0x2E, struct hwtcon_update_data)

/* Block until a previously submitted update (by marker) has completed. */
#define HWTCON_WAIT_FOR_UPDATE_COMPLETE \
    _IOWR(HWTCON_IOCTL_MAGIC_NUMBER, 0x2F, struct hwtcon_update_marker_data)

#endif /* SDL_kobo_hwtcon_h_ */

/* vi: set ts=4 sw=4 expandtab: */
