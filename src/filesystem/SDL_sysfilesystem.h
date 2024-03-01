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

#ifndef SDL_sysfilesystem_h_
#define SDL_sysfilesystem_h_

int SDL_SYS_FSenumerate(SDL_FSops *fs, const char *fullpath, const char *dirname, SDL_EnumerateCallback cb, void *userdata);
int SDL_SYS_FSremove(SDL_FSops *fs, const char *fullpath);
int SDL_SYS_FSrename(SDL_FSops *fs, const char *oldfullpath, const char *newfullpath);
int SDL_SYS_FSmkdir(SDL_FSops *fs, const char *fullpath);
int SDL_SYS_FSstat(SDL_FSops *fs, const char *fullpath, SDL_Stat *stat);

#endif

