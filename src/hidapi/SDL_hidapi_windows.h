/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

/* Define standard library functions in terms of SDL */
#define calloc      SDL_calloc
#define free        SDL_free
#define malloc      SDL_malloc
#define memcmp      SDL_memcmp
#define swprintf    SDL_swprintf
#define towupper    SDL_toupper
#define wcscmp      SDL_wcscmp
#define _wcsdup     SDL_wcsdup
#define wcslen      SDL_wcslen
#define wcsncpy     SDL_wcslcpy
#define wcsstr      SDL_wcsstr
#define wcstol      SDL_wcstol

#undef HIDAPI_H__
#include "windows/hid.c"
#define HAVE_PLATFORM_BACKEND 1
#define udev_ctx              1
