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
#include "SDL_internal.h"

#ifndef SDL_pumpkinvideo_h_
#define SDL_pumpkinvideo_h_

#include "../SDL_sysvideo.h"

struct SDL_VideoData
{
    SDL_Window *window;      // the single application window, or NULL
    int screen_w;            // task screen width, in host pixels
    int screen_h;            // task screen height, in host pixels
    int encoding;            // SURFACE_ENCODING_* of the task screen
    SDL_PixelFormat format;  // pixel format handed to the application
    bool quit_sent;

    // input state, polled with pumpkin_status()
    Uint32 key_mask;
    Uint32 mod_mask;
    bool ext_down[SDL_SCANCODE_COUNT];  // scancodes reported down through the ASCII key mask
    int pen_x;
    int pen_y;
    bool pen_valid;
    Uint8 pen_buttons;                  // SDL mouse buttons currently held
};

struct SDL_WindowData
{
    SDL_Surface *shadow;     // framebuffer handed to the application
};

extern bool PUMPKIN_RefreshScreenInfo(SDL_VideoDevice *_this);
extern void PUMPKIN_CheckScreenSize(SDL_VideoDevice *_this);

#endif // SDL_pumpkinvideo_h_
