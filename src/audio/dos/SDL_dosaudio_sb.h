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

#ifndef SDL_dosaudio_sb_h_
#define SDL_dosaudio_sb_h_

#include "../../core/dos/SDL_dos.h"
#include "../SDL_sysaudio.h"

struct SDL_PrivateAudioData
{
    Uint8 *dma_buffer;
    size_t dma_buflen;
    int dma_channel;
    bool is_16bit;
    _go32_dpmi_seginfo dma_seginfo;
    DOS_InterruptHook interrupt_hook;

    // IRQ-driven ring buffer
    Uint8 *ring_buffer;      // pre-allocated, memory-locked ring buffer
    volatile int ring_read;  // read position (advanced by IRQ handler only)
    volatile int ring_write; // write position (advanced by audio thread only)
    int ring_size;           // total ring buffer size (power-of-2, multiple of chunk_size)
    int chunk_size;          // == device->buffer_size (one DMA half-buffer worth)
    Uint8 *staging_buffer;   // audio thread writes here, then commits to ring
};

#endif // SDL_dosaudio_sb_h_
