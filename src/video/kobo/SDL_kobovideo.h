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

/* Kobo (MediaTek hwtcon) e-ink video driver -- ADDED for the Kobo port,
   not part of upstream SDL2. */

#include "../../SDL_internal.h"

#ifndef SDL_kobovideo_h_
#define SDL_kobovideo_h_

#include "../SDL_sysvideo.h"

/* Per-device state: the mmap'd Linux framebuffer + hwtcon fd. */
typedef struct
{
    int      fd;       /* open fd on /dev/fb0 (also the hwtcon ioctl target) */
    Uint8   *map;      /* mmap of the framebuffer scanout memory             */
    size_t   map_len;  /* fb_fix_screeninfo.smem_len                         */
    int      width;    /* fb_var_screeninfo.xres                             */
    int      height;   /* fb_var_screeninfo.yres                             */
    int      pitch;    /* fb_fix_screeninfo.line_length (bytes)              */
    Uint32   bpp;      /* fb_var_screeninfo.bits_per_pixel (expected 32)     */
    Uint32   marker;   /* rolling hwtcon update marker                       */
} KOBO_VideoData;

/* Hints controlling e-ink refresh. Honored from both the environment and
   SDL_SetHint() at runtime (SDL hint semantics give us "env var AND in code"
   for free). */
#define KOBO_HINT_WAVEFORM "SDL_KOBO_WAVEFORM"           /* AUTO,DU,GC16,GL16,A2,REAGL,... */
#define KOBO_HINT_FULL     "SDL_KOBO_FULL_REFRESH"       /* boolean: force FULL updates    */
#define KOBO_HINT_WAIT     "SDL_KOBO_WAIT_FOR_COMPLETE"  /* boolean: block until displayed */

#endif /* SDL_kobovideo_h_ */

/* vi: set ts=4 sw=4 expandtab: */
