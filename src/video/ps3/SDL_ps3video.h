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

#ifndef _SDL_ps3video_h
#define _SDL_ps3video_h

#include "../SDL_sysvideo.h"

#include <rsx/gcm_sys.h>
#include <sysutil/video.h>

#ifndef CELL_OK
#define CELL_OK 0
#endif
#ifndef CELL_EINVAL
#define CELL_EINVAL 0x80010002
#endif
#ifndef CELL_EBUSY
#define CELL_EBUSY 0x8001000B
#endif
#ifndef CELL_ETIMEDOUT
#define CELL_ETIMEDOUT 0x80010006
#endif
#ifndef CELL_ENOMEM
#define CELL_ENOMEM 0x80010004
#endif
#ifndef CELL_EPERM
#define CELL_EPERM 0x80010001
#endif

#define DEFAULT_CB_SIZE                      0x80000        // 512Kb default command buffer size
#define HOST_ADDR_ALIGNMENT                  (1024*1024)
#define HOST_SIZE                            (32*1024*1024)

/* Debugging
 * 0: No debug messages
 * 1: Video debug messages
 * 2: SPE debug messages
 * 3: Memory adresses
 */
#if defined(DEBUG) || defined(_DEBUG)
#define VIDEO_DEBUG_LEVEL 1
#else
#define VIDEO_DEBUG_LEVEL 0
#endif

#ifdef VIDEO_DEBUG_LEVEL
#define deprintf(level, fmt, args...)                 \
    do {                                              \
        if ((unsigned)(level) <= VIDEO_DEBUG_LEVEL) { \
            fprintf(stdout, fmt, ##args);             \
            fflush(stdout);                           \
        }                                             \
    } while (0)
#else
#define deprintf(level, fmt, args...)
#endif

/* Private RSX data */
typedef struct SDL_VideoData
{
    // Context to keep track of the RSX buffer.
    gcmContextData *_CommandBuffer;

    bool _keyboardConnected;
    Uint32 _keyboardMapping;

    bool _mouseConnected;
    Uint8 _mouseButtons;
} SDL_VideoData;

typedef struct SDL_DisplayModeData
{
    videoConfiguration vconfig;
} PS3_DisplayModeData;

typedef struct SDL_WindowData
{

} SDL_WindowData;

#endif // _SDL_ps3video_h
