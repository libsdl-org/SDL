/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2010 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_internal.h"

#ifndef _SDL_PS3video_h
#define _SDL_PS3video_h

#include "../SDL_sysvideo.h"

#include <rsx/gcm_sys.h>
#include <sysutil/video_out.h>

#ifndef CELL_OK
#define CELL_OK             0
#endif
#ifndef CELL_EINVAL
#define CELL_EINVAL         0x80010002
#endif
#ifndef CELL_EBUSY
#define CELL_EBUSY          0x8001000B
#endif
#ifndef CELL_ETIMEDOUT
#define CELL_ETIMEDOUT      0x80010006
#endif
#ifndef CELL_ENOMEM
#define CELL_ENOMEM         0x80010004
#endif
#ifndef CELL_EPERM
#define CELL_EPERM          0x80010001
#endif


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
#define deprintf( level, fmt, args... ) \
    do \
{ \
    if ( (unsigned)(level) <= VIDEO_DEBUG_LEVEL ) \
    { \
        fprintf( stdout, fmt, ##args ); \
        fflush( stdout ); \
    } \
} while ( 0 )
#else
#define deprintf( level, fmt, args... )
#endif

/* Private RSX data */
typedef struct SDL_DeviceData
{
    // Context to keep track of the RSX buffer.
    gcmContextData *_CommandBuffer;

    bool _keyboardConnected;
    Uint32 _keyboardMapping;

    bool _mouseConnected;
    Uint8 _mouseButtons;
} SDL_DeviceData;

typedef struct SDL_DisplayModeData
{
    videoOutConfiguration vconfig;
} PS3_DisplayModeData;

typedef struct SDL_WindowData
{

} SDL_WindowData;

#endif // _SDL_PS3video_h

/* vi: set ts=4 sw=4 expandtab: */
