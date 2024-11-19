/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#if defined(SDL_FSOPS_DUMMY)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// System dependent filesystem routines

#include "../SDL_sysfilesystem.h"

bool SDL_SYS_EnumerateDirectory(const char *path, const char *dirname, SDL_EnumerateDirectoryCallback cb, void *userdata)
{
    return SDL_Unsupported();
}

bool SDL_SYS_RemovePath(const char *path)
{
    return SDL_Unsupported();
}

bool SDL_SYS_RenamePath(const char *oldpath, const char *newpath)
{
    return SDL_Unsupported();
}

bool SDL_SYS_CopyFile(const char *oldpath, const char *newpath)
{
    return SDL_Unsupported();
}

bool SDL_SYS_CreateDirectory(const char *path)
{
    return SDL_Unsupported();
}

bool SDL_SYS_GetPathInfo(const char *path, SDL_PathInfo *info)
{
    return SDL_Unsupported();
}

struct SDL_MemoryMappedFile {};

SDL_MemoryMappedFile *SDL_SYS_MemoryMapFile(const char *file, size_t offset) {
    (void)file; (void)offset;
    SDL_Unsupported();
    return NULL;
}

bool SDL_SYS_UnmapMemoryFile(SDL_MemoryMappedFile *mmfile) {
    (void)mmfile;
    return SDL_Unsupported();
}

void *SDL_SYS_GetMemoryMappedData(const SDL_MemoryMappedFile *mmfile, size_t *datasize) {
    (void)mmfile; (void)datasize;
    SDL_Unsupported();
    return NULL;
}

#endif // SDL_FSOPS_DUMMY

