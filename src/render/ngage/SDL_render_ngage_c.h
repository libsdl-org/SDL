/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifndef ngage_video_render_ngage_c_h
#define ngage_video_render_ngage_c_h

#define NGAGE_SCREEN_WIDTH  176
#define NGAGE_SCREEN_HEIGHT 208

#ifdef __cplusplus
extern "C" {
#endif

#include "../SDL_sysrender.h"

typedef struct NGAGE_RendererData
{
    SDL_Rect *viewport;

} NGAGE_RendererData;

typedef struct NGAGE_Vertex
{
    int x;
    int y;

    struct
    {
        Uint8 a;
        Uint8 r;
        Uint8 g;
        Uint8 b;

    } color;

} NGAGE_Vertex;

typedef struct CFbsBitmap CFbsBitmap;

typedef struct NGAGE_TextureData
{
    CFbsBitmap *bitmap;
    SDL_Surface *surface;

} NGAGE_TextureData;

typedef struct NGAGE_CopyExData
{
    SDL_Rect srcrect;
    SDL_Rect dstrect;

    int angle;

    struct
    {
        int x;
        int y;

    } center;

    SDL_FlipMode flip;

    int scale_x;
    int scale_y;

} NGAGE_CopyExData;

void NGAGE_Clear(const Uint32 color);
Uint32 NGAGE_ConvertColor(float r, float g, float b, float a, float color_scale);
bool NGAGE_Copy(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Rect *srcrect, SDL_Rect *dstrect);
bool NGAGE_CopyEx(SDL_Renderer *renderer, SDL_Texture *texture, NGAGE_CopyExData *copydata);
bool NGAGE_CreateTextureData(NGAGE_TextureData *data, const int width, const int height);
void NGAGE_DestroyTextureData(NGAGE_TextureData *data);
void NGAGE_DrawLines(NGAGE_Vertex *verts, const int count);
void NGAGE_DrawPoints(NGAGE_Vertex *verts, const int count);
void NGAGE_FillRects(NGAGE_Vertex *verts, const int count);
void NGAGE_Flip(void);
void NGAGE_SetClipRect(const SDL_Rect *rect);
void NGAGE_SetDrawColor(const Uint32 color);
void NGAGE_PumpEventsInternal(void);
void NGAGE_SuspendScreenSaverInternal(bool suspend);

#ifdef __cplusplus
}
#endif

#endif // ngage_video_render_ngage_c_h
