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

#if SDL_VIDEO_DRIVER_SVGA

#include "SDL_svga_vbe.h"

#include <dpmi.h>
#include <go32.h>
#include <libc/dosio.h>
#include <string.h>

/* Check the DPMI registers for an error after a VBE function call. */
/* Returns -1 if VBE is not installed or the non-zero VBE error code. */
#define RETURN_IF_VBE_CALL_FAILED(regs) \
    if ((regs).h.al != 0x4F) return -1; \
    if ((regs).h.ah != 0) return (regs).h.ah;

int
SDL_SVGA_GetVBEInfo(VBEInfo *info)
{
    __dpmi_regs r;

    dosmemput("VBE2", 4, __tb);

    r.x.ax = 0x4F00;
    r.x.di = __tb_offset;
    r.x.es = __tb_segment;

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
SDL_SVGA_GetVBEModeAtIndex(const VBEInfo *info, int index)
{
    VBEMode mode;
    VBEFarPtr ptr = info->video_mode_ptr;

    dosmemget(ptr.segment * 16 + ptr.offset + index * sizeof(mode), sizeof(mode), &mode);

    return mode;
}

int
SDL_SVGA_GetVBEModeInfo(VBEMode mode, VBEModeInfo *info)
{
    __dpmi_regs r;

    r.x.ax = 0x4F01;
    r.x.cx = mode;
    r.x.di = __tb_offset;
    r.x.es = __tb_segment;

    __dpmi_int(0x10, &r);

    RETURN_IF_VBE_CALL_FAILED(r);

    dosmemget(__tb, sizeof(*info), info);

    return 0;
}

int
SDL_SVGA_GetCurrentVBEMode(VBEMode *mode, VBEModeInfo *info)
{
    __dpmi_regs r;

    r.x.ax = 0x4F03;

    __dpmi_int(0x10, &r);

    RETURN_IF_VBE_CALL_FAILED(r);

    *mode = r.x.bx & 0x3FFF; /* High bits are flags */

    if (!info) {
        return 0;
    }

    return SDL_SVGA_GetVBEModeInfo(*mode, info);
}

#endif /* SDL_VIDEO_DRIVER_SVGA */

/* vi: set ts=4 sw=4 expandtab: */
