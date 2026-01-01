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

#ifdef HAVE_LIBTHAI_H

#include "SDL_libthai.h"

#ifdef SDL_LIBTHAI_DYNAMIC
SDL_ELF_NOTE_DLOPEN(
    "Thai",
    "Thai language support",
    SDL_ELF_NOTE_DLOPEN_PRIORITY_SUGGESTED,
    SDL_LIBTHAI_DYNAMIC
)
#endif


SDL_LibThai *SDL_LibThai_Create(void)
{
    SDL_LibThai *th;

    th = (SDL_LibThai *)SDL_malloc(sizeof(SDL_LibThai));
    if (!th) {
        return NULL;
    }

#ifdef SDL_LIBTHAI_DYNAMIC
    #define SDL_LIBTHAI_LOAD_SYM(a, x, n, t) x = ((t)SDL_LoadFunction(a->lib, n)); if (!x) { SDL_UnloadObject(a->lib); SDL_free(a); return NULL; }

    th->lib = SDL_LoadObject(SDL_LIBTHAI_DYNAMIC);
    if (!th->lib) {
        SDL_free(th);
        return NULL;
    }

    SDL_LIBTHAI_LOAD_SYM(th, th->make_cells, "th_make_cells", SDL_LibThaiMakeCells);
#else
    th->make_cells = th_make_cells;
#endif

    return th;
}

void SDL_LibThai_Destroy(SDL_LibThai *th)
{
    if (!th) {
        return;
    }

#ifdef SDL_LIBTHAI_DYNAMIC
    SDL_UnloadObject(th->lib);
#endif

    SDL_free(th);
}

#endif
