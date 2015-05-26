/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_EMSCRIPTEN

#include "SDL_emscriptenvideo.h"
#include "SDL_emscriptenframebuffer.h"


int Emscripten_CreateWindowFramebuffer(_THIS, SDL_Window * window, Uint32 * format, void ** pixels, int *pitch)
{
    SDL_Surface *surface;
    const Uint32 surface_format = SDL_PIXELFORMAT_BGR888;
    int w, h;
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;

    /* Free the old framebuffer surface */
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    surface = data->surface;
    SDL_FreeSurface(surface);

    /* Create a new one */
    SDL_PixelFormatEnumToMasks(surface_format, &bpp, &Rmask, &Gmask, &Bmask, &Amask);
    SDL_GetWindowSize(window, &w, &h);

    surface = SDL_CreateRGBSurface(0, w, h, bpp, Rmask, Gmask, Bmask, Amask);
    if (!surface) {
        return -1;
    }

    /* Save the info and return! */
    data->surface = surface;
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return 0;
}

int Emscripten_UpdateWindowFramebuffer(_THIS, SDL_Window * window, const SDL_Rect * rects, int numrects)
{
    SDL_Surface *surface;

    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    surface = data->surface;
    if (!surface) {
        return SDL_SetError("Couldn't find framebuffer surface for window");
    }

    /* Send the data to the display */

    EM_ASM_INT({
        //TODO: don't create context every update
        var ctx = Module['canvas'].getContext('2d');

        //library_sdl.js SDL_UnlockSurface
        var image = ctx.createImageData($0, $1);
        var data = image.data;
        var src = $2 >> 2;
        var dst = 0;
        var isScreen = true;
        var num;
        if (typeof CanvasPixelArray !== 'undefined' && data instanceof CanvasPixelArray) {
            // IE10/IE11: ImageData objects are backed by the deprecated CanvasPixelArray,
            // not UInt8ClampedArray. These don't have buffers, so we need to revert
            // to copying a byte at a time. We do the undefined check because modern
            // browsers do not define CanvasPixelArray anymore.
            num = data.length;
            while (dst < num) {
                var val = HEAP32[src]; // This is optimized. Instead, we could do {{{ makeGetValue('buffer', 'dst', 'i32') }}};
                data[dst  ] = val & 0xff;
                data[dst+1] = (val >> 8) & 0xff;
                data[dst+2] = (val >> 16) & 0xff;
                data[dst+3] = isScreen ? 0xff : ((val >> 24) & 0xff);
                src++;
                dst += 4;
            }
        } else {
            var data32 = new Uint32Array(data.buffer);
            num = data32.length;
            if (isScreen) {
                while (dst < num) {
                    // HEAP32[src++] is an optimization. Instead, we could do {{{ makeGetValue('buffer', 'dst', 'i32') }}};
                    data32[dst++] = HEAP32[src++] | 0xff000000;
                }
            } else {
                while (dst < num) {
                    data32[dst++] = HEAP32[src++];
                }
            }
        }

        ctx.putImageData(image, 0, 0);
        return 0;
    }, surface->w, surface->h, surface->pixels);

    /*if (SDL_getenv("SDL_VIDEO_Emscripten_SAVE_FRAMES")) {
        static int frame_number = 0;
        char file[128];
        SDL_snprintf(file, sizeof(file), "SDL_window%d-%8.8d.bmp",
                     SDL_GetWindowID(window), ++frame_number);
        SDL_SaveBMP(surface, file);
    }*/
    return 0;
}

void Emscripten_DestroyWindowFramebuffer(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    SDL_FreeSurface(data->surface);
    data->surface = NULL;
}

#endif /* SDL_VIDEO_DRIVER_EMSCRIPTEN */

/* vi: set ts=4 sw=4 expandtab: */
