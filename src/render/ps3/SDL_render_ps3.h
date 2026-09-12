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

#ifndef SDL_render_ps3_h_
#define SDL_render_ps3_h_

#include <rsx/rsx.h>

#define FRAME_BUFFER_COUNT              2
#define GCM_PREPARED_BUFFER_INDEX       5
#define GCM_BUFFER_STATUS_INDEX         66
#define GCM_WAIT_LABEL_INDEX            255

#define MAX_BUFFER_QUEUE_SIZE           1

#define BUFFER_IDLE                     0
#define BUFFER_BUSY                     1

// Max sprites to be displayed on the screen.
// This possible could issue artifacts on the screen
// when a lot of scprites to be displayed.
#define QUAD_RING_SIZE 1024

typedef struct PS3_DrawStateCache
{
    const SDL_Rect *viewport;
    SDL_Rect cliprect;
    bool cliprect_enabled_dirty;
    bool cliprect_enabled;
    bool cliprect_dirty;
    SDL_Color color;
} PS3_DrawStateCache;

typedef struct PS3_CopyData
{
    SDL_FRect srcRect;
    SDL_FRect dstRect;
} PS3_CopyData;

typedef struct TexVertex
{
    float x, y, z;
    float u, v;
} TexVertex;

typedef struct ColorVertex
{
    float x, y, z;
    float r, g, b, a;
} ColorVertex;

typedef struct QuadSlot
{
    TexVertex *vbo;
    u32 offset;
} QuadSlot;

typedef struct PS3_TextureData
{
    SDL_Surface *surface;
    gcmTexture   rsx_texture;
    u32          offset;
} PS3_TextureData;

typedef struct PS3_RenderData
{
    gcmSurface surface;
    u32 fbOnDisplay;
    u32 fbFlipped;
    bool fbOnFlip;
    u32 curr_fb;
    sys_event_queue_t flipEventQueue;
    sys_event_port_t flipEventPort;
    u32 color_offset[FRAME_BUFFER_COUNT];
    u32 *color_buffer[FRAME_BUFFER_COUNT];

    u32 color_pitch;
    u32 depth_offset;
    u32 depth_pitch;
    u32 screenw, screenh;
    gcmContextData *context; // Context to keep track of the RSX buffer
    PS3_DrawStateCache drawstate;

    // Use quads array to handle drawing of
    // the same texture multiple times
    QuadSlot quad_ring[QUAD_RING_SIZE];
    u32 quad_ring_index;

    // texture
    rsxVertexProgram   *vpo;
    rsxFragmentProgram *fpo;
    void *vp_ucode;
    u32   vp_ucode_size;
    u32 fp_ucode_size;
    void *fp_ucode_rsx;
    void *fp_ucode_cpu;
    u32 fp_offset;

    // color
    rsxVertexProgram   *vpo_color;
    rsxFragmentProgram *fpo_color;
    void *vp_ucode_color;
    u32   vp_ucode_size_color;
    u32 fp_ucode_size_color;
    void *fp_ucode_cpu_color;
    u32 fp_offset_color;
    void *fp_ucode_rsx_color;

    f32 ortho_matrix[16];
} PS3_RenderData;

#endif // SDL_render_ps3_h_
