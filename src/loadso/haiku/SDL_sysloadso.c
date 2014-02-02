/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_LOADSO_HAIKU

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent library loading routines                           */

#include <stdio.h>
#include <os/kernel/image.h>

#include "SDL_loadso.h"

void *
SDL_LoadObject(const char *sofile)
{
    void *handle = NULL;
    image_id library_id = load_add_on(sofile);
    if (library_id < 0) {
        SDL_SetError(strerror((int) library_id));
    } else {
        handle = (void *) (library_id);
    }
    return (handle);
}

void *
SDL_LoadFunction(void *handle, const char *name)
{
    void *sym = NULL;
    image_id library_id = (image_id) handle;
    status_t rc =
        get_image_symbol(library_id, name, B_SYMBOL_TYPE_TEXT, &sym);
    if (rc != B_NO_ERROR) {
        SDL_SetError(strerror(rc));
    }
    return (sym);
}

void
SDL_UnloadObject(void *handle)
{
    image_id library_id;
    if (handle != NULL) {
        library_id = (image_id) handle;
        unload_add_on(library_id);
    }
}

#endif /* SDL_LOADSO_HAIKU */

/* vi: set ts=4 sw=4 expandtab: */
