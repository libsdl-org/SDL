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
 * Standalone driver for src/joystick/hidapi/SDL_report_descriptor.c, to make
 * it easier to experiment with HID report descriptor parsing.
 */

/* Hack #1: avoid inclusion of SDL_main.h by SDL_internal.h */
#define SDL_main_h_

/* Hack #2: avoid dynapi renaming (must be done before #include <SDL3/SDL.h>) */
#include "../src/dynapi/SDL_dynapi.h"
#ifdef SDL_DYNAMIC_API
#undef SDL_DYNAMIC_API
#endif
#define SDL_DYNAMIC_API 0

#ifdef HAVE_BUILD_CONFIG
#include "../src/SDL_internal.h"
#endif

/* Hack #3: undo Hack #1 */
#ifdef SDL_main_h_
#undef SDL_main_h_
#endif
#ifdef SDL_MAIN_NOIMPL
#undef SDL_MAIN_NOIMPL
#endif

#include <SDL3/SDL_main.h>

#include "../src/joystick/hidapi/SDL_report_descriptor.h"
#include "../src/joystick/hidapi/SDL_report_descriptor.c"

int main(int argc, char *argv[])
{
    const char *file = argv[1];
    if (argc < 2) {
        SDL_Log("Usage: %s file", argv[0]);
        return 1;
    }

    size_t descriptor_size = 0;
    Uint8 *descriptor = SDL_LoadFile(file, &descriptor_size);
    if (!descriptor) {
        SDL_Log("Couldn't load %s: %s", file, SDL_GetError());
        return 2;
    }

    DescriptorContext ctx;
    if (!ParseDescriptor(&ctx, descriptor, descriptor_size)) {
        SDL_Log("Couldn't parse %s: %s", file, SDL_GetError());
        return 3;
    }
    return 0;
}
