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
This is for generating thumbnails and videos of examples. Just include it
temporarily and let it override SDL_RenderPresent, etc, and it'll dump each
frame rendered to a new .png file.
*/

static bool SAVERENDERING_SDL_RenderPresent(SDL_Renderer *renderer)
{
    static unsigned int framenum = 0;
    SDL_Surface *surface = SDL_RenderReadPixels(renderer, NULL);
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to read pixels for frame #%u! (%s)", framenum, SDL_GetError());
    } else {
        char fname[64];
        SDL_snprintf(fname, sizeof (fname), "frame%05u.png", framenum);
        if (!SDL_SavePNG(surface, fname)) {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to save png for frame #%u! (%s)", framenum, SDL_GetError());
        }
        SDL_DestroySurface(surface);
    }

    framenum++;

    return SDL_RenderPresent(renderer);
}

#define SDL_RenderPresent SAVERENDERING_SDL_RenderPresent

