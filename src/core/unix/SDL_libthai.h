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

#ifndef SDL_libthai_h_
#define SDL_libthai_h_

#ifdef HAVE_LIBTHAI_H
#include <thai/thcell.h>

typedef size_t (*SDL_LibThaiMakeCells)(const thchar_t *s, size_t, struct thcell_t cells[], size_t *, int);

typedef struct SDL_LibThai {
    SDL_SharedObject *lib;
 
    SDL_LibThaiMakeCells make_cells;
} SDL_LibThai;

extern SDL_LibThai *SDL_LibThai_Create(void);
extern void SDL_LibThai_Destroy(SDL_LibThai *th);

#endif // HAVE_LIBTHAI_H

#endif // SDL_libthai_h_
