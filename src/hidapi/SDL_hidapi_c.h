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
#include "SDL_internal.h"
#define Please_use_SDL_memcpy_instead memcpy
#define Please_use_SDL_strchr_instead strchr
#define Please_use_SDL_strcmp_instead strcmp
#define Please_use_SDL_wcsdup_instead wcsdup
#define Please_use_SDL_wcscmp_instead wcscmp
#define Please_use_SDL_strdup_instead strdup
#define Please_use_SDL_calloc_instead calloc
#define Please_use_SDL_strtol_instead strtol
#define Please_use_SDL_sscanf_instead sscanf
#define Please_use_SDL_memset_instead memset
#define Please_use_SDL_memmove_instead memmove
#define Please_use_SDL_strlen_instead strlen
#define Please_use_SDL_snprintf_instead snprintf
#define Please_use_SDL_vsnprintf_instead vsnprintf


/* Return true if the HIDAPI should ignore a device during enumeration */
extern SDL_bool SDL_HIDAPI_ShouldIgnoreDevice(int bus_type, Uint16 vendor_id, Uint16 product_id, Uint16 usage_page, Uint16 usage);

#ifdef SDL_JOYSTICK_HIDAPI
#ifdef HAVE_LIBUSB
#define HAVE_ENABLE_GAMECUBE_ADAPTORS
#endif

#ifdef HAVE_ENABLE_GAMECUBE_ADAPTORS
extern void SDL_EnableGameCubeAdaptors(void);
#endif
#endif /* SDL_JOYSTICK_HIDAPI */
