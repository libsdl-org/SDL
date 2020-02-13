/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_RISCOS

#include "../SDL_sysvideo.h"
#include "SDL_riscosframebuffer_c.h"
#include "SDL_riscosvideo.h"

#include <kernel.h>
#include <swis.h>

#define DUMMY_SURFACE   "_SDL_DummySurface"

typedef struct {
    Uint32 size;
    Uint32 count;
    Uint32 start;
    Uint32 end;
} sprite_area;

int SDL_RISCOS_CreateWindowFramebuffer(_THIS, SDL_Window * window, Uint32 * format, void ** pixels, int *pitch)
{
    const char *sprite_name = "display";
    int width, height, size, bytesPerRow;
    unsigned int sprite_mode;
    _kernel_oserror *error;
    _kernel_swi_regs regs;
    sprite_area *buffer;
    Uint32 pixelformat;
    SDL_DisplayMode mode;

    /* Free the old framebuffer surface */
    buffer = (sprite_area *) SDL_SetWindowData(window, DUMMY_SURFACE, NULL);
    SDL_free(buffer);

    /* Create a new one */
    SDL_GetWindowSize(window, &width, &height);
    SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &mode);

    if ((SDL_ISPIXELFORMAT_PACKED(mode.format) || SDL_ISPIXELFORMAT_ARRAY(mode.format))) {
        pixelformat = mode.format;
        sprite_mode = (unsigned int)mode.driverdata;
    } else {
        pixelformat = SDL_PIXELFORMAT_BGR888;
        sprite_mode = (1 | (90 << 1) | (90 << 14) | (6 << 27));
    }

    bytesPerRow = SDL_BYTESPERPIXEL(pixelformat) * width;
    if ((bytesPerRow & 3) != 0) {
        bytesPerRow += 4 - (bytesPerRow & 3);
    }
    size = 60 + (bytesPerRow * height);

    buffer = SDL_malloc(size);
    if (!buffer) {
        SDL_OutOfMemory();
        return -1;
    }

    /* Initialise a sprite area */

    buffer->size  = size;
    buffer->start = 16;

    regs.r[0] = 256+9;
    regs.r[1] = (unsigned int)buffer;
    error = _kernel_swi(OS_SpriteOp, &regs, &regs);
    if (error != NULL) {
        SDL_SetError("Unable to initialise sprite area: %s (%i)", error->errmess, error->errnum);
        SDL_free(buffer);
        return -1;
    }

    regs.r[0] = 256+15;
    regs.r[1] = (unsigned int)buffer;
    regs.r[2] = (unsigned int)sprite_name;
    regs.r[3] = 0;
    regs.r[4] = width;
    regs.r[5] = height;
    regs.r[6] = sprite_mode;
    error = _kernel_swi(OS_SpriteOp, &regs, &regs);
    if (error != NULL) {
        SDL_SetError("Unable to create sprite: %s (%i)", error->errmess, error->errnum);
        SDL_free(buffer);
        return -1;
    }

    /* Save the info and return! */
    SDL_SetWindowData(window, DUMMY_SURFACE, buffer);
    *format = pixelformat;
    *pixels = ((Uint8 *)buffer) + 60;
    *pitch = bytesPerRow;

    return 0;
}

int SDL_RISCOS_UpdateWindowFramebuffer(_THIS, SDL_Window * window, const SDL_Rect * rects, int numrects)
{
    _kernel_swi_regs regs;
    sprite_area *buffer;

    buffer = (sprite_area *) SDL_GetWindowData(window, DUMMY_SURFACE);
    if (!buffer) {
        return SDL_SetError("Couldn't find sprite for window");
    }

    regs.r[0] = 512+52;
    regs.r[1] = (unsigned int)buffer;
    regs.r[2] = (unsigned int)buffer + buffer->start;
    regs.r[3] = window->x * 2;
    regs.r[4] = window->y * 2;
    regs.r[5] = 0x50;
    regs.r[6] = 0;
    regs.r[7] = 0;
    _kernel_swi(OS_SpriteOp, &regs, &regs);

    return 0;
}

void SDL_RISCOS_DestroyWindowFramebuffer(_THIS, SDL_Window * window)
{
    sprite_area *buffer = (sprite_area *) SDL_SetWindowData(window, DUMMY_SURFACE, NULL);
    SDL_free(buffer);
}

#endif /* SDL_VIDEO_DRIVER_RISCOS */

/* vi: set ts=4 sw=4 expandtab: */
